//
// Created by AmazingBuff on 2026/10/29.
//
#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    inline constexpr std::uint32_t Source_Timeline_Line_Budget = 800;
    inline constexpr std::chrono::seconds Source_Timeline_Window{ 20 };

    struct SourceTimelineIdentity
    {
        std::uintptr_t actor;
        std::uintptr_t graph;
        std::uintptr_t behavior;
        std::uintptr_t data;
        std::uintptr_t variable_set;
        std::uintptr_t words;
        std::size_t count;
    };

    enum class SourceTimelineIntent : std::uint8_t
    {
        e_none,
        e_arm,
        e_invalidate
    };

    // Callbacks publish intents only; invalidation wins until the sampler consumes it.
    class SourceTimelineRequests
    {
    public:
        SourceTimelineRequests() : m_pending(SourceTimelineIntent::e_none) {}

        void arm()
        {
            SourceTimelineIntent expected = SourceTimelineIntent::e_none;
            (void)m_pending.compare_exchange_strong(expected, SourceTimelineIntent::e_arm);
        }

        void invalidate() { m_pending.store(SourceTimelineIntent::e_invalidate); }
        [[nodiscard]] SourceTimelineIntent take() { return m_pending.exchange(SourceTimelineIntent::e_none); }

    private:
        std::atomic<SourceTimelineIntent> m_pending;
    };

    enum class SourceTimelineSample : std::uint8_t
    {
        e_baseline,
        e_compare,
        e_aborted
    };

    class SourceTimelineWindow
    {
    public:
        SourceTimelineWindow() : m_active(false), m_exhausted(false), m_lines(0), m_deadline{}, m_source{} {}

        void arm(std::chrono::steady_clock::time_point now)
        {
            m_active = true;
            m_exhausted = false;
            m_lines = 0;
            m_deadline = now + Source_Timeline_Window;
            m_source.reset();
        }

        void cancel() { m_active = false; }

        [[nodiscard]] bool expire(std::chrono::steady_clock::time_point now)
        {
            if (!m_active || now < m_deadline) return false;
            cancel();
            return true;
        }

        [[nodiscard]] SourceTimelineSample observe_source(SourceTimelineIdentity const& source)
        {
            if (!m_active) return SourceTimelineSample::e_aborted;
            if (!m_source)
            {
                m_source = source;
                return SourceTimelineSample::e_baseline;
            }
            SourceTimelineIdentity const& previous = *m_source;
            if (previous.actor != source.actor || previous.graph != source.graph ||
                previous.behavior != source.behavior || previous.data != source.data ||
                previous.variable_set != source.variable_set || previous.words != source.words ||
                previous.count != source.count)
            {
                cancel();
                return SourceTimelineSample::e_aborted;
            }
            return SourceTimelineSample::e_compare;
        }

        [[nodiscard]] bool take_line()
        {
            if (m_lines < Source_Timeline_Line_Budget)
            {
                ++m_lines;
                return true;
            }
            m_exhausted = true;
            return false;
        }

        [[nodiscard]] bool active() const { return m_active; }
        [[nodiscard]] bool exhausted() const { return m_exhausted; }
        [[nodiscard]] std::uint32_t lines() const { return m_lines; }

    private:
        bool m_active;
        bool m_exhausted;
        std::uint32_t m_lines;
        std::chrono::steady_clock::time_point m_deadline;
        std::optional<SourceTimelineIdentity> m_source;
    };
}
PLUGIN_NAMESPACE_END
