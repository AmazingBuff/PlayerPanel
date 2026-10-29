//
// Created by AmazingBuff on 2026/10/29.
//
#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    inline constexpr std::uint32_t Display_Step_Rate = 60;
    inline constexpr std::uint32_t Display_Max_Steps = 6;
    inline constexpr std::chrono::milliseconds Display_Max_Delta{ 100 };

    class DisplayStepClock
    {
    public:
        DisplayStepClock() : m_previous{}, m_scaled_time{} {}

        void reset()
        {
            m_previous.reset();
            m_scaled_time = {};
        }

        [[nodiscard]] std::uint32_t advance(std::chrono::steady_clock::time_point now)
        {
            if (!m_previous)
            {
                m_previous = now;
                return 0;
            }
            std::chrono::steady_clock::duration const delta = std::min(now - *m_previous,
                std::chrono::duration_cast<std::chrono::steady_clock::duration>(Display_Max_Delta));
            m_previous = now;
            m_scaled_time += delta * Display_Step_Rate;
            std::uint32_t const steps = std::min(Display_Max_Steps,
                static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(m_scaled_time).count()));
            m_scaled_time -= std::chrono::seconds(steps);
            return steps;
        }

    private:
        std::optional<std::chrono::steady_clock::time_point> m_previous;
        // Scale elapsed ticks by 60 so fractional simulation steps retain clock precision.
        std::chrono::steady_clock::duration m_scaled_time;
    };
}
PLUGIN_NAMESPACE_END
