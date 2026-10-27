//
// Created by AmazingBuff on 2026/10/27.
//
#pragma once

#include "pose_content_policy.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // v23 semantic-correspondence check (2026-10-27).
    //
    // v22 established that the candidate's poseLocal is finite, changing and not aliased
    // to the source, but not that the values mean anything: any generator writing plausible
    // numbers would pass that gate. This policy owns the pure part of the follow-up - is
    // poseLocal the sample of an active clip at that clip's own time? - so the arithmetic can
    // be verified engine-free before a game run.
    //
    // A rotation is compared by |dot| of the two unit quaternions: q and -q describe the same
    // rotation, so a component-wise comparison would report a false mismatch. Translation and
    // scale are compared per component. A non-finite operand yields infinity, never "equal".
    inline constexpr float kClipPositionEpsilon = 1.0e-4f;
    inline constexpr float kClipRotationEpsilon = 1.0e-5f;
    inline constexpr float kClipScaleEpsilon = 1.0e-4f;

    [[nodiscard]] inline float pose_component_delta(std::array<float, 3> const& lhs, std::array<float, 3> const& rhs)
    {
        float maximum = 0.0f;
        for (std::size_t i = 0; i < lhs.size(); ++i)
            maximum = std::max(maximum, pose_absolute_delta(lhs[i], rhs[i]));
        return maximum;
    }

    [[nodiscard]] inline float pose_rotation_agreement_error(std::array<float, 4> const& lhs, std::array<float, 4> const& rhs)
    {
        double left_length = 0.0;
        double right_length = 0.0;
        for (std::size_t i = 0; i < lhs.size(); ++i)
        {
            if (!std::isfinite(lhs[i]) || !std::isfinite(rhs[i]))
                return std::numeric_limits<float>::infinity();
            left_length += static_cast<double>(lhs[i]) * static_cast<double>(lhs[i]);
            right_length += static_cast<double>(rhs[i]) * static_cast<double>(rhs[i]);
        }
        if (left_length <= 0.0 || right_length <= 0.0)
            return std::numeric_limits<float>::infinity();
        double dot = 0.0;
        for (std::size_t i = 0; i < lhs.size(); ++i)
            dot += static_cast<double>(lhs[i]) * static_cast<double>(rhs[i]);
        const double magnitude = std::sqrt(left_length) * std::sqrt(right_length);
        const double cosine = std::min(1.0, std::abs(dot) / magnitude);
        return static_cast<float>(1.0 - cosine);
    }

    struct ClipReferenceMatch
    {
        std::size_t compared{};      // mapped tracks compared against poseLocal
        std::size_t agreed{};        // entries inside every epsilon
        std::size_t unmapped{};      // tracks whose bone index is absent from poseLocal
        float       max_position{};  // largest translation component delta seen
        float       max_rotation{};  // largest 1-|dot| seen
        float       max_scale{};     // largest scale component delta seen
    };

    // Returns true when this entry agrees; the aggregates are updated either way, so a
    // disagreement is visible in the maximums instead of only in the count. The return is
    // not [[nodiscard]] because accumulating is the primary purpose here.
    inline bool clip_reference_agrees(ClipReferenceMatch& match, PoseSample const& reference,
        PoseSample const& actual)
    {
        const float position = pose_component_delta(reference.translation, actual.translation);
        const float rotation = pose_rotation_agreement_error(reference.rotation, actual.rotation);
        const float scale = pose_component_delta(reference.scale, actual.scale);
        match.max_position = std::max(match.max_position, position);
        match.max_rotation = std::max(match.max_rotation, rotation);
        match.max_scale = std::max(match.max_scale, scale);
        ++match.compared;
        const bool agrees = position <= kClipPositionEpsilon && rotation <= kClipRotationEpsilon &&
            scale <= kClipScaleEpsilon;
        if (agrees)
            ++match.agreed;
        return agrees;
    }
}
PLUGIN_NAMESPACE_END
