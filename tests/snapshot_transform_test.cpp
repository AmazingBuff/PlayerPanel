//
// Created by AmazingBuff on 2026/10/08.
//

#include "character/snapshot_transform.h"

#include <cmath>
#include <cstdio>

namespace
{
    bool close(float lhs, float rhs, float tolerance)
    {
        return std::abs(lhs - rhs) <= tolerance;
    }

    RE::NiTransform make_transform(const RE::NiPoint3& translate, float heading_z, float scale)
    {
        RE::NiTransform transform;
        transform.translate = translate;
        transform.rotate.SetEulerAnglesXYZ(0.0f, 0.0f, heading_z);
        transform.scale = scale;
        return transform;
    }
}

int main()
{
    int failures = 0;
    const auto check = [&failures](bool condition, const char* message)
    {
        if (!condition)
        {
            std::printf("FAIL: %s\n", message);
            ++failures;
        }
    };

    // Studio centering: the captured centre lands on the anchor and the relative bone pose
    // survives four display rotations (the original coverage of snapshot_delta).
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
            check((delta * center - anchor).Length() <= 0.002f, "captured centre does not land on the studio anchor");
            check(close((delta * bone_b - delta * bone_a).Length(), 0.7f, 0.002f), "relative bone pose or display scale was not preserved");
        }
    }

    // A bone hung under a rotated, offset parent, and a joint further down that must follow the
    // swing through the cascade. A vector offset from the bone's origin is what carries the
    // rotation: transforming the origin itself is a no-op by construction and proves nothing.
    const RE::NiPoint3 parent_translate{ 1234.0f, -485.7f, 2.4f };
    const RE::NiTransform parent_world = make_transform(parent_translate, 0.7f, 1.0f);
    const RE::NiTransform bone_local = make_transform({ 0.0f, 12.0f, 0.0f }, 0.35f, 1.0f);
    const RE::NiTransform bone_world = parent_world * bone_local;
    const RE::NiPoint3 pivot = bone_world.translate;

    const RE::NiTransform child_world = bone_world * make_transform({ 0.0f, 5.0f, 0.0f }, 0.0f, 1.0f);
    const float child_radius = (child_world.translate - pivot).Length();

    for (const float angle : { 0.0f, 0.25f, 0.6f, 1.0f, -1.0f })
    {
        RE::NiMatrix3 delta_rotation;
        delta_rotation.MakeZRotation(angle);
        const RE::NiTransform swung = PLUGIN_NAMESPACE::swing_about_pivot(bone_world, delta_rotation, pivot);

        // 1. The pivot stays exactly where it was. This is the assertion that catches using the
        //    PARENT's origin as the pivot, which moved the bone instead of turning it.
        const RE::NiPoint3 origin_local{ 0.0f, 0.0f, 0.0f };
        const RE::NiPoint3 swung_origin_world = swung * (bone_world.Invert() * pivot);
        check((swung_origin_world - pivot).Length() < 0.01f, "the bone origin must not move when swinging about it");

        // 2. The orientation gains exactly the requested rotation: composing the same point
        //    through the swung and the original transform must give the same result as rotating
        //    the offset directly by the delta matrix. This is a delta comparison, so it does not
        //    depend on the bone's heading being a pure Z rotation.
        const RE::NiPoint3 offset_local{ 0.0f, 7.0f, 0.0f };
        const RE::NiPoint3 moved = swung * offset_local;
        const RE::NiPoint3 expected = pivot + delta_rotation * (bone_world * offset_local - pivot);
        check((moved - expected).Length() < 0.01f, "the swing must apply exactly the given delta rotation about the pivot");

        // 3. The joint further down swings through the arc of the same angle. This is the
        //    quantity the in-game probe tried to measure and mis-measured.
        const RE::NiTransform swung_child = swung * bone_world.Invert() * child_world;
        const float expected_arc = 2.0f * child_radius * std::sin(std::abs(angle) * 0.5f);
        check(close((swung_child.translate - child_world.translate).Length(), expected_arc, 0.02f), "a child joint must swing through the arc of the same angle");

        // 4. Zero rotation is the identity, the cheapest sentinel for a sign or order mistake.
        if (angle == 0.0f)
            check((swung.translate - bone_world.translate).Length() < 0.001f && close(swung.rotate.entry[0][0], bone_world.rotate.entry[0][0], 0.001f),
                "a zero swing must leave the transform unchanged");
    }

    // 5. Regression lock for the mistake that cost several in-game rounds: pivoting about the
    //    PARENT's origin drags the bone off its own origin.
    RE::NiMatrix3 quarter_turn;
    quarter_turn.MakeZRotation(1.0f);
    const RE::NiTransform about_parent_origin = PLUGIN_NAMESPACE::swing_about_pivot(bone_world, quarter_turn, parent_translate);
    const RE::NiPoint3 moved_origin = about_parent_origin * (bone_world.Invert() * pivot);
    check((moved_origin - pivot).Length() > 1.0f, "pivoting about the parent's origin MUST move the bone (the mistake this locks out)");

    if (failures != 0)
    {
        std::printf("FAILED: %d assertion(s)\n", failures);
        return 1;
    }
    std::puts("PASS: a swing rotates about the node's own origin, keeps it fixed, applies the requested delta rotation, and carries child joints through the matching arc");
    return 0;
}
