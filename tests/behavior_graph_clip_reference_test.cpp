//
// Created by AmazingBuff on 2026/10/27.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/clip_reference_policy.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

#ifdef NDEBUG
#error This test requires assertions.
#endif

namespace
{
    using test_plugin::animation::behavior_graph::clip_reference_agrees;
    using test_plugin::animation::behavior_graph::ClipReferenceMatch;
    using test_plugin::animation::behavior_graph::kClipPositionEpsilon;
    using test_plugin::animation::behavior_graph::pose_component_delta;
    using test_plugin::animation::behavior_graph::pose_rotation_agreement_error;
    using test_plugin::animation::behavior_graph::PoseSample;

    PoseSample sample(float const translation_x, float const rotation_w = 1.0f)
    {
        return PoseSample{ { translation_x, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, rotation_w }, { 1.0f, 1.0f, 1.0f } };
    }
}

void clip_reference_distinguishes_rotation_sign_and_accumulates()
{
    constexpr float kSqrtHalf = 0.70710678f;
    const PoseSample unit = sample(0.0f);

    // Rotations are compared through |dot| of the unit quaternions, so neither the sign nor
    // the length of the input can turn an identical rotation into a mismatch.
    assert(pose_rotation_agreement_error(unit.rotation, unit.rotation) == 0.0f);
    const std::array<float, 4> negated{ 0.0f, 0.0f, 0.0f, -1.0f };
    assert(pose_rotation_agreement_error(unit.rotation, negated) == 0.0f);
    const std::array<float, 4> doubled{ 0.0f, 0.0f, 0.0f, 2.0f };
    assert(pose_rotation_agreement_error(unit.rotation, doubled) == 0.0f);

    // 90 degrees: |dot| = sqrt(1/2), so the error is 1 - sqrt(1/2).
    const std::array<float, 4> quarter{ 0.0f, 0.0f, kSqrtHalf, kSqrtHalf };
    const float quarter_error = pose_rotation_agreement_error(unit.rotation, quarter);
    assert(std::abs(quarter_error - (1.0f - kSqrtHalf)) < 1.0e-6f);

    // Degenerate operands are infinitely different, never "equal".
    const std::array<float, 4> broken{ std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 1.0f };
    assert(!std::isfinite(pose_rotation_agreement_error(unit.rotation, broken)));
    const std::array<float, 4> zero{ 0.0f, 0.0f, 0.0f, 0.0f };
    assert(!std::isfinite(pose_rotation_agreement_error(unit.rotation, zero)));

    // Translation and scale are plain component deltas.
    assert(pose_component_delta(unit.translation, unit.translation) == 0.0f);
    assert(pose_component_delta(unit.translation, sample(0.5f).translation) == 0.5f);
    PoseSample broken_position = unit;
    broken_position.translation[2] = std::numeric_limits<float>::infinity();
    assert(!std::isfinite(pose_component_delta(unit.translation, broken_position.translation)));

    // Aggregation over three entries: one disagrees on translation only, and the maximum must
    // still record it even though the two others agreed.
    ClipReferenceMatch match;
    assert(clip_reference_agrees(match, unit, unit));
    assert(clip_reference_agrees(match, unit, sample(0.0f, -1.0f)));
    assert(!clip_reference_agrees(match, unit, sample(0.01f)));
    assert(match.compared == 3);
    assert(match.agreed == 2);
    assert(match.unmapped == 0);
    assert(std::abs(match.max_position - 0.01f) < 1.0e-6f);
    assert(match.max_rotation == 0.0f);
    assert(match.max_scale == 0.0f);
    assert(kClipPositionEpsilon < 0.01f);

    std::cout << "1 parameterized clip-reference policy case passed (19 assertions, engine-free)\n";
}

int main()
{
    clip_reference_distinguishes_rotation_sign_and_accumulates();
}
