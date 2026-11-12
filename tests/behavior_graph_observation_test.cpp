//
// Created by AmazingBuff on 2026/11/12.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/drive_step_policy.h"
#include "animation/behavior_graph/observation_policy.h"

#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <string_view>

#ifdef NDEBUG
#error This test requires assertions.
#endif

namespace
{
    using test_plugin::animation::behavior_graph::ClipClockHistory;
    using test_plugin::animation::behavior_graph::ClipClockTrace;
    using test_plugin::animation::behavior_graph::FrameStage;
    using test_plugin::animation::behavior_graph::ObservationOutcome;
    using test_plugin::animation::behavior_graph::ObservationSegment;
    using test_plugin::animation::behavior_graph::ObservedClipClock;
    using test_plugin::animation::behavior_graph::SegmentObservation;
    using test_plugin::animation::behavior_graph::clock_delta;
    using test_plugin::animation::behavior_graph::drive_graph_step;

    constexpr float Timestep = 1.0F / 60.0F;

    struct Behavior { bool updateActiveNodes; };
    struct Character { Behavior* behaviorGraph; std::uint16_t numPoseLocal; };
    struct Graph
    {
        Character characterInstance;
        void* physicsWorld;
        std::uint16_t numAnimBones;
        std::int32_t state;
        std::uint32_t generated;
        float clock;
    };

    template <class Observe>
    void advance(Graph& graph, SegmentObservation& segment, std::int32_t state, float time,
        Observe const& observe)
    {
        Graph const source{ {}, nullptr, 0, 0, 0, 0.0F };
        segment.command_advance(Timestep);
        bool returned = false;
        std::uint32_t accepted = 0;
        drive_graph_step(graph, source, Timestep, nullptr, [](Graph&) {},
            [](Graph&, void*) {}, [&](Graph& target) {
                target.state = state;
                target.clock = time;
                ++target.generated;
                returned = true;
            }, [&](FrameStage stage) {
                if (!segment.observe_stage(stage))
                {
                    assert(!returned);
                    return;
                }
                assert(returned && graph.generated == segment.completed());
                assert(std::string_view(segment.stage_name()) == "post-sample");
                ++accepted;
                segment.observe_state(graph.state);
                observe();
                segment.finish_point();
            });
        assert(accepted == 1);
    }

