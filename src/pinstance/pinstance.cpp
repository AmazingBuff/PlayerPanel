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
// (2026-10-05 src migration: split out of tools/m0_proto, behavior byte-identical)

#include "panel.h"
#include "pinstance/pinstance.h"
#include "pinstance/pinstance_detail.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <algorithm>
#include <string>
#include <string_view>

using namespace std::literals;

namespace CharacterPanelProto
{
    namespace logger = SKSE::log;

    namespace
    {
        // Spike-verified grace: the engine finishes the clone's secondary
        // bases and AI process asynchronously; virtuals before that crash
        // (call [rax+0x38] evidence). Same 60-frame window as the spike.
        constexpr std::uint32_t Grace_Frames = 60;
        // After dressing, wait for the biped 3D to appear, then hold the
        // arm for this many more frames (skeleton settle).
        constexpr std::uint32_t Settle_Frames = 15;

        // Hard cap on the whole build in REAL frames; past it the attempt
        // fails and the panel stays on the item preview (sticky until the
        // next open). ~20 s at 60 fps.
        constexpr std::uint32_t Build_Timeout_Frames = 1200;

        // v6.24 residue sweep: the clone is a placed reference and SAVES
        // with the game — a save made while it existed reloads it next
        // session as an unmanaged, un-pinned, VISIBLE player duplicate
        // (run 57: the "3BA" actor the user talked to). Cadence of the
        // player-cell sweep on the game-thread pump.
        constexpr std::uint32_t Residue_Scan_Interval_Frames = 300;
        // v6.24: marker base name for clones this plugin places — saved
        // clones match it precisely; legacy saves match the player's name.
        constexpr std::string_view Clone_Base_Name = "CharacterPanel_Clone";
        // v6.25: geometric isolation. Run 58 proved the AI-side levers
        // (activation flag, bool flags) don't hold on this actor — the user
        // still activated and talked to OUR pinned clone. Distance is the
        // lever that cannot fail: interaction/dialogue/collision reach is
        // a few hundred units, so parking P 8000 units below the player
        // (the run-31 route-3 configuration) makes it unreachable while
        // every perceptible channel stays dead. Stage-2b: the park's
        // remaining job is the BUILD WINDOW only (place → grace → dress →
        // relocate, ~1.5 s) — at relocation the shell is deleted, so the
        // world holds no clone afterwards (v6.64; the ghost layer that used
        // to hide the parked double is retired with it). The studio path
        // re-poses the root's LOCAL transform at draw time and never reads
        // the park.
        constexpr float Park_Depth_Below_Player = 8000.0f;

