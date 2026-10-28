//
// Created by AmazingBuff on 2026/10/28.
//
#pragma once

#include "animation/behavior_graph/clip_reference_policy.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // Unit-quaternion chord distance min(|q-r|,|q+r|), at the raw channel's 1e-6 sensitivity.
    inline constexpr float Display_Motion_Epsilon = 1.0e-6F;

    struct PoseMotionComparison
    {
        std::size_t compared;
        std::size_t moved;
        bool valid;
        float max_translation;
        float max_rotation_chord;
        float max_scale;
    };

    [[nodiscard]] inline PoseMotionComparison compare_display_motion(std::vector<PoseSample> const& current,
        std::vector<PoseSample> const& previous)
    {
        PoseMotionComparison result{ 0, 0, !current.empty() && current.size() == previous.size(), 0.0F, 0.0F, 0.0F };
        std::size_t const count = std::min(current.size(), previous.size());
        for (std::size_t i = 0; i < count; ++i)
        {
            float const translation = pose_component_delta(current[i].translation, previous[i].translation);
            float const rotation = std::sqrt(2.0F * pose_rotation_agreement_error(current[i].rotation, previous[i].rotation));
            // ni_local_from_pose uses only scale X as the display's uniform scale.
            float const scale = pose_absolute_delta(current[i].scale[0], previous[i].scale[0]);
            bool const valid = std::isfinite(translation) && std::isfinite(rotation) && std::isfinite(scale);
            result.valid = result.valid && valid;
            ++result.compared;
            result.max_translation = std::max(result.max_translation, translation);
            result.max_rotation_chord = std::max(result.max_rotation_chord, rotation);
            result.max_scale = std::max(result.max_scale, scale);
            if (valid && (translation > Display_Motion_Epsilon || rotation > Display_Motion_Epsilon || scale > Display_Motion_Epsilon))
                ++result.moved;
        }
        return result;
    }
}
PLUGIN_NAMESPACE_END
