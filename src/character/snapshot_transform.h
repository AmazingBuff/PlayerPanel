

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

PLUGIN_NAMESPACE_END
