//
// Created by AmazingBuff on 2026/10/11.
//

#include "character/puppet_driver.h"

#include "character/puppet_draw.h"
#include "character/scene_graph_copy.h"
#include "character/snapshot_transform.h"

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

// Windows macros conflict with the qualified REX constants below.
#undef MEM_COMMIT
#undef PAGE_NOACCESS

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // Assembly needs a culled/rendered shell before its skin binds; fade must remain nonzero.
    constexpr std::uint32_t Grace_Frames = 240;

    constexpr float Park_Depth_Offset = -8000.0f;

    // How often the per-frame re-assertion reports itself: the park RUNS every unpaused frame (the
    // engine re-derives the transform each frame and wins over a one-shot write), but only this
    // every-th one prints, which is what keeps the log readable.
    constexpr std::uint32_t Repark_Log_Period = 60;
    constexpr float Max_Manual_Step = 0.1f;

    constexpr float Declared_Step_Seconds = 1.0f / 60.0f;

    constexpr std::uint32_t Digest_Ticks = 30;

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

    m_data_logged = false;
    logger::info("SCOPY PUPPET force-generate {} ({})",
        a_enabled ? "on" : "off",
        a_enabled
            ? "every tick asks UpdateAnimation for hkbGenerator::generate() and the skip is bypassed"
            : "the engine's own skip decides on which frames this tick produces a pose");
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
        {
            park("grace-complete");
        }
        return;
    }

    // Engine-applied locals are the independent calibration reference while the world runs.
    if (!paused)
    {
        // Re-asserted EVERY frame, not every N: this actor's transform is re-derived by the engine
        // each frame and the derivation wins over a one-shot write (the 2026-10-13 23:35 session
        // measured the parked world back at the spawn spot one second later). The v6.25 route parked
        // per tick for the same reason; the log stays sparse.
        park(++m_repark_frames % Repark_Log_Period == 0 ? "keep-parked" : nullptr);
        return;
    }

    // While the panel pauses the world the figure must still be held where the panel put it: either
    // the studio framing owns the transform this frame, or the figure has to stay parked - the menu
    // renders the world behind it, and the engine's per-frame derivation would otherwise put the
    // clone back beside the player the moment the panel opens (2026-10-13 23:45 observation).
    if (!PuppetDraw::instance().framing_active())
        park(nullptr);

    const auto now = std::chrono::steady_clock::now();
    const bool step_only = m_step_requested.exchange(false, std::memory_order_acq_rel);
    if (!tick_enabled() && !step_only)
    {
        // Freeze must not accumulate a catch-up step.
        m_last_tick = now;
        return;
    }
    float step = step_only ? Declared_Step_Seconds : std::chrono::duration<float>(now - m_last_tick).count();
    m_last_tick = now;
    step = std::clamp(step, 0.0f, Max_Manual_Step);

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
    // Camera, post-animation functor and force-generation flag mirror the engine update block.
    puppet->UpdateAnimationGraphManager(update);
    ++m_manual_ticks;

    poll_equipment(*puppet);

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

    m_right_form = 0;
    m_left_form = 0;
    m_equip_reactions = 0;
    m_current_generator = nullptr;
    m_playback = ClipPlayback{};
    m_mapping.clear();
    m_last_tick = std::chrono::steady_clock::now();
    logger::info("SCOPY PUPPET born form={:08x} base={:08x} grace={} frames (fade-pinned at the player; parks deep after assembly)",
        m_puppet_form_id, source_base ? source_base->GetFormID() : 0, Grace_Frames);
}

void PuppetDriver::park(char const* a_why)
{
    RE::Actor* const puppet = m_puppet ? m_puppet->As<RE::Actor>() : nullptr;
    RE::PlayerCharacter* const player = RE::PlayerCharacter::GetSingleton();
    if (!puppet || !player)
        return;
    const RE::NiPoint3 here = player->GetPosition();
    const RE::NiPoint3 target{ here.x, here.y, here.z + Park_Depth_Offset };

    // `Actor::SetPosition`'s second parameter is `a_updateCharController`, and the reference alone does
    // NOT take the character controller (the collision and the activation target) with it: with it
    // false the shell stayed interactable beside the player while the figure itself was invisible
    // (2026-10-13 user report), and the engine kept re-deriving the 3D from the controller's stale
    // position. The historical route parked with this flag set and measured interaction as
    // unreachable. It is passed only when the reference actually moves, so the per-frame re-assertion
    // does not warp the controller every frame.
    const bool warp = (target - m_park_target).Length() > 1.0f;
    puppet->SetPosition(target, warp);
    if (warp)
        m_park_target = target;

    puppet->SetMotionType(RE::hkpMotion::MotionType::kFixed, false);

    // The reference position alone does NOT take the figure out of the world, and neither does a
    // world-only write: this actor's subtree is re-derived from its root's LOCAL every frame, so the
    // derived world snaps back (AFP9e measured the world parked at 23:24:32 and back at the player's
    // height 1.3 s later, and a fully visible clone standing beside the player). The local is the
    // input the derivation reads, so the local is what gets written - plus the worlds for the frames
    // that draw before the next derivation. `a_why` is null on the per-frame re-assertions that would
    // otherwise flood the log.
    RE::NiAVObject* const root = puppet->Get3D(false);
    if (root && readable(root, sizeof(RE::NiAVObject)))
    {
        const RE::NiPoint3 before = root->world.translate;
        RE::NiTransform target_world = root->world;   // keep the engine's rotation and scale
        target_world.translate = target;
        root->local = root->parent && readable(root->parent, sizeof(RE::NiAVObject))
            ? root->parent->world.Invert() * target_world
            : target_world;
        root->world = target_world;
        root->previousWorld = target_world;
        recompute_subtree_worlds(*root, 0);

        if (a_why)
        {
            // The read-back settles whether the reference itself moves: `ref-now` is where the game
            // says the actor is, `ref-t` is where this call asked it to go.
            const RE::NiPoint3 ref_now = puppet->GetPosition();
            logger::info("SCOPY PUPPET park-3d reason={} warp={} ref-t=({:.1f},{:.1f},{:.1f}) ref-now=({:.1f},{:.1f},{:.1f}) world-t-before=({:.1f},{:.1f},{:.1f}) world-t-after=({:.1f},{:.1f},{:.1f})",
                a_why, warp ? 1 : 0,
                target.x, target.y, target.z,
                ref_now.x, ref_now.y, ref_now.z,
                before.x, before.y, before.z,
                root->world.translate.x, root->world.translate.y, root->world.translate.z);
        }
    }

    restore_fade(root);
    if (a_why)
        logger::info("SCOPY PUPPET parked reason={} offset={:.0f} (player-side invisible; the engine may idle the graph there - panel time is ours)", a_why, Park_Depth_Offset);
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

    SceneGraphCopy::instance().note_world_change("world-equipment");
}

std::string PuppetDriver::drive(RE::NiAVObject& copy_root)
{
    RE::Actor* const puppet = m_puppet ? m_puppet->As<RE::Actor>() : nullptr;
    if (!puppet || !readable(puppet, sizeof(RE::Actor)))
        return {};

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
        m_current_generator = nullptr;
        return {};
    }

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
