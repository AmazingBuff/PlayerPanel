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
#include "RE/M/Main.h"
#include "RE/N/NiCamera.h"
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
    // The declared dt of a single step (R00 fixes it before shipping; the manifest carries it).
    constexpr float Declared_Step_Seconds = 1.0f / 60.0f;
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

RE::Actor* PuppetDriver::actor() const
{
    RE::TESObjectREFR* const reference = m_puppet.get();
    if (!reference || !readable(reference, sizeof(RE::TESObjectREFR)))
        return nullptr;
    return reference->As<RE::Actor>();
}

void PuppetDriver::set_tick_enabled(bool a_enabled)
{
    if (m_tick_enabled.exchange(a_enabled, std::memory_order_acq_rel) == a_enabled)
        return;
    logger::info("SCOPY PUPPET tick {} ({}); the UI pause is the game's, not this",
        a_enabled ? "on" : "off", a_enabled ? "the panel drives the graph" : "the graph is frozen on our clock");
}

void PuppetDriver::request_step()
{
    m_step_requested.store(true, std::memory_order_release);
}

void PuppetDriver::set_force_generate(bool a_enabled)
{
    if (m_force_generate.exchange(a_enabled, std::memory_order_acq_rel) == a_enabled)
        return;
    // The update block is logged when it changes, so the log says which inputs the engine got on the
    // frames whose pose it is about to report.
    m_data_logged = false;
    logger::info("SCOPY PUPPET force-generate {} ({})",
        a_enabled ? "on" : "off",
        a_enabled
            ? "every tick asks UpdateAnimation for hkbGenerator::generate() and the skip is bypassed"
            : "the engine's own skip decides on which frames this tick produces a pose");
}

char const* PuppetDriver::placement_name(Placement a_placement)
{
    return a_placement == Placement::e_parked_deep ? "parked-deep" : "held-pinned";
}

