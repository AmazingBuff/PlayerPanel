// Stage-2 work package: independent display instance P. See the header and
// docs/stage2-p-instance-plan.md. Route 3 (run-31 correction) after two
// falsifications:
//
// - Route 1 (v5): generic engine deep copy of the player graph returns no
//   usable node (F5).
// - Route 2 (v5.1-v5.3): clone actor built and dressed correctly, but
//   moving the graph anywhere never produced passes — the menu culler
//   ignores foreign graphs attached under menuObjects (runs 29/30: attach
//   succeeded, zero p passes; the menu scene collects geometry through the
//   culler's private queue, capture-report addendum 2), and a detached
//   graph belongs to no culler at all.
// - Route 3 (this): P stays a LIVE WORLD ACTOR. The world culler is the
//   one chain that demonstrably emits its passes; those passes reach the
//   same three RenderPassImmediately call sites, where the v5.3 whitelist
//   (root ancestry + panel-open gate) picks them out of the world stream
//   and replays them into the studio target. The actor is parked ~8k units
//   below the player — inside the frustum far plane (20480) so culling
//   keeps collecting it, far below any gameplay pitch so no camera sees
//   it. The world-stream passthrough still draws P where the world would
//   show it (nowhere a camera points); hiding the world-side pixels is
//   follow-up work, scoped by PRD 0.5 to the menu-frame contract.
// - PACING (run-28 finding): SKSE's task interface drains queued tasks
//   within the SAME game frame, so the state machine is paced by the
//   RENDER thread — every DrawInterfaceStart queues at most ONE game-thread
//   step, making "frames" real rendered frames.
// - Lifecycle (F8): killing the clone actor is deferred past the grace
//   window when a despawn races it; the build times out on real-frame
//   counts. No scene-graph surgery happens on this route, so the run-29/30
//   retire-list machinery is gone.

#include "proto_pinstance.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

namespace CharacterPanelProto
{
    namespace
    {
        namespace logger = SKSE::log;

        // Spike-verified grace: the engine finishes the clone's secondary
        // bases and AI process asynchronously; virtuals before that crash
        // (call [rax+0x38] evidence). Same 60-frame window as the spike.
        constexpr std::uint32_t Grace_Frames = 60;
        // After dressing, wait for the biped 3D to appear, then hold the
        // arm for this many more frames (skeleton settle).
        constexpr std::uint32_t Settle_Frames = 15;
        // Run 55 (v6.20): figure framing. The run-53/54 figure rendered
        // correctly but tiny — the body sat at the item anchor (camera
        // depth ~485, SV w≈500) with the round-1 guess scale item×0.1.
        // The multiplier is the framing knob: 1.0 puts the figure at the
        // item preview's own scale; one calibration round against the
        // logged anchor/scale/bound values nails the final number.
        constexpr float Studio_Figure_Scale = 1.0f;
        // Hard cap on the whole build in REAL frames; past it the attempt
        // fails and the panel stays on the item preview (sticky until the
        // next open). ~20 s at 60 fps.
        constexpr std::uint32_t Build_Timeout_Frames = 1200;
        // Run 51: renderer-init heartbeat cadence and cap (live frames /
        // probes) — how long we wait for the world renderer to create the
        // device buffers before giving up with a warning.
        constexpr std::uint32_t Init_Heartbeat_Frames = 60;
        constexpr std::uint32_t Init_Heartbeat_Max_Beats = 20;
        // Run 36: how far along the UI3D camera's view direction P stands
        // from the camera position (game units). The item preview lives in
        // the same space; round 1 calibrates so the whole body fits.
        constexpr float Studio_Standoff = 40.0f;
    }