        // v6.25: re-applied every live frame (the engine's update chain
        // re-derives a live actor's transform, so a one-shot park does not
        // stick — the same reason route 3 re-parked per tick, run 31).
        void park_below_player(RE::Actor* a_clone)
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!a_clone || !player)
                return;
            const RE::NiPoint3 pos = player->GetPosition();
            // Actor's override carries the character-controller update flag
            // (true = warp the controller with the ref).
            a_clone->SetPosition(RE::NiPoint3{ pos.x, pos.y, pos.z - Park_Depth_Below_Player }, true);
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
                // v6.24: residue sweep first — a stale clone from a previous
                // session's save may sit exactly where the new one spawns.
                sweep_stale_clones();
                // v6.34: the MAIN MENU is not a preview context — close the
                // panel the moment it opens. The clone-3D liveness check
                // below fires a frame LATE (the world unload trails the menu
                // load), and run 67 showed exactly one menu frame still
                // compositing the panel. IsMenuOpen is checked while the
                // instance is alive; after the close the pump early-outs
                // (kNone + world not ready) and never queues again until
                // the next session arms it.
                if (m_state.load(std::memory_order_acquire) != State::kNone)
                {
                    auto* ui = RE::UI::GetSingleton();
                    if (ui && ui->IsMenuOpen(RE::MainMenu::MENU_NAME))
                    {
                        PInstance::instance().despawn();
                        CharacterPanelProto::Proto::instance().close_panel("main menu");
                        logger::info("Proto P panel closed: main menu open");
                    }
                }
                // v6.32: quit/menu-transition liveness check. Pre-relocation
                // the graph is engine-owned: a lost 3D means the world
                // unloaded and the whitelisted root dangles — disarm before
                // the render pass (run 65 crash: TraverseScenegraphGeometries
                // on the stale root). Stage-2b: once the graph is relocated
                // it is owned by this instance (NiPointer + holder slot) and
                // the shell is already DELETED (v6.64) — the clone ref's
                // fate is irrelevant there, so the Get3D/ref probe runs only
                // in the pre-relocation build window, and the menu-home
                // hosting is verified instead (U2). The pump runs on the
                // game thread at DrawInterfaceStart — BEFORE this frame's
                // render work — so any disarm here leads the studio draw by
                // a frame.
                if (m_state.load(std::memory_order_acquire) == State::kAttached)
                {
                    const bool homed =
                        m_home.load(std::memory_order_acquire) == HomeState::kMenuHome;
                    if (!homed)
                    {
                        auto* live = m_clone.get().get() ? m_clone.get()->As<RE::Actor>() : nullptr;
                        if (!live || !live->Get3D(false))
                        {
                            logger::warn("Proto P clone graph lost (world unloaded?); disarming before "
                                         "the render pass");
                            // v6.33: the world is gone — that is ALSO no valid
                            // preview context (FR-06): close the panel itself,
                            // otherwise the main menu keeps compositing the last
                            // studio image (run 65 screenshot). The pump runs at
                            // DrawInterfaceStart entry, so the very next
                            // panel_frame_active() read takes the non-bracketed
                            // path and the release consumes this same frame.
                            PInstance::instance().despawn();
                            CharacterPanelProto::Proto::instance().close_panel("world unloaded");
                        }
                    }
                    else
                    {
                        verify_home();
                    }
                }
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
        // v6.24: marker name — the base is runtime-created and dies with
        // the session unless saved; a saved clone carries this name into
        // the next session, where the residue sweep matches it precisely
        // (legacy saves without the marker fall back to the player's name).
        clone_base->fullName = RE::BSFixedString(Clone_Base_Name);

