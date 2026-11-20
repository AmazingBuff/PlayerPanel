//
// C-G5-TRANS v10 (2026-11-20): swept-time candidate policy.
//
// v9 showed the captured pose is piecewise constant on the probe timeline and steps once
// per command, while the weight table is constant and both recorded clock fields are frozen.
// Two explanations remain: the graph samples an advancing time while the read fields are
// stale (S2), or the graph writes the pose incrementally (S1). Sweeping the sample time and
// asking which candidate best matches the captured pose separates them: an advancing best-k
// is S2, a constant best-k is S1.
//
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // v10 used a radius of 4 frames (9 candidates); v11 widens it to 16 (33 candidates, about
    // 0.267 s) because the first run's minima sat on the +-4 boundary. The step stays at 1/60 s.
    inline constexpr std::int32_t Clip_Sweep_Radius = 16;
    inline constexpr float Clip_Sweep_Step = 1.0f / 60.0f;

    struct ClipSweepTime
    {
        float time{};
        bool  clamped{};
    };

    [[nodiscard]] inline ClipSweepTime clip_sweep_time(float a_local_time, std::int32_t a_offset,
        float a_duration) noexcept
    {
        ClipSweepTime out{};
        out.time = a_local_time + static_cast<float>(a_offset) * Clip_Sweep_Step;
        if (!std::isfinite(out.time))
        {
            out.time = 0.0f;
            out.clamped = true;
            return out;
        }
        if (out.time < 0.0f)
        {
            out.time = 0.0f;
            out.clamped = true;
        }
        else if (std::isfinite(a_duration) && a_duration > 0.0f && out.time > a_duration)
        {
            out.time = a_duration;
            out.clamped = true;
        }
        return out;
    }

    // Index of the smallest entry; ties resolve to the candidate closest to the centre so a
    // flat or noisy curve does not report an arbitrary edge.
    [[nodiscard]] inline std::size_t clip_sweep_best(std::span<float const> a_scores) noexcept
    {
        if (a_scores.empty())
            return 0;
        std::size_t const centre = a_scores.size() / 2;
        auto const distance = [centre](std::size_t a_index) {
            return a_index > centre ? a_index - centre : centre - a_index;
        };
        std::size_t best = centre;
        for (std::size_t i = 0; i < a_scores.size(); ++i)
        {
            bool const lower = a_scores[i] < a_scores[best];
            bool const closer = a_scores[i] == a_scores[best] && distance(i) < distance(best);
            if (lower || closer)
                best = i;
        }
        return best;
    }
}
PLUGIN_NAMESPACE_END
