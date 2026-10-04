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

namespace CharacterPanelProto
{
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
        // P root, WITHOUT the skinned-only gate. The skin filter is a studio
        // content choice (props would blob the studio frame); hiding wants
        // the whole graph — body, armor AND the props riding it.
        [[nodiscard]] bool is_p_descendant(const RE::BSGeometry* geometry) const;

        // Render thread (thunks, WORLD frames only): the v6.22 layer-1 ghost
        // gate. True when this pass belongs to the P graph and must be
        // dropped from the engine's own draw, so the clone never reaches
        // pixels in normal play — the v6.19 clause covers panel-open frames,
        // this extends the same silence to panel-closed ones. Fails open:
        // it arms only after Ghost_Warmup_Passes world-stream P passes have
        // ARRIVED (an arrival proves the engine just drew the clone — real
        // draws are the only creator of rendererData/VB/IB, run 50), and
        // only while the whitelist is armed (kAttached). M1 contract: a
        // re-dress must reset the warm window (new geometries need a fresh
        // init window) via spawn-time reset semantics.
        [[nodiscard]] bool ghost_should_suppress(const RE::BSGeometry* geometry);

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

        // Game-thread frame step of the build/kill state machine (queued by
        // pump(), one per rendered frame).
        void tick();
        void attach_graph();
        void kill_actor();

        // Game thread, kAttached only (run 51): periodic probe until the
        // world renderer has created the clone's device-side buffers —
        // P=clone vs player=own graph as control group. The manual draw's
        // rd gate starts passing the moment the buffers appear.
        void init_heartbeat();

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
        // Run 51: renderer-init heartbeat state (game thread only).
        std::uint32_t m_frames_since_attach{ 0 };
        std::uint32_t m_init_beats{ 0 };
        bool m_init_done{ false };
        // v6.22 ghost layer (render-thread state, mutated from the pass
        // thunks; spawn resets on the game thread — benign race, worst case
        // a slightly longer warmup on a fresh clone).
        std::atomic<bool> m_ghost_warm{ false };
        std::atomic<std::uint32_t> m_ghost_warmup_passes{ 0 };
        std::atomic<bool> m_ghost_first_drop{ false };
        // v6.24 residue sweep cadence (game thread only).
        std::uint32_t m_frames_until_residue_scan{ 0 };
        // v6.32: the last studio anchor pose_for_studio computed (render
        // thread only — the frontal light rig reads it).
        RE::NiPoint3 m_studio_anchor{ 0.0f, 0.0f, 0.0f };
    };
}
