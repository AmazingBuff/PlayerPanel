//
// Created by AmazingBuff on 2026/10/11.
//

#include "character/puppet_driver.h"

#include "character/scene_graph_copy.h"

#include "RE/A/Actor.h"
#include "RE/B/BSAnimationGraphManager.h"
#include "RE/B/BSFadeNode.h"
#include "RE/I/IAnimationGraphManagerHolder.h"
#include "RE/A/ActorEquipManager.h"
#include "RE/P/PlayerCharacter.h"
#include "RE/T/TESObjectREFR.h"

#include <algorithm>
#include <cmath>

// winnt.h defines macros of these names, which would rewrite the REX constants used below as
// `REX::W32::0x1000`; every Windows header this file needs is already parsed at this point.
#undef MEM_COMMIT
#undef PAGE_NOACCESS

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // Full-LOD assembly window at the player (the culler must render the shell or the skin never
    // binds - run 104's born-at-depth lesson). After it the shell moves deep.
    constexpr std::uint32_t Grace_Frames = 240;
    // v6.25's proven park: directly below the player, out of every sight and interaction range.
    constexpr float Park_Depth_Offset = -8000.0f;
    // The engine's update chain overrides a one-shot SetPosition (run 38), so the park is renewed
    // on this cadence while the world runs. During the pause the world does not move anyway.
    constexpr std::uint32_t Repark_Frames = 60;
    constexpr float Max_Manual_Step = 0.1f;
    // The blend after a clip switch needs a beat before the A/B means anything; ~1 second of
    // manual ticks is well past it.
    constexpr std::uint32_t Ab_Settle_Ticks = 60;
    // Digest cadence while the manual tick owns the graph: one line per ~half second.
    constexpr std::uint32_t Digest_Ticks = 30;

    // The flash guard, the recipe character_clone.cpp validated: the fade node is pinned
    // near-invisible (not zero - a zeroed BSFadeNode is skipped by the culler and the skin never
    // binds) until the grace ends and the shell is parked.
    void suppress_fade(RE::NiAVObject* graph)
    {
        if (RE::BSFadeNode* const fade = graph ? graph->AsFadeNode() : nullptr)
        {
            fade->unk128 = 0.05f;
            fade->unk12C = 0.0f;
            fade->currentFade = 0.05f;
        }
    }

    void restore_fade(RE::NiAVObject* graph)
    {
        if (RE::BSFadeNode* const fade = graph ? graph->AsFadeNode() : nullptr)
        {
            fade->unk128 = 1.0f;
            fade->unk12C = 0.0f;
            fade->currentFade = 1.0f;
        }
    }

    bool readable(void const* address, std::size_t bytes)
    {
        if (!address || bytes == 0)
            return false;
        REX::W32::MEMORY_BASIC_INFORMATION info{};
        if (REX::W32::VirtualQuery(address, &info, sizeof(info)) == 0)
            return false;
        if (info.state != REX::W32::MEM_COMMIT || info.protect == REX::W32::PAGE_NOACCESS)
            return false;
        const auto* const begin = static_cast<const std::uint8_t*>(address);
        const auto* const end = static_cast<const std::uint8_t*>(info.baseAddress) + info.regionSize;
        return begin + bytes <= end;
    }
}

PuppetDriver& PuppetDriver::instance()
{
    static PuppetDriver s_instance;
    return s_instance;
}

void PuppetDriver::request_spawn()
{
    m_spawn_requested.store(true, std::memory_order_release);
}

void PuppetDriver::request_release()
{
    m_release_requested.store(true, std::memory_order_release);
}

bool PuppetDriver::is_owned_form(std::uint32_t form_id)
{
    return instance().m_puppet_form_id != 0 && instance().m_puppet_form_id == form_id;
}