    void PInstance::pose_for_studio()
    {
        // Run 38: pose the P root DIRECTLY in the studio camera's frame at
        // draw time. Moving the actor (SetPosition every tick) proved
        // unreliable — the engine re-derives the 3D root's world transform
        // from its own update chain, and the run-38 VS dump still showed
        // world gameplay coordinates. Drawing happens after the engine's
        // scene update, so setting the root's LOCAL transform here (its
        // parent is the world cell root, so local == world target) and
        // cascading an Update re-poses the whole skeleton for the skin
        // matrices that SetupGeometry re-reads this same window.
        //
        // Run 39: the camera-derived pose landed the vertices at
        // SV_Position (353, 1087, -20) — near the camera but still outside
        // the clip volume, meaning the camera's world rotate column did
        // not match the actual studio view transform. Calibration now uses
        // the ONLY known-good reference: the highlighted item preview's
        // world transform under menuObjects[1] — the manager poses it in
        // exactly the space the studio camera projects correctly. P stands
        // a body-height in FRONT of the item's position (toward the item's
        // facing camera side), scaled to the item's preview scale so the
        // full body fits the same framing. When no item geometry exists
        // the pose falls back to identity near the origin.
        RE::NiAVObject* root =
            reinterpret_cast<RE::NiAVObject*>(m_active_root.load(std::memory_order_acquire));
        if (!root)
            return;

        RE::NiPoint3 anchor{ 0.0f, 0.0f, 0.0f };
        float scale = 1.0f;
        if (auto* ui3d = RE::UI3DSceneManager::GetSingleton())
        {
            if (RE::NiNode* item_root = ui3d->menuObjects[1].get())
            {
                RE::BSGeometry* item_geom = nullptr;
                RE::BSVisit::TraverseScenegraphGeometries(item_root, [&](RE::BSGeometry* geometry) {
                    if (!item_geom)
                        item_geom = geometry;
                    return RE::BSVisit::BSVisitControl::kContinue;
                });
                if (item_geom)
                {
                    anchor = item_geom->world.translate;
                    scale = item_geom->world.scale;
                }
            }
        }
        // The item preview hangs at the anchor with its local +Y pointing
        // at the studio camera (the manager's convention).
        //
        // Run 53 (v6.18): park P AT THE ITEM ANCHOR, not on a derived view
        // axis. Two reasons converged:
        // 1. The v6.7-6.10 near-plane parking relies on the worldToCam
        //    eye-point derivation, whose translation convention has a
        //    KNOWN unresolved drift (run 46: eye flips 485.1 -> -15.0
        //    within a session). The anchor needs no derivation at all —
        //    it is read straight from the scene.
        // 2. Run 40 already proved the anchor pose projects in-bounds
        //    (NDC x/w 0.22 y/w 0.28), and run 52's RenderDoc shot shows
        //    the figure fully formed in the DEPTH buffer while the RT is
        //    near-black — the PS lighting terms collapse because P's
        //    passes carry DUNGEON (world) lights thousands of units from
        //    the menu-space fragments. The menu lights are positioned for
        //    the item preview — parking P exactly at the anchor puts the
        //    fragments where those lights work (the draw side overrides
        //    the pass lights with the menu lights in v6.18).
        RE::NiPoint3 target = anchor;
        root->local.translate = target;
        root->local.rotate.SetEulerAnglesXYZ(0.0f, 0.0f, 3.14159265f);
        root->local.scale = scale * Studio_Figure_Scale;  // run 55: framing knob (was 0.1)
        // Run 41: Update(kDirty) alone is NOT enough — the selective-update
        // flags (the clone carries animation controllers) short-circuit the
        // cascade, so the BONE world transforms stayed at the engine's
        // world-space pose: the run-40/41 SV z/w=0.971 is exactly the
        // gameplay distance from the studio camera origin to the dungeon —
        // the skinned vertices were still projected from the OLD bone
        // positions. UpdateDownwardPass forces the FULL transform cascade
        // over every child regardless of flags, which is what the skin
        // matrix re-read (frameID path) consumes.
        RE::NiUpdateData update_data{ 0.0f, RE::NiUpdateData::Flag::kDirty };
        root->UpdateDownwardPass(update_data, 0);
        root->UpdateWorldBound();
        // Run 56: DYNAMIC CENTERING. The run-55 render showed the figure
        // rising from the anchor (feet) — scaled up, the head left the
        // frame. The world bound (measured right above) gives the body's
        // actual center; shifting the root by (anchor − center) lands the
        // body center exactly on the anchor — the one position whose
        // projection is proven in-frame (runs 40/53/55). No camera
        // convention assumptions anywhere: pure vector arithmetic on the
        // measured bound. Then re-cascade so the skin matrices read the
        // final pose.
        const RE::NiPoint3 center_shift{ anchor.x - root->worldBound.center.x,
            anchor.y - root->worldBound.center.y, anchor.z - root->worldBound.center.z };
        root->local.translate.x += center_shift.x;
        root->local.translate.y += center_shift.y;
        root->local.translate.z += center_shift.z;
        root->UpdateDownwardPass(update_data, 0);
        root->UpdateWorldBound();
        // Run 55/56: per-open calibration dump — anchor/scale/bound before
        // and after centering.
        static thread_local std::uint32_t s_pose_logs = 0;
        if (s_pose_logs++ < 3)
        {
            const auto& bound = root->worldBound;
            logger::info(
                "Proto v6.21 studio pose: anchor=({:.1f},{:.1f},{:.1f}) item_scale={:.3f} root_scale={:.3f} "
                "centered bound r={:.1f} c=({:.1f},{:.1f},{:.1f})",
                anchor.x, anchor.y, anchor.z, scale, root->local.scale, bound.radius, bound.center.x,
                bound.center.y, bound.center.z);
        }
    }

