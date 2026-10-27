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
    using test_plugin::animation::behavior_graph::pose_buffer_alias;
    using test_plugin::animation::behavior_graph::pose_sample_differs;
    using test_plugin::animation::behavior_graph::pose_sample_finite;
    using test_plugin::animation::behavior_graph::pose_sample_max_delta;
    using test_plugin::animation::behavior_graph::watch_aliasing;

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

    // v22 alias-watch rows: the raw equal count cannot separate constant bones from a
    // shared buffer, so the split is taken against the step-0 baseline and buffer
    // identity is checked directly. A moving entry that still matches the source is the
    // discriminating case; constant entries matching the source are expected.
    const std::vector<PoseSample> baseline{ bind, bind, bind };
    const std::vector<PoseSample> still{ bind, bind, bind };
    const std::vector<PoseSample> moved_bone{ bind, moved, bind };
    const std::vector<PoseSample> source_matching{ bind, moved, bind };

    const auto no_baseline = watch_aliasing(moved_bone, source_matching, {}, false);
    assert(!no_baseline.has_baseline);
    assert(no_baseline.equal == 3);
    assert(no_baseline.equal_moving == 0);
    assert(no_baseline.equal_static == 0);

    const auto alias_watch = watch_aliasing(moved_bone, source_matching, baseline, false);
    assert(alias_watch.has_baseline);
    assert(alias_watch.equal == 3);
    assert(alias_watch.equal_moving == 1);   // index 1 moved since the baseline and still matches
    assert(alias_watch.equal_static == 2);

    const auto static_watch = watch_aliasing(still, still, baseline, false);
    assert(static_watch.equal == 3);
    assert(static_watch.equal_moving == 0);  // constant bones matching the source are not aliasing
    assert(static_watch.equal_static == 3);

    assert(!watch_aliasing(still, still, { bind }, false).has_baseline);  // short baseline is unavailable
    assert(watch_aliasing(still, still, baseline, true).buffer_alias);

    int object = 0;
    assert(!pose_buffer_alias(nullptr, nullptr));
    assert(pose_buffer_alias(&object, &object));
    assert(!pose_buffer_alias(&object, nullptr));

    std::cout << "1 parameterized pose-content policy case passed (36 assertions, engine-free)\n";
}

int main()
{
    pose_content_distinguishes_finite_change_and_alias();
}
