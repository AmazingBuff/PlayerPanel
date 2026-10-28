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
}
PLUGIN_NAMESPACE_END
