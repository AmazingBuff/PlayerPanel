//
// Created by AmazingBuff on 2026/10/27.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/pose_content_policy.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#ifdef NDEBUG
#error This test requires assertions.
#endif

namespace
{
    using test_plugin::animation::behavior_graph::compare_pose;
    using test_plugin::animation::behavior_graph::count_identical;
    using test_plugin::animation::behavior_graph::kPoseDeltaEpsilon;
    using test_plugin::animation::behavior_graph::PoseSample;
    using test_plugin::animation::behavior_graph::pose_sample_differs;
    using test_plugin::animation::behavior_graph::pose_sample_finite;
    using test_plugin::animation::behavior_graph::pose_sample_max_delta;

    PoseSample sample(float const translation_x, float const rotation_w = 1.0f)
    {
        return PoseSample{ { translation_x, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, rotation_w }, { 1.0f, 1.0f, 1.0f } };
    }
}

void pose_content_distinguishes_finite_change_and_alias()
{
    // Finite/reference rows: a bind-like sample is finite; NaN or inf in any meaningful
    // component makes the entry unusable, and the fourth lane of the vectors must not
    // influence the decision.
    const PoseSample bind = sample(0.0f);
    assert(pose_sample_finite(bind));

    PoseSample broken = bind;
    broken.translation[1] = std::numeric_limits<float>::quiet_NaN();
    assert(!pose_sample_finite(broken));

    broken = bind;
    broken.rotation[3] = std::numeric_limits<float>::infinity();
    assert(!pose_sample_finite(broken));

    broken = bind;
    broken.scale[2] = -std::numeric_limits<float>::infinity();
    assert(!pose_sample_finite(broken));

    // Delta/change rows: sub-epsilon motion is not "changed", a real component change is,
    // and a NaN can never be reported as unchanged.
    assert(pose_sample_max_delta(bind, bind) == 0.0f);
    assert(!pose_sample_differs(bind, bind));
    const PoseSample tiny = sample(5.0e-7f);
    assert(pose_sample_max_delta(bind, tiny) > 0.0f);
    assert(!pose_sample_differs(bind, tiny, kPoseDeltaEpsilon));
    const PoseSample moved = sample(0.0f, 0.5f);
    assert(pose_sample_differs(bind, moved));
    assert(std::abs(pose_sample_max_delta(bind, moved) - 0.5f) < 1.0e-6f);
    assert(pose_sample_differs(bind, broken));

    // Aggregate rows over a four-entry checkpoint.
    const std::vector<PoseSample> previous{ bind, moved, bind, sample(1.0f) };
    const std::vector<PoseSample> current{ bind, sample(0.0f, 0.25f), broken, sample(1.0f) };
    const auto first = compare_pose(current, {});
    assert(first.entries == 4);
    assert(first.finite == 3);
    assert(first.non_finite == 1);
    assert(!first.has_previous);
    assert(first.moved == 0);

    const auto compared = compare_pose(current, previous);
    assert(compared.has_previous);
    assert(compared.moved == 2);  // index 1 (rotation 0.5->0.25) and index 2 (NaN => delta inf)
    assert(compared.max_delta > 0.0f);
    assert(count_identical(current, previous) == 2);  // indices 0 and 3 are bit-identical

    std::cout << "1 parameterized pose-content policy case passed (20 assertions, engine-free)\n";
}

int main()
{
    pose_content_distinguishes_finite_change_and_alias();
}
