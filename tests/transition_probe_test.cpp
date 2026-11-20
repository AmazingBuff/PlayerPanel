//
// C-G5-TRANS v1 engine-free policy cases (2026-11-18): fixed capacities and budget,
// name truncation, class/side/segment names and the overflow mask.
//
// Build:
//   cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests\transition_probe_test.cpp
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/transition_clip_clock_policy.h"
#include "animation/behavior_graph/transition_parent_chain_policy.h"
#include "animation/behavior_graph/transition_probe.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>

using namespace test_plugin::animation::behavior_graph;

namespace
{
    void capacities_and_budget_are_the_registered_values()
    {
        static_assert(std::is_trivially_copyable_v<TransitionProbeRecord>);
        static_assert(sizeof(TransitionProbeRecord) <= Transition_Probe_Record_Budget);
        assert(Transition_Probe_Name_Length == 96);
        assert(Transition_Probe_Node_Limit == 512);
        assert(Transition_Probe_State_Machine_Capacity == 32);
        assert(Transition_Probe_Effect_Capacity == 32);
        assert(Transition_Probe_Blender_Capacity == 8);
        assert(Transition_Probe_Child_Capacity == 8);
        assert(Transition_Probe_Internal_Capacity == 8);
        assert(Transition_Probe_Source_Capacity == 2560);
        assert(Transition_Probe_Record_Budget == 16384);
        assert(Transition_Probe_Version == 3);
        assert(Transition_Probe_Magic == 0x31525054u);
    }

    void names_are_truncated_and_terminated()
    {
        char dest[Transition_Probe_Name_Length]{};
        std::string const long_name(200, 'a');
        transition_probe_copy_name(dest, long_name);
        assert(std::strlen(dest) == Transition_Probe_Name_Length - 1);
        assert(dest[Transition_Probe_Name_Length - 1] == '\0');
        assert(dest[0] == 'a');
        assert(dest[Transition_Probe_Name_Length - 2] == 'a');

        char empty[Transition_Probe_Name_Length]{};
        std::memset(empty, 'x', sizeof(empty));
        transition_probe_copy_name(empty, {});
        assert(empty[0] == '\0');
    }

    void names_are_stable()
    {
        assert(std::strcmp(transition_probe_side_name(TransitionProbeSide::candidate), "candidate") == 0);
        assert(std::strcmp(transition_probe_side_name(TransitionProbeSide::source), "source") == 0);
        assert(std::strcmp(transition_probe_segment_name(TransitionProbeSegment::equip), "equip") == 0);
        assert(std::strcmp(transition_probe_segment_name(TransitionProbeSegment::unequip), "unequip") == 0);
        assert(std::strcmp(transition_probe_segment_name(TransitionProbeSegment::window), "window") == 0);
        assert(std::strcmp(transition_probe_phase_name(TransitionProbePhase::post_update), "post-update") == 0);
        assert(std::strcmp(transition_probe_phase_name(TransitionProbePhase::pre_sample), "pre-sample") == 0);
        assert(std::strcmp(transition_probe_phase_name(TransitionProbePhase::post_sample), "post-sample") == 0);
        assert(std::strcmp(transition_probe_class_name(TransitionProbeNodeClass::clip), "clip") == 0);
        assert(std::strcmp(transition_probe_class_name(TransitionProbeNodeClass::state_machine), "state-machine") == 0);
        assert(std::strcmp(transition_probe_class_name(TransitionProbeNodeClass::blender), "blender") == 0);
        assert(std::strcmp(transition_probe_class_name(TransitionProbeNodeClass::blending_transition), "blending-transition") == 0);
        assert(std::strcmp(transition_probe_class_name(TransitionProbeNodeClass::generator_transition), "generator-transition") == 0);
        assert(std::strcmp(transition_probe_class_name(TransitionProbeNodeClass::behavior_reference), "behavior-reference") == 0);
        assert(std::strcmp(transition_probe_class_name(TransitionProbeNodeClass::modifier_generator), "modifier-generator") == 0);
        assert(std::strcmp(transition_probe_class_name(TransitionProbeNodeClass::unknown), "unknown") == 0);
    }