    void successful_trace_preserves_clocks_and_delayed_state_return()
    {
        Behavior behavior{ false };
        Graph graph{ { &behavior, 116 }, nullptr, 0, 0, 0, 0.0F };
        SegmentObservation equip(ObservationSegment::e_equip);
        ClipClockHistory equip_clocks(equip.limit());
        std::array<ObservedClipClock, 2> clocks{
            ObservedClipClock{ { "Animations/one/idle.hkx", 0x10000, 22 }, 0.0F },
            ObservedClipClock{ { "Animations/two/idle.hkx", 0x20000, 75 }, 0.0F }
        };
        assert(equip_clocks.record(clocks));
        assert(equip.completed() == 0 && std::string_view(equip.stage_name()) == "baseline");
        std::vector<std::uint32_t> equip_checkpoints;
        while (equip.can_advance())
        {
            std::uint32_t const next = equip.completed() + 1;
            float const time = next == 1 ? 1.0F / 128.0F :
                (next == 2 ? 1.0F / 128.0F + 23.0F / 60.0F :
                    (next == 3 ? -0.125F : static_cast<float>(next) / 60.0F));
            advance(graph, equip, next < 90 ? 10 : 1, time, [&] {
                clocks[0].local_time = graph.clock;
                clocks[1].local_time = -graph.clock;
                std::span<ObservedClipClock const> const visible = next == 4
                    ? std::span<ObservedClipClock const>(clocks).subspan(1) : clocks;
                assert(equip_clocks.record(visible));
            });
            if (equip.checkpoint()) equip_checkpoints.push_back(equip.completed());
        }
        assert(equip.outcome() == ObservationOutcome::e_completed && equip.completed() == 120);
        assert((equip_checkpoints == std::vector<std::uint32_t>{ 1, 60, 120 }));
        assert(equip.elapsed() == 120.0 * static_cast<double>(Timestep));
        assert(equip_clocks.traces().size() == 2 && equip_clocks.points() == 121);
        ClipClockTrace const& first = equip_clocks.traces()[0];
        assert(*first.time_bits[1] == std::bit_cast<std::uint32_t>(1.0F / 128.0F));
        assert(*clock_delta(first, 1) == 1.0 / 128.0);
        assert(std::lround(*clock_delta(first, 2) * 60.0) == 23);
        assert(*clock_delta(first, 3) < 0.0 && !first.time_bits[4] && !clock_delta(first, 5));
        assert(std::bit_cast<float>(*first.time_bits[3]) == -0.125F);
        char const old_positive = static_cast<char>('A' + (23 - 10));
        char const old_negative = 'N';
        assert(old_positive == old_negative);

        SegmentObservation unequip(ObservationSegment::e_unequip);
        ClipClockHistory unequip_clocks(unequip.limit());
        unequip.observe_state(0);
        assert(unequip.can_advance() && !unequip.active_seen());
        clocks[0].local_time = 0.0F;
        assert(unequip_clocks.record(std::span<ObservedClipClock const>(clocks).first(1)));
        graph.generated = 0;
        std::vector<std::uint32_t> unequip_checkpoints;
        while (unequip.can_advance())
        {
            std::uint32_t const next = unequip.completed() + 1;
            advance(graph, unequip, next == 123 ? 0 : 12, static_cast<float>(next) / 60.0F, [&] {
                clocks[0].local_time = graph.clock;
                assert(unequip_clocks.record(std::span<ObservedClipClock const>(clocks).first(1)));
            });
            if (unequip.checkpoint()) unequip_checkpoints.push_back(unequip.completed());
        }
        assert(unequip.outcome() == ObservationOutcome::e_completed && unequip.completed() == 123);
        assert(unequip.commanded() == 123 && unequip.first_zero() == 123u && unequip.last_state() == 0);
        assert(unequip.active_seen() && std::string_view(unequip.reason()) == "state-12-to-0");
        assert(unequip.elapsed() == 123.0 * static_cast<double>(Timestep));
        assert((unequip_checkpoints == std::vector<std::uint32_t>{ 1, 60, 123 }));
        assert(std::string_view(equip.segment_name()) == "equip" && std::string_view(unequip.segment_name()) == "unequip");
        assert(unequip_clocks.points() == 124 && equip_clocks.points() == 121);
        assert(!clock_delta(unequip_clocks.traces()[0], 0));
        std::cout << "A PASS: Generate-gated equip=120, unequip=123 state-12-to-0; exact fractional/+23/negative bits, absence, distinct same-leaf instances; original +23=N collision reproduced\n";
    }

    void active_state_at_limit_is_timeout()
    {
        Behavior behavior{ false };
        Graph graph{ { &behavior, 116 }, nullptr, 0, 12, 0, 0.0F };
        SegmentObservation unequip(ObservationSegment::e_unequip);
        unequip.observe_state(graph.state);
        while (unequip.can_advance())
        {
            advance(graph, unequip, 12, static_cast<float>(unequip.commanded() + 1) / 60.0F, [] {});
            assert(unequip.outcome() != ObservationOutcome::e_completed && !unequip.first_zero());
        }
        assert(unequip.outcome() == ObservationOutcome::e_timeout && unequip.completed() == 180);
        assert(unequip.commanded() == 180 && unequip.last_state() == 12 && unequip.active_seen());
        assert(unequip.elapsed() == 180.0 * static_cast<double>(Timestep));
        assert(std::string_view(unequip.reason()) == "advance-limit-without-state-return");
        std::cout << "B PASS: state 12 held through 180 Generate returns; timeout, no completion, elapsed=" << unequip.elapsed() << "\n";
    }
}

int main()
{
    successful_trace_preserves_clocks_and_delayed_state_return();
    active_state_at_limit_is_timeout();
    std::cout << "2 observation scenarios passed (engine-free; native behavior unverified)\n";
}