        // v6.70 (run 104): born-at-depth is FALSIFIED — CreateReferenceAt
        // Location at the park depth assembled a DEGRADED graph (the engine
        // picks character LOD by reference distance at load time): skin
        // instances with zero bind matrices ("0 bind bones" vs the 74 of
        // the v6.23 era), parts rendered at raw model space, mirrored items
        // invisible, per-cycle nondeterminism. The pre-2b evidence (place
        // at the player, then park deep) stays authoritative: the reference
        // must sit near the player while the graph assembles; parking deep
        // AFTER assembly never downgrades it. The v6.68 flash is therefore
        // fixed at the VISIBILITY layer, not the placement layer: the
        // grace node park (v6.38) plus the grace fade guard (v6.69, below)
        // make every ordering-losing frame render fully transparent, and a
        // transparent, displaced graph gives the activation raycast nothing
        // to hit.
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
        // Stage-2b: a fresh instance starts parked again — any previous
        // home went away with its despawn.
        m_home.store(HomeState::kWorldParked, std::memory_order_release);
        m_home_graph = nullptr;
        m_home_node = nullptr;
        m_state.store(State::kWaitingGrace, std::memory_order_release);
        logger::info("Proto P clone placed at the player (PlaceObjectAtMe; high-model assembly), "
                     "grace node park + fade guard hold it invisible");
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
                // v6.25: keep P parked 8k below the player every live frame
                // (run-31 configuration) — unreachable beats behavior flags
                // (run 58: the flags didn't hold). The studio never reads
                // the park. Stage-2b: this branch runs at most once — the
                // FIRST kAttached tick relocates the graph AND deletes the
                // shell (v6.64); the park only matters for the fail-open
                // path (stuck-parked shells keep needing the re-park).
                if (auto* parked = m_clone.get().get() ? m_clone.get()->As<RE::Actor>() : nullptr)
                    park_below_player(parked);
                // Stage-2b: move the graph to its menu-scene home. No init
                // gate — run 95 proved the engine's SetupAndDrawPass
                // (call_site_original) lazy-initializes the device buffers,
                // so there is nothing to wait for.
                try_relocate();
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
            // v6.38 (user report: the clone flashed at the player for a few
            // frames after a save load). During the grace window the actor
            // must not be touched via virtuals (the spike's [rax+0x38]
            // crashes — secondary bases/AI process are still async), but
            // the 3D GRAPH is plain NiAVObject data: as soon as it exists,
            // shifting its root node's LOCAL translate needs no virtual
            // dispatch and is a frame-accurate fix — the clone never shows
            // at the player at all. park_below_player (actor SetPosition)
            // still runs at grace end as the engine-authoritative park; the
            // engine's update chain re-derives a live actor's transform,
            // which is exactly why this node-level shift is re-applied
            // every grace tick.
            // v6.38 (user report: the clone flashed at the player for a few
            // frames after a save load). During the grace window the actor
            // must not be touched via virtuals (the spike's [rax+0x38]
            // crashes — secondary bases/AI process are still async), so
            // Get3D() (virtual Get3D2) is off-limits too. The 3D graph
            // pointer is plain data though: LOADED_REF_DATA::data3D (0x68).
            // As soon as the graph exists, shifting its root's LOCAL
            // translate needs no virtual dispatch and is frame-accurate —
            // the clone never shows at the player at all.
            // park_below_player (actor SetPosition) still runs at grace end
            // as the engine-authoritative park; the engine's update chain
            // re-derives a live actor's transform, which is exactly why
            // this node-level shift is re-applied every grace tick.
            if (auto* loaded = clone->loadedData; loaded && loaded->data3D)
            {
                auto* early = loaded->data3D.get();
                auto* player_refr = RE::PlayerCharacter::GetSingleton();
                auto* player_loaded = player_refr ? player_refr->loadedData : nullptr;
                if (early && player_loaded && player_loaded->data3D)
                {
                    const RE::NiPoint3 down = player_loaded->data3D.get()->world.translate;
                    early->local.translate = RE::NiPoint3{ down.x, down.y,
                        down.z - Park_Depth_Below_Player };
                    RE::NiUpdateData early_update{ 0.0f, RE::NiUpdateData::Flag::kDirty };
                    early->UpdateDownwardPass(early_update, 0);
                }
                // v6.69/70: fade guard — the PRIMARY flash fix. A freshly
                // placed character ramps in through its BSFadeNode, and an
                // ordering-losing frame renders the graph before the shift
                // above lands (the user's transparent, interactive double).
                // Pin the fade trio (target/rate/current) to zero each grace
                // tick: plain floats, and AsFadeNode dispatches on the
                // fully-constructed node (UpdateDownwardPass above is the
                // same safety class). Whatever frames the node park loses,
                // this pin still renders them fully transparent, and the
                // ramp cannot climb while the target sits at zero. Restored
                // at grace end, before the dress adds new geometry.
                if (auto* fade = early->AsFadeNode())
                {
                    auto& fade_rt = fade->GetRuntimeData();
                    fade_rt.unk128 = 0.0f;
                    fade_rt.unk12C = 0.0f;
                    fade_rt.currentFade = 0.0f;
                }
            }
            if (m_frames_since_place < Grace_Frames)
                return;  // the next pump queues the next step
            // v6.69: restore the fade the grace guard pinned to zero —
            // target=1, rate=0, current=1: fully opaque and no further ramp.
            // The world never draws the parked graph visibly, but the studio
            // must see it opaque; dress geometry attaches after this point.
            if (auto* loaded = clone->loadedData; loaded && loaded->data3D)
                if (auto* fade = loaded->data3D.get()->AsFadeNode())
                {
                    auto& fade_rt = fade->GetRuntimeData();
                    fade_rt.unk128 = 1.0f;
                    fade_rt.unk12C = 0.0f;
                    fade_rt.currentFade = 1.0f;
                }
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
            // v6.23 behavior pinning (plan §0af), v6.24 correction: the
            // clone must be behavior-inert in the world — no interaction or
            // dialogue (the E prompt), no hostile acts. SetCollision is
            // RETIRED: TESObjectREFR::SetCollision only edits the record
            // flags (load-time), zero runtime effect — run 57 proved it.
            // Runtime collision/ghost handling is re-evaluated after the
            // residue sweep (§0ag): the actor the user met was most likely
            // a stale clone loaded from a previous session's save, not this
            // pinned one — the sweep log names every deletion.
            clone->GetActorRuntimeData().boolFlags.set(RE::Actor::BOOL_FLAGS::kAttackingDisabled);
            clone->GetActorRuntimeData().boolFlags.set(RE::Actor::BOOL_FLAGS::kCastingDisabled);
            clone->SetActivationBlocked(true);
            clone->StopCombat();
            // v6.25: the primary imperceptibility lever is GEOMETRIC — out
            // of reach, out of dialogue, out of collision (see
            // Park_Depth_Below_Player). The flags above stay as secondary
            // cover; run 58 proved they don't hold on their own.
            park_below_player(clone);
            logger::info("Proto P pinned (ref=0x{:X}): no-move/attack/cast flags, activation blocked, "
                         "parked 8k below the player",
                clone->GetFormID());
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
            // graph to dress: the shell kill is safe now. Stage-2b order:
            // kill FIRST (the engine releases its refs — loadedData->data3D
            // etc.), THEN drop the home; our NiPointer and the holder's
            // child slot kept the graph alive across the kill (refcounts,
            // not raw addresses — the run-51 lesson applied in the safe
            // direction), and the last release frees it here on the game
            // thread with the whitelist already disarmed.
            kill_actor();
            release_home();
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

