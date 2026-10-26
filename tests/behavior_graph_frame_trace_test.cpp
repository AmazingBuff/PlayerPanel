//
// Created by AmazingBuff on 2026/10/26.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/frame_trace_policy.h"

#include <array>
#include <cassert>
#include <iostream>
#include <limits>

#ifdef NDEBUG
#error This test requires assertions.
#endif

void selected_boundaries_are_bounded_and_not_repeated()
{
    using test_plugin::animation::behavior_graph::FrameStage;
    test_plugin::animation::behavior_graph::FrameTraceBudget budget;
    std::uint32_t emitted = 0;
    for (std::uint32_t step = 0; step < 120; ++step)
    {
        for (std::uint8_t phase = 0; phase < 5; ++phase)
        {
            const FrameStage stage = static_cast<FrameStage>(phase);
            if (budget.take(step, stage)) ++emitted;
            assert(!budget.take(step, stage));
        }
    }
    assert(emitted == 30 && budget.rows() == 30);
    assert(!budget.take(120, FrameStage::e_pre_pipeline));
    assert(!budget.take(0, static_cast<FrameStage>(255)));
    assert(budget.take_summary() && !budget.take_summary());
    test_plugin::animation::behavior_graph::FrameTraceBudget next_run;
    assert(next_run.take(0, FrameStage::e_pre_pipeline) && next_run.rows() == 1);
}

void raw_pose_progress_keeps_transient_changes_and_missing_observations()
{
    test_plugin::animation::behavior_graph::RawPoseProgress progress(7);
    for (std::uint32_t completed = 1; completed <= 120; ++completed)
    {
        if (completed == 3) progress.observe(std::nullopt);
        else if (completed == 4) progress.observe(9);
        else progress.observe(7);
    }
    assert(progress.completed() == 120);
    assert(progress.available() == 119 && progress.missing() == 1);
    assert(progress.baseline() == 7 && progress.changes() == 2);
    assert(progress.first_changed() == 4);
}

void eye_point_and_arm_detection_are_bounded()
{
    using test_plugin::animation::behavior_graph::arm_observed;
    using test_plugin::animation::behavior_graph::Eye_Offset_Z;
    using test_plugin::animation::behavior_graph::EyePoint;
    using test_plugin::animation::behavior_graph::make_eye_point;

    const EyePoint zero = make_eye_point(std::nullopt);
    assert(zero.x == 0.0F && zero.y == 0.0F && zero.z == 0.0F);

    const EyePoint shifted = make_eye_point(std::array<float, 3>{ 1.5F, -2.0F, 3.0F });
    assert(shifted.x == 1.5F && shifted.y == -2.0F && shifted.z == 3.0F + Eye_Offset_Z);

    const float lowest = std::numeric_limits<float>::lowest();
    assert(!arm_observed(0.0F, lowest));
    assert(!arm_observed(lowest, lowest));
    assert(arm_observed(-0.0167F, 0.2333F));
    assert(arm_observed(0.0F, 0.0F));
}

int main()
{
    selected_boundaries_are_bounded_and_not_repeated();
    raw_pose_progress_keeps_transient_changes_and_missing_observations();
    eye_point_and_arm_detection_are_bounded();
    std::cout << "3 frame-trace policy/progress cases passed (engine-free)\n";
}
