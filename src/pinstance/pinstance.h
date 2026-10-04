#pragma once
// Stage-2 work package: the independent display instance P (PRD M0).
//
// Route 3 (run-31 correction; routes 1-2 falsified, see
// docs/stage2-p-instance-plan.md §0/§0a/§0b): P is a LIVE WORLD ACTOR —
// duplicate of the player base placed via PlaceObjectAtMe, dressed with the
// player's body-worn equipment after the spike-verified grace window, and
// parked ~8k units below the player. The WORLD culler is the only chain
// that emits its passes (the menu culler ignores foreign graphs attached
// under menuObjects — runs 29/30); those world-stream passes reach the
// same three RenderPassImmediately call sites, where the replay hook's
// whitelist (root ancestry + panel-open gate) picks them out and replays
// them into the studio target.
//
// Stage-2b (docs/stage2b-studio-home-plan.md, v6.64 = both rounds landed):
// once the whitelist arms, the graph is DETACHED from the world cell,
// re-homed under a private CP_StudioHome node attached to
// UI3DSceneManager::menuObjects[0] (the light rig's proven host), held by a
// strong NiPointer, and the shell actor is DELETED in the same tick — its
// data3D is severed first, so Disable has no 3D to destroy (the run-51
// mechanism is gone) and the engine's per-frame 3D bookkeeping has nothing
// to fight over (run 96's re-parent tug-of-war). The graph object address
// is unchanged — the whitelist, discovery logs and pass recipes survive
// untouched; the pose math carries over because menuObjects[0]'s world is
// the menu-space identity cascade (U5, runs 96/97 confirmed). The world
// holds no clone from the relocation tick on — FR-06 is structural. What
// remains is the ~1.5 s build window (place → grace → dress → relocate):
// the node-level early park keeps the 3D off the player, the actor-level
// park keeps it unreachable, and the AI pinning keeps it inert.
//
// The generic deep-copy route is falsified: NiObject::CreateDeepCopy
// returns a non-node for the player's dynamic graph classes.
//
// Lifecycle (F8): the clone actor is killed either immediately (despawn
// past the grace window) or by the state machine once the window ends;
// the whole build times out on real-frame counts paced by the render
// thread (run-28 finding: SKSE task queues drain within one game frame).

#include <RE/Skyrim.h>

#include <atomic>
#include <cstdint>

