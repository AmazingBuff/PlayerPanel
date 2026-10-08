//
// Created by AmazingBuff on 2026/10/08.
//

#include "character/snapshot_transform.h"

#include <cmath>
#include <cstdio>

int main()
{
    RE::NiTransform captured_root;
    captured_root.translate = { 12000.0f, -5000.0f, 800.0f };
    captured_root.rotate.SetEulerAnglesXYZ(0.0f, 0.0f, 1.57079632679f);
    captured_root.scale = 2.0f;
    const RE::NiPoint3 center = captured_root * RE::NiPoint3{ 4.0f, 8.0f, 10.0f };
    const RE::NiPoint3 bone_a = captured_root * RE::NiPoint3{ 5.0f, 8.0f, 10.0f };
    const RE::NiPoint3 bone_b = captured_root * RE::NiPoint3{ 7.0f, 8.0f, 10.0f };
    const RE::NiPoint3 anchor{ 0.0f, -485.0f, 0.0f };
    for (uint32_t quarter_turn = 0; quarter_turn < 4; ++quarter_turn)
    {
        RE::NiTransform placement;
        placement.translate = anchor;
        placement.rotate.SetEulerAnglesXYZ(0.0f, 0.0f, static_cast<float>(quarter_turn) * 1.57079632679f);
        placement.scale = 0.35f;
        const RE::NiTransform delta = PLUGIN_NAMESPACE::snapshot_delta(captured_root, placement, center, anchor);
        if ((delta * center - anchor).Length() > 0.002f)
        {
            std::puts("FAIL: captured center does not land on the studio anchor");
            return 1;
        }
        if (std::abs((delta * bone_b - delta * bone_a).Length() - 0.7f) > 0.002f)
        {
            std::puts("FAIL: relative bone pose or display scale was not preserved");
            return 1;
        }
    }
    std::puts("PASS: studio centering and relative bone pose survive source translation/rotation/scale and four display rotations");
    return 0;
}