    void overflow_mask_covers_each_structural_cap()
    {
        TransitionProbeRecord record{};
        assert(!transition_probe_overflowed(record));

        record.flags = Transition_Probe_Flag_Unavailable;
        assert(!transition_probe_overflowed(record));
        record.flags = Transition_Probe_Flag_Name_Invalid;
        assert(!transition_probe_overflowed(record));

        for (std::uint16_t const flag : { Transition_Probe_Flag_State_Machine_Overflow,
                 Transition_Probe_Flag_Effect_Overflow, Transition_Probe_Flag_Blender_Overflow,
                 Transition_Probe_Flag_Child_Overflow, Transition_Probe_Flag_Internal_Overflow })
        {
            record.flags = flag;
            assert(transition_probe_overflowed(record));
        }
    }

    void zero_record_is_empty()
    {
        TransitionProbeRecord record{};
        assert(record.magic == 0);
        assert(record.version == 0);
        assert(record.state_machine_count == 0);
        assert(record.effect_count == 0);
        assert(record.blender_count == 0);
        assert(record.walked == 0);
        assert(record.active_nodes == 0);
    }

    void subgraph_bounds_are_the_registered_values()
    {
        assert(Transition_Subgraph_Bfr_Capacity == 8);
        assert(Transition_Subgraph_Node_Capacity == 256);
        assert(Transition_Subgraph_State_Machine_Capacity == 8);
        assert(Transition_Subgraph_Blender_Capacity == 8);
        assert(Transition_Subgraph_Child_Capacity == 8);
        assert(Transition_Subgraph_Equip_Max_Commanded == 13);
        assert(Transition_Subgraph_Unequip_Max_Commanded == 12);
    }

    void subgraph_window_is_bounded()
    {
        assert(!transition_subgraph_window_matches(TransitionProbeSegment::equip, 0));
        assert(transition_subgraph_window_matches(TransitionProbeSegment::equip, 1));
        assert(transition_subgraph_window_matches(TransitionProbeSegment::equip, 13));
        assert(!transition_subgraph_window_matches(TransitionProbeSegment::equip, 14));
        assert(!transition_subgraph_window_matches(TransitionProbeSegment::unequip, 0));
        assert(transition_subgraph_window_matches(TransitionProbeSegment::unequip, 1));
        assert(transition_subgraph_window_matches(TransitionProbeSegment::unequip, 12));
        assert(!transition_subgraph_window_matches(TransitionProbeSegment::unequip, 13));
        assert(!transition_subgraph_window_matches(TransitionProbeSegment::window, 1));
    }

    void subgraph_status_names_are_stable()
    {
        assert(std::strcmp(transition_subgraph_status_name(TransitionSubgraphStatus::ok), "ok") == 0);
        assert(std::strcmp(transition_subgraph_status_name(TransitionSubgraphStatus::unavailable), "unavailable") == 0);
        assert(std::strcmp(transition_subgraph_status_name(TransitionSubgraphStatus::unknown_behavior_class), "unknown-behavior-class") == 0);
        assert(std::strcmp(transition_subgraph_status_name(TransitionSubgraphStatus::no_active_nodes), "no-active-nodes") == 0);
        assert(std::strcmp(transition_subgraph_status_name(TransitionSubgraphStatus::invalid_array), "invalid-array") == 0);
        assert(std::strcmp(transition_subgraph_status_name(TransitionSubgraphStatus::inactive), "inactive") == 0);
    }

    void parent_chain_retains_shared_parents_and_skips_empty_slots()
    {
        // Nodes 1 and 2 both reference the seed, and node 3 references both.
        // Node 4 is a sibling with no path to the seed.
        ParentChainChildRange const ranges[] = { { -1, 0 }, { 0, 2 }, { 2, 1 }, { 3, 3 }, { -1, 0 } };
        std::int32_t const indices[] = { 0, -1, 0, 1, 2, 4 };
        ParentChainTopology const chain = trace_parent_chain(ranges, indices, 0);

        assert(chain.status == ParentChainStatus::e_ok);
        assert(chain.node_count == 4 && chain.edge_count == 4 && chain.roots == 1 && chain.empty_slots == 1);
        assert(chain.path[0] && chain.path[1] && chain.path[2] && chain.path[3] && !chain.path[4]);
        std::uint32_t slots{};
        for (std::size_t i = 0; i < chain.edge_count; ++i)
        {
            ParentChainEdge const edge = chain.edges[i];
            assert(indices[edge.slot] == edge.child);
            slots |= 1u << edge.slot;
        }
        assert(slots == ((1u << 0) | (1u << 2) | (1u << 3) | (1u << 4)));
    }

