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

PLUGIN_NAMESPACE_END
