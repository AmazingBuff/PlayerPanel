//
// Created by AmazingBuff on 2026/11/27.
//
#pragma once

PLUGIN_NAMESPACE_BEGIN
namespace animation
{
    template <class Transform>
    void write_animated_pose(Transform& target, Transform const& pose)
    {
        // The original scene-pose writer retains the destination's scale.
        target.rotate = pose.rotate;
        target.translate = pose.translate;
    }
}
PLUGIN_NAMESPACE_END
