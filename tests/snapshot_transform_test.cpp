//
// Created by AmazingBuff on 2026/10/08.
//

#include "character/snapshot_transform.h"

#include <cmath>
#include <cstdio>

// Conventions below are MEASURED, not assumed (see the harness that produced them in this file's
// history): A * B applies B first, a transform times its own inverse is the identity, matrix
// multiplication is associative, and SetEulerAnglesXYZ's positive Z angle is CLOCKWISE, so a
// heading read as atan2(x_axis.y, x_axis.x) comes out negated.

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

    float heading_of(const RE::NiMatrix3& matrix)
    {
        const RE::NiPoint3 x_axis = matrix * RE::NiPoint3{ 1.0f, 0.0f, 0.0f };
        return std::atan2(x_axis.y, x_axis.x);
    }

    RE::NiMatrix3 rotation_about_z(float angle)
    {
        RE::NiMatrix3 matrix;
        matrix.MakeZRotation(angle);
        return matrix;
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

    // The conventions the swing math depends on, asserted rather than assumed.
    {
        const RE::NiTransform a = make_transform({ 100.0f, 200.0f, 0.0f }, 1.0f, 1.0f);
        const RE::NiTransform b = make_transform({ 10.0f, 0.0f, 0.0f }, 2.0f, 1.0f);
        const RE::NiPoint3 point{ 1.0f, 0.0f, 0.0f };
        check((((a * b) * point) - (a * (b * point))).Length() < 1e-4f, "A * B must apply B first");
        const RE::NiTransform recovered = (a * b) * b.Invert();
        check((recovered.translate - a.translate).Length() < 1e-3f && close(heading_of(recovered.rotate), heading_of(a.rotate), 1e-3f),
            "a composition times the second factor's inverse must recover the first factor");

        const RE::NiMatrix3 r = a.rotate;
        const RE::NiMatrix3 d = rotation_about_z(1.0f);
        check(close(heading_of(d), -1.0f, 0.01f), "SetEulerAnglesXYZ/MakeZRotation's positive Z angle must read as a negative heading (measured convention)");
        check(close(heading_of(r * d), heading_of(r) + heading_of(d), 0.01f), "composing two Z rotations must add their headings");

        RE::NiMatrix3 r_transpose;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                r_transpose.entry[i][j] = r.entry[j][i];
        check(close(heading_of((r * d) * r_transpose), heading_of(r * (d * r_transpose)), 0.01f), "matrix multiplication must be associative over three factors");
    }

    // A bone hung under a rotated, offset parent, and a joint further down that must follow the
    // swing. A vector offset from the bone's origin carries the rotation; transforming the origin
    // itself is a no-op by construction and would prove nothing.
    const RE::NiTransform parent_world = make_transform({ 1234.0f, -485.7f, 2.4f }, 0.7f, 1.0f);
    const RE::NiTransform bone_local = make_transform({ 0.0f, 12.0f, 0.0f }, 0.35f, 1.0f);
    const RE::NiTransform bone_world = parent_world * bone_local;
    const RE::NiPoint3 pivot = bone_world.translate;
    const RE::NiTransform child_world = bone_world * make_transform({ 0.0f, 5.0f, 0.0f }, 0.0f, 1.0f);
    const float child_radius = (child_world.translate - pivot).Length();
    const float bone_heading = heading_of(bone_world.rotate);

    for (const float angle : { 0.0f, 0.25f, 0.6f, 1.0f, -1.0f })
    {
        const RE::NiMatrix3 delta_rotation = rotation_about_z(angle);
        const RE::NiTransform swung = PLUGIN_NAMESPACE::swing_about_pivot(bone_world, delta_rotation, pivot);

        // 1. The pivot stays where it was. This catches pivoting about the PARENT's origin, which
        //    moved the bone instead of turning it.
        const RE::NiPoint3 swung_origin = swung * (bone_world.Invert() * pivot);
        check((swung_origin - pivot).Length() < 0.01f, "the bone origin must not move when swinging about it");

        // 2. The node's heading advances by exactly the requested angle (the measured sign
        //    convention makes that a subtraction of the requested angle).
        check(close(heading_of(swung.rotate), bone_heading - angle, 0.02f), "the swung node's heading must advance by the requested angle");

        // 3. The joint further down swings through the arc of the same angle.
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
    {
        const RE::NiTransform about_parent = PLUGIN_NAMESPACE::swing_about_pivot(bone_world, rotation_about_z(1.0f), parent_world.translate);
        const RE::NiPoint3 moved_origin = about_parent * (bone_world.Invert() * pivot);
        check((moved_origin - pivot).Length() > 1.0f, "pivoting about the parent's origin MUST move the bone (the mistake this locks out)");
    }

    // 6. The delta the probe actually applies: D about the bone's own origin, then composed onto
    //    every node of the subtree. This is the path a node the probe swings takes, so assert the
    //    node's heading moves AND its origin does not.
    {
        const RE::NiMatrix3 d = rotation_about_z(1.0f);
        RE::NiTransform delta;
        delta.rotate = d;
        delta.translate = bone_world.translate - d * bone_world.translate;
        delta.scale = 1.0f;

        const RE::NiTransform posed = delta * bone_world;
        check(close(heading_of(posed.rotate), bone_heading - 1.0f, 0.02f), "the probe's delta must advance the node's heading by the swing");
        check((posed.translate - bone_world.translate).Length() < 0.02f, "the probe's delta must keep the bone on its own origin");

        // The same delta must work for a bone at any heading, since the probe picks its bone at
        // runtime and cannot assume an orientation.
        for (const float heading : { 0.0f, 0.35f, 1.9f, -2.4f })
        {
            const RE::NiTransform other = make_transform({ 10.0f, 20.0f, -3.0f }, heading, 1.0f);
            RE::NiTransform other_delta;
            other_delta.rotate = d;
            other_delta.translate = other.translate - d * other.translate;
            other_delta.scale = 1.0f;
            const RE::NiTransform other_posed = other_delta * other;
            check(close(heading_of(other_posed.rotate), heading_of(other.rotate) - 1.0f, 0.02f), "the probe's delta must advance any bone's heading by the swing");
            check((other_posed.translate - other.translate).Length() < 0.02f, "the probe's delta must keep any bone on its own origin");
        }
    }

    if (failures != 0)
    {
        std::printf("FAILED: %d assertion(s)\n", failures);
        return 1;
    }
    std::puts("PASS: a swing rotates about the node's own origin, keeps it fixed, advances the heading by the requested angle, and carries child joints through the matching arc");
    return 0;
}
