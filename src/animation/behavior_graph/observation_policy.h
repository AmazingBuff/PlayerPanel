//
// Created by AmazingBuff on 2026/11/12.
//
#pragma once

#include "animation/behavior_graph/frame_trace_policy.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    inline constexpr std::uint32_t Equip_Observation_Advances = 120;
    inline constexpr std::uint32_t Unequip_Observation_Advance_Limit = 180;
    inline constexpr std::size_t Observation_Node_Limit = 256;
    inline constexpr std::size_t Observation_Clip_Limit = 8;
    inline constexpr std::size_t Observation_Trace_Limit = 32;
    inline constexpr std::size_t Observation_Clip_Name_Limit = 1024;

    enum class ObservationSegment : std::uint8_t { e_equip, e_unequip };
    enum class ObservationOutcome : std::uint8_t { e_running, e_completed, e_timeout, e_invalid };

    class SegmentObservation
    {
    public:
        explicit SegmentObservation(ObservationSegment segment, bool full_generation = true) :
            m_segment(segment), m_full_generation(full_generation), m_commanded(0), m_completed(0),
            m_elapsed(0.0), m_active_seen(false), m_first_zero(), m_last_state(),
            m_outcome(full_generation ? ObservationOutcome::e_running : ObservationOutcome::e_invalid),
            m_reason(full_generation ? "pending" : "full-generate-required") {}

        void command_advance(float timestep)
        {
            ++m_commanded;
            m_elapsed += static_cast<double>(timestep);
        }

        [[nodiscard]] bool observe_stage(FrameStage stage)
        {
            if (stage != FrameStage::e_post_sample || !m_full_generation) return false;
            ++m_completed;
            return true;
        }

        void observe_state(std::optional<std::int32_t> state)
        {
            m_last_state = state;
            if (m_segment != ObservationSegment::e_unequip) return;
            if (!state) { invalidate("state-unreadable"); return; }
            if (*state == 12) m_active_seen = true;
            if (m_completed > 0 && m_active_seen && *state == 0 && !m_first_zero)
                m_first_zero = m_completed;
        }

        void finish_point()
        {
            if (m_outcome == ObservationOutcome::e_invalid) return;
            if (m_segment == ObservationSegment::e_unequip && m_first_zero)
            {
                m_outcome = ObservationOutcome::e_completed;
                m_reason = "state-12-to-0";
            }
            else if (m_completed == limit())
            {
                m_outcome = m_segment == ObservationSegment::e_equip
                    ? ObservationOutcome::e_completed : ObservationOutcome::e_timeout;
                m_reason = m_segment == ObservationSegment::e_equip
                    ? "fixed-equip-budget" : "advance-limit-without-state-return";
            }
        }

        void invalidate(char const* reason)
        {
            if (m_outcome == ObservationOutcome::e_invalid) return;
            m_outcome = ObservationOutcome::e_invalid;
            m_reason = reason;
        }

        [[nodiscard]] bool can_advance() const
        {
            return m_commanded < limit() &&
                (m_segment == ObservationSegment::e_equip || m_outcome == ObservationOutcome::e_running);
        }

        [[nodiscard]] bool checkpoint() const
        {
            return m_completed == 1 || m_completed == 60 || m_completed == limit() ||
                (m_completed > 0 && m_segment == ObservationSegment::e_unequip &&
                    m_outcome != ObservationOutcome::e_running);
        }

        [[nodiscard]] std::uint32_t limit() const
        {
            return m_segment == ObservationSegment::e_equip
                ? Equip_Observation_Advances : Unequip_Observation_Advance_Limit;
        }
        [[nodiscard]] char const* segment_name() const
        {
            return m_segment == ObservationSegment::e_equip ? "equip" : "unequip";
        }
        [[nodiscard]] char const* stage_name() const { return m_completed == 0 ? "baseline" : "post-sample"; }
        [[nodiscard]] char const* outcome_name() const
        {
            switch (m_outcome)
            {
            case ObservationOutcome::e_running: return "running";
            case ObservationOutcome::e_completed: return "completed";
            case ObservationOutcome::e_timeout: return "timeout";
            case ObservationOutcome::e_invalid: return "invalid";
            }
            return "invalid";
        }
        [[nodiscard]] ObservationOutcome outcome() const { return m_outcome; }
        [[nodiscard]] char const* reason() const { return m_reason; }
        [[nodiscard]] std::uint32_t commanded() const { return m_commanded; }
        [[nodiscard]] std::uint32_t completed() const { return m_completed; }
        [[nodiscard]] double elapsed() const { return m_elapsed; }
        [[nodiscard]] bool active_seen() const { return m_active_seen; }
        [[nodiscard]] std::optional<std::uint32_t> first_zero() const { return m_first_zero; }
        [[nodiscard]] std::optional<std::int32_t> last_state() const { return m_last_state; }

    private:
        ObservationSegment m_segment;
        bool m_full_generation;
        std::uint32_t m_commanded;
        std::uint32_t m_completed;
        double m_elapsed;
        bool m_active_seen;
        std::optional<std::uint32_t> m_first_zero;
        std::optional<std::int32_t> m_last_state;
        ObservationOutcome m_outcome;
        char const* m_reason;
    };

    struct ClipInstanceKey
    {
        std::string full_name;
        std::uintptr_t node;
        std::uint32_t binding_index;

        bool operator==(ClipInstanceKey const&) const = default;
    };

    struct ObservedClipClock
    {
        ClipInstanceKey key;
        float local_time;
    };

    struct ClipClockTrace
    {
        ClipInstanceKey key;
        std::vector<std::optional<std::uint32_t>> time_bits;
    };

    [[nodiscard]] inline std::optional<double> clock_delta(ClipClockTrace const& trace, std::size_t point)
    {
        if (point == 0 || !trace.time_bits[point] || !trace.time_bits[point - 1]) return std::nullopt;
        return static_cast<double>(std::bit_cast<float>(*trace.time_bits[point])) -
            static_cast<double>(std::bit_cast<float>(*trace.time_bits[point - 1]));
    }

    class ClipClockHistory
    {
    public:
        explicit ClipClockHistory(std::uint32_t advance_limit) :
            m_point_limit(advance_limit + 1), m_points(0), m_overflow(false), m_traces() {}

        [[nodiscard]] bool record(std::span<ObservedClipClock const> clocks)
        {
            if (m_points == m_point_limit) { m_overflow = true; return false; }
            for (ClipClockTrace& trace : m_traces) trace.time_bits.push_back(std::nullopt);
            for (ObservedClipClock const& clock : clocks)
            {
                std::vector<ClipClockTrace>::iterator found = std::find_if(m_traces.begin(), m_traces.end(),
                    [&](ClipClockTrace const& trace) { return trace.key == clock.key; });
                if (found == m_traces.end())
                {
                    if (m_traces.size() == Observation_Trace_Limit) { m_overflow = true; continue; }
                    m_traces.push_back({ clock.key, std::vector<std::optional<std::uint32_t>>(m_points + 1) });
                    found = m_traces.end() - 1;
                }
                found->time_bits.back() = std::bit_cast<std::uint32_t>(clock.local_time);
            }
            ++m_points;
            return !m_overflow;
        }

        [[nodiscard]] std::vector<ClipClockTrace> const& traces() const { return m_traces; }
        [[nodiscard]] std::uint32_t points() const { return m_points; }
        [[nodiscard]] bool overflow() const { return m_overflow; }

    private:
        std::uint32_t m_point_limit;
        std::uint32_t m_points;
        bool m_overflow;
        std::vector<ClipClockTrace> m_traces;
    };
}
PLUGIN_NAMESPACE_END
