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

PLUGIN_NAMESPACE_END
