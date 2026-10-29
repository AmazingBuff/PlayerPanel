//
// Created by AmazingBuff on 2026/10/29.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/source_timeline_policy.h"

#include <cassert>
#include <iostream>

#ifdef NDEBUG
#error This test requires assertions.
#endif

using test_plugin::animation::behavior_graph::SourceTimelineIdentity;
using test_plugin::animation::behavior_graph::SourceTimelineIntent;
using test_plugin::animation::behavior_graph::SourceTimelineRequests;
using test_plugin::animation::behavior_graph::SourceTimelineSample;
using test_plugin::animation::behavior_graph::SourceTimelineWindow;
using test_plugin::animation::behavior_graph::Source_Timeline_Line_Budget;

void exhausted_window_and_two_rearms_start_independent_baselines()
{
    std::chrono::steady_clock::time_point const start{};
    SourceTimelineIdentity const source{ 1, 2, 3, 4, 5, 6, 463 };
    SourceTimelineWindow window;
    window.arm(start);
    assert(window.observe_source(source) == SourceTimelineSample::e_baseline);
    assert(window.observe_source(source) == SourceTimelineSample::e_compare);
    for (std::uint32_t line = 0; line < Source_Timeline_Line_Budget; ++line)
        assert(window.take_line());
    assert(!window.take_line() && window.exhausted());
    assert(window.lines() == 800 && !window.take_line());

    window.arm(start + std::chrono::seconds(5));
    assert(window.active() && !window.exhausted() && window.lines() == 0);
    assert(window.observe_source(source) == SourceTimelineSample::e_baseline);
    assert(window.observe_source(source) == SourceTimelineSample::e_compare);
    for (std::uint32_t line = 0; line < Source_Timeline_Line_Budget; ++line)
        assert(window.take_line());
    assert(!window.take_line() && window.exhausted());

    window.arm(start + std::chrono::seconds(10));
    assert(!window.exhausted() && window.lines() == 0);
    assert(window.observe_source(source) == SourceTimelineSample::e_baseline);
    for (std::uint32_t line = 0; line < Source_Timeline_Line_Budget; ++line)
        assert(window.take_line());
    assert(!window.take_line() && window.lines() == 800);
    assert(!window.expire(start + std::chrono::seconds(29)));
    assert(window.expire(start + std::chrono::seconds(30)) && !window.active());
}

void replaced_lost_and_invalidated_sources_require_explicit_new_windows()
{
    std::chrono::steady_clock::time_point const start{};
    SourceTimelineIdentity const source{ 1, 2, 3, 4, 5, 6, 463 };
    SourceTimelineIdentity const replaced{ 1, 7, 8, 9, 10, 11, 463 };
    SourceTimelineIdentity const moved_storage{ 1, 7, 8, 9, 10, 12, 463 };
    SourceTimelineRequests requests;
    SourceTimelineWindow window;

    requests.arm();
    requests.invalidate();
    requests.arm();
    assert(requests.take() == SourceTimelineIntent::e_invalidate);
    window.cancel();
    assert(requests.take() == SourceTimelineIntent::e_none);
    assert(window.observe_source(source) == SourceTimelineSample::e_aborted);

    requests.arm();
    assert(requests.take() == SourceTimelineIntent::e_arm);
    window.arm(start);
    assert(window.observe_source(source) == SourceTimelineSample::e_baseline);
    assert(window.observe_source(replaced) == SourceTimelineSample::e_aborted);
    assert(!window.active());
    assert(window.observe_source(replaced) == SourceTimelineSample::e_aborted);

    window.arm(start + std::chrono::seconds(1));
    assert(window.observe_source(replaced) == SourceTimelineSample::e_baseline);
    assert(window.observe_source(moved_storage) == SourceTimelineSample::e_aborted);
    assert(window.observe_source(moved_storage) == SourceTimelineSample::e_aborted);

    window.arm(start + std::chrono::seconds(2));
    assert(window.observe_source(moved_storage) == SourceTimelineSample::e_baseline);
    window.cancel(); // The sampler reports unavailable graph/data/storage through this path.
    assert(window.observe_source(moved_storage) == SourceTimelineSample::e_aborted);

    window.arm(start + std::chrono::seconds(3));
    assert(!window.expire(start + std::chrono::seconds(22)));
    assert(window.expire(start + std::chrono::seconds(23)));
    assert(window.observe_source(source) == SourceTimelineSample::e_aborted);

    window.arm(start + std::chrono::seconds(30));
    assert(window.observe_source(source) == SourceTimelineSample::e_baseline);
    requests.invalidate();
    assert(requests.take() == SourceTimelineIntent::e_invalidate);
    window.cancel();
    assert(!window.active() && requests.take() == SourceTimelineIntent::e_none);
    assert(window.observe_source(source) == SourceTimelineSample::e_aborted);
}

int main()
{
    exhausted_window_and_two_rearms_start_independent_baselines();
    replaced_lost_and_invalidated_sources_require_explicit_new_windows();
    std::cout << "2 source-timeline session regression cases passed (engine-free)\n";
}
