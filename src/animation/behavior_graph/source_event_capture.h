//
// Created by AmazingBuff on 2026/10/31.
//
#pragma once

namespace RE
{
    class Actor;
}

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // C-G3B-E1: install the read-only 58377 observer detour. Called once at kDataLoaded; a
    // logged no-op unless SourceEventCapture is on with IndependentGraph=off and
    // SourceTimeline off. The pre-registered head bytes are verified before patching; a
    // mismatch leaves the process unpatched and logs the observed bytes instead.
    void install_source_event_capture();

    // Read-only observer, called once per post-UI frame: owns the F7 window state, drains the
    // event ring and samples the named phase variables. Off or uninstalled, this is one
    // settings read and returns.
    void sample_source_event_capture(RE::Actor& actor);

    // F7 outside the panel publishes a fresh bounded window request.
    void arm_source_event_capture();

    // Queues cancellation across a world boundary, consumed before the next source read.
    void invalidate_source_event_capture();
}
PLUGIN_NAMESPACE_END
