//
// Created by AmazingBuff on 2026/11/03.
//
#pragma once

#include <cstdint>

namespace RE
{
    class Actor;
    class BShkbAnimationGraph;
}

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // C-G5-E1: install the two read-only liveness detours (63589 sampling entry, 63856 poseLocal
    // last writer). Called once at kDataLoaded. Off leaves both functions unpatched; a head-byte
    // mismatch or a missing trampoline leaves the process unpatched and logs the observed bytes.
    // Refuses to install beside the other detour probes so one run cannot mix instruments.
    void install_probe_liveness();

    // Per-frame render drain: expires the window, drains detail records, reads the deferred-update
    // queue state and counts frames. Off or uninstalled this is one settings read and returns.
    void sample_probe_liveness();

    // F7 outside the panel publishes one window request (the `on` lifecycle only).
    void arm_probe_liveness();

    // C-G5-E1 concurrent lifecycle: pins the source graph, resets the counters and arms before the
    // experiment's own dispatches. Returns false (and logs why) when the mode is not concurrent,
    // nothing is installed, or the graph is no longer managed.
    [[nodiscard]] bool arm_probe_liveness_concurrent(RE::BShkbAnimationGraph const& a_source_graph,
        std::uint32_t a_actor_id);

    [[nodiscard]] bool probe_liveness_window_open();

    // Closes the open window and emits every counter. Ignores the call when no window is open.
    void close_probe_liveness_window(char const* a_reason);

    // Queues cancellation across a world boundary, consumed before the next arm.
    void invalidate_probe_liveness();
}
PLUGIN_NAMESPACE_END