    namespace
    {

        // Body-worn mirroring in the order SKSE's EquipItemEx uses (spike
        // verified: AddObjectToContainer first, then ActorEquipManager;
        // AddWornItem without container membership crashed in
        // ExtraDataList::GetEnchantment). Only biped body slots.
        void mirror_worn_equipment(RE::Actor* clone, RE::PlayerCharacter* player)
        {
            RE::InventoryChanges* changes = player->GetInventoryChanges(true);
            RE::ActorEquipManager* equip_manager = RE::ActorEquipManager::GetSingleton();
            if (!changes || !changes->entryList || !equip_manager)
            {
                logger::warn("Proto P dressing unavailable (changes={} equip={})", static_cast<void*>(changes),
                    static_cast<void*>(equip_manager));
                return;
            }

            using BipedSlot = RE::BGSBipedObjectForm::BipedObjectSlot;
            constexpr BipedSlot Body_Slots[] = {
                BipedSlot::kBody,     BipedSlot::kHead,     BipedSlot::kHands,
                BipedSlot::kForearms, BipedSlot::kAmulet,   BipedSlot::kRing,
                BipedSlot::kFeet,     BipedSlot::kCalves,   BipedSlot::kTail,
                BipedSlot::kLongHair, BipedSlot::kCirclet,  BipedSlot::kEars,
                BipedSlot::kModMouth, BipedSlot::kModNeck,  BipedSlot::kModChestPrimary,
                BipedSlot::kModChestSecondary, BipedSlot::kModShoulder, BipedSlot::kModArmLeft,
                BipedSlot::kModArmRight, BipedSlot::kModLegRight, BipedSlot::kModLegLeft,
                BipedSlot::kModFaceJewelry,
            };

            std::uint32_t added = 0;
            for (RE::InventoryEntryData* entry : *changes->entryList)
            {
                if (!entry)
                    continue;
                RE::TESBoundObject* object = entry->object;  // GetObject is macro-clashed by Windows.h
                if (!object || !entry->IsWorn())
                    continue;
                RE::BGSBipedObjectForm* biped = object->As<RE::BGSBipedObjectForm>();
                if (!biped)
                    continue;
                const bool body_worn = std::any_of(std::begin(Body_Slots), std::end(Body_Slots),
                    [biped](BipedSlot slot) { return biped->HasPartOf(slot); });
                if (!body_worn)
                    continue;

                clone->AddObjectToContainer(object, nullptr, 1, nullptr);
                equip_manager->EquipObject(clone, object, nullptr, 1, nullptr,
                    false,  // a_queueEquip: applied in this call
                    true,   // a_forceEquip: the clone has no AI to choose
                    false,  // a_playSounds: silent
                    true);  // a_applyNow: dressed immediately
                ++added;
            }
            logger::info("Proto P body-worn items mirrored: {}", added);
        }
    }

