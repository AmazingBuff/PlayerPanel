//
// C-G5-TRANS v1 engine-free policy cases (2026-11-18): fixed capacities and budget,
// name truncation, class/side/segment names and the overflow mask.
//
// Build:
//   cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests\transition_probe_test.cpp
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
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
        assert(Transition_Probe_State_Machine_Capacity == 16);
        assert(Transition_Probe_Effect_Capacity == 16);
        assert(Transition_Probe_Blender_Capacity == 8);
        assert(Transition_Probe_Child_Capacity == 8);
        assert(Transition_Probe_Internal_Capacity == 8);
        assert(Transition_Probe_Source_Capacity == 2560);
        assert(Transition_Probe_Record_Budget == 8192);
        assert(Transition_Probe_Version == 2);
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
}

int main()
{
    capacities_and_budget_are_the_registered_values();
    names_are_truncated_and_terminated();
    names_are_stable();
    overflow_mask_covers_each_structural_cap();
    zero_record_is_empty();
    return 0;
}
