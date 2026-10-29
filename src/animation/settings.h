//
// Created by AmazingBuff on 2026/10/20.
//
#pragma once
#include "animation/behavior_graph/independent_graph.h"
#include "animation/idle_policy.h"
PLUGIN_NAMESPACE_BEGIN
namespace animation
{
    void load_settings();
    [[nodiscard]] UnarmedIdle unarmed_idle_setting();
    [[nodiscard]] bool detailed_animation_trace();
    [[nodiscard]] bool behavior_graph_probe();
    [[nodiscard]] behavior_graph::IndependentGraphMode independent_graph_setting();
    // v27: when true and a candidate is retained, the panel's display pose comes from that
    // candidate instead of the OAR clip sampler.
    [[nodiscard]] bool display_pose();
    // Pace fixed 1/60 steps by eligible elapsed time, capped at 100 ms / 6 steps per display call.
    [[nodiscard]] bool display_step();
    // G3-A: log the player graph's variable/active-node changes during normal play.
    [[nodiscard]] bool source_timeline();
}
PLUGIN_NAMESPACE_END
