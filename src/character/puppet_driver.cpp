//
// Created by AmazingBuff on 2026/10/11.
//

#include "character/puppet_driver.h"

#include "character/puppet_draw.h"
#include "character/body_shape.h"
#include "character/equipment_mirror.h"
#include "character/scene_graph_copy.h"
#include "character/snapshot_transform.h"

#include "RE/A/Actor.h"
#include "RE/B/BSAnimationGraphManager.h"
#include "RE/B/BSFadeNode.h"
#include "RE/B/BSGeometry.h"
#include "RE/B/BSVisit.h"
#include "RE/B/BGSDefaultObjectManager.h"
#include "RE/B/BGSEquipSlot.h"
#include "RE/I/IAnimationGraphManagerHolder.h"
#include "RE/A/ActorEquipManager.h"
#include "RE/M/Main.h"
#include "RE/N/NiCamera.h"
#include "RE/P/PlayerCharacter.h"
#include "RE/T/TESObjectREFR.h"
#include "RE/T/TESObjectWEAP.h"

#include <algorithm>
#include <cmath>

// Windows macros conflict with the qualified REX constants below.
#undef MEM_COMMIT
#undef PAGE_NOACCESS
#undef GetObject

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // How long the reference may stay beside the player while the shell assembles. This window is a
    // VISIBILITY compromise with a hard engine constraint behind it: the engine picks the character
    // LOD when the reference is loaded, so the reference has to be near the player for the graph to
    // come out at full detail (run 104: a reference born at the park depth assembled a low-detail
    // graph with missing parts and a stripped skin), and it cannot be parked before that load ran.
    //
    // It is a CAP, not a wait: the park happens as soon as the census says the shell is assembled,
    // bound and settled, which is typically a fraction of a second. The cap only bounds a shell that
    // never reports itself ready.
    constexpr std::uint32_t Grace_Frames = 240;
    // ... and this is the floor, so the reference is demonstrably near the player when the graph is
    // loaded rather than for one frame that may have raced the load.
    constexpr std::uint32_t Grace_Min_Frames = 20;
    // The census has to hold still this long before the shell counts as assembled: armor and facegen
    // mount asynchronously, and parking on the frame a mesh set is still growing is the race the copy
    // route's settle gate was built for (2026-10-07: detached 14 ms after dressed, geoms 12→8).
    constexpr std::uint32_t Assembly_Stable_Frames = 10;
    // Dressing is a biped change and the engine applies it over the following frames, so the park
    // waits for the census to settle again after it.
    constexpr std::uint32_t Dress_Wait_Frames = 10;

    constexpr float Park_Depth_Offset = -8000.0f;

    // How often the per-frame re-assertion reports itself: the park RUNS every unpaused frame (the
    // engine re-derives the transform each frame and wins over a one-shot write), but only this
    // every-th one prints, which is what keeps the log readable.
    constexpr std::uint32_t Repark_Log_Period = 60;
    constexpr float Max_Manual_Step = 0.1f;

    constexpr float Declared_Step_Seconds = 1.0f / 60.0f;

    constexpr std::uint32_t Digest_Ticks = 30;

    // The worn body set is re-read at this cadence once the world runs; the paused panel reads it
    // every frame. Both are flag tests over an inventory list, but the paused case is the one the
    // player is looking at while changing gear.
    constexpr std::uint32_t Worn_Poll_Period = 30;

    // How often the panel re-asserts the player's body shape on the puppet. OBody shapes an actor
    // whenever its 3D loads - including a shell it has already processed - and the player can change
    // their own body in RaceMenu, so the agreement is re-checked on a slow cadence rather than
    // assumed once. The check is a comparison walk; only a real disagreement writes.
    constexpr std::uint32_t Shape_Refresh_Ticks = 300;

    // The assembly window's visibility guard, and it MUST be zero: anything above it is a figure the
    // player can see standing inside their own body - the 2026-10-14 report was exactly that, a 0.05
    // ghost for the first seconds of every session. The copy route needed a nonzero fade because its
    // detach gate waited for `numMatrices`, which a zeroed BSFadeNode never gets; this route has no
    // such gate - the puppet's binding is read from the tree, and the panel's own draw is what gives
    // the figure its device buffers.
    constexpr float Assembly_Fade = 0.0f;

    void suppress_fade(RE::NiAVObject* graph)
    {
        if (RE::BSFadeNode* const fade = graph ? graph->AsFadeNode() : nullptr)
        {
            fade->unk128 = Assembly_Fade;
            fade->unk12C = 0.0f;
            fade->currentFade = Assembly_Fade;
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

    // What the shell is actually made of, by name. The 2026-10-14 sessions reported meshes on the
    // figure that neither the player nor this plugin had equipped (a bow, a shield, an axe) while the
    // shell's own inventory was empty, so the names are the evidence that says where they came from -
    // a picture cannot, and a count cannot either.
    std::string shell_geometry_names(RE::NiAVObject& a_root, std::uint32_t a_limit)
    {
        std::string names;
        std::uint32_t shown = 0;
        std::uint32_t total = 0;
        RE::BSVisit::TraverseScenegraphGeometries(&a_root, [&](RE::BSGeometry* a_geometry)
        {
            ++total;
            if (shown < a_limit)
            {
                const char* const name = a_geometry ? a_geometry->name.c_str() : nullptr;
                names += fmt::format("'{}' ", name && *name ? name : "?");
                ++shown;
            }
            return RE::BSVisit::BSVisitControl::kContinue;
        });
        if (total > shown)
            names += fmt::format("+{} more", total - shown);
        return names;
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

void PuppetDriver::set_force_when_paused(bool a_enabled)
{
    if (m_force_when_paused.exchange(a_enabled, std::memory_order_acq_rel) == a_enabled)
        return;
    logger::info("SCOPY PUPPET force-when-paused {} ({})",
        a_enabled ? "on" : "off",
        a_enabled
            ? "auto: the paused panel's generate is forced, the running world's is not"
            : "the drive state decides force on its own");
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

    // The assembly window. The reference cannot leave the player before the engine has loaded the
    // character graph (it picks the LOD by reference distance at load), so the window is spent
    // invisible instead of spent beside the player: the fade is pinned to zero every frame and the
    // park happens the moment the census reports the shell assembled, bound and settled. The cap is
    // the backstop for a shell that never reports ready - it parks anyway, exactly as before.
    if (m_grace_frames < Grace_Frames)
    {
        ++m_grace_frames;
        RE::NiAVObject* const root = puppet->Get3D(false);
        suppress_fade(root);

        const bool settled = assembly_settled(root);
        const bool timed_out = m_grace_frames >= Grace_Frames;

        if (!m_dressed)
        {
            // Wait for the shell (or the cap), then dress it: the biped change is applied by the
            // engine over the following frames, so it happens while the reference still stands
            // beside the player - the order the copy route validated before its own detach.
            if (!timed_out && (m_grace_frames < Grace_Min_Frames || !settled))
                return;

            const std::uint32_t equipped = dress(*puppet);
            m_dressed = true;
            m_dress_frames = 0;
            m_assembly_signature = 0;
            m_assembly_stable = 0;
            logger::info("SCOPY PUPPET dressed equipped={} frames={} reason={} geoms={} skinned={} bound-nodes={}/{} (the player's worn body items; the reference is still beside the player)",
                equipped, m_grace_frames, timed_out ? "assembly-timeout" : "assembled",
                m_assembly_census.geometries, m_assembly_census.skinned,
                m_assembly_census.bound_nodes, m_assembly_census.alloc_slots);
            if (!timed_out)
                return;   // let the dressing settle before the park moves the figure away
        }

        // Park on the settled dressing, or at the cap whatever the census says.
        if (!timed_out && (++m_dress_frames < Dress_Wait_Frames || !settled))
            return;

        logger::info("SCOPY PUPPET assembly frames={} reason={} geoms={} skinned={} roots={} bound-nodes={}/{} skin-data-null={} bones-null={} stable={}",
            m_grace_frames, timed_out ? "assembly-timeout" : "assembled",
            m_assembly_census.geometries, m_assembly_census.skinned, m_assembly_census.roots,
            m_assembly_census.bound_nodes, m_assembly_census.alloc_slots,
            m_assembly_census.skin_data_null, m_assembly_census.bones_array_null, m_assembly_stable);
        park(timed_out ? "assembly-timeout" : "assembled");
        // The window is over on EVERY path once the park ran, not only on the timeout path: leaving
        // `m_grace_frames` below the cap re-entered this block on the next frame, which re-printed
        // this line and re-parked the figure every frame until the cap was reached (2026-10-14 log).
        m_grace_frames = Grace_Frames;
        return;
    }

    // Engine-applied locals are the independent calibration reference while the world runs.
    if (!paused)
    {
        // Gear changes happen in the world too (quick-equip, a follower handing something over), so
        // the worn set is reconciled here as well - on the slow cadence, after the park so an equip's
        // own 3D work cannot land between this frame's transform write and the draw.
        park(++m_repark_frames % Repark_Log_Period == 0 ? "keep-parked" : nullptr);
        poll_equipment(*puppet, false);
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
    // Auto mode resolves here: a paused frame is forced (the panel needs a pose to write at all - the
    // engine's own skip leaves the output empty), an unpaused one is not (the mapping proof compares
    // the pose against the locals the engine applied, which only an unforced frame provides).
    update.unk2A = force_generate() || (force_when_paused() && paused);
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

    poll_equipment(*puppet, true);

    // The shape agreement is re-established on a slow panel cadence: OBody re-shapes an actor
    // whenever its 3D loads, and the player's own body can change while the panel is open.
    if (++m_shape_ticks >= Shape_Refresh_Ticks)
    {
        m_shape_ticks = 0;
        apply_body_shape(*puppet, "panel-refresh");
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
    // Cut the record's own kit at the source, the order the copy route validated from its first
    // session: a duplicated NPC record arrives carrying the player's authored starting kit (on this
    // machine a bow, a shield and an iron war axe), the reference's 3D load puts it into the
    // inventory, and the engine's best-fit equip dresses a no-AI figure with whatever it finds there.
    // With the container empty and both outfits nulled there is nothing to arrive, so the strip and
    // the prune below are guards against other sources (scripts, mods), not the first line of defense.
    puppet_base->defaultOutfit = nullptr;
    puppet_base->sleepOutfit = nullptr;
    static_cast<RE::TESContainer*>(puppet_base)->ClearDataComponent();

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

    // Guardrail, not the first line of defense: the record's container was cleared above, so the
    // authored kit cannot arrive - what this strip catches now is anything a script or a mod handed
    // the fresh reference between placement and here. It still logs names and form ids, so whatever
    // does arrive is identifiable from the log rather than only from the picture.
    const EquipmentStrip inherited = equipment_strip(*puppet);
    logger::info("SCOPY PUPPET stripped items={} removed={} unequipped={} inherited=[{}] unequipped=[{}]",
        inherited.items, inherited.removed, inherited.unequipped, inherited.sample, inherited.unequipped_sample);
    m_grace_frames = 0;
    m_repark_frames = 0;
    m_manual_ticks = 0;
    m_stall_frames = 0;
    m_stall_logged = false;

    m_right_form = 0;
    m_left_form = 0;
    m_equip_reactions = 0;
    m_worn_poll_frames = 0;
    m_prune_poll_frames = 0;
    m_shape_ticks = 0;
    m_assembly_census = FigureAssembly{};
    m_assembly_signature = 0;
    m_assembly_stable = 0;
    m_dressed = false;
    m_dress_frames = 0;
    m_current_generator = nullptr;
    m_playback_root = nullptr;
    m_playback = ClipPlayback{};
    m_mapping.clear();
    m_last_tick = std::chrono::steady_clock::now();
    logger::info("SCOPY PUPPET born form={:08x} base={:08x} grace={} frames (fade-pinned INVISIBLE beside the player; parks as soon as the shell is bound and settled, or at the cap)",
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
    const bool warp = (target - puppet->GetPosition()).Length() > 1.0f;
    puppet->SetPosition(target, warp);

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
    m_playback_root = nullptr;
    m_playback = ClipPlayback{};
    m_mapping.clear();
    m_grace_frames = 0;
    m_repark_frames = 0;
    m_manual_ticks = 0;
    m_stall_frames = 0;
    m_stall_logged = false;
    m_assembly_census = FigureAssembly{};
    m_assembly_signature = 0;
    m_assembly_stable = 0;
    m_dressed = false;
    m_dress_frames = 0;
    m_worn_poll_frames = 0;
    m_prune_poll_frames = 0;
    m_shape_ticks = 0;
}

bool PuppetDriver::assembly_settled(RE::NiAVObject* a_root)
{
    if (!a_root || !readable(a_root, sizeof(RE::NiAVObject)))
    {
        m_assembly_census = FigureAssembly{};
        m_assembly_signature = 0;
        m_assembly_stable = 0;
        return false;
    }

    m_assembly_census = figure_measure_assembly(*a_root);
    if (m_assembly_census.signature() == m_assembly_signature)
        ++m_assembly_stable;
    else
    {
        m_assembly_signature = m_assembly_census.signature();
        m_assembly_stable = 0;
    }

    // "Ready" is the copy route's own fully-bound criterion, moved onto this route's census: every
    // skinned geometry has its skin data, no palette array is missing, and every ALLOCATED slot is
    // bound. A shell that never reaches it is not stuck in this window - the frame cap parks it.
    const bool fully_bound = m_assembly_census.skin_data_null == 0 && m_assembly_census.bones_array_null == 0 &&
        m_assembly_census.bound_ready();
    return m_assembly_census.assembled() && fully_bound && m_assembly_stable >= Assembly_Stable_Frames;
}

std::uint32_t PuppetDriver::dress(RE::Actor& a_puppet)
{
    RE::PlayerCharacter* const player = RE::PlayerCharacter::GetSingleton();
    if (!player)
        return 0;

    // Stripped again here, not only at birth: the shell's own 3D load re-runs the record's default
    // items after the birth strip, and dressing on top of those is what left the figure with two of
    // everything (the 2026-10-14 "equipment stacking" report).
    const EquipmentStrip stripped = equipment_strip(a_puppet);
    if (stripped.changed())
    {
        logger::info("SCOPY PUPPET stripped again items={} removed={} unequipped={} unequipped=[{}] sample=[{}]",
            stripped.items, stripped.removed, stripped.unequipped, stripped.unequipped_sample, stripped.sample);
    }

    if (RE::NiAVObject* const root = a_puppet.Get3D(false); root && readable(root, sizeof(RE::NiAVObject)))
        logger::info("SCOPY PUPPET shell names=[{}]", shell_geometry_names(*root, 12));

    // The strip emptied the puppet's hands, so the cached "what the player's hands were" no longer
    // describes the figure: clearing it makes the next poll re-equip AND re-push the draw state.
    m_right_form = 0;
    m_left_form = 0;

    const std::uint32_t equipped = equipment_equip_all_worn(a_puppet, *player);
    // The hand slots are the other poll's business, but the figure is about to leave the world: push
    // them here too so the first panel frame is not the first time it holds anything.
    poll_equipment(a_puppet, true);
    // OBody has already shaped this shell by now (it applies its own - random - preset when the
    // reference's 3D loads), so the player's shape is written after it, not instead of it.
    apply_body_shape(a_puppet, "dressing");
    return equipped;
}

void PuppetDriver::apply_body_shape(RE::Actor& a_puppet, char const* a_why)
{
    RE::PlayerCharacter* const player = RE::PlayerCharacter::GetSingleton();
    if (!player)
        return;
    const BodyShapeSync sync = body_shape_sync(a_puppet, *player);
    if (!sync.changed())
        return;
    logger::info("SCOPY SHAPE applied reason={} compared={} mismatched={} copied={} skipped={} sample=[{}]",
        a_why, sync.compared, sync.mismatched, sync.copied, sync.skipped, sync.sample);
}

void PuppetDriver::poll_equipment(RE::Actor& puppet, bool a_every_frame){
    RE::PlayerCharacter* const player = RE::PlayerCharacter::GetSingleton();
    if (!player)
        return;

    // AFP10f: anything the player is not using leaves the figure's inventory outright. Equipping the
    // player's set is not enough on its own - the engine equips best-fit gear from an actor's own
    // inventory and this figure has no AI to decide otherwise, so a carried-but-unwanted item comes
    // straight back on (that is how the record's starting kit survived the strip, and how a replaced
    // weapon stayed in the hand).
    //
    // The two cadence counters are independent (see the members): a shared one let the worn sync's
    // reset keep this threshold permanently unreachable while the world ran.
    if (a_every_frame || ++m_prune_poll_frames >= Worn_Poll_Period)
    {
        m_prune_poll_frames = 0;
        const EquipmentPrune prune = equipment_prune(puppet, *player);
        if (prune.changed())
        {
            logger::info("SCOPY PUPPET pruned removed={} of={} sample=[{}]",
                prune.removed, prune.considered, prune.sample);
            SceneGraphCopy::instance().note_world_change("world-equipment");
        }
    }

    // The quiver's model comes from the actor's CURRENT ammo, and the engine picks the current ammo
    // from the actor's own inventory - so a residual stack (the record's starting arrows) renders a
    // quiver the player is not wearing. Kept to exactly the player's current ammo, carried and
    // equipped; on the same cadence as the body set.
    if (a_every_frame || ++m_worn_poll_frames >= Worn_Poll_Period)
    {
        m_worn_poll_frames = 0;

        // AFP10-B: the worn BODY set - armor, clothing, jewelry - which the hand poll below never
        // saw. The weapons a hand carries and the body items a biped slot carries are disjoint
        // sets, so the two polls do not fight over anything.
        const EquipmentSync sync = equipment_sync_worn(puppet, *player);
        if (sync.changed())
        {
            logger::info("SCOPY PUPPET worn equipped={} removed={} worn={} (the player's body items, reconciled)",
                sync.equipped, sync.removed, sync.worn);
            SceneGraphCopy::instance().note_world_change("world-equipment");
        }

        const EquipmentAmmo ammo = equipment_sync_ammo(puppet, *player);
        if (ammo.changed())
        {
            logger::info("SCOPY PUPPET ammo wanted={:08x} equipped={} removed={} (the quiver follows the player's current ammo)",
                ammo.wanted, ammo.equipped, ammo.removed);
            SceneGraphCopy::instance().note_world_change("world-equipment");
        }
    }

    const RE::InventoryEntryData* const hands[2] = { player->GetEquippedEntryData(false), player->GetEquippedEntryData(true) };
    RE::TESBoundObject* const wanted[2] = {
        hands[0] ? hands[0]->object : nullptr,
        hands[1] ? hands[1]->object : nullptr,
    };
    const std::uint32_t forms[2] = {
        wanted[0] ? wanted[0]->GetFormID() : 0,
        wanted[1] ? wanted[1]->GetFormID() : 0,
    };
    const bool right_changed = forms[0] != m_right_form;
    const bool left_changed = forms[1] != m_left_form;
    m_right_form = forms[0];
    m_left_form = forms[1];

    RE::ActorEquipManager* const equip_manager = RE::ActorEquipManager::GetSingleton();
    if (!equip_manager)
        return;

    const RE::BGSDefaultObjectManager* const defaults = RE::BGSDefaultObjectManager::GetSingleton();
    if (!defaults)
        return;
    RE::BGSEquipSlot* const slots[2] = {
        defaults->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECT::kRightHandEquip),
        defaults->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECT::kLeftHandEquip),
    };
    RE::BGSEquipSlot* const both_hands = defaults->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECT::kEitherHandEquip);
    if (!slots[0] || !slots[1] || !both_hands)
        return;
    const auto slot_for = [&](RE::TESBoundObject* object, int hand)
    {
        const RE::TESObjectWEAP* const weapon = object->As<RE::TESObjectWEAP>();
        return weapon && weapon->GetEquipSlot() == both_hands ? both_hands : slots[hand];
    };

    std::uint32_t hand_equipped = 0;
    std::uint32_t hand_removed = 0;

    // Finish both removals before equipping: swapping hands must not delete the other hand's item.
    for (int hand = 0; hand < 2; ++hand)
    {
        const bool left = hand == 1;
        RE::InventoryEntryData* const held_entry = puppet.GetEquippedEntryData(left);
        RE::TESBoundObject* const held = held_entry && held_entry->object ? held_entry->object : nullptr;
        if (!held || held == wanted[hand])
            continue;

        equip_manager->UnequipObject(&puppet, held, nullptr, 1, slot_for(held, hand), false, true, false, true);
        if (held != wanted[0] && held != wanted[1])
        {
            RE::InventoryChanges* const stale = puppet.GetInventoryChanges(false);
            const std::int16_t carried = stale ? stale->GetItemCount(held) : 0;
            puppet.RemoveItem(held, carried > 0 ? carried : 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
        }
        ++hand_removed;
    }

    for (int hand = 0; hand < 2; ++hand)
    {
        RE::TESBoundObject* const object = wanted[hand];
        if (!object)
            continue;
        const RE::BGSEquipSlot* const slot = slot_for(object, hand);
        const std::int32_t needed = wanted[0] == wanted[1] && slot != both_hands ? 2 : 1;
        RE::InventoryChanges* const changes = puppet.GetInventoryChanges(false);
        const std::int32_t carried = changes ? changes->GetItemCount(object) : 0;
        if (carried < needed)
            puppet.AddObjectToContainer(object, nullptr, needed - carried, nullptr);

        const RE::InventoryEntryData* const held = puppet.GetEquippedEntryData(hand == 1);
        if (!held || held->object != object)
        {
            equip_manager->EquipObject(&puppet, object, nullptr, 1, slot, false, true, false, true);
            ++hand_equipped;
        }
    }

    const char* pushed = "none";
    if (right_changed || left_changed || hand_equipped != 0 || hand_removed != 0)
    {
        const bool any_hand_filled = forms[0] != 0 || forms[1] != 0;
        puppet.DrawWeaponMagicHands(any_hand_filled);
        pushed = any_hand_filled ? "draw" : "sheathe";
        ++m_equip_reactions;
    }

    if (hand_equipped == 0 && hand_removed == 0 && !right_changed && !left_changed)
        return;   // the hands already agree and the player's own set did not move

    logger::info("SCOPY PUPPET equip right={:08x} left={:08x} pushed={} equipped={} removed={} reactions={}",
        forms[0], forms[1], pushed, hand_equipped, hand_removed, m_equip_reactions);

    SceneGraphCopy::instance().note_world_change("world-equipment");
}

std::string PuppetDriver::drive(RE::NiAVObject& copy_root)
{
    RE::Actor* const puppet = m_puppet ? m_puppet->As<RE::Actor>() : nullptr;
    if (!puppet || !readable(puppet, sizeof(RE::Actor)))
        return {};

    RE::hkbClipGenerator const* const pick = pick_running_clip(*puppet);
    if (pick != m_current_generator || m_playback_root != &copy_root)
    {
        std::string coverage;
        if (!pick || !resolve_clip_playback(*puppet, copy_root, pick, m_playback, m_mapping, coverage))
        {
            m_current_generator = nullptr;
            m_playback_root = nullptr;
            m_playback = ClipPlayback{};
            if (!m_stall_logged)
            {
                logger::warn("SCOPY PUPPET driver stall reason=resolve-failed (the fallback path serves; the next switch retries)");
                m_stall_logged = true;
            }
            return {};
        }
        m_current_generator = pick;
        m_playback_root = &copy_root;
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
