//
// Created by AmazingBuff on 2026/10/29.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/display_step_clock.h"

#include <cassert>
#include <iostream>

#ifdef NDEBUG
#error This test requires assertions.
#endif

using test_plugin::animation::behavior_graph::DisplayStepClock;
using test_plugin::animation::behavior_graph::Display_Max_Steps;

void thirty_fps_advances_120_steps_over_two_elapsed_seconds()
{
    std::chrono::steady_clock::time_point const start{};
    DisplayStepClock clock;
    assert(clock.advance(start) == 0);
    std::uint32_t total = 0;
    for (std::uint32_t frame = 1; frame <= 60; ++frame)
    {
        std::chrono::nanoseconds const elapsed(2'000'000'000LL * frame / 60);
        std::uint32_t const due = clock.advance(start +
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(elapsed));
        assert(due <= Display_Max_Steps);
        total += due;
    }
    assert(total == 120);
}

void high_fps_waits_and_resets_drop_hidden_time_with_bounded_stall_catchup()
{
    std::chrono::steady_clock::time_point const start{};
    DisplayStepClock clock;
    assert(clock.advance(start) == 0);
    std::uint32_t total = 0;
    std::uint32_t waits = 0;
    for (std::uint32_t frame = 1; frame <= 240; ++frame)
    {
        std::chrono::nanoseconds const elapsed(2'000'000'000LL * frame / 240);
        std::uint32_t const due = clock.advance(start +
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(elapsed));
        assert(due <= Display_Max_Steps);
        total += due;
        waits += due == 0 ? 1u : 0u;
    }
    assert(total == 120 && waits > 0);

    clock.reset();
    assert(clock.advance(start + std::chrono::seconds(30)) == 0);
    assert(clock.advance(start + std::chrono::seconds(30) + std::chrono::milliseconds(8)) == 0);
    assert(clock.advance(start + std::chrono::seconds(35)) == 6);
    assert(clock.advance(start + std::chrono::seconds(35) + std::chrono::milliseconds(8)) == 0);
    assert(clock.advance(start + std::chrono::seconds(35) + std::chrono::milliseconds(9)) == 1);

    clock.reset();
    assert(clock.advance(start + std::chrono::seconds(60)) == 0);
    assert(clock.advance(start + std::chrono::seconds(60) + std::chrono::milliseconds(17)) == 1);
}

int main()
{
    thirty_fps_advances_120_steps_over_two_elapsed_seconds();
    high_fps_waits_and_resets_drop_hidden_time_with_bounded_stall_catchup();
    std::cout << "2 display-clock regression cases passed (engine-free)\n";
}
