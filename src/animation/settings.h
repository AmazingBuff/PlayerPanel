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
    // v28: advance the retained graph one step per rendered frame instead of showing the frozen pose.
    [[nodiscard]] bool display_step();
}
PLUGIN_NAMESPACE_END