PLUGIN_NAMESPACE_BEGIN
    class PInstance
    {
    public:
        static PInstance& instance();

        // Game thread (panel toggle / load teardown). Starts the async P
        // build: place the clone actor now, then render-thread-paced steps
        // walk the state machine (grace -> dress -> 3D ready -> park +
        // whitelist). The panel keeps showing the item preview until the
        // whitelist arms.
        void spawn();

        // Game thread (SKSE messages). Arms the auto-spawn: P must go
        // through REAL unpaused world rendering at least once so the engine
        // creates the device-side rendererData (run 49: every pass rejected
        // at rendererData=null when the whole build ran paused in the
        // inventory). False on load/new-game teardown.
        void set_world_ready(bool a_ready);

        // Game thread, despawn/load teardown only (the panel toggle no
        // longer kills P). Disarms the whitelist and kills the clone actor.
        void despawn();

        // Game thread (pump task, every Residue_Scan_Interval_Frames):
        // deletes stale clones saved by PREVIOUS sessions — the placed
        // reference saves with the game, and a save made while a clone
        // existed reloads it as an unmanaged, un-pinned, visible player
        // duplicate (run 57). Match: runtime-created base (formID ≥
        // 0xFF000000) whose name is the v6.24 marker or the player's own
        // name; our live clone and the player are skipped. Logs every
        // deletion.
        void sweep_stale_clones();

        // Render thread, EVERY DrawInterfaceStart: kicks ONE game-thread
        // state-machine step per rendered frame (run-28 pacing fix — a
        // self-rescheduling task drains within one game frame and never
        // lets the engine load anything).
        void pump();

        // Game thread (panel opened): the U2 probe — verify the relocated
        // graph is still hosted under CP_StudioHome and re-home it if the
        // engine ever stripped the attachment. Logs the verdict every open
        // (stage-2b plan §3-7).
        void note_panel_open();

        // Render thread (draw entry): true while the game thread is inside
        // the relocation window (world detach -> menu-home attach); the
        // studio draw skips such frames.
        [[nodiscard]] bool relocating() const
        {
            return m_relocating.load(std::memory_order_acquire);
        }

        // Render thread: no-op on route 3 (no scene-graph surgery, no
        // retire list); kept for call-site symmetry with the redirector.
        void drain_retired();

        // Render thread (replay matcher): whether the geometry descends from
        // the whitelisted P root AND is a skinned body/armor piece — the
        // actor's whole graph (run 31: handheld props — quivers, torches,
        // weapons — ride the same graph but their world-transform draws blew
        // up to frame-filling blobs in the studio; skinned pieces are the
        // character, so the whitelist is skinned-only).
        [[nodiscard]] bool is_p_geometry(const RE::BSGeometry* geometry) const;

        // Render thread: whether the geometry descends from the whitelisted
        // P root (ancestor-chain match, no skinned-only gate).
        [[nodiscard]] bool is_p_descendant(const RE::BSGeometry* geometry) const;

        // Render thread: the studio anchor pose_for_studio last computed —
        // the fixed view-axis point the frontal light rig is built around.
        [[nodiscard]] RE::NiPoint3 studio_anchor() const { return m_studio_anchor; }

        // Render thread, inside the studio OM window BEFORE the pass
        // generator runs: poses the P root directly in the UI3D camera's
        // frame (run 38 — moving the actor proved unreliable; the draw-time
        // local-transform pose + Update cascade is what the skin matrices
        // actually read).
        void pose_for_studio();

        // Render thread / OM window: the whitelisted P root (null when the
        // whitelist is disarmed) — the proactive pass generator walks this
        // graph (run 35).
        [[nodiscard]] RE::NiAVObject* root() const
        {
            return reinterpret_cast<RE::NiAVObject*>(m_active_root.load(std::memory_order_acquire));
        }

    private:
        PInstance() = default;

        enum class State : std::uint8_t
        {
            kNone,
            kWaitingGrace,  // clone placed, engine finishing its bases
            kWaiting3D,     // dressed, waiting for the biped graph to settle
            kAttached,      // whitelist armed; ticks stop
            kKillPending,   // despawned before grace; kill when graced
        };

        // Stage-2b: where the P graph currently lives (game-thread state).
        enum class HomeState : std::uint8_t
        {
            kWorldParked,  // route 3 as before: graph in the world cell (grace/settle)
            kMenuHome,     // relocated: world-detached, hosted under menuObjects[0]
            kStuckParked,  // relocation gated off for this instance (fail-open, §5-H4)
        };

        // Game-thread frame step of the build/kill state machine (queued by
        // pump(), one per rendered frame).
        void tick();
        void attach_graph();
        void kill_actor();

        // Stage-2b (game thread): perform the graph relocation on the first
        // unpaused kAttached tick — no init gate (U1 answered by run 95:
        // the engine SetupAndDrawPass lazy-initializes the device buffers,
        // the world never needs to draw the clone first). Failure paths
        // inside relocate_home fail open to the parked architecture.
        void try_relocate();
        void relocate_home(RE::NiAVObject* a_graph);
        // Stage-2b teardown (game thread, despawn only, whitelist already
        // disarmed): detach the graph from CP_StudioHome and drop our
        // NiPointer — the last release frees the graph.
        void release_home();
        // Stage-2b (game thread, U2): the menu scene must keep hosting the
        // relocated graph; re-home from our NiPointer if stripped.
        void verify_home();

        std::atomic<State> m_state{ State::kNone };
        std::atomic<bool> m_step_queued{ false };
        // Run 49 (v6.15): set by the SKSE message handler; the auto-spawn
        // fires on the first UNPAUSED world frame with a rendered player so
        // the engine renderer-initializes P's geometries.
        std::atomic<bool> m_world_ready{ false };
        // The whitelisted P graph root (the parked actor's 3D) as a plain
        // address; written by the game thread, read by the render thread.
        std::atomic<std::uintptr_t> m_active_root{ 0 };
        RE::ObjectRefHandle m_clone{};
        std::uint32_t m_frames_since_place{ 0 };
        std::uint32_t m_dressed_frames{ 0 };
        std::uint32_t m_total_frames{ 0 };
        // v6.24 residue sweep cadence (game thread only).
        std::uint32_t m_frames_until_residue_scan{ 0 };
        // Stage-2b home state (game thread only, except the atomic latch the
        // render thread polls at draw entry).
        std::atomic<HomeState> m_home{ HomeState::kWorldParked };
        std::atomic<bool> m_relocating{ false };
        // Strong ownership once relocated — the engine holds no parent link
        // to the graph anymore, so this NiPointer plus the holder's child
        // slot are what keep it alive (the run-51 dangling-whitelist crash
        // class is structurally gone).
        RE::NiPointer<RE::NiAVObject> m_home_graph;
        RE::NiPointer<RE::NiNode> m_home_node;
        // v6.32: the last studio anchor pose_for_studio computed (render
        // thread only — the frontal light rig reads it).
        RE::NiPoint3 m_studio_anchor{ 0.0f, 0.0f, 0.0f };
    };
PLUGIN_NAMESPACE_END
