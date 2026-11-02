//
// Created by AmazingBuff on 2026/10/31.
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
    // C-G3B-E1: install the read-only 58377 observer detour. Called once at kDataLoaded. With
    // SourceEventCapture=off this is a logged no-op; `on` additionally requires
    // IndependentGraph=off and SourceTimeline=off, `concurrent` only SourceTimeline=off. The
    // pre-registered head bytes are verified before patching; a mismatch leaves the process
    // unpatched and logs the observed bytes instead.
    void install_source_event_capture();

    // Read-only observer, called once per post-UI frame: owns the window state, drains the event
    // ring and samples the named phase variables. Off or uninstalled, this is one settings read
    // and returns.
    void sample_source_event_capture(RE::Actor& actor);

    // F7 outside the panel publishes a fresh bounded window request (the `on` lifecycle only).
    void arm_source_event_capture();

    // C-G4-C1: the concurrent lifecycle. Arms synchronously - identity, baseline and the detour
    // gate are established here, before the experiment's own WeapEquip/Unequip dispatches, so
    // those calls are always visible to the counters. Returns false (and logs why) when the mode
    // is not concurrent, the hook is not installed, or the source fields are unavailable.
    [[nodiscard]] bool arm_source_event_capture_concurrent(RE::BShkbAnimationGraph const& a_source_graph,
        std::uint32_t a_actor_id);

    // True while a window is open; the display close path uses it to end the concurrent window at
    // the close/reset boundary instead of waiting for the 20-second expiry.
    [[nodiscard]] bool source_event_capture_window_open();

    // Closes the open window with `reason`, emitting the counters (calls / other-graph / source
    // events / ring drops) that the pre-registered liveness control reads.
    void close_source_event_capture_window(char const* a_reason);

    // Queues cancellation across a world boundary, consumed before the next source read.
    void invalidate_source_event_capture();
}
PLUGIN_NAMESPACE_END
