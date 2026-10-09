//
// Created by AmazingBuff on 2026/10/08.
//

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

// The world-space delta that swings a subtree about `pivot`: `delta_rotation` applied about the
// pivot, which itself stays where it was. Composing it onto a node's world (delta * world) turns
// the node and carries its children through the matching arc.
//
// Rotating about the PARENT's origin instead of the node's own moves the node as well as turning
// it, which is why the pivot is a parameter rather than derived here: that mistake made a swing
// collapse into a translation and left every in-game probe looking like "nothing moved".
inline RE::NiTransform swing_delta_about_pivot(RE::NiMatrix3 const& delta_rotation, RE::NiPoint3 const& pivot)
{
    RE::NiTransform delta;
    delta.translate = pivot - delta_rotation * pivot;
    delta.rotate = delta_rotation;
    delta.scale = 1.0f;
    return delta;
}

// The same swing composed onto a node that already carries its own world transform. Composition
// is right-to-left (A * B applies B first), so delta_rotation must be built in the space its axis
// is expressed in.
inline RE::NiTransform swing_about_pivot(RE::NiTransform const& original, RE::NiMatrix3 const& delta_rotation, RE::NiPoint3 const& pivot)
{
    return swing_delta_about_pivot(delta_rotation, pivot) * original;
}

// A witness point's perpendicular distance to the world-X axis through `pivot`. A point ON that
// axis cannot move under an X swing whatever its angle, so its displacement says nothing about
// whether the write reached the drawn tree; the probe picks the descendant with the largest
// radius instead (test sheet 2026-10-09, R03).
inline float swing_radius_about_x(RE::NiPoint3 const& point, RE::NiPoint3 const& pivot)
{
    const float dy = point.y - pivot.y;
    const float dz = point.z - pivot.z;
    return std::sqrt(dy * dy + dz * dz);
}

// The angle between two orientations, from the trace of the relative rotation: acos((tr - 1) / 2).
// A single row cannot stand in for this: a swing about world X leaves the X basis vector — row 0 —
// untouched, which is how one probe build reported an orientation change of 0 for a swing it had
// demonstrably applied.
inline float rotation_angle_degrees(RE::NiMatrix3 const& before, RE::NiMatrix3 const& after)
{
    const RE::NiMatrix3 relative = before.Transpose() * after;
    const float trace = relative.entry[0][0] + relative.entry[1][1] + relative.entry[2][2];
    return std::acos(std::clamp((trace - 1.0f) * 0.5f, -1.0f, 1.0f)) * 57.29578f;
}

// L1 distance between a stored 3x4 slot (row-major floats, translation at 3, 7 and 11) and a
// candidate orientation, over the nine rotation components. Comparing the whole block rather than
// one row keeps the reading meaningful whichever axis a swing uses. The two sides need different
// strides: the slot interleaves a translation component after each row, the candidate matrix is
// packed.
inline float rotation_distance(float const* slot_values, RE::NiMatrix3 const& rotate)
{
    constexpr int Rotation_Indices[9] = { 0, 1, 2, 4, 5, 6, 8, 9, 10 };
    float const* const entries = &rotate.entry[0][0];
    float distance = 0.0f;
    for (int component = 0; component < 9; ++component)
        distance += std::abs(slot_values[Rotation_Indices[component]] - entries[component]);
    return distance;
}

// Compose a world-space delta onto a subtree by recomputing each node's world transform, with no
// dependency on the engine's dirty-update cascade: the census proved that cascade does not run
// inside the studio's draw window (a rotated bone left its child's world untouched).
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

// Recompute a subtree's world transforms from its locals, top down, starting below `object`.
// A single-node change uses apply_world_delta_downward; a pass that writes many nodes' locals at
// once has no single delta, and recomputing downward is order-free (the skeleton's parent indices
// are not guaranteed to come before their children).
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

// Rotate a node and its descendants about the node's own origin by `angle` radians around the given
// world axis. Parents must be driven before their children: each call pivots on the node's CURRENT
// world position, so a child inherits whatever its parent has already been given.
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

// Shift a node and its descendants along a world axis, keeping every orientation. A rotation about a
// joint's own origin cannot move that joint, so a weight shift needs this: the hips have to travel
// sideways, not just turn.
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

// Write one bone's matrix into a skin's matrix buffer. NOT in use: the first probe round changes
// node transforms only, and the read-back comparisons that "measured" this buffer as the bone's
// raw world matrix were retracted as circular (they compared the slot against the value the same
// code had just written into it). Kept for the evidence-gated step: write only after the layout,
// the coordinate space and the upload path are established, and keep the replaced values so the
// write stays recoverable. Test coverage: the 48-byte stride and a round trip.
//
// Layout is a 3x4 row-major float matrix per slot, 48 bytes apart, with the translation in the
// w components (indices 3, 7, 11).
inline void write_bone_matrix(void* buffer, uint32_t slot, RE::NiTransform const& bone_world)
{
    float* const values = static_cast<float*>(buffer) + static_cast<size_t>(slot) * 12;
    values[0] = bone_world.rotate.entry[0][0];
    values[1] = bone_world.rotate.entry[0][1];
    values[2] = bone_world.rotate.entry[0][2];
    values[3] = bone_world.translate.x;
    values[4] = bone_world.rotate.entry[1][0];
    values[5] = bone_world.rotate.entry[1][1];
    values[6] = bone_world.rotate.entry[1][2];
    values[7] = bone_world.translate.y;
    values[8] = bone_world.rotate.entry[2][0];
    values[9] = bone_world.rotate.entry[2][1];
    values[10] = bone_world.rotate.entry[2][2];
    values[11] = bone_world.translate.z;
}

PLUGIN_NAMESPACE_END
