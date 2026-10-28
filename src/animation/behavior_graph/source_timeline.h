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

    // Opens a bounded recording window (F9). The window closes by itself so a forgotten key
    // press cannot consume the whole budget; pressing the key again restarts it.
    void arm_source_timeline();
}
PLUGIN_NAMESPACE_END