void PuppetDriver::tick(bool paused)
{
    if (m_release_requested.exchange(false, std::memory_order_acq_rel))
        release("load-boundary");

    if (m_spawn_requested.exchange(false, std::memory_order_acq_rel))
    {
        if (!m_puppet && !paused)
            spawn();
        else if (!m_puppet)
            m_spawn_requested.store(true, std::memory_order_release);  // still loading; retry next frame
    }

    RE::Actor* const puppet = m_puppet ? m_puppet->As<RE::Actor>() : nullptr;
    if (!puppet || !readable(puppet, sizeof(RE::Actor)))
    {
        if (m_puppet)
        {
            logger::warn("SCOPY PUPPET lost reason=reference-invalid (load boundary raced the release)");
            m_puppet = nullptr;
            m_puppet_form_id = 0;
        }
        return;
    }

    if (m_grace_frames < Grace_Frames)
    {
        ++m_grace_frames;
        suppress_fade(puppet->Get3D(false));
        if (m_grace_frames == Grace_Frames)
            park("grace-complete");
        return;
    }

    if (!paused)
    {
        // The world runs: keep the shell deep (the engine's update chain overrides a one-shot
        // park), and leave the graph to the engine's own ticking.
        if (++m_repark_frames >= Repark_Frames)
        {
            m_repark_frames = 0;
            park("keep-parked");
        }
        return;
    }

    // The panel pauses the world: OUR clock drives the puppet's behaviour graph now.
    const auto now = std::chrono::steady_clock::now();
    float step = std::chrono::duration<float>(now - m_last_tick).count();
    m_last_tick = now;
    step = std::clamp(step, 0.0f, Max_Manual_Step);

    RE::BSAnimationUpdateData update{};
    update.deltaTime = step;
    update.refr = puppet;
    puppet->UpdateAnimationGraphManager(update);
    ++m_manual_ticks;

    // HKX39 equip lever: the mirror and the draw/sheathe push ride the same tick that advances
    // the graph, so the panel's equip reaction happens inside the pause instead of waiting for
    // the engine update that never comes (the HKX37 finding).
    poll_equipment(*puppet);

    // HKX39 tier-2 A/B: each switch schedules it once the blend has had time to settle.
    if (m_ab_pending && m_manual_ticks >= m_ab_deadline && m_current_generator)
    {
        m_ab_pending = false;
        logger::info("{}", measure_clip_against_puppet(*puppet, m_current_generator, m_playback, m_mapping));
    }

    if (m_manual_ticks % Digest_Ticks == 0)
    {
        std::vector<ActiveClipEntry> running;
        collect_running_clip_generators(*puppet, running);
        std::string line;
        for (std::size_t i = 0; i < running.size() && i < 6; ++i)
        {
            float time = -1.0f;
            std::uint16_t index = 0;
            if (readable(running[i].generator, sizeof(RE::hkbClipGenerator)))
            {
                time = live_clip_time(running[i].generator);
                index = running[i].generator->animationBindingIndex;
            }
            line += fmt::format("{}{}#{}({:.2f}s)", line.empty() ? "" : "; ", running[i].name, index, time);
        }
        logger::info("SCOPY PUPPET digest ticks={} step={:.4f} clips={} [{}]", m_manual_ticks, step, running.size(), line);
    }
}