    void PInstance::sweep_stale_clones()
    {
        if (++m_frames_until_residue_scan < Residue_Scan_Interval_Frames)
            return;
        m_frames_until_residue_scan = 0;

        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* cell = player ? player->GetParentCell() : nullptr;
        const RE::TESNPC* player_base = player ? player->GetActorBase() : nullptr;
        // Only touch references while the world is live (same gate as the
        // spawn path) — mid-load cell contents are transient.
        if (!cell || !player_base || !player->Get3D(false))
            return;

        const RE::NiPointer<RE::TESObjectREFR> self = m_clone.get();
        const char* player_name_c = player_base->GetFullName();
        const std::string_view player_name = player_name_c ? player_name_c : "";
        std::uint32_t deleted = 0;
        for (const auto& ref : cell->GetRuntimeData().references)
        {
            auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
            if (!actor || actor->IsPlayerRef())
                continue;
            if (self && actor == self.get())
                continue;  // our live, pinned clone
            // v6.65: refs already deleted/disabled are inert — our own
            // killed shell lingers in the cell's reference list until the
            // engine purges it (SetDelete does not remove it immediately),
            // and re-sweeping it every cadence was pure log noise (run 98:
            // 4 deletions of the same ref 0xFF0063BD). Legacy stale clones
            // load ENABLED, so they are still matched.
            if (actor->IsDisabled() || actor->IsDeleted())
                continue;
            auto* base = actor->GetActorBase();
            if (!base || base->GetFormID() < 0xFF000000)
                continue;  // runtime-created bases only — mod content is untouchable
            const char* name = base->GetFullName();
            const bool marked = name && Clone_Base_Name == name;
            const bool legacy = name && !player_name.empty() && player_name == name;
            if (!marked && !legacy)
                continue;
            actor->Disable();
            actor->SetDelete(true);
            ++deleted;
            logger::warn("Proto P residue sweep: deleted stale clone ref=0x{:X} base=0x{:X} name=[{}]",
                actor->GetFormID(), base->GetFormID(), name ? name : "");
        }
        if (deleted > 0)
            logger::info("Proto P residue sweep: {} stale clone(s) removed from the player cell",
                deleted);
    }

    bool PInstance::is_p_descendant(const RE::BSGeometry* geometry) const
    {
        const std::uintptr_t root = m_active_root.load(std::memory_order_acquire);
        if (!root || !geometry)
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

    bool PInstance::is_p_geometry(const RE::BSGeometry* geometry) const
    {
        // Run 31: skinned pieces only. Handheld props (Scb quiver, Torch,
        // bows, weapons — run 31 log) are static meshes on bone attach
        // nodes; their draws use the node's world transform, which in the
        // studio replay filled the frame with giant blobs. Skinned
        // geometries (body, armor, hair, face) deform through the skeleton
        // and are exactly the character the panel must show.
        if (!geometry || !geometry->GetGeometryRuntimeData().skinInstance)
            return false;
        return is_p_descendant(geometry);
    }
}
