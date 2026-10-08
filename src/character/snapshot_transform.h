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

// Write one bone's matrix into a skin's matrix buffer, which is what the shader samples. MEASURED:
// the buffer holds the bone's RAW WORLD matrix, not a combined skinning matrix. Classifying its
// clone-time contents against three candidates gave vs-world-row0 = 1.45 against 507.73 for the
// combined form and 538.08 for the bind pose's inverse, so composing rootParentToSkin and
// skinToBone into it replaces the correct data with something the shader cannot use — which is
// exactly why rebuilding the buffer never moved the mesh.
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