void PuppetDriver::cycle_placement()
{
    const Placement next = m_placement.load(std::memory_order_acquire) == Placement::e_parked_deep
        ? Placement::e_held_pinned
        : Placement::e_parked_deep;
    m_placement.store(next, std::memory_order_release);
    m_grace_frames = 0;
    logger::info("SCOPY PUPPET placement={} ({}); the next mark reads the graph in this placement",
        placement_name(next),
        next == Placement::e_parked_deep
            ? "deep below the player, out of the engine's animation scope"
            : "standing at the player with the fade pinned, inside the animation scope");
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

    const Placement placement = m_placement.load(std::memory_order_acquire);
    if (m_grace_frames < Grace_Frames)
    {
        ++m_grace_frames;
        suppress_fade(puppet->Get3D(false));
        if (m_grace_frames == Grace_Frames)
        {
            if (placement == Placement::e_parked_deep)
                park("grace-complete");
            else
                logger::info("SCOPY PUPPET held at the player (placement=held-pinned); the shell stays inside the engine's own animation scope with the fade pinned - the variable the parked puppet's empty output pose needs isolated");
        }
        return;
    }

    // Held placement: keep the shell near-invisible and never move it, so it stays inside whatever
    // scope the engine animates. Fade 0 would make the culler skip it entirely (v6.x), so it is
    // pinned at 0.05 rather than zeroed.
    if (placement == Placement::e_held_pinned)
        suppress_fade(puppet->Get3D(false));

    if (!paused)
    {
        // The world runs: the solver observes here (node locals move under the engine's tick),
        // and the repark keeps the shell deep. The graph itself is the engine's own business -
        // including while the force flag is armed, which is deliberate. Forcing a generate here
        // would make the pose and the locals the engine applied disagree, and that pair is
        // exactly what the mapping is proved against: AFP4 measured the proof fall from 0.99 to
        // 0.67 on the frame force started driving the running world. Force belongs to the paused
        // clock, where the panel owns the graph and nothing of the engine's is being applied.
        if (m_solving && m_current_generator)
        {
            if (RE::NiAVObject* const copy_root = SceneGraphCopy::instance().copy_root())
            {
                std::string note;
                if (solve_tracks_step(*puppet, *copy_root, m_current_generator, paused, m_playback, note))
                {
                    m_mapping = "solved";
                    m_solving = false;
                    logger::info("{}", note);
                }
                else if (!note.empty() && (m_repark_frames % 90 == 0 || note.find("UNAVAILABLE") != std::string::npos || note.find("retry") != std::string::npos))
                    logger::info("{}", note);
            }
        }
        // Keep the shell deep (the engine's update chain overrides a one-shot park).
        if (placement == Placement::e_parked_deep && ++m_repark_frames >= Repark_Frames)
        {
            m_repark_frames = 0;
            park("keep-parked");
        }
        return;
    }

    // The panel pauses the world: OUR clock drives the puppet's behaviour graph now. A frozen tick
    // (AFP1's push/freeze control) advances nothing and keeps the clock fresh, so unfreezing delivers
    // one step rather than however long the freeze lasted - unless this frame is a requested single
    // step, which advances exactly the declared dt and nothing else.
    const auto now = std::chrono::steady_clock::now();
    const bool step_only = m_step_requested.exchange(false, std::memory_order_acq_rel);
    if (!tick_enabled() && !step_only)
    {
        m_last_tick = now;
        return;
    }
    float step = step_only ? Declared_Step_Seconds : std::chrono::duration<float>(now - m_last_tick).count();
    m_last_tick = now;
    step = std::clamp(step, 0.0f, Max_Manual_Step);

    // AFP3: the engine calls this same entry point every frame with a block it filled itself, and
    // three of the inputs ours left zero are real inputs to the update: the camera position (the
    // translation inside the WorldRoot camera's world transform - Precision/src/Havok/Havok.h:238),
    // this Actor's own post-animation functor (+0x20, Actor+0xE0 on AE), and `unk2A`, the flag that
    // forbids the frame-skip around hkbGenerator::generate(). Supplying them is what makes our tick
    // comparable to the engine's; F3's force state is what makes `unk2A` an A/B rather than a guess.
    RE::BSAnimationUpdateData update{};
    update.deltaTime = step;
    update.refr = puppet;
    if (RE::NiCamera* const camera = RE::Main::WorldRootCamera(); camera && readable(camera, sizeof(RE::NiAVObject)))
        update.optionalEyePosition = std::addressof(camera->world.translate);
    update.updateFunctor = puppet->AsIPostAnimationChannelUpdateFunctor();
    update.unk2A = force_generate();
    if (std::exchange(m_data_logged, true) == false)
    {
        logger::info("SCOPY PUPPET update-data delta-time={:.4f} refr={:08x} camera=0x{:x} functor=0x{:x} flags=0x{:x} force-generate={}",
            update.deltaTime,
            update.refr ? update.refr->GetFormID() : 0,
            reinterpret_cast<std::uintptr_t>(update.optionalEyePosition),
            reinterpret_cast<std::uintptr_t>(update.updateFunctor),
            static_cast<std::uint32_t>(update.flags),
            update.unk2A ? 1 : 0);
    }
    puppet->UpdateAnimationGraphManager(update);
    ++m_manual_ticks;

    // HKX39 equip lever: the mirror and the draw/sheathe push ride the same tick that advances
    // the graph, so the panel's equip reaction happens inside the pause instead of waiting for
    // the engine update that never comes (the HKX37 finding).
    poll_equipment(*puppet);

    // HKX43: the empirical solver accumulates where the truth lives - node locals move under the
    // engine's own tick (unpaused), so the paused phase only consumes; a pause that catches the
    // solve mid-window continues on pose locals, the measured-dead node path stays off.
    if (m_solving && m_current_generator)
    {
        if (RE::NiAVObject* const copy_root = SceneGraphCopy::instance().copy_root())
        {
            std::string note;
            if (solve_tracks_step(*puppet, *copy_root, m_current_generator, paused, m_playback, note))
            {
                m_mapping = "solved";
                m_solving = false;
                logger::info("{}", note);
            }
            else if (!note.empty() && (m_manual_ticks % 45 == 0 || note.find("UNAVAILABLE") != std::string::npos || note.find("retry") != std::string::npos))
                logger::info("{}", note);
        }
    }

    // HKX39 tier-2 A/B: judges the mapping the display actually writes.
    if (m_ab_pending && !m_solving && m_manual_ticks >= m_ab_deadline && m_current_generator)
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
        m_solving = m_mapping == "identity-unverified";
        // The A/B judges whatever mapping the display finally writes: past the solve when one is
        // armed, otherwise past the blend window.
        m_ab_deadline = m_manual_ticks + (m_solving ? Track_Solve_Frames : 0) + Ab_Settle_Ticks;
        m_ab_pending = true;
        logger::info("SCOPY PUPPET driver clip='{}' mode={} mapping={} solving={} coverage={}",
            m_playback.name,
            readable(m_current_generator, sizeof(RE::hkbClipGenerator))
                ? (m_current_generator->mode.get() == RE::hkbClipGenerator::PlaybackMode::kModeLooping ? "loop" : "single")
                : "?",
            m_mapping, m_solving ? 1 : 0, coverage);
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
