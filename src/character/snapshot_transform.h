//
// Created by AmazingBuff on 2026/10/08.
//

#pragma once

#include "RE/N/NiTransform.h"

PLUGIN_NAMESPACE_BEGIN

inline RE::NiTransform snapshot_delta(RE::NiTransform const& captured_root, RE::NiTransform placement,
    RE::NiPoint3 const& captured_center, RE::NiPoint3 const& anchor)
{
    RE::NiTransform delta = placement * captured_root.Invert();
    placement.translate += anchor - delta * captured_center;
    return placement * captured_root.Invert();
}

// Rotate a node's world transform about the fix point `pivot` by `delta_rotation`, leaving the
// pivot point itself where it was. Composition is right-to-left (A * B applies B first), so
// delta_rotation must be built in the space its axis is expressed in.
//
// Rotating about the PARENT's origin instead of the node's own moves the node as well as turning
// it, which is why the pivot is a parameter rather than derived here: that mistake made a swing
// collapse into a translation and left every in-game probe looking like "nothing moved".
inline RE::NiTransform swing_about_pivot(RE::NiTransform const& original, RE::NiMatrix3 const& delta_rotation, RE::NiPoint3 const& pivot)
{
    RE::NiTransform delta;
    delta.translate = pivot - delta_rotation * pivot;
    delta.rotate = delta_rotation;
    delta.scale = 1.0f;
    return delta * original;
}

// Write one bone's skinning matrix into a skin's matrix buffer. The shader samples THIS array,
// not the bone's node transform: the buffer was measured to stay frozen at its clone-time values
// while the node it belongs to was being swung, which is why rotating the copy's bones moved
// nothing on screen. Layout is a 3x4 row-major float matrix per slot, 48 bytes apart.
//
// The stored skinToBone maps a bind-pose vertex into the bone's own space, so it is INVERTED here
// to return to bind space: skinning * (skinToBone * v) = rootParentToSkin * boneWorld * v, which
// puts a bind-space origin exactly on rootParentToSkin * bone.translate.
inline void write_skinning_matrix(void* buffer, uint32_t slot, RE::NiTransform const& root_parent_to_skin, RE::NiTransform const& bone_world, RE::NiTransform const& skin_to_bone)
{
    const RE::NiTransform skinning = root_parent_to_skin * bone_world * skin_to_bone.Invert();
    float* const values = static_cast<float*>(buffer) + static_cast<size_t>(slot) * 12;
    values[0] = skinning.rotate.entry[0][0];
    values[1] = skinning.rotate.entry[0][1];
    values[2] = skinning.rotate.entry[0][2];
    values[3] = skinning.translate.x;
    values[4] = skinning.rotate.entry[1][0];
    values[5] = skinning.rotate.entry[1][1];
    values[6] = skinning.rotate.entry[1][2];
    values[7] = skinning.translate.y;
    values[8] = skinning.rotate.entry[2][0];
    values[9] = skinning.rotate.entry[2][1];
    values[10] = skinning.rotate.entry[2][2];
    values[11] = skinning.translate.z;
}

PLUGIN_NAMESPACE_END