    PInstance& PInstance::instance()
    {
        static PInstance s_instance;
        return s_instance;
    }

    void PInstance::set_world_ready(bool a_ready)
    {
        m_world_ready.store(a_ready, std::memory_order_release);
    }

    void PInstance::pump()
    {
        // Render thread, once per rendered frame: queue ONE game-thread
        // state-machine step. Runs in every non-None state — kAttached
        // included, whose step re-parks P under the player every frame
        // (run 31: a static park point left the frustum when the player
        // turned, culling P and silencing its passes).
        const State state = m_state.load(std::memory_order_acquire);
        if (state == State::kNone && !m_world_ready.load(std::memory_order_acquire))
            return;

        bool expected = false;
        if (m_step_queued.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            SKSE::GetTaskInterface()->AddTask([this]() {
                m_step_queued.store(false, std::memory_order_release);
                // Run 49 (v6.15): with no P alive, every idle frame retries
                // the auto-spawn ONCE the world is unpaused and actually
                // rendering a player — the clone's geometries only get their
                // device buffers (rendererData) when the world renderer
                // processes them, which never happens paused in a menu.
                if (m_state.load(std::memory_order_acquire) == State::kNone)
                {
                    auto* ui = RE::UI::GetSingleton();
                    auto* player = RE::PlayerCharacter::GetSingleton();
                    if (ui && !ui->GameIsPaused() && player && player->Get3D(false))
                    {
                        spawn();
                        return;
                    }
                }
                tick();
            });
        }
    }

    void PInstance::spawn()
    {
        if (m_state.load(std::memory_order_acquire) != State::kNone)
            return;

        RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->GetActorBase() || !player->GetSequencer())
        {
            logger::warn("Proto P spawn ignored: player or AI process not ready");
            return;
        }
        RE::TESNPC* player_base = player->GetActorBase();
        RE::TESForm* duplicate = player_base->CreateDuplicateForm(false, nullptr);
        auto* clone_base = duplicate ? duplicate->As<RE::TESNPC>() : nullptr;
        if (!clone_base)
        {
            logger::warn("Proto P spawn failed: CreateDuplicateForm (plan F5 successor)");
            return;
        }
        clone_base->faceNPC = player_base;

