//
// Created by AmazingBuff on 2026/10/27.
//
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // v21 pose-content verification (2026-10-27, design only until it runs).
    //
    // The v20a run moved the candidate's raw pose buffer for the first time, but a
    // raw FNV over `poseLocal` is padding-sensitive: it proves bytes changed, not
    // that any bone carries a finite, changing transform. This policy owns the pure
    // part of the follow-up check - per-entry finiteness, component-wise delta,
    // "moved since the previous checkpoint" and candidate/source identity counts - so
    // the arithmetic can be verified engine-free before any game run.
    //
    // Only the meaningful components are compared: translation xyz, rotation xyzw and
    // scale xyz. The fourth lane of each hkVector4 is ignored because it is padding in
    // this data, not a transform component. `kPoseDeltaEpsilon` is the pre-registered
    // "changed" threshold for a single component.
    struct PoseSample
    {
        std::array<float, 3> translation{};
        std::array<float, 4> rotation{};
        std::array<float, 3> scale{};
    };

    // 116 was observed for both the source and the candidate; the cap only bounds a
    // malformed count so a bad read cannot walk past the declared buffer.
    inline constexpr std::size_t kMaxPoseEntries = 256;
    inline constexpr float kPoseDeltaEpsilon = 1.0e-6f;

    [[nodiscard]] inline float pose_absolute_delta(float const lhs, float const rhs)
    {
        // A non-finite operand yields infinity, so a NaN can never be reported as
        // "unchanged" and poisons max_delta instead of hiding.
        const float delta = lhs > rhs ? lhs - rhs : rhs - lhs;
        return std::isfinite(delta) ? delta : std::numeric_limits<float>::infinity();
    }

    [[nodiscard]] inline bool pose_sample_finite(PoseSample const& sample)
    {
        for (float const value : sample.translation)
            if (!std::isfinite(value)) return false;
        for (float const value : sample.rotation)
            if (!std::isfinite(value)) return false;
        for (float const value : sample.scale)
            if (!std::isfinite(value)) return false;
        return true;
    }

    [[nodiscard]] inline float pose_sample_max_delta(PoseSample const& lhs, PoseSample const& rhs)
    {
        float maximum = 0.0f;
        const auto fold = [&maximum](auto const& a, auto const& b) {
            for (std::size_t i = 0; i < a.size(); ++i)
                maximum = std::max(maximum, pose_absolute_delta(a[i], b[i]));
        };
        fold(lhs.translation, rhs.translation);
        fold(lhs.rotation, rhs.rotation);
        fold(lhs.scale, rhs.scale);
        return maximum;
    }

    [[nodiscard]] inline bool pose_sample_differs(PoseSample const& lhs, PoseSample const& rhs,
        float const epsilon = kPoseDeltaEpsilon)
    {
        return pose_sample_max_delta(lhs, rhs) > epsilon;
    }

    [[nodiscard]] inline bool pose_sample_identical(PoseSample const& lhs, PoseSample const& rhs)
    {
        return pose_sample_max_delta(lhs, rhs) == 0.0f;
    }

    struct PoseComparison
    {
        std::size_t entries{};      // entries read at this checkpoint
        std::size_t finite{};       // entries whose ten meaningful components are finite
        std::size_t non_finite{};   // entries with at least one NaN/inf
        std::size_t moved{};        // entries differing from the previous checkpoint
        float       max_delta{};    // largest single-component change seen
        bool        has_previous{}; // false at the first checkpoint: moved/max_delta unmeaningful
    };

    [[nodiscard]] inline PoseComparison compare_pose(std::vector<PoseSample> const& current,
        std::vector<PoseSample> const& previous, float const epsilon = kPoseDeltaEpsilon)
    {
        PoseComparison result;
        result.entries = current.size();
        result.has_previous = !previous.empty();
        for (std::size_t i = 0; i < current.size(); ++i)
        {
            if (pose_sample_finite(current[i]))
                ++result.finite;
            else
                ++result.non_finite;
            if (!result.has_previous || i >= previous.size())
                continue;
            const float delta = pose_sample_max_delta(current[i], previous[i]);
            result.max_delta = std::max(result.max_delta, delta);
            if (delta > epsilon)
                ++result.moved;
        }
        return result;
    }

    // Candidate/source aliasing watch: identical entries at the same index mean the two
    // graphs are reading the same buffer, which would make every other comparison moot.
    [[nodiscard]] inline std::size_t count_identical(std::vector<PoseSample> const& lhs,
        std::vector<PoseSample> const& rhs)
    {
        const std::size_t limit = std::min(lhs.size(), rhs.size());
        std::size_t identical = 0;
        for (std::size_t i = 0; i < limit; ++i)
            if (pose_sample_identical(lhs[i], rhs[i]))
                ++identical;
        return identical;
    }
}
PLUGIN_NAMESPACE_END
