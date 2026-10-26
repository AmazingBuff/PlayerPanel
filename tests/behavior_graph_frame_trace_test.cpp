//
// Created by AmazingBuff on 2026/10/26.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/frame_trace_policy.h"

#include <cassert>
#include <iostream>

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

int main()
{
    selected_boundaries_are_bounded_and_not_repeated();
    raw_pose_progress_keeps_transient_changes_and_missing_observations();
    std::cout << "2 frame-trace policy/progress cases passed (engine-free)\n";
}
