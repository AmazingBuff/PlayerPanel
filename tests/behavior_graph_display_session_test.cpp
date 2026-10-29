//
// Created by AmazingBuff on 2026/10/28.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/display_state_policy.h"
#include "animation/behavior_graph/pose_motion_policy.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef NDEBUG
#error This test requires assertions.
#endif

namespace
{
    using test_plugin::animation::behavior_graph::DisplaySourceIdentity;
    using test_plugin::animation::behavior_graph::DisplayWindow;
    using test_plugin::animation::behavior_graph::DisplaySourceRejection;
    using test_plugin::animation::behavior_graph::PoseSample;

    void source_sessions_do_not_reuse_stale_views_or_partial_measurements()
    {
        DisplaySourceIdentity retained{ 0x14, 1, 0x1000, 0x20 };
        DisplayWindow window;
        assert(window.bind(retained, 1));
        assert(window.accepts(retained));
        DisplaySourceIdentity replaced = retained;
        replaced.graph = 0x2000;
        assert(window.bind(replaced, 2));
        assert(!window.accepts(retained));
        assert(window.rejection(retained, replaced.graph) == DisplaySourceRejection::e_graph_mismatch);
        replaced = retained;
        ++replaced.generation;
        assert(window.bind(replaced, 3));
        assert(!window.accepts(retained));
        assert(window.rejection(retained, replaced.graph) == DisplaySourceRejection::e_generation_mismatch);

        replaced = retained;
        ++replaced.actor_id;
        assert(window.bind(replaced, 3));
        assert(window.rejection(retained, replaced.graph) == DisplaySourceRejection::e_actor_mismatch);
        replaced = retained;
        ++replaced.owner;
        assert(window.bind(replaced, 3));
        assert(window.rejection(retained, replaced.graph) == DisplaySourceRejection::e_owner_mismatch);

        assert(window.bind(retained, 4));
        assert(window.rejection(retained, 0x2000) == DisplaySourceRejection::e_call_graph_mismatch);
        assert(window.rejection(retained, retained.graph) == DisplaySourceRejection::e_none);
        for (int i = 0; i < 60; ++i) window.advanced();
        assert(window.steps() == 60);
        window.end();
        assert(!window.accepts(retained));
        assert(window.bind(retained, 5));
        assert(window.steps() == 0 && window.accepts(retained));
        for (int i = 0; i < 60; ++i) window.advanced();
        assert(window.steps() == 60);
        window.retarget();
        assert(window.steps() == 0 && window.view() == 1 && window.accepts(retained));

        assert(test_plugin::animation::behavior_graph::invalidate_display_source(retained, 0x20));
        assert(!window.accepts(retained));
        // Even an allocator reusing the former owner's address cannot revive its cleared view.
        assert(window.bind(DisplaySourceIdentity{ 0x14, 1, 0x1000, 0x20 }, 1));
        assert(!window.accepts(retained));
        assert(window.rejection(retained, 0x1000) == DisplaySourceRejection::e_unbound_retained);
        assert(window.reject());
        assert(window.rejection(retained, 0x1000) == DisplaySourceRejection::e_inactive_view);
        assert(!window.reject() && !window.take_row());
    }

    void candidate_motion_and_applied_coverage_do_not_overstate_visible_motion()
    {
        PoseSample pose{ { 0.0F, 0.0F, 0.0F }, { 0.0F, 0.0F, 0.0F, 1.0F }, { 1.0F, 1.0F, 1.0F } };
        std::vector<PoseSample> previous{ pose };
        std::size_t raw_changed = 0;
        std::size_t semantic_changed = 0;
        for (int frame = 1; frame < 120; ++frame)
        {
            pose.rotation[3] = -previous[0].rotation[3];
            pose.scale[1] += 1.0F;
            pose.scale[2] += 2.0F;
            std::vector<PoseSample> const current{ pose };
            raw_changed += test_plugin::animation::behavior_graph::compare_pose(current, previous).moved > 0 ? 1u : 0u;
            const auto semantic = test_plugin::animation::behavior_graph::compare_display_motion(current, previous);
            assert(semantic.valid);
            semantic_changed += semantic.moved > 0 ? 1u : 0u;
            previous = current;
        }
        assert(raw_changed == 119 && semantic_changed == 0);
        pose.translation[0] += 1.0e-4F;
        assert(test_plugin::animation::behavior_graph::compare_display_motion({ pose }, previous).moved == 1);
        previous = { pose };
        pose.rotation = { 0.0F, 0.0F, std::sin(1.0e-5F), std::cos(1.0e-5F) };
        assert(test_plugin::animation::behavior_graph::compare_display_motion({ pose }, previous).moved == 1);

        struct Node { float local; };
        Node root{ 9.0F };
        Node hand{ 0.0F };
        std::unordered_map<std::string, Node*> const nodes{ { "Hand", &hand } };
        std::vector<std::pair<std::string, float>> const named_pose{
            { "SceneRoot", 1.0F }, { "Hand", 3.0F }, { "MissingBone", 5.0F },
            { "Missing2", 5.0F }, { "Missing3", 5.0F }, { "Missing4", 5.0F },
            { "Missing5", 5.0F }, { "Missing6", 5.0F }, { "Missing7", 5.0F }
        };
        const auto coverage = test_plugin::animation::behavior_graph::apply_named_display_pose(named_pose, nodes, "SceneRoot");
        assert(coverage.requested == 9 && coverage.applied == 1 && coverage.missing == 7 && coverage.skipped_root == 1);
        assert(coverage.requested == coverage.applied + coverage.missing + coverage.skipped_root);
        assert(coverage.missing_name_count == 7);
        for (std::size_t i = 0; i < coverage.missing_name_count; ++i)
            assert(coverage.missing_names[i] == named_pose[i + 2].first);
        assert(hand.local == 3.0F && root.local == 9.0F);
    }
}

int main()
{
    source_sessions_do_not_reuse_stale_views_or_partial_measurements();
    candidate_motion_and_applied_coverage_do_not_overstate_visible_motion();
    std::cout << "2 display-session regression cases passed (engine-free)\n";
}
