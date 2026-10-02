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

        // Game thread (load / new-game teardown only — the panel toggle no
        // longer kills P: run 49 showed a clone built while paused can never
        // be renderer-initialized, so the built instance is kept for the
        // whole session and rebuilt only across loads). Disarms the
        // whitelist and kills the clone actor — immediately when past the
        // grace window, otherwise via the state machine (plan F8).
        void despawn();

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
    };
}
