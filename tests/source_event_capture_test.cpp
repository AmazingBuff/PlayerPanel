//
// Created by AmazingBuff on 2026/10/31.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/source_event_capture_policy.h"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <string_view>

using test_plugin::animation::behavior_graph::SourceEventBudget;
using test_plugin::animation::behavior_graph::SourceEventCaptureMode;
using test_plugin::animation::behavior_graph::SourceEventRecord;
using test_plugin::animation::behavior_graph::SourceEventRing;
using test_plugin::animation::behavior_graph::SourceEventWindow;
using test_plugin::animation::behavior_graph::parse_source_event_capture;

namespace
{
    // C-G4-C1: the INI key is a three-state mode now. A typo must never arm the hook, and the
    // historical boolean spellings must keep parsing so old INI files reproduce verbatim.
    void mode_text_maps_to_three_states()
    {
        assert(parse_source_event_capture("off") == SourceEventCaptureMode::e_off);
        assert(parse_source_event_capture("false") == SourceEventCaptureMode::e_off);
        assert(parse_source_event_capture("0") == SourceEventCaptureMode::e_off);
        assert(parse_source_event_capture("") == SourceEventCaptureMode::e_off);
        assert(parse_source_event_capture("on") == SourceEventCaptureMode::e_on);
        assert(parse_source_event_capture("TRUE") == SourceEventCaptureMode::e_on);
        assert(parse_source_event_capture("1") == SourceEventCaptureMode::e_on);
        assert(parse_source_event_capture("concurrent") == SourceEventCaptureMode::e_concurrent);
        assert(parse_source_event_capture("Concurrent") == SourceEventCaptureMode::e_concurrent);
        assert(!parse_source_event_capture("concurrentt").has_value());
        assert(!parse_source_event_capture("yes").has_value());
    }

    void ring_preserves_order_and_counts_overflow()
    {
        SourceEventRing ring;
        for (std::uint32_t i = 0; i < 2000; ++i)
            (void)ring.push(SourceEventRecord{ i, 0, 0 });
        // Depth 1024 with drop-newest overflow: the producer must never touch the consumer's
        // tail index, so a full ring drops the arriving records and counts them. Draining every
        // frame makes overflow unreachable at game event rates; the counter is the loud path if
        // it ever happens.
        assert(ring.dropped() == 976);
        SourceEventRecord record{};
        for (std::uint32_t i = 0; i < 1024; ++i)
        {
            assert(ring.pop(record));
            assert(record.id == i);
        }
        assert(!ring.pop(record));
        // drain_discard on a fresh window must hide pre-window records without inventing any.
        (void)ring.push(SourceEventRecord{ 42, 1, 0 });
        ring.drain_discard();
        assert(!ring.pop(record));
        assert(ring.dropped() == 976);
        ring.reset_dropped();
        assert(ring.dropped() == 0);
    }

    void budgets_run_independently_and_close_the_window()
    {
        SourceEventWindow window;
        auto const now = std::chrono::steady_clock::now();
        window.arm(now);
        for (std::uint32_t i = 0; i < 600; ++i)
            assert(window.take_event_line());
        // The 601st event line exhausts the event budget and closes the window; the phase
        // budget is untouched but can no longer take lines.
        assert(!window.take_event_line());
        assert(!window.active());
        assert(window.exhausted_budget() == SourceEventBudget::e_event);
        assert(window.event_lines() == 600);
        assert(!window.take_phase_line());
        assert(window.phase_lines() == 0);

        window.arm(now);
        for (std::uint32_t i = 0; i < 200; ++i)
            assert(window.take_phase_line());
        assert(!window.take_phase_line());
        assert(!window.active());
        assert(window.exhausted_budget() == SourceEventBudget::e_phase);
        assert(window.phase_lines() == 200);
        assert(window.event_lines() == 0);
    }

    void cancel_and_expire_beat_open_budgets()
    {
        SourceEventWindow window;
        auto const now = std::chrono::steady_clock::now();
        window.arm(now);
        window.cancel();
        assert(!window.active());
        assert(!window.take_event_line());

        window.arm(now);
        assert(window.expire(now + std::chrono::seconds(20)));
        assert(!window.active());
        // Expiring once stays expired; a second observation is not a second close.
        assert(!window.expire(now + std::chrono::seconds(40)));
    }
}

int main()
{
    mode_text_maps_to_three_states();
    ring_preserves_order_and_counts_overflow();
    budgets_run_independently_and_close_the_window();
    cancel_and_expire_beat_open_budgets();
    return 0;
}
