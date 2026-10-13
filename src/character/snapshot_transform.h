

#pragma once

#include "RE/N/NiAVObject.h"
#include "RE/N/NiNode.h"
#include "RE/N/NiTransform.h"

#include <algorithm>
#include <cmath>

PLUGIN_NAMESPACE_BEGIN

inline RE::NiTransform snapshot_delta(RE::NiTransform const& captured_root, RE::NiTransform placement,
    RE::NiPoint3 const& captured_center, RE::NiPoint3 const& anchor)
{
    RE::NiTransform delta = placement * captured_root.Invert();
    placement.translate += anchor - delta * captured_center;
    return placement * captured_root.Invert();
}

inline RE::NiTransform swing_delta_about_pivot(RE::NiMatrix3 const& delta_rotation, RE::NiPoint3 const& pivot)
{
    RE::NiTransform delta;
    delta.translate = pivot - delta_rotation * pivot;
    delta.rotate = delta_rotation;
    delta.scale = 1.0f;
    return delta;
}

inline float rotation_angle_degrees(RE::NiMatrix3 const& before, RE::NiMatrix3 const& after)
{
    const RE::NiMatrix3 relative = before.Transpose() * after;
    const float trace = relative.entry[0][0] + relative.entry[1][1] + relative.entry[2][2];
    return std::acos(std::clamp((trace - 1.0f) * 0.5f, -1.0f, 1.0f)) * 57.29578f;
}

inline void apply_world_delta_downward(RE::NiAVObject* object, RE::NiTransform const& delta, uint32_t depth)
{
    if (!object || depth > 64)
        return;
    const RE::NiTransform world = delta * object->world;
    object->world = world;
    object->previousWorld = world;
    RE::NiNode* node = object->AsNode();
    if (!node)
        return;
    for (const RE::NiPointer<RE::NiAVObject>& child : node->children)
        if (child)
            apply_world_delta_downward(child.get(), delta, depth + 1);
}

inline void recompute_subtree_worlds(RE::NiAVObject& object, uint32_t depth)
{
    if (depth > 64)
        return;
    RE::NiNode* const node = object.AsNode();
    if (!node)
        return;
    for (const RE::NiPointer<RE::NiAVObject>& child : node->children)
    {
        if (!child)
            continue;
        const RE::NiTransform world = object.world * child->local;
        child->world = world;
        child->previousWorld = world;
        recompute_subtree_worlds(*child, depth + 1);
    }
}

inline void rotate_about_own_origin(RE::NiAVObject* node, int axis, float angle_radians)
{
    if (!node)
        return;
    RE::NiMatrix3 rotation;
    if (axis == 0)
        rotation.MakeXRotation(angle_radians);
    else if (axis == 1)
        rotation.MakeYRotation(angle_radians);
    else
        rotation.MakeZRotation(angle_radians);
    apply_world_delta_downward(node, swing_delta_about_pivot(rotation, node->world.translate), 0);
}

inline void translate_subtree(RE::NiAVObject* node, int axis, float distance)
{
    if (!node)
        return;
    RE::NiTransform delta;
    delta.scale = 1.0f;
    if (axis == 0)
        delta.translate.x = distance;
    else if (axis == 1)
        delta.translate.y = distance;
    else
        delta.translate.z = distance;
    apply_world_delta_downward(node, delta, 0);
}

PLUGIN_NAMESPACE_END
