//
// C-G5-TRANS v1 (2026-11-18): read-only transition/blender state probe.
//
// All offsets are the AE 1.6.1170 values registered in the pre-registration:
// StateMachine::activeTransitions +0xB0 with a 0x28 element stride,
// BlenderGenerator::children +0x60 / internal states +0x70, and the
// hkbBlendingTransitionEffect timing fields +0x80/+0x84.
//
#include "animation/behavior_graph/transition_probe.h"

#include "animation/behavior_graph/observation_chain_policy.h"

#include "RE/B/BShkbAnimationGraph.h"
#include "RE/Offsets_VTABLE.h"
#include "RE/H/hkbBehaviorGraph.h"
#include "RE/H/hkbBlendingTransitionEffect.h"
#include "RE/H/hkbCharacter.h"
#include "RE/H/hkbNode.h"
#include "RE/H/hkbStateMachine.h"

#include <algorithm>
#include <bit>
#include <cstring>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    namespace
    {
        constexpr std::size_t State_Machine_Active_Transitions_Offset = 0xB0;
        constexpr std::size_t State_Machine_Active_Transitions_Stride = 0x28;
        constexpr std::size_t Blender_Blend_Parameter_Offset = 0x4C;
        constexpr std::size_t Blender_Children_Offset = 0x60;
        constexpr std::size_t Blender_Internal_States_Offset = 0x70;
        constexpr std::size_t Blender_End_Interval_Weight_Offset = 0x90;
        constexpr std::size_t Blender_Num_Active_Children_Offset = 0x94;
        constexpr std::size_t Blender_Begin_Interval_Index_Offset = 0x98;
        constexpr std::size_t Blender_End_Interval_Index_Offset = 0x9A;
        constexpr std::size_t Blender_Child_Generator_Offset = 0x30;
        constexpr std::size_t Blender_Child_Weight_Offset = 0x40;
        constexpr std::size_t Blender_Child_World_From_Model_Offset = 0x44;

        template <class T>
        T read_field(void const* a_base, std::size_t a_offset) noexcept
        {
            T value{};
            std::memcpy(&value, static_cast<std::byte const*>(a_base) + a_offset, sizeof(T));
            return value;
        }

        std::string_view read_node_name(RE::hkbNode const* a_node) noexcept
        {
            if (a_node == nullptr || !plausible_object_pointer(reinterpret_cast<std::uintptr_t>(a_node)))
                return {};
            char const* const raw = a_node->name.c_str();
            if (raw == nullptr || !plausible_object_pointer(reinterpret_cast<std::uintptr_t>(raw)))
                return {};
            std::size_t length = 0;
            while (length < Transition_Probe_Name_Length && raw[length] != '\0')
                ++length;
            return { raw, length };
        }

        TransitionProbeNodeClass classify(std::uintptr_t a_vtable) noexcept
        {
            if (a_vtable == RE::VTABLE_hkbClipGenerator[0].address())
                return TransitionProbeNodeClass::clip;
            if (a_vtable == RE::VTABLE_hkbStateMachine[0].address())
                return TransitionProbeNodeClass::state_machine;
            if (a_vtable == RE::VTABLE_hkbBlenderGenerator[0].address())
                return TransitionProbeNodeClass::blender;
            if (a_vtable == RE::VTABLE_hkbManualSelectorGenerator[0].address())
                return TransitionProbeNodeClass::manual_selector;
            if (a_vtable == RE::VTABLE_hkbBlendingTransitionEffect[0].address())
                return TransitionProbeNodeClass::blending_transition;
            if (a_vtable == RE::VTABLE_hkbGeneratorTransitionEffect[0].address())
                return TransitionProbeNodeClass::generator_transition;
            return TransitionProbeNodeClass::unknown;
        }

        std::uint16_t capped_count(std::int32_t a_count) noexcept
        {
            if (a_count <= 0)
                return 0;
            return a_count > 0xFFFF ? 0xFFFF : static_cast<std::uint16_t>(a_count);
        }

        void capture_state_machine(RE::hkbNode* a_node, std::uintptr_t a_vtable,
            TransitionProbeRecord& a_record) noexcept
        {
            if (a_record.state_machine_count >= Transition_Probe_State_Machine_Capacity)
            {
                a_record.flags |= Transition_Probe_Flag_State_Machine_Overflow;
                return;
            }
            std::uint8_t const state_machine_index =
                static_cast<std::uint8_t>(a_record.state_machine_count++);
            TransitionProbeStateMachine& out = a_record.state_machines[state_machine_index];
            out.node = reinterpret_cast<std::uintptr_t>(a_node);
            out.vtable = a_vtable;
            std::string_view const name = read_node_name(a_node);
            out.name_length = static_cast<std::uint8_t>(std::min<std::size_t>(name.size(),
                Transition_Probe_Name_Length - 1));
            transition_probe_copy_name(out.name, name);

            auto const* const state_machine = static_cast<RE::hkbStateMachine const*>(a_node);
            out.current_state = state_machine->currentStateID;

            std::int32_t const transition_count = read_field<std::int32_t>(a_node,
                State_Machine_Active_Transitions_Offset + 0x08);
            out.transition_count = capped_count(transition_count);
            if (transition_count <= 0)
                return;

            std::uintptr_t const data = read_field<std::uintptr_t>(a_node,
                State_Machine_Active_Transitions_Offset);
            if (!plausible_object_pointer(data))
            {
                a_record.flags |= Transition_Probe_Flag_Unavailable;
                return;
            }

            std::size_t const limit = std::min<std::size_t>(static_cast<std::size_t>(transition_count),
                Transition_Probe_Effect_Capacity);
            if (static_cast<std::size_t>(transition_count) > Transition_Probe_Effect_Capacity)
                a_record.flags |= Transition_Probe_Flag_Effect_Overflow;
            for (std::size_t i = 0; i < limit; ++i)
            {
                if (a_record.effect_count >= Transition_Probe_Effect_Capacity)
                {
                    a_record.flags |= Transition_Probe_Flag_Effect_Overflow;
                    break;
                }
                TransitionProbeEffect& effect_out = a_record.effects[a_record.effect_count++];
                effect_out.state_machine_index = state_machine_index;
                std::uintptr_t const effect = read_field<std::uintptr_t>(
                    reinterpret_cast<void const*>(data), i * State_Machine_Active_Transitions_Stride);
                effect_out.effect = effect;
                if (!plausible_object_pointer(effect))
                {
                    effect_out.class_id = static_cast<std::uint8_t>(TransitionProbeNodeClass::unknown);
                    continue;
                }
                std::uintptr_t const vtable = read_field<std::uintptr_t>(
                    reinterpret_cast<void const*>(effect), 0);
                effect_out.vtable = vtable;
                TransitionProbeNodeClass const effect_class = classify(vtable);
                effect_out.class_id = static_cast<std::uint8_t>(effect_class);
                if (effect_class != TransitionProbeNodeClass::blending_transition)
                    continue;
                auto const* const blending =
                    static_cast<RE::hkbBlendingTransitionEffect const*>(reinterpret_cast<void const*>(effect));
                effect_out.duration_bits = std::bit_cast<std::uint32_t>(blending->duration);
                effect_out.start_fraction_bits =
                    std::bit_cast<std::uint32_t>(blending->toGeneratorStartTimeFraction);
                effect_out.time_remaining_bits =
                    std::bit_cast<std::uint32_t>(blending->timeRemaining);
                effect_out.time_in_transition_bits =
                    std::bit_cast<std::uint32_t>(blending->timeInTransition);
                effect_out.flags = blending->flags;
                effect_out.blend_curve = blending->blendCurve;
                effect_out.end_mode = blending->endMode;
                effect_out.self_mode = blending->selfTransitionMode;
                effect_out.event_mode = blending->eventMode;
            }
        }

        void capture_blender(RE::hkbNode* a_node, std::uintptr_t a_vtable,
            TransitionProbeRecord& a_record) noexcept
        {
            if (a_record.blender_count >= Transition_Probe_Blender_Capacity)
            {
                a_record.flags |= Transition_Probe_Flag_Blender_Overflow;
                return;
            }
            TransitionProbeBlender& out = a_record.blenders[a_record.blender_count++];
            out.node = reinterpret_cast<std::uintptr_t>(a_node);
            out.vtable = a_vtable;
            std::string_view const name = read_node_name(a_node);
            out.name_length = static_cast<std::uint8_t>(std::min<std::size_t>(name.size(),
                Transition_Probe_Name_Length - 1));
            transition_probe_copy_name(out.name, name);

            out.blend_parameter_bits = read_field<std::uint32_t>(a_node, Blender_Blend_Parameter_Offset);
            out.end_interval_weight_bits = read_field<std::uint32_t>(a_node,
                Blender_End_Interval_Weight_Offset);
            out.num_active_children = read_field<std::int32_t>(a_node,
                Blender_Num_Active_Children_Offset);
            out.begin_interval_index = read_field<std::int16_t>(a_node,
                Blender_Begin_Interval_Index_Offset);
            out.end_interval_index = read_field<std::int16_t>(a_node,
                Blender_End_Interval_Index_Offset);

            std::int32_t const children_count = read_field<std::int32_t>(a_node,
                Blender_Children_Offset + 0x08);
            out.children_count = capped_count(children_count);
            if (children_count > static_cast<std::int32_t>(Transition_Probe_Child_Capacity))
            {
                a_record.flags |= Transition_Probe_Flag_Child_Overflow;
                out.overflow = 1;
            }
            if (children_count > 0)
            {
                std::uintptr_t const children = read_field<std::uintptr_t>(a_node, Blender_Children_Offset);
                if (!plausible_object_pointer(children))
                {
                    a_record.flags |= Transition_Probe_Flag_Unavailable;
                }
                else
                {
                    std::size_t const limit = std::min<std::size_t>(
                        static_cast<std::size_t>(children_count), Transition_Probe_Child_Capacity);
                    for (std::size_t i = 0; i < limit; ++i)
                    {
                        TransitionProbeChild& child_out = out.children[i];
                        std::uintptr_t const child = read_field<std::uintptr_t>(
                            reinterpret_cast<void const*>(children), i * sizeof(std::uintptr_t));
                        if (!plausible_object_pointer(child))
                        {
                            child_out.class_id =
                                static_cast<std::uint8_t>(TransitionProbeNodeClass::unknown);
                            continue;
                        }
                        child_out.generator = read_field<std::uintptr_t>(
                            reinterpret_cast<void const*>(child), Blender_Child_Generator_Offset);
                        child_out.weight_bits = read_field<std::uint32_t>(
                            reinterpret_cast<void const*>(child), Blender_Child_Weight_Offset);
                        child_out.world_from_model_bits = read_field<std::uint32_t>(
                            reinterpret_cast<void const*>(child), Blender_Child_World_From_Model_Offset);
                        if (!plausible_object_pointer(child_out.generator))
                        {
                            child_out.class_id =
                                static_cast<std::uint8_t>(TransitionProbeNodeClass::unknown);
                            continue;
                        }
                        child_out.vtable = read_field<std::uintptr_t>(
                            reinterpret_cast<void const*>(child_out.generator), 0);
                        child_out.class_id = static_cast<std::uint8_t>(classify(child_out.vtable));
                    }
                }
            }

            std::int32_t const internal_count = read_field<std::int32_t>(a_node,
                Blender_Internal_States_Offset + 0x08);
            out.internal_count = capped_count(internal_count);
            if (internal_count > static_cast<std::int32_t>(Transition_Probe_Internal_Capacity))
            {
                a_record.flags |= Transition_Probe_Flag_Internal_Overflow;
                out.overflow = 1;
            }
            if (internal_count > 0)
            {
                std::uintptr_t const internals = read_field<std::uintptr_t>(a_node,
                    Blender_Internal_States_Offset);
                if (!plausible_object_pointer(internals))
                {
                    a_record.flags |= Transition_Probe_Flag_Unavailable;
                }
                else
                {
                    std::size_t const limit = std::min<std::size_t>(
                        static_cast<std::size_t>(internal_count), Transition_Probe_Internal_Capacity);
                    for (std::size_t i = 0; i < limit; ++i)
                    {
                        out.internals[i] = read_field<std::uint16_t>(
                            reinterpret_cast<void const*>(internals), i * sizeof(std::uint16_t));
                    }
                }
            }
        }
    }

    void log_transition_census(RE::BShkbAnimationGraph const& a_graph, char const* a_segment,
        std::uint32_t a_commanded) noexcept
    {
        char const* const segment = a_segment ? a_segment : "";
        RE::hkbBehaviorGraph const* const behavior = a_graph.characterInstance.behaviorGraph.get();
        if (behavior == nullptr || behavior->activeNodes == nullptr)
        {
            logger::info("TRANSITION CENSUS side=candidate segment={} commanded={} status=unavailable",
                segment, a_commanded);
            return;
        }
        RE::NodeList const& nodes = *behavior->activeNodes;
        std::size_t const limit = std::min<std::size_t>(nodes.size(), 256);
        std::size_t unknown = 0;
        for (std::size_t i = 0; i < limit; ++i)
        {
            RE::hkbNode* const node = nodes.data()[i].nodeClone;
            if (node == nullptr || !plausible_object_pointer(reinterpret_cast<std::uintptr_t>(node)))
                continue;
            std::uintptr_t const vtable = read_field<std::uintptr_t>(node, 0);
            TransitionProbeNodeClass const node_class = classify(vtable);
            if (node_class == TransitionProbeNodeClass::unknown)
                ++unknown;
            logger::info(
                "TRANSITION CENSUS side=candidate segment={} commanded={} index={} node={:#x} vtable={:#x} class={} name='{}' id={}",
                segment, a_commanded, i, reinterpret_cast<std::uintptr_t>(node), vtable,
                transition_probe_class_name(node_class), read_node_name(node),
                static_cast<std::uint32_t>(static_cast<RE::hkbNode const*>(node)->id));
        }
        logger::info("TRANSITION CENSUS SUMMARY side=candidate segment={} commanded={} walked={} active-nodes={} unknown={}",
            segment, a_commanded, limit, nodes.size(), unknown);
    }

    void capture_transition_probe(RE::BShkbAnimationGraph const& a_graph,
        TransitionProbeSide a_side, TransitionProbeSegment a_segment,
        TransitionProbePhase a_phase, std::uint32_t a_sequence,
        std::uint32_t a_commanded, std::uint32_t a_probe_seq, std::uint64_t a_tsc,
        TransitionProbeRecord& a_out) noexcept
    {
        a_out = {};
        a_out.magic = Transition_Probe_Magic;
        a_out.version = Transition_Probe_Version;
        a_out.side = static_cast<std::uint8_t>(a_side);
        a_out.segment = static_cast<std::uint8_t>(a_segment);
        a_out.phase = static_cast<std::uint8_t>(a_phase);
        a_out.sequence = a_sequence;
        a_out.commanded = a_commanded;
        a_out.probe_seq = a_probe_seq;
        a_out.tsc = a_tsc;
        a_out.graph = reinterpret_cast<std::uintptr_t>(&a_graph);

        RE::hkbBehaviorGraph const* const behavior = a_graph.characterInstance.behaviorGraph.get();
        if (behavior == nullptr || behavior->activeNodes == nullptr)
        {
            a_out.flags |= Transition_Probe_Flag_Unavailable;
            return;
        }
        RE::NodeList const& nodes = *behavior->activeNodes;
        std::size_t const node_count = nodes.size() > 0
            ? static_cast<std::size_t>(nodes.size()) : 0;
        a_out.active_nodes = node_count > 0xFFFFFFFFu
            ? 0xFFFFFFFFu : static_cast<std::uint32_t>(node_count);
        std::size_t const limit = std::min<std::size_t>(node_count, Transition_Probe_Node_Limit);
        for (std::size_t i = 0; i < limit; ++i)
        {
            RE::hkbNode* const node = nodes.data()[i].nodeClone;
            if (node == nullptr || !plausible_object_pointer(reinterpret_cast<std::uintptr_t>(node)))
                continue;
            ++a_out.walked;
            std::uintptr_t const vtable = read_field<std::uintptr_t>(node, 0);
            if (vtable == RE::VTABLE_hkbStateMachine[0].address())
                capture_state_machine(node, vtable, a_out);
            else if (vtable == RE::VTABLE_hkbBlenderGenerator[0].address())
                capture_blender(node, vtable, a_out);
        }
    }

    void log_transition_probe(TransitionProbeRecord const& a_record) noexcept
    {
        char const* const side = transition_probe_side_name(
            static_cast<TransitionProbeSide>(a_record.side));
        char const* const segment = transition_probe_segment_name(
            static_cast<TransitionProbeSegment>(a_record.segment));
        char const* const phase = transition_probe_phase_name(
            static_cast<TransitionProbePhase>(a_record.phase));
        logger::info(
            "TRANSITION PROBE side={} graph={:#x} actor={:08x} request={} segment={} phase={} seq={} commanded={} probe-seq={} tsc={} walked={} active-nodes={} state-machines={} effects={} blenders={} flags={:#06x}",
            side, a_record.graph, a_record.actor, a_record.request, segment, phase,
            a_record.sequence, a_record.commanded, a_record.probe_seq, a_record.tsc,
            a_record.walked, a_record.active_nodes, a_record.state_machine_count,
            a_record.effect_count, a_record.blender_count, a_record.flags);

        std::size_t const state_machine_count = std::min<std::size_t>(a_record.state_machine_count,
            Transition_Probe_State_Machine_Capacity);
        for (std::size_t i = 0; i < state_machine_count; ++i)
        {
            TransitionProbeStateMachine const& state_machine = a_record.state_machines[i];
            logger::info(
                "TRANSITION PROBE SM side={} phase={} seq={} index={} node={:#x} vtable={:#x} name='{}' state={} transitions={} overflow={}",
                side, phase, a_record.sequence, i, state_machine.node, state_machine.vtable,
                std::string_view{ state_machine.name, state_machine.name_length },
                state_machine.current_state, state_machine.transition_count, state_machine.overflow);
        }

        std::size_t const effect_count = std::min<std::size_t>(a_record.effect_count,
            Transition_Probe_Effect_Capacity);
        for (std::size_t i = 0; i < effect_count; ++i)
        {
            TransitionProbeEffect const& effect = a_record.effects[i];
            logger::info(
                "TRANSITION PROBE EFFECT side={} phase={} seq={} sm-index={} index={} effect={:#x} vtable={:#x} class={} duration={:.6f} duration-bits={:08X} time-in={:.6f} time-in-bits={:08X} time-remaining={:.6f} time-remaining-bits={:08X} blend-curve={} end-mode={} flags={} self={} event={}",
                side, phase, a_record.sequence, effect.state_machine_index, i, effect.effect, effect.vtable,
                transition_probe_class_name(static_cast<TransitionProbeNodeClass>(effect.class_id)),
                std::bit_cast<float>(effect.duration_bits), effect.duration_bits,
                std::bit_cast<float>(effect.time_in_transition_bits), effect.time_in_transition_bits,
                std::bit_cast<float>(effect.time_remaining_bits), effect.time_remaining_bits,
                effect.blend_curve, effect.end_mode, effect.flags, effect.self_mode, effect.event_mode);
        }

        std::size_t const blender_count = std::min<std::size_t>(a_record.blender_count,
            Transition_Probe_Blender_Capacity);
        for (std::size_t i = 0; i < blender_count; ++i)
        {
            TransitionProbeBlender const& blender = a_record.blenders[i];
            logger::info(
                "TRANSITION PROBE BLENDER side={} phase={} seq={} index={} node={:#x} vtable={:#x} name='{}' blend-param={:.6f} blend-param-bits={:08X} end-weight={:.6f} end-weight-bits={:08X} active-children={} begin={} end={} children={} internals={} overflow={}",
                side, phase, a_record.sequence, i, blender.node, blender.vtable,
                std::string_view{ blender.name, blender.name_length },
                std::bit_cast<float>(blender.blend_parameter_bits), blender.blend_parameter_bits,
                std::bit_cast<float>(blender.end_interval_weight_bits), blender.end_interval_weight_bits,
                blender.num_active_children, blender.begin_interval_index, blender.end_interval_index,
                blender.children_count, blender.internal_count, blender.overflow);

            std::size_t const child_count = std::min<std::size_t>(blender.children_count,
                Transition_Probe_Child_Capacity);
            for (std::size_t child_index = 0; child_index < child_count; ++child_index)
            {
                TransitionProbeChild const& child = blender.children[child_index];
                logger::info(
                    "TRANSITION PROBE CHILD side={} phase={} seq={} blender-index={} index={} generator={:#x} vtable={:#x} class={} weight={:.6f} weight-bits={:08X} wfm={:.6f} wfm-bits={:08X}",
                    side, phase, a_record.sequence, i, child_index, child.generator, child.vtable,
                    transition_probe_class_name(static_cast<TransitionProbeNodeClass>(child.class_id)),
                    std::bit_cast<float>(child.weight_bits), child.weight_bits,
                    std::bit_cast<float>(child.world_from_model_bits), child.world_from_model_bits);
            }
            if (blender.internal_count > 0)
            {
                logger::info(
                    "TRANSITION PROBE INTERNAL side={} phase={} seq={} blender-index={} count={} values='{:04X},{:04X},{:04X},{:04X}'",
                    side, phase, a_record.sequence, i, blender.internal_count,
                    blender.internals[0], blender.internals[1], blender.internals[2],
                    blender.internals[3]);
            }
        }
    }
}
PLUGIN_NAMESPACE_END