void PuppetDriver::spawn()
{
    RE::PlayerCharacter* const player = RE::PlayerCharacter::GetSingleton();
    if (!player)
    {
        logger::info("SCOPY PUPPET spawn UNAVAILABLE reason=no-player (the next load boundary retries)");
        return;
    }
    RE::TESNPC* const source_base = player->GetActorBase();
    RE::TESForm* const duplicate = source_base ? source_base->CreateDuplicateForm(false, nullptr) : nullptr;
    RE::TESNPC* const puppet_base = duplicate ? duplicate->As<RE::TESNPC>() : nullptr;
    if (!puppet_base)
    {
        logger::info("SCOPY PUPPET spawn UNAVAILABLE reason=duplicate-form-failed");
        return;
    }
    puppet_base->fullName = "CharacterPanel Puppet";
    puppet_base->defaultOutfit = nullptr;

    // The v6.70 recipe verbatim: born AT the player so the engine assembles the full-LOD graph,
    // fade pinned until the grace ends, pin flags keep the shell inert the whole session.
    const RE::NiPointer<RE::TESObjectREFR> placed = player->PlaceObjectAtMe(puppet_base, false);
    RE::Actor* const puppet = placed ? placed->As<RE::Actor>() : nullptr;
    if (!puppet)
    {
        logger::info("SCOPY PUPPET spawn UNAVAILABLE reason=place-failed");
        return;
    }
    puppet->GetActorRuntimeData().boolFlags.set(RE::Actor::BOOL_FLAGS::kMovementBlocked);
    puppet->GetActorRuntimeData().boolFlags.set(RE::Actor::BOOL_FLAGS::kAttackingDisabled);
    puppet->GetActorRuntimeData().boolFlags.set(RE::Actor::BOOL_FLAGS::kCastingDisabled);
    puppet->SetActivationBlocked(true);
    puppet->StopCombat();

    m_puppet = placed;
    m_puppet_form_id = puppet->GetFormID();
    m_grace_frames = 0;
    m_repark_frames = 0;
    m_manual_ticks = 0;
    m_stall_frames = 0;
    m_stall_logged = false;
    m_ab_pending = false;
    m_ab_deadline = 0;
    m_right_form = 0;
    m_left_form = 0;
    m_equip_reactions = 0;
    m_current_generator = nullptr;
    m_playback = ClipPlayback{};
    m_mapping.clear();
    m_driver_note.clear();
    m_last_tick = std::chrono::steady_clock::now();
    logger::info("SCOPY PUPPET born form={:08x} base={:08x} grace={} frames (fade-pinned at the player; parks deep after assembly)",
        m_puppet_form_id, source_base ? source_base->GetFormID() : 0, Grace_Frames);
}

void PuppetDriver::park(char const* why)
{
    RE::Actor* const puppet = m_puppet ? m_puppet->As<RE::Actor>() : nullptr;
    RE::PlayerCharacter* const player = RE::PlayerCharacter::GetSingleton();
    if (!puppet || !player)
        return;
    const RE::NiPoint3 here = player->GetPosition();
    puppet->SetPosition(RE::NiPoint3{ here.x, here.y, here.z + Park_Depth_Offset }, false);
    // HKX39: pin the physics too - at that depth there is no ground, and the engine phase made
    // the graph cycle jump-fall-land forever (the HKX38 log). A fixed motion stops the fall
    // without touching the behaviour graph, so OAR keeps evaluating a standing actor.
    puppet->SetMotionType(RE::hkpMotion::MotionType::kFixed, false);
    restore_fade(puppet->Get3D(false));
    logger::info("SCOPY PUPPET parked reason={} offset={:.0f} (invisible; the engine may idle the graph there - panel time is ours)", why, Park_Depth_Offset);
}

void PuppetDriver::release(char const* reason)
{
    if (!m_puppet)
        return;
    logger::info("SCOPY PUPPET released reason={} ticks={} (runtime form id; the engine reclaims the shell at the boundary)", reason, m_manual_ticks);
    m_puppet = nullptr;
    m_puppet_form_id = 0;
    m_current_generator = nullptr;
    m_playback = ClipPlayback{};
    m_mapping.clear();
    m_driver_note.clear();
    m_grace_frames = 0;
    m_repark_frames = 0;
    m_manual_ticks = 0;
    m_stall_frames = 0;
    m_stall_logged = false;
}