        RE::NiPointer<RE::TESObjectREFR> placed = player->PlaceObjectAtMe(clone_base, false);
        auto* clone = placed ? placed->As<RE::Actor>() : nullptr;
        if (!clone)
        {
            logger::warn("Proto P spawn failed: PlaceObjectAtMe (plan F5 successor)");
            return;
        }
        m_clone = RE::ObjectRefHandle(clone);
        m_frames_since_place = 0;
        m_dressed_frames = 0;
        m_total_frames = 0;
        m_state.store(State::kWaitingGrace, std::memory_order_release);
        logger::info("Proto P clone placed (world-render init route: the build only advances unpaused "
                     "so the engine creates the device buffers); building toward the whitelist");
    }

    void PInstance::tick()
    {
        // Run 38: the actor-level re-park (SetPosition every frame) is
        // RETIRED — the engine's update chain re-derives the 3D root's
        // world transform from its own bookkeeping, and the run-38 VS dump
        // proved the studio draw still saw world gameplay coordinates. The
        // authoritative pose now happens at draw time inside the studio OM
        // window (pose_for_studio), on the root's LOCAL transform. The tick
        // keeps walking only the build/kill state machine.
        //
        // Run 49 (v6.15): the build must NEVER advance while the game is
        // paused — dressed armor meshes only get their device buffers from
        // real unpaused world rendering, so a build that runs inside the
        // inventory produces a whitelist with zero renderer-initialized
        // geometries (the run-49 rd=9 failure). Stall until the world is
        // live again; the build timeout only counts live frames.
        if (auto* ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused())
            return;

        const State state = m_state.load(std::memory_order_acquire);
        switch (state)
        {
            case State::kWaitingGrace:
            case State::kWaiting3D:
            case State::kKillPending:
                break;
            case State::kAttached:
                // Run 51: the graph is alive and whitelisted; probe until
                // the world renderer has created the device buffers.
                init_heartbeat();
                return;
            default:
                return;  // kNone: nothing to walk
        }

        ++m_total_frames;
        if (m_total_frames > Build_Timeout_Frames)
        {
            logger::warn("Proto P build timeout ({} real frames); killing the clone (plan F8)", m_total_frames);
            m_state.store(State::kNone, std::memory_order_release);
            kill_actor();
            return;
        }

        RE::Actor* clone = m_clone.get().get() ? m_clone.get()->As<RE::Actor>() : nullptr;
        if (!clone)
        {
            // FR-06 invalidation destroyed the placed ref before the build
            // finished; the panel close path has already logged its reason.
            // Run 54: this was a SILENT kNone transition — the prime
            // suspect for run 53's steady-state draws vanishing without a
            // single log line. Make it visible.
            logger::warn("Proto P build aborted: the placed clone ref is gone (state was {})",
                state == State::kWaitingGrace   ? "kWaitingGrace"
                    : state == State::kWaiting3D ? "kWaiting3D"
                                                 : "kKillPending");
            m_state.store(State::kNone, std::memory_order_release);
            return;
        }

        if (state == State::kKillPending)
        {
            // Despawn raced the grace window; now that it is over, virtuals
            // are safe (spike evidence) and the kill can run.
            kill_actor();
            m_state.store(State::kNone, std::memory_order_release);
            return;
        }

        if (state == State::kWaitingGrace)
        {
            ++m_frames_since_place;
            if (m_frames_since_place < Grace_Frames)
                return;  // the next pump queues the next step
            // Grace over (spike order): dress from the player's worn set and
            // park IN FRONT OF the camera, just outside its typical pitch.
            // Route 3 (run 31 correction): the graph is NEVER detached from
            // the world — the menu culler ignores attached foreign graphs
            // (run 29/30: attach succeeded, zero passes) while the WORLD
            // culler still sees the parked actor and emits its passes into
            // the world stream, which the replay whitelist (v5.3) catches.
            // The park must therefore stay INSIDE the frustum far plane
            // (camera far = 20480) but outside ordinary view directions:
            // ~8k units below the player is culled by nothing, visible to
            // no gameplay camera pitched less than ~60 degrees down, and
            // its geometry cost is one character.
            clone->GetActorRuntimeData().boolFlags.set(RE::Actor::BOOL_FLAGS::kMovementBlocked);
            mirror_worn_equipment(clone, RE::PlayerCharacter::GetSingleton());
            m_dressed_frames = 0;
            m_state.store(State::kWaiting3D, std::memory_order_release);
            logger::info("Proto P clone dressed; waiting for the biped 3D (park follows)");
            return;
        }

        // kWaiting3D: the biped graph must exist after dressing, then settle
        // for a few frames before we promote it to the whitelist.
        ++m_dressed_frames;
        RE::NiAVObject* player3d = clone->Get3D(false);
        if (!player3d || m_dressed_frames < Settle_Frames)
            return;  // the next pump queues the next step

        attach_graph();
    }

    // Run 51 (v6.16): the run-50 session showed 0/N device buffers at arm
    // time despite unpaused world rendering — and the v6.15 attempt to
    // Disable() the clone crashed the next draw (RIP=0: disabling an actor
    // destroys its 3D graph, the whitelisted root dangled). The graph now
    // stays alive; this heartbeat probes every Init_Heartbeat_Frames live
    // frames whether the world renderer has created the device buffers yet,
    // with the player's own graph as the control group (the player renders
    // every frame, so its skinned geoms must eventually report initialized —
    // otherwise the check itself is measuring the wrong thing). The manual
    // draw needs no change: the rd gate starts passing the moment the
    // buffers appear.
    void PInstance::init_heartbeat()
    {
        if (m_init_done)
            return;
        if (++m_frames_since_attach < Init_Heartbeat_Frames)
            return;
        m_frames_since_attach = 0;
        if (++m_init_beats > Init_Heartbeat_Max_Beats)
        {
            m_init_done = true;
            logger::warn("Proto P renderer init never completed after {} probes; device buffers did not "
                         "appear (see stage2 plan §0y)",
                Init_Heartbeat_Max_Beats);
            return;
        }

        const auto count = [](RE::NiAVObject* a_root) {
            std::size_t skinned = 0;
            std::size_t initialized = 0;
            if (!a_root)
                return std::pair<std::size_t, std::size_t>{ initialized, skinned };
            RE::BSVisit::TraverseScenegraphGeometries(a_root, [&](RE::BSGeometry* geometry) {
                auto& geom_rt = geometry->GetGeometryRuntimeData();
                if (!geom_rt.skinInstance)
                    return RE::BSVisit::BSVisitControl::kContinue;
                ++skinned;
                if (geom_rt.rendererData && geom_rt.rendererData->vertexBuffer)
                    ++initialized;
                return RE::BSVisit::BSVisitControl::kContinue;
            });
            return std::pair<std::size_t, std::size_t>{ initialized, skinned };
        };

        auto* clone = m_clone.get().get() ? m_clone.get()->As<RE::Actor>() : nullptr;
        auto* player = RE::PlayerCharacter::GetSingleton();
        const auto p = count(clone ? clone->Get3D(false) : nullptr);
        const auto pl = count(player ? player->Get3D(false) : nullptr);
        logger::info("Proto P init heartbeat: P={}/{} player={}/{} skinned geoms with device buffers",
            p.first, p.second, pl.first, pl.second);
        if (p.second > 0 && p.first == p.second)
        {
            m_init_done = true;
            logger::info("Proto P fully renderer-initialized; the manual draw can now rasterize");
        }
    }

    // Route 3 (run 31): P stays a WORLD actor. The menu culler ignored the
    // attached graph (runs 29/30) and a detached graph has no culler at all;
    // the WORLD culler is the only chain that ever emitted P passes. The
    // "attach" step registers the graph on the whitelist; the pose happens
    // at draw time in the UI3D camera frame (run 38), and the world-side
    // passthrough suppression (v5.3) keeps the double out of the world view.
    void PInstance::attach_graph()
    {
        RE::Actor* clone = m_clone.get().get() ? m_clone.get()->As<RE::Actor>() : nullptr;
        if (!clone)
        {
            logger::warn("Proto P attach lost its subject (clone=null)");
            m_state.store(State::kNone, std::memory_order_release);
            return;
        }
        RE::NiAVObject* graph = clone->Get3D(false);
        if (!graph)
        {
            logger::warn("Proto P attach: biped 3D vanished");
            m_state.store(State::kNone, std::memory_order_release);
            return;
        }

        // Re-entrant safety: drop a stale whitelist mark before promoting
        // the new root.
        m_active_root.store(0, std::memory_order_relaxed);

        // Run 38: no actor-level park at all — the authoritative pose is
        // draw-time (pose_for_studio on the root's local transform); any
        // SetPosition here is overridden by the engine's own update chain.

        m_active_root.store(reinterpret_cast<std::uintptr_t>(graph), std::memory_order_release);
        m_state.store(State::kAttached, std::memory_order_release);
        logger::info("Proto P whitelist armed: root={} (draw-time pose in the UI3D camera frame; no "
                     "dependence on a highlighted item)",
            static_cast<void*>(graph));

        // Run 49/50 (v6.15): renderer-init census at arm time — a zero here
        // is the run-49 rd=9 signature (the manual draw rejects every pass
        // until device buffers exist). Informational only: the manual draw
        // needs no re-arm, the rd gate starts passing the moment the world
        // renderer creates the buffers.
        //
        // Run 51 (v6.16): the v6.15 Disable() here CRASHED the next draw
        // (crash-2026-10-02-20-59-23: pose_for_studio line 169, RIP=0 null
        // vtable — disabling an actor DESTROYS its 3D graph, so the
        // whitelisted root dangled). The clone now stays alive and enabled;
        // init_heartbeat() (tick, kAttached) keeps probing until the
        // buffers appear.
        std::size_t skinned = 0;
        std::size_t initialized = 0;
        RE::BSVisit::TraverseScenegraphGeometries(graph, [&](RE::BSGeometry* geometry) {
            auto& geom_rt = geometry->GetGeometryRuntimeData();
            if (!geom_rt.skinInstance)
                return RE::BSVisit::BSVisitControl::kContinue;
            ++skinned;
            if (geom_rt.rendererData && geom_rt.rendererData->vertexBuffer)
                ++initialized;
            return RE::BSVisit::BSVisitControl::kContinue;
        });
        logger::info("Proto P renderer init check: {}/{} skinned geometries have device buffers "
                     "(heartbeat keeps probing until full)",
            initialized, skinned);
    }

    void PInstance::kill_actor()
    {
        if (RE::TESObjectREFR* clone = m_clone.get().get())
        {
            if (auto* actor = clone->As<RE::Actor>())
            {
                actor->Disable();
                actor->SetDelete(true);
            }
        }
        m_clone = RE::ObjectRefHandle{};
        logger::info("Proto P clone actor killed");
    }

    void PInstance::despawn()
    {
        const State state = m_state.exchange(State::kNone, std::memory_order_acq_rel);
        if (state == State::kNone)
            return;

        // Route 3: the whitelist mark is the only render-side registration;
        // dropping it stops all replays. The graph itself stays owned by the
        // actor until the shell is deleted — no scene-graph surgery, no
        // retire list this route.
        if (const std::uintptr_t root = m_active_root.exchange(0, std::memory_order_acq_rel); root)
            logger::info("Proto P whitelist disarmed (root=0x{:X})", root);

        if (state == State::kAttached || state == State::kWaiting3D)
        {
            // Either long past its grace window or safely parked without a
            // graph to dress: the shell kill is safe now.
            kill_actor();
        }
        else
        {
            // kWaitingGrace: the state machine must finish the kill once
            // virtuals are safe (F8) — revive it in kKillPending.
            m_state.store(State::kKillPending, std::memory_order_release);
            logger::info("Proto P despawn during grace; kill deferred to the state machine");
        }
    }

    void PInstance::drain_retired() {}

    bool PInstance::is_p_geometry(const RE::BSGeometry* geometry) const
    {
        const std::uintptr_t root = m_active_root.load(std::memory_order_acquire);
        if (!root || !geometry)
            return false;
        // Run 31: skinned pieces only. Handheld props (Scb quiver, Torch,
        // bows, weapons — run 31 log) are static meshes on bone attach
        // nodes; their draws use the node's world transform, which in the
        // studio replay filled the frame with giant blobs. Skinned
        // geometries (body, armor, hair, face) deform through the skeleton
        // and are exactly the character the panel must show.
        if (!geometry->GetGeometryRuntimeData().skinInstance)
            return false;
        const RE::NiAVObject* node = geometry;
        for (std::size_t depth = 0; node && depth < 32; ++depth)
        {
            if (reinterpret_cast<std::uintptr_t>(node) == root)
                return true;
            node = node->parent;
        }
        return false;
    }
}
