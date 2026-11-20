//
// C-G5-TRANS v8 (2026-11-20): capture-point and binding-map policy.
//
// v7 established that the clip's own clock is frozen while the captured pose keeps
// changing, with weight and track weights constant. The remaining question is whether
// the captured buffer is what the clip's control samples. These helpers are the pure part
// of that comparison, so the mapping and the shift/residual arithmetic can be verified
// engine-free before a game run.
//
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    inline constexpr std::size_t Clip_Capture_Track_Limit = 256;
    inline constexpr std::size_t Clip_Capture_Map_Head = 8;
    inline constexpr std::size_t Clip_Capture_Mismatch_Head = 8;

    // Engine rule (0xB03380 ConvertTrackPoseToBonePose): an empty map means identity.
    // A negative or out-of-range bone is reported as -1 and never dereferenced.
    [[nodiscard]] constexpr std::int32_t clip_capture_bone(std::int32_t a_track, bool a_identity,
        std::span<std::int16_t const> a_map, std::int32_t a_pose_count) noexcept
    {
        if (a_track < 0 || a_pose_count <= 0)
            return -1;
        std::int32_t bone = a_track;
        if (!a_identity)
        {
            if (static_cast<std::size_t>(a_track) >= a_map.size())
                return -1;
            bone = a_map[static_cast<std::size_t>(a_track)];
        }
        if (bone < 0 || bone >= a_pose_count)
            return -1;
        return bone;
    }

    // Translation deltas (pose - sample) summarised two ways: the mean shift is what a
    // common (rigid) offset would look like, and the residual is what is left after that
    // offset is removed. A small residual means "same content, different frame" while a
    // large one means the two buffers hold different poses.
    struct ClipCaptureShift
    {
        std::array<float, 3> mean{};
        float                residual_max{};
        std::size_t          moved{};
        std::size_t          compared{};
    };

    [[nodiscard]] inline ClipCaptureShift clip_capture_shift(
        std::span<std::array<float, 3> const> a_deltas) noexcept
    {
        ClipCaptureShift out{};
        if (a_deltas.empty())
            return out;
        std::array<double, 3> sum{};
        std::size_t finite{};
        for (auto const& delta : a_deltas)
        {
            bool const ok = std::isfinite(delta[0]) && std::isfinite(delta[1]) && std::isfinite(delta[2]);
            if (!ok)
                continue;
            for (std::size_t axis = 0; axis < 3; ++axis)
                sum[axis] += static_cast<double>(delta[axis]);
            ++finite;
        }
        if (finite == 0)
            return out;
        for (std::size_t axis = 0; axis < 3; ++axis)
            out.mean[axis] = static_cast<float>(sum[axis] / static_cast<double>(finite));
        for (auto const& delta : a_deltas)
        {
            if (!std::isfinite(delta[0]) || !std::isfinite(delta[1]) || !std::isfinite(delta[2]))
                continue;
            double const dx = static_cast<double>(delta[0]) - out.mean[0];
            double const dy = static_cast<double>(delta[1]) - out.mean[1];
            double const dz = static_cast<double>(delta[2]) - out.mean[2];
            float const residual = static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));
            out.residual_max = std::max(out.residual_max, residual);
            ++out.compared;
            double const length = static_cast<double>(delta[0]) * delta[0] +
                static_cast<double>(delta[1]) * delta[1] +
                static_cast<double>(delta[2]) * delta[2];
            if (length > 1.0e-8)
                ++out.moved;
        }
        return out;
    }
}
PLUGIN_NAMESPACE_END
