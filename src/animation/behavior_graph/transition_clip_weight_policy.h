//
// C-G5-TRANS v9 (2026-11-20): control weight-table summary and the three-point weight solve.
//
// v8 showed the captured pose differs from the control's sample by an amount that shrinks
// by a constant 2.8 per observation point - the fingerprint of a weight that ramps linearly
// in time, not of a post-process residual. The control keeps a per-track weight table
// (hkaAnimationControl::transformTrackWeights, +0x18, one byte per track), so the next
// question is what those weights actually are and what weight the three known poses imply.
// These helpers are the pure part of that reading.
//
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    inline constexpr std::size_t Clip_Weight_Head = 8;
    inline constexpr float Clip_Weight_Solve_Min_Denominator = 1.0e-3f;

    struct ClipWeightSummary
    {
        std::size_t  count{};
        std::uint8_t minimum{};
        std::uint8_t maximum{};
        std::uint32_t hash{};  // FNV-1a 32 over the raw bytes, order included
    };

    [[nodiscard]] inline ClipWeightSummary clip_weight_summary(
        std::span<std::uint8_t const> a_weights) noexcept
    {
        ClipWeightSummary out{};
        out.count = a_weights.size();
        out.minimum = 0xFF;
        std::uint32_t hash = 2166136261u;
        for (std::uint8_t const value : a_weights)
        {
            out.minimum = std::min(out.minimum, value);
            out.maximum = std::max(out.maximum, value);
            hash ^= value;
            hash *= 16777619u;
        }
        out.hash = a_weights.empty() ? 0u : hash;
        if (a_weights.empty())
            out.minimum = 0;
        return out;
    }

    // One component of w = (current - previous) / (sample - previous). A denominator that is
    // too small cannot separate the two ends, so the component is rejected instead of
    // producing an arbitrary ratio.
    [[nodiscard]] inline bool clip_weight_solve_sample(float a_previous, float a_current,
        float a_sample, float a_min_denominator, float& a_weight) noexcept
    {
        if (!std::isfinite(a_previous) || !std::isfinite(a_current) || !std::isfinite(a_sample))
            return false;
        float const denominator = a_sample - a_previous;
        if (std::fabs(denominator) <= a_min_denominator)
            return false;
        a_weight = (a_current - a_previous) / denominator;
        return std::isfinite(a_weight);
    }

    struct ClipWeightSolve
    {
        std::size_t solved{};
        float       minimum{};
        float       maximum{};
        float       median{};
    };

    // Reorders a_weights (nth_element) while summarising it.
    [[nodiscard]] inline ClipWeightSolve clip_weight_solve_summary(std::span<float> a_weights) noexcept
    {
        ClipWeightSolve out{};
        if (a_weights.empty())
            return out;
        out.solved = a_weights.size();
        out.minimum = a_weights.front();
        out.maximum = a_weights.front();
        for (std::size_t i = 1; i < a_weights.size(); ++i)
        {
            out.minimum = std::min(out.minimum, a_weights[i]);
            out.maximum = std::max(out.maximum, a_weights[i]);
        }
        std::size_t const middle = a_weights.size() / 2;
        std::nth_element(a_weights.begin(), a_weights.begin() + static_cast<std::ptrdiff_t>(middle),
            a_weights.end());
        out.median = a_weights[middle];
        return out;
    }
}
PLUGIN_NAMESPACE_END
