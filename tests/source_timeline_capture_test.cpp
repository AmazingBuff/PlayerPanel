//
// C-G5-E4 engine-free cases: the capture budgets and the clock-sample stride.
//
// Build:
//   cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests\source_timeline_capture_test.cpp
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/source_timeline_policy.h"

#include <cassert>
#include <cstdint>
#include <cstdio>

using namespace test_plugin::animation::behavior_graph;

namespace
{
    void clock_samples_follow_the_declared_stride()
    {
        assert(source_clock_sample_due(0));
        assert(source_clock_sample_due(10));
        assert(source_clock_sample_due(120));
        assert(!source_clock_sample_due(1));
        assert(!source_clock_sample_due(9));
        assert(!source_clock_sample_due(11));
    }

    void snapshots_stop_at_the_declared_limit_and_count_the_overflow()
    {
        SourceCaptureBudget budget;
        for (std::uint32_t i = 0; i < Source_Pose_Snapshot_Limit; ++i)
            assert(budget.take_snapshot(1));
        assert(budget.snapshots() == Source_Pose_Snapshot_Limit);
        assert(!budget.take_snapshot(1));
        assert(budget.snapshot_dropped() == 1);
        assert(budget.snapshots() == Source_Pose_Snapshot_Limit);
        budget.reset();
        assert(budget.snapshots() == 0);
        assert(budget.clock_dropped() == 0);
    }

    void the_line_budget_refuses_rather_than_truncates()
    {
        SourceCaptureBudget budget;
        // A snapshot that would overflow the line budget is refused and both counters move.
        assert(!budget.take_snapshot(Source_Capture_Line_Budget + 1));
        assert(budget.snapshot_dropped() == 1);
        assert(budget.line_dropped() == 1);
        assert(budget.lines() == 0);
        // A clock row that fits is accepted and only the drop paths move on failure.
        assert(budget.take_clock(1));
        assert(budget.clocks() == 1);
        assert(budget.lines() == 1);
        SourceCaptureBudget full;
        for (std::uint32_t i = 0; i < Source_Clock_Sample_Limit; ++i)
            assert(full.take_clock(1));
        assert(!full.take_clock(1));
        assert(full.clock_dropped() == 1);
    }

    void only_command_edges_anchor_a_snapshot()
    {
        // C-G5-E4 v2: the 2026-11-10 run took eight snapshots because any change of the six
        // equipment-chain variables anchored. Only the two command edges do now, matching the two
        // inputs of the E3 ledger; everything else is confirmation.
        assert(source_settle_is_anchor("currentDefaultState", 0, 10));
        assert(source_settle_is_anchor("currentDefaultState", 1, 12));
        assert(source_settle_is_anchor("CURRENTDEFAULTSTATE", 0, 10));
        // Same variable, but not a command edge: equip settle, unequip return, transient edges.
        assert(!source_settle_is_anchor("currentDefaultState", 10, 1));
        assert(!source_settle_is_anchor("currentDefaultState", 12, 0));
        assert(!source_settle_is_anchor("currentDefaultState", 0, 1));
        assert(!source_settle_is_anchor("currentDefaultState", 10, 12));
        // Other equipment variables confirm, never anchor.
        assert(!source_settle_is_anchor("iState", 0, 9));
        assert(!source_settle_is_anchor("iRightHandEquipped", 0, 1));
        assert(!source_settle_is_anchor("IsEquipping", 0, 1));
        assert(!source_settle_is_anchor("bEquipUnequip", 0, 1));
        // Per-frame scalars and empty names never anchor.
        assert(!source_settle_is_anchor("TimeDelta", 0, 1));
        assert(!source_settle_is_anchor("", 0, 10));
    }

    void the_declared_limits_match_the_contract()
    {
        // These constants are the pre-registered budget of C-G5-E4 section 3.
        assert(Source_Pose_Snapshot_Limit == 8);
        assert(Source_Clock_Sample_Stride == 10);
        assert(Source_Clock_Sample_Limit == 120);
        assert(Source_Capture_Line_Budget == 1500);
        assert(Source_Capture_Max_Clips == 8);
        assert(Source_Capture_Max_Tracks == 116);
    }
}

int main()
{
    clock_samples_follow_the_declared_stride();
    snapshots_stop_at_the_declared_limit_and_count_the_overflow();
    the_line_budget_refuses_rather_than_truncates();
    only_command_edges_anchor_a_snapshot();
    the_declared_limits_match_the_contract();
    std::puts("5 source-timeline capture cases passed (engine-free)");
}