void PuppetDriver::poll_equipment(RE::Actor& puppet)
{
    RE::PlayerCharacter* const player = RE::PlayerCharacter::GetSingleton();
    if (!player)
        return;
    const RE::InventoryEntryData* const hands[2] = { player->GetEquippedEntryData(false), player->GetEquippedEntryData(true) };
    const std::uint32_t forms[2] = {
        hands[0] && hands[0]->object ? hands[0]->object->GetFormID() : 0,
        hands[1] && hands[1]->object ? hands[1]->object->GetFormID() : 0,
    };
    if (forms[0] == m_right_form && forms[1] == m_left_form)
        return;
    const bool right_changed = forms[0] != m_right_form;
    const bool left_changed = forms[1] != m_left_form;
    m_right_form = forms[0];
    m_left_form = forms[1];

    RE::ActorEquipManager* const equip_manager = RE::ActorEquipManager::GetSingleton();
    if (!equip_manager)
        return;
    // Mirror the changed hands onto the puppet (the recipe the HKX37 probe validated), then push
    // the graph's draw/sheathe state in the same tick - the lever the pause never delivers.
    for (int hand = 0; hand < 2; ++hand)
    {
        if ((hand == 0 ? !right_changed : !left_changed) || !hands[hand] || !hands[hand]->object)
            continue;
        RE::TESBoundObject* const object = hands[hand]->object;
        puppet.AddObjectToContainer(object, nullptr, 1, nullptr);
        equip_manager->EquipObject(&puppet, object, nullptr, 1, nullptr, false, true, false, true);
    }
    const char* pushed = "none";
    if (right_changed || left_changed)
    {
        const bool any_hand_filled = forms[0] != 0 || forms[1] != 0;
        puppet.DrawWeaponMagicHands(any_hand_filled);
        pushed = any_hand_filled ? "draw" : "sheathe";
        ++m_equip_reactions;
    }
    logger::info("SCOPY PUPPET equip right={:08x} left={:08x} pushed={} reactions={}", forms[0], forms[1], pushed, m_equip_reactions);
    // The weapon mesh just joined or left O's own graph - a legitimate world change the panel did
    // not make, so the isolation session re-baselines instead of reporting it forever (HKX39's
    // 64-line noise).
    SceneGraphCopy::instance().note_world_change("world-equipment");
}

std::string PuppetDriver::drive(RE::NiAVObject& copy_root)
{
    RE::Actor* const puppet = m_puppet ? m_puppet->As<RE::Actor>() : nullptr;
    if (!puppet || !readable(puppet, sizeof(RE::Actor)))
        return {};

    // The graph's strongest control picks the clip; the full resolution reruns only on a switch.
    RE::hkbClipGenerator const* const pick = pick_running_clip(*puppet);
    if (pick != m_current_generator)
    {
        std::string coverage;
        if (!pick || !resolve_clip_playback(*puppet, copy_root, pick, m_playback, m_mapping, coverage))
        {
            m_current_generator = nullptr;
            m_playback = ClipPlayback{};
            if (!m_stall_logged)
            {
                logger::warn("SCOPY PUPPET driver stall reason=resolve-failed (the fallback path serves; the next switch retries)");
                m_stall_logged = true;
            }
            return {};
        }
        m_current_generator = pick;
        m_stall_frames = 0;
        m_stall_logged = false;
        m_ab_pending = true;
        m_ab_deadline = m_manual_ticks + Ab_Settle_Ticks;
        logger::info("SCOPY PUPPET driver clip='{}' mode={} mapping={} coverage={}",
            m_playback.name,
            readable(m_current_generator, sizeof(RE::hkbClipGenerator))
                ? (m_current_generator->mode.get() == RE::hkbClipGenerator::PlaybackMode::kModeLooping ? "loop" : "single")
                : "?",
            m_mapping, coverage);
    }

    const float time = live_clip_time(m_current_generator);
    if (time < 0.0f)
    {
        // The graph moved past this clip mid-frame (or its control died): drop the playback so the
        // next frame re-resolves from whatever runs now.
        m_current_generator = nullptr;
        return {};
    }
    // The generator's own playback mode decides how the sample time behaves past the clip's end: a
    // looping clip wraps (its control time already does, engine-side), a single-play clip holds the
    // final frame while the state machine blends it out - wrapping it here was the equip-replay bug
    // from the HKX40 field report.
    bool loops = true;
    if (readable(m_current_generator, sizeof(RE::hkbClipGenerator)))
        loops = m_current_generator->mode.get() == RE::hkbClipGenerator::PlaybackMode::kModeLooping;
    const size_t written = play_animation_clip(m_playback, time, copy_root, loops);
    if (written == 0)
    {
        ++m_stall_frames;
        if (m_stall_frames >= 30)
        {
            if (!m_stall_logged)
            {
                logger::warn("SCOPY PUPPET driver stall frames=30 clip='{}' (zero nodes written; re-resolving)", m_playback.name);
                m_stall_logged = true;
            }
            m_current_generator = nullptr;
            m_stall_frames = 0;
        }
        return {};
    }
    m_stall_frames = 0;
    return fmt::format("puppet:{}#{}", m_playback.name,
        m_current_generator ? m_current_generator->animationBindingIndex : 0);
}

PLUGIN_NAMESPACE_END