    void parent_chain_rejects_invalid_topology()
    {
        ParentChainChildRange ranges[] = { { 1, 1 }, { 0, 1 }, { 2, 2 } };
        std::int32_t const indices[] = { 0, 1, 0 };
        ParentChainTopology const invalid_range = trace_parent_chain(ranges, indices, 0);

        assert(invalid_range.status == ParentChainStatus::e_invalid_range);
        assert(invalid_range.node_count == 0 && invalid_range.edge_count == 0);

        // Fix the range, leaving a cycle between 0 and 1 and a valid root at 2.
        ranges[2].count = 1;
        ParentChainTopology const cycle = trace_parent_chain(ranges, indices, 0);
        assert(cycle.status == ParentChainStatus::e_cycle);
        assert(cycle.node_count == 3 && cycle.edge_count == 3 && cycle.roots == 1);
    }
    void clip_clock_offsets_are_the_audited_values()
    {
        assert(Clip_Local_Time_Offset == 0xF0);
        assert(Clip_Time_Offset == 0xF4);
        assert(Clip_Crop_Start_Offset == 0x58);
        assert(Clip_Crop_End_Offset == 0x5C);
        assert(Clip_Playback_Speed_Offset == 0x64);
        assert(Clip_Mode_Offset == 0x72);
        assert(Clip_Control_Offset == 0x88);
        assert(Clip_At_End_Offset == 0x104);
        assert(Control_Local_Time_Offset == 0x10);
        assert(Control_Weight_Offset == 0x14);
        assert(Control_Track_Weights_Size_Offset == 0x20);
        assert(Control_Playback_Speed_Offset == 0x5C);
        assert(Control_Crop_Start_Offset == 0x9C);
        assert(Control_Crop_End_Offset == 0xA0);
        assert(Clip_Mode_User_Controlled == 2);
        assert(clip_clock_mode_is_user_controlled(2));
        assert(!clip_clock_mode_is_user_controlled(0));
        assert(!clip_clock_mode_is_user_controlled(3));
    }

    void clip_clock_classifies_advance_and_freeze()
    {
        // No previous sample: the classification stays unknown.
        assert(clip_clock_advance(0x3E555556u, 0x3E555556u, false, false) ==
            ClipClockAdvance::e_unknown);

        // Time moved: advancing regardless of the at-end flag.
        assert(clip_clock_advance(0x3C888889u, 0x3E555556u, false, true) ==
            ClipClockAdvance::e_advancing);

        // Frozen at end: Update skips the localTime write (0xACDF8A).
        assert(clip_clock_advance(0x3E555556u, 0x3E555556u, true, true) ==
            ClipClockAdvance::e_frozen_at_end);

        // Frozen without at-end: the clip saw no timestep.
        assert(clip_clock_advance(0x3E555556u, 0x3E555556u, false, true) ==
            ClipClockAdvance::e_frozen_no_step);

        assert(std::strcmp(clip_clock_advance_name(ClipClockAdvance::e_unknown), "unknown") == 0);
        assert(std::strcmp(clip_clock_advance_name(ClipClockAdvance::e_advancing), "advancing") == 0);
        assert(std::strcmp(clip_clock_advance_name(ClipClockAdvance::e_frozen_at_end), "frozen-at-end") == 0);
        assert(std::strcmp(clip_clock_advance_name(ClipClockAdvance::e_frozen_no_step), "frozen-no-step") == 0);
    }
}

int main()
{
    capacities_and_budget_are_the_registered_values();
    names_are_truncated_and_terminated();
    names_are_stable();
    overflow_mask_covers_each_structural_cap();
    zero_record_is_empty();
    subgraph_bounds_are_the_registered_values();
    subgraph_window_is_bounded();
    subgraph_status_names_are_stable();
    parent_chain_retains_shared_parents_and_skips_empty_slots();
    parent_chain_rejects_invalid_topology();
    clip_clock_offsets_are_the_audited_values();
    clip_clock_classifies_advance_and_freeze();
    return 0;
}
