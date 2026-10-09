//
// Created by AmazingBuff on 2026/10/08.
//

#include "character/idle_driver.h"
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

    RE::NiMatrix3 rotation_about_x(float angle)
    {
        RE::NiMatrix3 matrix;
        matrix.MakeXRotation(angle);
        return matrix;
    }

    RE::NiPoint3 y_axis_of(const RE::NiMatrix3& matrix)
    {
        return matrix * RE::NiPoint3{ 0.0f, 1.0f, 0.0f };
    }

    float angle_between(const RE::NiPoint3& lhs, const RE::NiPoint3& rhs)
    {
        const float lengths = lhs.Length() * rhs.Length();
        if (lengths <= 0.0f)
            return 0.0f;
        return std::acos(std::clamp(lhs.Dot(rhs) / lengths, -1.0f, 1.0f));
    }

    // A world-X swing rotates only the YZ-plane projection of a direction; its X component is
    // invariant, so the full 3D angle between the two directions is smaller than the swing
    // whenever the direction leans along X.
    float yz_angle_between(const RE::NiPoint3& lhs, const RE::NiPoint3& rhs)
    {
        return angle_between(RE::NiPoint3{ 0.0f, lhs.y, lhs.z }, RE::NiPoint3{ 0.0f, rhs.y, rhs.z });
    }

    RE::NiTransform make_pose(const RE::NiPoint3& translate, float pitch_x, float roll_y, float heading_z, float scale)
    {
        RE::NiTransform transform;
        transform.translate = translate;
        transform.rotate.SetEulerAnglesXYZ(pitch_x, roll_y, heading_z);
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

    // 6. The delta the probe applies, through the production helper instead of a copy of its math:
    //    D about the bone's own origin, composed onto every node of the subtree. The probe swings
    //    about world X, so the node's Y axis is what must advance by the requested angle.
    {
        for (const float angle : { 0.0f, 0.35f, 1.0f, -1.0f })
        {
            const RE::NiMatrix3 d = rotation_about_x(angle);
            const RE::NiTransform posed = PLUGIN_NAMESPACE::swing_delta_about_pivot(d, bone_world.translate) * bone_world;
            check(close(yz_angle_between(y_axis_of(bone_world.rotate), y_axis_of(posed.rotate)), std::abs(angle), 0.02f),
                "the probe's X swing must advance the node's Y axis by the requested angle");
            check((posed.translate - bone_world.translate).Length() < 0.02f, "the probe's delta must keep the bone on its own origin");
        }

        // The same delta on a bone carrying non-zero pitch and roll and a non-unit scale: the probe
        // picks its bone at runtime and can assume neither.
        for (const float heading : { 0.0f, 0.35f, 1.9f, -2.4f })
        {
            const RE::NiTransform other = make_pose({ 10.0f, 20.0f, -3.0f }, 0.4f, -0.7f, heading, 1.8f);
            const RE::NiTransform posed = PLUGIN_NAMESPACE::swing_delta_about_pivot(rotation_about_x(0.6f), other.translate) * other;
            check(close(yz_angle_between(y_axis_of(other.rotate), y_axis_of(posed.rotate)), 0.6f, 0.02f),
                "the probe's X swing must advance a pitched, rolled, scaled bone by the requested angle");
            check((posed.translate - other.translate).Length() < 0.02f, "the probe's delta must keep a pitched, rolled, scaled bone on its own origin");
        }
    }

    // 7. The witness rule the probe's displacement measurement rests on: a point ON the swing axis
    //    cannot move, so its flat reading must never be taken as "the write did nothing", while an
    //    off-axis witness travels the arc 2 r sin(theta/2) that T1 measures.
    {
        const RE::NiPoint3 axis_origin = bone_world.translate;
        const RE::NiTransform delta = PLUGIN_NAMESPACE::swing_delta_about_pivot(rotation_about_x(0.6f), axis_origin);

        const RE::NiPoint3 on_axis = axis_origin + RE::NiPoint3{ 12.0f, 0.0f, 0.0f };
        check(close(PLUGIN_NAMESPACE::swing_radius_about_x(on_axis, axis_origin), 0.0f, 0.001f), "a point on the X axis through the pivot must measure zero radius");
        check(((delta * on_axis) - on_axis).Length() < 0.001f, "a witness on the swing axis must not move, and that is not evidence about the write");

        const RE::NiPoint3 off_axis = axis_origin + RE::NiPoint3{ 3.0f, 4.0f, 0.0f };
        const float radius = PLUGIN_NAMESPACE::swing_radius_about_x(off_axis, axis_origin);
        check(close(radius, 4.0f, 0.001f), "the witness radius must be the distance to the swing axis, not to the pivot");
        const float expected_arc = 2.0f * radius * std::sin(0.6f * 0.5f);
        check(close(((delta * off_axis) - off_axis).Length(), expected_arc, 0.01f), "an off-axis witness must travel the arc of the swing angle");
    }

    // 8. The orientation reading the probe reports. Row 0 is what the first probe build compared and
    //    it is exactly the row an X swing leaves alone, so this locks both halves: the invariant row
    //    (which must not be used as the witness) and the trace-based angle (which must work for any
    //    axis). The slot distance has the same requirement: it must see a swing about any axis.
    {
        const RE::NiTransform base = make_pose({ 5.0f, -2.0f, 1.0f }, 0.4f, -0.7f, 0.9f, 1.0f);
        for (const float angle : { 0.0f, 0.35f, 1.0f, -1.0f })
        {
            for (const int axis : { 0, 1, 2 })
            {
                RE::NiMatrix3 d;
                if (axis == 0)
                    d.MakeXRotation(angle);
                else if (axis == 1)
                    d.MakeYRotation(angle);
                else
                    d.MakeZRotation(angle);
                const RE::NiTransform swung = PLUGIN_NAMESPACE::swing_delta_about_pivot(d, base.translate) * base;
                // The reading is in degrees, the loop variable in radians.
                check(close(PLUGIN_NAMESPACE::rotation_angle_degrees(base.rotate, swung.rotate), std::abs(angle) * 57.29578f, 1.2f),
                    "the trace-based orientation delta must report the applied angle for an X, Y or Z swing");
            }
        }

        const RE::NiTransform swung_x = PLUGIN_NAMESPACE::swing_delta_about_pivot(rotation_about_x(0.6f), base.translate) * base;
        check(close(swung_x.rotate.entry[0][0], base.rotate.entry[0][0], 0.001f) &&
              close(swung_x.rotate.entry[0][1], base.rotate.entry[0][1], 0.001f) &&
              close(swung_x.rotate.entry[0][2], base.rotate.entry[0][2], 0.001f),
            "an X swing must leave row 0 unchanged, which is why row 0 cannot witness an X swing");

        // The slot comparison must separate the two candidates for an X swing, where row 0 cannot.
        float slot[12] = {};
        PLUGIN_NAMESPACE::write_bone_matrix(slot, 0, swung_x);
        check(PLUGIN_NAMESPACE::rotation_distance(slot, swung_x.rotate) < 0.001f, "the slot distance must match the orientation the slot holds");
        check(PLUGIN_NAMESPACE::rotation_distance(slot, base.rotate) > 0.1f, "the slot distance must distinguish a swung orientation from the captured one for an X swing");

        // The candidate set contains both a rotation and its transpose, so the reading must tell
        // them apart — a tie would make the classification ambiguous by construction.
        const RE::NiMatrix3 transpose = swung_x.rotate.Transpose();
        const float transpose_distance = PLUGIN_NAMESPACE::rotation_distance(slot, transpose);
        check(transpose_distance > 0.1f, "the slot distance must separate a rotation from its transpose");
        float transposed_slot[12] = {};
        RE::NiTransform transposed = swung_x;
        transposed.rotate = transpose;
        PLUGIN_NAMESPACE::write_bone_matrix(transposed_slot, 0, transposed);
        check(PLUGIN_NAMESPACE::rotation_distance(transposed_slot, transpose) < 0.001f && PLUGIN_NAMESPACE::rotation_distance(transposed_slot, swung_x.rotate) > 0.1f,
            "a transposed slot must match the transposed candidate, not the original");
    }

    // 9. The matrix write kept for the evidence-gated step (not used by the probe this round): the
    //    layout is a straight 3x4 row-major copy at a 48-byte stride. What the buffer stores is NOT
    //    asserted here — the classification that claimed it was the raw world matrix compared the
    //    slot against the value the same code had just written into it.
    {
        float buffer[24] = {};
        const RE::NiTransform bone = bone_world;
        PLUGIN_NAMESPACE::write_bone_matrix(buffer, 1, bone);

        RE::NiTransform written;
        written.rotate.entry[0][0] = buffer[12];
        written.rotate.entry[0][1] = buffer[13];
        written.rotate.entry[0][2] = buffer[14];
        written.translate.x = buffer[15];
        written.rotate.entry[1][0] = buffer[16];
        written.rotate.entry[1][1] = buffer[17];
        written.rotate.entry[1][2] = buffer[18];
        written.translate.y = buffer[19];
        written.rotate.entry[2][0] = buffer[20];
        written.rotate.entry[2][1] = buffer[21];
        written.rotate.entry[2][2] = buffer[22];
        written.translate.z = buffer[23];
        written.scale = 1.0f;

        check((written.translate - bone.translate).Length() < 0.001f, "the written matrix must carry the bone's own world translation");
        check(close(written.rotate.entry[0][0], bone.rotate.entry[0][0], 0.001f) && close(written.rotate.entry[1][1], bone.rotate.entry[1][1], 0.001f),
            "the written matrix must carry the bone's own world rotation");

        // A bind-space point (the bind origin is the bone's own origin) must land on the bone.
        const RE::NiPoint3 bind_origin = bone.translate;
        check((written * bind_origin - bone * bind_origin).Length() < 0.001f, "the buffer's matrix must reproduce the bone's own transform");

        // Slot 0 must stay untouched when slot 1 is written: 48-byte stride.
        check(close(buffer[0], 0.0f, 0.001f) && close(buffer[11], 0.0f, 0.001f), "writing slot 1 must not disturb slot 0 (48-byte stride)");
    }

    // 10. The procedural idle's animation table and phase math. A wrong sign, period or amplitude is
    //     invisible in one frame and obvious only over a cycle, so the table's shape is asserted here
    //     rather than discovered in-game.
    {
        check(PLUGIN_NAMESPACE::Idle_Channel_Count >= 4, "the idle must drive more than one joint to read as an idle");
        size_t translations = 0;
        for (const PLUGIN_NAMESPACE::IdleChannel& channel : PLUGIN_NAMESPACE::Idle_Channels)
        {
            check(channel.joint != nullptr && channel.joint[0] != '\0', "every idle channel must name a joint");
            check(channel.axis >= 0 && channel.axis <= 2, "an idle channel's axis must be X, Y or Z");
            check(channel.period_seconds > 0.5, "an idle period that short would read as a twitch");
            if (channel.motion == PLUGIN_NAMESPACE::IdleMotion::e_rotate)
                check(std::abs(channel.amplitude) <= 5.0f, "an idle rotation beyond a few degrees stops being an idle");
            else
            {
                // Studio units: the figure is roughly 45 units tall at the studio's 0.35 scale, so a
                // translation much beyond a unit stops reading as a weight shift and starts sliding.
                check(channel.amplitude > 0.0f && channel.amplitude <= 2.0f, "an idle translation must be small and positive");
                ++translations;
            }
        }
        // A rotation about a joint's own origin cannot move that joint, and this skeleton's pelvis is
        // a sibling of the spine, so the hips need a translation channel to read as a weight shift.
        check(translations >= 1, "the idle needs at least one translation channel: pelvis and spine are siblings here");

        // Zero, half-period and full period are the cheapest sentinels for a sign or period mistake.
        const float amplitude = 2.0f;
        const double period = 6.0;
        check(close(PLUGIN_NAMESPACE::idle_angle_degrees(amplitude, period, 0.0, 0.0), 0.0f, 0.001f), "the idle angle must start at zero");
        check(close(PLUGIN_NAMESPACE::idle_angle_degrees(amplitude, period, 0.0, period * 0.25), amplitude, 0.001f), "a quarter period must reach the amplitude");
        check(close(PLUGIN_NAMESPACE::idle_angle_degrees(amplitude, period, 0.0, period * 0.5), 0.0f, 0.001f), "half a period must cross zero");
        check(close(PLUGIN_NAMESPACE::idle_angle_degrees(amplitude, period, 0.0, period * 0.75), -amplitude, 0.001f), "three quarters must reach the negative amplitude");
        check(close(PLUGIN_NAMESPACE::idle_angle_degrees(amplitude, period, 0.0, period), 0.0f, 0.001f), "a full period must return to the start");
        check(close(PLUGIN_NAMESPACE::idle_angle_degrees(amplitude, period, 1.5, 0.0), PLUGIN_NAMESPACE::idle_angle_degrees(amplitude, period, 0.0, 1.5), 0.001f),
            "a phase offset must equal the same channel evaluated later");

        // Every frame's angle is bounded, which is what keeps an accumulated clock from ever posing
        // the figure: the driver recomputes from the captured pose rather than integrating.
        float worst = 0.0f;
        for (double seconds = 0.0; seconds < 40.0; seconds += 0.13)
            worst = std::max(worst, std::abs(PLUGIN_NAMESPACE::idle_angle_degrees(amplitude, period, 0.0, seconds)));
        check(worst <= amplitude + 0.001f, "the idle angle must never exceed its amplitude");

        // A driven joint turns about its own origin and carries its child, which is the same contract
        // the probe relied on; a translated joint travels without turning.
        const auto rotation_for_axis = [](int axis, float radians)
        {
            RE::NiMatrix3 matrix;
            if (axis == 0)
                matrix.MakeXRotation(radians);
            else if (axis == 1)
                matrix.MakeYRotation(radians);
            else
                matrix.MakeZRotation(radians);
            return matrix;
        };

        const PLUGIN_NAMESPACE::IdleChannel* rotate_channel = nullptr;
        const PLUGIN_NAMESPACE::IdleChannel* translate_channel = nullptr;
        for (const PLUGIN_NAMESPACE::IdleChannel& channel : PLUGIN_NAMESPACE::Idle_Channels)
        {
            if (!rotate_channel && channel.motion == PLUGIN_NAMESPACE::IdleMotion::e_rotate)
                rotate_channel = &channel;
            if (!translate_channel && channel.motion == PLUGIN_NAMESPACE::IdleMotion::e_translate)
                translate_channel = &channel;
        }
        check(rotate_channel != nullptr && translate_channel != nullptr, "the idle table must contain both motion kinds");

        if (rotate_channel)
        {
            const float angle = PLUGIN_NAMESPACE::idle_angle_degrees(rotate_channel->amplitude, rotate_channel->period_seconds, rotate_channel->phase_seconds, rotate_channel->period_seconds * 0.25);
            const RE::NiTransform driven = PLUGIN_NAMESPACE::swing_delta_about_pivot(rotation_for_axis(rotate_channel->axis, angle * 0.017453292f), bone_world.translate) * bone_world;
            check((driven.translate - bone_world.translate).Length() < 0.01f, "a rotated joint must stay on its own origin");
            check(close(PLUGIN_NAMESPACE::rotation_angle_degrees(bone_world.rotate, driven.rotate), std::abs(angle), 0.02f),
                "a rotated joint must advance by the channel's angle");
        }

        if (translate_channel)
        {
            RE::NiTransform shift;
            shift.scale = 1.0f;
            const float distance = PLUGIN_NAMESPACE::idle_angle_degrees(translate_channel->amplitude, translate_channel->period_seconds, translate_channel->phase_seconds, translate_channel->period_seconds * 0.25);
            RE::NiPoint3 offset{};
            if (translate_channel->axis == 0)
                offset.x = distance;
            else if (translate_channel->axis == 1)
                offset.y = distance;
            else
                offset.z = distance;
            shift.translate = offset;
            const RE::NiTransform shifted = shift * bone_world;
            check((shifted.translate - bone_world.translate - offset).Length() < 0.001f, "a translated joint must move by exactly its offset");
            check(close(shifted.rotate.entry[0][0], bone_world.rotate.entry[0][0], 0.0001f) && close(shifted.rotate.entry[2][2], bone_world.rotate.entry[2][2], 0.0001f),
                "a translation channel must not turn the joint");
        }
    }

    if (failures != 0)
    {
        std::printf("FAILED: %d assertion(s)\n", failures);
        return 1;
    }
    std::puts("PASS: a swing rotates about the node's own origin, keeps it fixed, advances the swung axis by the requested angle for any pose and scale, carries child joints through the matching arc, and leaves an on-axis witness (correctly) motionless");
    return 0;
}
