//
// Created by AmazingBuff on 2026/11/11.
//
#pragma once

#include "animation/behavior_graph/pose_content_policy.h"

#include <cstdint>
#include <cstring>
#include <format>
#include <string>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    template <class Transform>
    [[nodiscard]] PoseSample capture_pose_sample(Transform const& entry)
    {
        float translation[4]{};
        float rotation[4]{};
        float scale[4]{};
        std::memcpy(translation, &entry.translation, sizeof(translation));
        std::memcpy(rotation, &entry.rotation, sizeof(rotation));
        std::memcpy(scale, &entry.scale, sizeof(scale));
        return PoseSample{
            { translation[0], translation[1], translation[2] },
            { rotation[0], rotation[1], rotation[2], rotation[3] },
            { scale[0], scale[1], scale[2] }
        };
    }

    [[nodiscard]] inline std::string format_source_pose_track(std::int32_t const track, PoseSample const& sample)
    {
        return std::format("SOURCE TIMELINE pose-snapshot track={} trans={:.6f},{:.6f},{:.6f} rot={:.6f},{:.6f},{:.6f},{:.6f} scale={:.6f},{:.6f},{:.6f}",
            track, sample.translation[0], sample.translation[1], sample.translation[2],
            sample.rotation[0], sample.rotation[1], sample.rotation[2], sample.rotation[3],
            sample.scale[0], sample.scale[1], sample.scale[2]);
    }
}
PLUGIN_NAMESPACE_END
