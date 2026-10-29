//
// Created by AmazingBuff on 2026/10/28.
//
#pragma once

namespace RE
{
    class Actor;
}

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // G3-A: read the player's own graph during normal play and log only what changes.
    // Called once per post-UI frame; it never writes, never dispatches events and never
    // touches the retained candidate. Off unless [Experiment] SourceTimeline is on.
    void sample_source_timeline(RE::Actor& actor);

    // Publishes a fresh bounded window request (F7); the sampler owns all recording state.
    void arm_source_timeline();

    // Queues cancellation across a world boundary, consumed before the next source read.
    void invalidate_source_timeline();
}
PLUGIN_NAMESPACE_END
