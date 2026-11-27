//
// C-G5-TRANS v1 (2026-11-18): read-only transition/blender state probe.
//
// All offsets are the AE 1.6.1170 values registered in the pre-registration:
// StateMachine::activeTransitions +0xB0 with a 0x28 element stride,
// BlenderGenerator::children +0x60 / internal states +0x70, and the
// hkbBlendingTransitionEffect timing fields +0x80/+0x84.
//
#include "animation/behavior_graph/transition_probe.h"

#include "animation/behavior_graph/clip_reference_policy.h"
#include "animation/behavior_graph/idle_chain_probe_policy.h"
#include "animation/behavior_graph/observation_chain_policy.h"
#include "animation/behavior_graph/pose_capture_policy.h"
#include "animation/behavior_graph/transition_parent_chain_policy.h"
#include "animation/behavior_graph/k5_snapshot_policy.h"
#include "animation/clip.h"

#include "RE/B/BShkbAnimationGraph.h"
#include "RE/Offsets_VTABLE.h"
#include "RE/H/hkaAnimationBinding.h"
#include "RE/H/hkaDefaultAnimationControl.h"
#include "RE/H/hkaSkeleton.h"
#include "RE/H/hkbBehaviorGraph.h"
#include "RE/H/hkbBehaviorGraphData.h"
#include "RE/H/hkbBehaviorGraphStringData.h"
#include "RE/H/hkbBlendingTransitionEffect.h"
#include "RE/H/hkbCharacter.h"
#include "RE/H/hkbNode.h"
#include "RE/H/hkbStateMachine.h"
#include "RE/H/hkbVariableValueSet.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#include <Windows.h>

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

        // C-G5-TRANS v5 (AE 1.6.1170 offline layout audit, 2026-11-19):
        // hkbBehaviorReferenceGenerator is 0x58 bytes; +0x48 is behaviorName and
        // +0x50 is the referenced hkbBehaviorGraph. hkbBehaviorGraph::activeNodes
        // is +0x98 and hkbNodeInfo is 0x90 bytes with nodeClone at +0x58.
        constexpr std::size_t Behavior_Reference_Behavior_Name_Offset = 0x48;
        constexpr std::size_t Behavior_Reference_Behavior_Offset = 0x50;
        constexpr std::size_t Behavior_Graph_Active_Nodes_Offset = 0x98;
        constexpr std::size_t Behavior_Graph_Is_Active_Offset = 0x12C;
        constexpr std::size_t Node_Info_Size = 0x90;
        constexpr std::size_t Node_Info_Node_Clone_Offset = 0x58;
        constexpr std::size_t Node_Info_Behavior_Offset = 0x60;

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

        std::string_view read_string_ptr_field(void const* a_base, std::size_t a_offset) noexcept
        {
            // hkStringPtr stores the managed flag in bit 0; the rest is the char pointer.
            std::uintptr_t const raw = read_field<std::uintptr_t>(a_base, a_offset) &
                ~std::uintptr_t{ 1 };
            if (!plausible_object_pointer(raw))
                return {};
            char const* const text = reinterpret_cast<char const*>(raw);
            std::size_t length = 0;
            while (length < Transition_Probe_Name_Length && text[length] != '\0')
                ++length;
            return { text, length };
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
            if (a_vtable == RE::VTABLE_hkbBehaviorReferenceGenerator[0].address())
                return TransitionProbeNodeClass::behavior_reference;
            if (a_vtable == RE::VTABLE_hkbModifierGenerator[0].address())
                return TransitionProbeNodeClass::modifier_generator;
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

        std::string hex_head(void const* a_data, std::size_t a_length) noexcept
        {
            static char const digits[] = "0123456789ABCDEF";
            std::string out;
            out.reserve(a_length * 2);
            auto const* const bytes = static_cast<std::byte const*>(a_data);
            for (std::size_t i = 0; i < a_length; ++i)
            {
                std::uint8_t const value = static_cast<std::uint8_t>(bytes[i]);
                out.push_back(digits[value >> 4]);
                out.push_back(digits[value & 0x0F]);
            }
            return out;
        }

        void log_transition_aux(TransitionProbeStateMachine const& a_state_machine,
            char const* a_side, char const* a_segment, char const* a_phase,
            std::uint32_t a_sequence, std::size_t a_index) noexcept
        {
            if (std::strcmp(a_state_machine.name, "Default_Behavior") != 0)
                return;
            auto const node = reinterpret_cast<void const*>(a_state_machine.node);
            if (node == nullptr || !plausible_object_pointer(a_state_machine.node))
                return;
            struct AuxArray
            {
                char const* kind;
                std::size_t offset;
            };
            for (AuxArray const aux : { AuxArray{ "transition-flags", 0xC0 },
                     AuxArray{ "wildcard-flags", 0xD0 }, AuxArray{ "delayed", 0xE0 } })
            {
                std::uintptr_t const data = read_field<std::uintptr_t>(node, aux.offset);
                std::int32_t const size = read_field<std::int32_t>(node, aux.offset + 0x08);
                std::int32_t const capacity = read_field<std::int32_t>(node, aux.offset + 0x0C);
                if (size <= 0 || !plausible_object_pointer(data))
                {
                    logger::info(
                        "TRANSITION AUX side={} segment={} phase={} seq={} sm-index={} kind={} node={:#x} data={:#x} size={} capacity={} head=''",
                        a_side, a_segment, a_phase, a_sequence, a_index, aux.kind,
                        a_state_machine.node, data, size, capacity);
                    continue;
                }
                std::size_t const length = std::min<std::size_t>(
                    static_cast<std::size_t>(size), 0x10);
                logger::info(
                    "TRANSITION AUX side={} segment={} phase={} seq={} sm-index={} kind={} node={:#x} data={:#x} size={} capacity={} head='{}'",
                    a_side, a_segment, a_phase, a_sequence, a_index, aux.kind,
                    a_state_machine.node, data, size, capacity,
                    hex_head(reinterpret_cast<void const*>(data), length));
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

        struct SubgraphLogContext
        {
            char const* segment;
            char const* phase;
            std::uint32_t sequence;
            std::uint32_t commanded;
            std::size_t bfr_index;
            std::size_t sm_logged{ 0 };
            std::size_t blender_logged{ 0 };
            bool overflow{ false };
        };

        void log_subgraph_state_machine(SubgraphLogContext& a_context, std::uintptr_t a_node,
            std::size_t a_index) noexcept
        {
            std::int32_t const current_state = read_field<std::int32_t>(
                reinterpret_cast<void const*>(a_node), 0x80);
            std::int32_t const transition_count = read_field<std::int32_t>(
                reinterpret_cast<void const*>(a_node), 0xB0 + 0x08);
            logger::info(
                "TRANSITION SUBGRAPH SM side=candidate segment={} phase={} seq={} commanded={} bfr-index={} index={} node={:#x} name='{}' current-state={} transitions={}",
                a_context.segment, a_context.phase, a_context.sequence, a_context.commanded,
                a_context.bfr_index, a_index, a_node,
                read_node_name(reinterpret_cast<RE::hkbNode const*>(a_node)), current_state,
                transition_count > 0 ? transition_count : 0);
        }

        void log_subgraph_blender(SubgraphLogContext& a_context, std::uintptr_t a_node,
            std::size_t a_index) noexcept
        {
            std::uint32_t const blend_parameter = read_field<std::uint32_t>(
                reinterpret_cast<void const*>(a_node),
                Blender_Blend_Parameter_Offset);
            std::uint32_t const end_weight = read_field<std::uint32_t>(
                reinterpret_cast<void const*>(a_node),
                Blender_End_Interval_Weight_Offset);
            std::int32_t const num_active = read_field<std::int32_t>(
                reinterpret_cast<void const*>(a_node),
                Blender_Num_Active_Children_Offset);
            std::int16_t const begin = read_field<std::int16_t>(
                reinterpret_cast<void const*>(a_node),
                Blender_Begin_Interval_Index_Offset);
            std::int16_t const end = read_field<std::int16_t>(
                reinterpret_cast<void const*>(a_node),
                Blender_End_Interval_Index_Offset);
            std::uintptr_t const children = read_field<std::uintptr_t>(
                reinterpret_cast<void const*>(a_node),
                Blender_Children_Offset);
            std::int32_t const child_count = read_field<std::int32_t>(
                reinterpret_cast<void const*>(a_node),
                Blender_Children_Offset + 0x08);
            logger::info(
                "TRANSITION SUBGRAPH BLEND side=candidate segment={} phase={} seq={} commanded={} bfr-index={} index={} node={:#x} name='{}' blend-param={} blend-param-f={:.6f} end-weight={} end-weight-f={:.6f} num-active={} begin={} end={} children={}",
                a_context.segment, a_context.phase, a_context.sequence, a_context.commanded,
                a_context.bfr_index, a_index, a_node,
                read_node_name(reinterpret_cast<RE::hkbNode const*>(a_node)),
                blend_parameter, std::bit_cast<float>(blend_parameter),
                end_weight, std::bit_cast<float>(end_weight), num_active, begin, end,
                child_count > 0 ? child_count : 0);
            if (child_count <= 0 || !plausible_object_pointer(children))
                return;
            std::size_t const limit = std::min<std::size_t>(
                static_cast<std::size_t>(child_count), Transition_Subgraph_Child_Capacity);
            if (static_cast<std::size_t>(child_count) > Transition_Subgraph_Child_Capacity)
                a_context.overflow = true;
            for (std::size_t i = 0; i < limit; ++i)
            {
                std::uintptr_t const child = read_field<std::uintptr_t>(
                    reinterpret_cast<void const*>(children), i * sizeof(std::uintptr_t));
                if (!plausible_object_pointer(child))
                    continue;
                std::uintptr_t const generator = read_field<std::uintptr_t>(
                    reinterpret_cast<void const*>(child),
                    Blender_Child_Generator_Offset);
                std::uint32_t const weight = read_field<std::uint32_t>(
                    reinterpret_cast<void const*>(child),
                    Blender_Child_Weight_Offset);
                std::uint32_t const world_from_model = read_field<std::uint32_t>(
                    reinterpret_cast<void const*>(child),
                    Blender_Child_World_From_Model_Offset);
                std::uintptr_t const generator_vtable = plausible_object_pointer(generator)
                    ? read_field<std::uintptr_t>(reinterpret_cast<void const*>(generator), 0) : 0;
                logger::info(
                    "TRANSITION SUBGRAPH CHILD side=candidate segment={} phase={} seq={} commanded={} bfr-index={} blend-index={} child={} generator={:#x} vtable={:#x} class={} weight={} weight-f={:.6f} wfm={} wfm-f={:.6f}",
                    a_context.segment, a_context.phase, a_context.sequence, a_context.commanded,
                    a_context.bfr_index, a_index, i, generator, generator_vtable,
                    transition_probe_class_name(classify(generator_vtable)),
                    weight, std::bit_cast<float>(weight), world_from_model,
                    std::bit_cast<float>(world_from_model));
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

    void log_transition_modifiers(RE::BShkbAnimationGraph const& a_graph, char const* a_segment,
        TransitionProbePhase a_phase, std::uint32_t a_sequence,
        std::uint32_t a_commanded) noexcept
    {
        char const* const segment = a_segment ? a_segment : "";
        char const* const phase = transition_probe_phase_name(a_phase);
        RE::hkbBehaviorGraph const* const behavior = a_graph.characterInstance.behaviorGraph.get();
        if (behavior == nullptr || behavior->activeNodes == nullptr)
        {
            logger::info("TRANSITION MODIFIER SUMMARY side=candidate segment={} phase={} seq={} commanded={} status=unavailable",
                segment, phase, a_sequence, a_commanded);
            return;
        }

        auto const* const data = behavior->data.get();
        auto const* const strings = data ? data->stringData.get() : nullptr;
        auto const* const values = behavior->variableValueSet.get();
        std::uintptr_t const words = values ? read_field<std::uintptr_t>(values, 0x10) : 0;
        std::int32_t const word_count = values ? read_field<std::int32_t>(values, 0x18) : 0;
        std::uintptr_t const infos = data ? read_field<std::uintptr_t>(data, 0x20) : 0;
        std::int32_t const info_count = data ? read_field<std::int32_t>(data, 0x28) : 0;

        auto variable_name = [&](std::int32_t a_index) -> std::string_view
        {
            if (strings == nullptr)
                return {};
            auto const& names = strings->variableNames;
            if (a_index < 0 || a_index >= names.size())
                return {};
            char const* const raw = names.data()[a_index].c_str();
            if (raw == nullptr || !plausible_object_pointer(reinterpret_cast<std::uintptr_t>(raw)))
                return {};
            std::size_t length = 0;
            while (length < 96 && raw[length] != '\0')
                ++length;
            return { raw, length };
        };

        RE::NodeList const& nodes = *behavior->activeNodes;
        std::size_t const limit = std::min<std::size_t>(nodes.size(), 512);
        std::size_t modifiers = 0;
        bool overflow = false;
        for (std::size_t i = 0; i < limit; ++i)
        {
            RE::hkbNode* const node = nodes.data()[i].nodeClone;
            if (node == nullptr || !plausible_object_pointer(reinterpret_cast<std::uintptr_t>(node)))
                continue;
            std::uintptr_t const vtable = read_field<std::uintptr_t>(node, 0);
            if (vtable != RE::VTABLE_hkbModifierGenerator[0].address())
                continue;
            if (modifiers >= 8)
            {
                overflow = true;
                break;
            }
            ++modifiers;
            std::string_view const generator_name = read_node_name(node);
            std::uintptr_t const modifier = read_field<std::uintptr_t>(node, 0x48);
            std::uintptr_t const generator = read_field<std::uintptr_t>(node, 0x50);
            std::uintptr_t modifier_vtable = 0;
            std::uintptr_t generator_vtable = 0;
            std::string_view modifier_name{};
            std::uint32_t modifier_id = 0;
            std::uint8_t modifier_enable = 0;
            std::uintptr_t binding_set = 0;
            if (plausible_object_pointer(modifier))
            {
                modifier_vtable = read_field<std::uintptr_t>(
                    reinterpret_cast<void const*>(modifier), 0);
                modifier_name = read_node_name(reinterpret_cast<RE::hkbNode const*>(modifier));
                modifier_id = read_field<std::uint16_t>(
                    reinterpret_cast<void const*>(modifier), 0x40);
                modifier_enable = read_field<std::uint8_t>(
                    reinterpret_cast<void const*>(modifier), 0x48);
                binding_set = read_field<std::uintptr_t>(
                    reinterpret_cast<void const*>(modifier), 0x10);
            }
            TransitionProbeNodeClass generator_class = TransitionProbeNodeClass::unknown;
            if (plausible_object_pointer(generator))
            {
                generator_vtable = read_field<std::uintptr_t>(
                    reinterpret_cast<void const*>(generator), 0);
                generator_class = classify(generator_vtable);
            }
            std::uintptr_t binding_data = 0;
            std::int32_t binding_count = 0;
            if (plausible_object_pointer(binding_set))
            {
                binding_data = read_field<std::uintptr_t>(
                    reinterpret_cast<void const*>(binding_set), 0x10);
                binding_count = read_field<std::int32_t>(
                    reinterpret_cast<void const*>(binding_set), 0x18);
            }
            logger::info(
                "TRANSITION MODIFIER side=candidate segment={} phase={} seq={} commanded={} index={} node={:#x} name='{}' modifier={:#x} modifier-vtable={:#x} modifier-name='{}' modifier-id={} enable={} generator={:#x} generator-vtable={:#x} generator-class={} bindings={} layout=hkx2",
                segment, phase, a_sequence, a_commanded, i, reinterpret_cast<std::uintptr_t>(node),
                generator_name, modifier, modifier_vtable, modifier_name, modifier_id,
                modifier_enable, generator, generator_vtable,
                transition_probe_class_name(generator_class), binding_count);
            if (!plausible_object_pointer(binding_set) || binding_count <= 0 ||
                !plausible_object_pointer(binding_data))
                continue;
            std::size_t const bindings = std::min<std::size_t>(
                static_cast<std::size_t>(binding_count), 8);
            for (std::size_t b = 0; b < bindings; ++b)
            {
                void const* const binding = reinterpret_cast<void const*>(binding_data + b * 0x28);
                std::int32_t const member_offset = read_field<std::int32_t>(binding, 0x10);
                std::int32_t const variable_index = read_field<std::int32_t>(binding, 0x1C);
                std::uint8_t const mode = read_field<std::uint8_t>(binding, 0x21);
                std::uint8_t const member_type = read_field<std::uint8_t>(binding, 0x22);
                std::uint32_t word = 0;
                if (words != 0 && variable_index >= 0 && variable_index < word_count &&
                    plausible_object_pointer(words))
                {
                    word = read_field<std::uint32_t>(
                        reinterpret_cast<void const*>(words), static_cast<std::size_t>(variable_index) * 4);
                }
                int8_t type = -1;
                if (infos != 0 && variable_index >= 0 && variable_index < info_count &&
                    plausible_object_pointer(infos))
                {
                    type = read_field<std::int8_t>(reinterpret_cast<void const*>(infos),
                        static_cast<std::size_t>(variable_index) * 6 + 4);
                }
                logger::info(
                    "TRANSITION MODIFIER BIND side=candidate segment={} phase={} seq={} commanded={} index={} binding={} member-offset={} var={} var-name='{}' type={} member-type={} mode={} word={} float={:.6f}",
                    segment, phase, a_sequence, a_commanded, i, b, member_offset, variable_index,
                    variable_name(variable_index), static_cast<int>(type),
                    static_cast<int>(member_type), static_cast<int>(mode), word,
                    std::bit_cast<float>(word));
            }
        }
        logger::info(
            "TRANSITION MODIFIER SUMMARY side=candidate segment={} phase={} seq={} commanded={} modifiers={} overflow={}",
            segment, phase, a_sequence, a_commanded, modifiers, overflow);
    }

    void log_transition_subgraph(RE::BShkbAnimationGraph const& a_graph,
        TransitionProbeSegment a_segment, TransitionProbePhase a_phase,
        std::uint32_t a_sequence, std::uint32_t a_commanded) noexcept
    {
        if (!transition_subgraph_window_matches(a_segment, a_commanded))
            return;

        char const* const segment = transition_probe_segment_name(a_segment);
        char const* const phase = transition_probe_phase_name(a_phase);
        RE::hkbBehaviorGraph const* const main_behavior =
            a_graph.characterInstance.behaviorGraph.get();
        if (main_behavior == nullptr || main_behavior->activeNodes == nullptr)
        {
            logger::info(
                "TRANSITION SUBGRAPH side=candidate segment={} phase={} seq={} commanded={} status=unavailable reason=no-main-graph",
                segment, phase, a_sequence, a_commanded);
            return;
        }

        RE::NodeList const& main_nodes = *main_behavior->activeNodes;
        std::size_t const limit = std::min<std::size_t>(main_nodes.size(),
            Transition_Probe_Node_Limit);
        std::size_t bfrs = 0;
        std::size_t behaviors = 0;
        std::size_t nodes = 0;
        std::size_t state_machines = 0;
        std::size_t blenders = 0;
        bool overflow = false;

        for (std::size_t i = 0; i < limit; ++i)
        {
            RE::hkbNode* const node = main_nodes.data()[i].nodeClone;
            if (node == nullptr || !plausible_object_pointer(
                    reinterpret_cast<std::uintptr_t>(node)))
                continue;
            std::uintptr_t const vtable = read_field<std::uintptr_t>(node, 0);
            if (vtable != RE::VTABLE_hkbBehaviorReferenceGenerator[0].address())
                continue;
            if (bfrs >= Transition_Subgraph_Bfr_Capacity)
            {
                overflow = true;
                break;
            }

            std::size_t const bfr_index = bfrs++;
            std::string_view const bfr_name = read_node_name(node);
            std::uint16_t const bfr_id = read_field<std::uint16_t>(node, 0x40);
            std::string_view const behavior_name = read_string_ptr_field(node,
                Behavior_Reference_Behavior_Name_Offset);
            std::uintptr_t const behavior = read_field<std::uintptr_t>(node,
                Behavior_Reference_Behavior_Offset);
            std::uintptr_t behavior_vtable = 0;
            std::uint8_t behavior_active = 0;
            std::uintptr_t active_nodes = 0;
            std::int32_t active_size = 0;
            std::int32_t active_capacity = 0;
            TransitionSubgraphStatus status = TransitionSubgraphStatus::ok;
            if (!plausible_object_pointer(behavior))
            {
                status = TransitionSubgraphStatus::unavailable;
            }
            else
            {
                behavior_vtable = read_field<std::uintptr_t>(
                    reinterpret_cast<void const*>(behavior), 0);
                if (behavior_vtable != RE::VTABLE_hkbBehaviorGraph[0].address())
                {
                    status = TransitionSubgraphStatus::unknown_behavior_class;
                }
                else
                {
                    ++behaviors;
                    behavior_active = read_field<std::uint8_t>(
                        reinterpret_cast<void const*>(behavior),
                        Behavior_Graph_Is_Active_Offset);
                    active_nodes = read_field<std::uintptr_t>(
                        reinterpret_cast<void const*>(behavior),
                        Behavior_Graph_Active_Nodes_Offset);
                    if (!plausible_object_pointer(active_nodes))
                    {
                        status = TransitionSubgraphStatus::no_active_nodes;
                    }
                    else
                    {
                        active_size = read_field<std::int32_t>(
                            reinterpret_cast<void const*>(active_nodes), 0x08);
                        active_capacity = read_field<std::int32_t>(
                            reinterpret_cast<void const*>(active_nodes), 0x0C);
                        if (active_size <= 0)
                            status = TransitionSubgraphStatus::no_active_nodes;
                    }
                }
            }

            logger::info(
                "TRANSITION SUBGRAPH side=candidate segment={} phase={} seq={} commanded={} index={} bfr={:#x} bfr-name='{}' bfr-id={} behavior-name='{}' behavior={:#x} behavior-vtable={:#x} behavior-active={} active-nodes={} active-capacity={} status={}",
                segment, phase, a_sequence, a_commanded, i,
                reinterpret_cast<std::uintptr_t>(node), bfr_name, bfr_id, behavior_name,
                behavior, behavior_vtable, static_cast<std::uint32_t>(behavior_active),
                active_size > 0 ? active_size : 0,
                active_capacity > 0 ? active_capacity : 0,
                transition_subgraph_status_name(status));
            if (status != TransitionSubgraphStatus::ok)
                continue;

            std::uintptr_t const data = read_field<std::uintptr_t>(
                reinterpret_cast<void const*>(active_nodes), 0x00);
            if (!plausible_object_pointer(data))
            {
                logger::info(
                    "TRANSITION SUBGRAPH side=candidate segment={} phase={} seq={} commanded={} index={} bfr={:#x} status=invalid-array",
                    segment, phase, a_sequence, a_commanded, i,
                    reinterpret_cast<std::uintptr_t>(node));
                continue;
            }
            std::size_t const node_limit = std::min<std::size_t>(
                static_cast<std::size_t>(active_size), Transition_Subgraph_Node_Capacity);
            if (static_cast<std::size_t>(active_size) > Transition_Subgraph_Node_Capacity)
                overflow = true;

            SubgraphLogContext context{ segment, phase, a_sequence, a_commanded, bfr_index };
            for (std::size_t n = 0; n < node_limit; ++n)
            {
                std::uintptr_t const info = data + n * Node_Info_Size;
                std::uintptr_t const clone = read_field<std::uintptr_t>(
                    reinterpret_cast<void const*>(info),
                    Node_Info_Node_Clone_Offset);
                if (!plausible_object_pointer(clone))
                    continue;
                std::uintptr_t const node_vtable = read_field<std::uintptr_t>(
                    reinterpret_cast<void const*>(clone), 0);
                TransitionProbeNodeClass const node_class = classify(node_vtable);
                std::string_view const node_name = read_node_name(
                    reinterpret_cast<RE::hkbNode const*>(clone));
                std::uint16_t const node_id = read_field<std::uint16_t>(
                    reinterpret_cast<void const*>(clone), 0x40);
                std::uintptr_t const node_behavior = read_field<std::uintptr_t>(
                    reinterpret_cast<void const*>(info),
                    Node_Info_Behavior_Offset);
                ++nodes;
                if (a_phase == TransitionProbePhase::pre_sample)
                {
                    logger::info(
                        "TRANSITION SUBGRAPH NODE side=candidate segment={} phase={} seq={} commanded={} bfr-index={} index={} node={:#x} vtable={:#x} class={} name='{}' id={} behavior={:#x}",
                        segment, phase, a_sequence, a_commanded, bfr_index, n, clone,
                        node_vtable, transition_probe_class_name(node_class), node_name,
                        node_id, node_behavior);
                }
                if (node_class == TransitionProbeNodeClass::state_machine)
                {
                    if (context.sm_logged >= Transition_Subgraph_State_Machine_Capacity)
                    {
                        context.overflow = true;
                        continue;
                    }
                    log_subgraph_state_machine(context, clone, n);
                    ++context.sm_logged;
                }
                else if (node_class == TransitionProbeNodeClass::blender)
                {
                    if (context.blender_logged >= Transition_Subgraph_Blender_Capacity)
                    {
                        context.overflow = true;
                        continue;
                    }
                    log_subgraph_blender(context, clone, n);
                    ++context.blender_logged;
                }
            }
            state_machines += context.sm_logged;
            blenders += context.blender_logged;
            overflow = overflow || context.overflow;
        }

        logger::info(
            "TRANSITION SUBGRAPH SUMMARY side=candidate segment={} phase={} seq={} commanded={} bfrs={} behaviors={} nodes={} sm={} blenders={} overflow={}",
            segment, phase, a_sequence, a_commanded, bfrs, behaviors, nodes, state_machines,
            blenders, overflow);
    }

    namespace
    {
        using ChainNode = std::array<std::byte, Node_Info_Size>;

        struct ChainArray
        {
            std::uintptr_t data{};
            std::int32_t size{};
            std::uint32_t capacity_and_flags{};
        };
        static_assert(sizeof(ChainArray) == 0x10);

        constexpr std::size_t Graph_Child_Indices_Offset = 0xA8;
        constexpr std::size_t Node_Info_Template_Offset = 0x50;
        constexpr std::size_t Node_Info_Alias_Offset = 0x68;
        constexpr std::size_t Node_Info_Child_Count_Offset = 0x70;
        constexpr std::size_t Node_Info_Child_First_Offset = 0x74;
        constexpr std::size_t Node_Info_Uses_Alias_Offset = 0x89;

        bool read_chain_memory(std::uintptr_t a_address, void* a_out, std::size_t a_size) noexcept
        {
            SIZE_T copied{};
            return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(a_address),
                a_out, a_size, &copied) != 0 && copied == a_size;
        }

        template <class T>
        bool read_chain_field(std::uintptr_t a_base, std::size_t a_offset, T& a_out) noexcept
        {
            return read_chain_memory(a_base + a_offset, &a_out, sizeof(a_out));
        }

        bool read_chain_array(std::uintptr_t a_address, ChainArray& a_out) noexcept
        {
            constexpr std::uint32_t capacity_mask = 0x3FFFFFFF;
            return plausible_object_pointer(a_address) && read_chain_field(a_address, 0, a_out) &&
                a_out.size >= 0 && static_cast<std::uint32_t>(a_out.size) <=
                    (a_out.capacity_and_flags & capacity_mask) &&
                (a_out.size == 0 || plausible_object_pointer(a_out.data));
        }

        bool read_chain_string(std::uintptr_t a_address,
            std::array<char, Transition_Probe_Name_Length>& a_text) noexcept
        {
            std::uintptr_t pointer{};
            if (!read_chain_field(a_address, 0, pointer))
                return false;
            pointer &= ~std::uintptr_t{ 1 };  // hkStringPtr managed bit
            if (pointer == 0)
            {
                a_text.front() = '\0';
                return true;
            }

            // Stop at the first terminator instead of requiring the next page to be readable.
            constexpr std::size_t page_size = 4096;  // Windows x64 page granularity
            std::size_t const first = std::min(a_text.size(), page_size - (pointer % page_size));
            if (!read_chain_memory(pointer, a_text.data(), first))
                return false;
            if (std::memchr(a_text.data(), 0, first))
                return true;
            return first < a_text.size() &&
                read_chain_memory(pointer + first, a_text.data() + first, a_text.size() - first) &&
                std::memchr(a_text.data() + first, 0, a_text.size() - first) != nullptr;
        }

        struct ChainChildMatch
        {
            std::int32_t index{ -1 };
            std::size_t count{};
            bool exact_clone{};
        };

        ChainChildMatch match_chain_child(std::uintptr_t a_expected, ParentChainChildRange a_range,
            std::span<std::int32_t const> a_indices, std::span<ChainNode const> a_nodes) noexcept
        {
            ChainChildMatch match;
            if (a_expected == 0)
                return match;
            for (std::int32_t slot = 0; slot < a_range.count; ++slot)
            {
                std::int32_t const child = a_indices[static_cast<std::size_t>(a_range.first + slot)];
                if (child == -1)
                    continue;
                ChainNode const& item = a_nodes[static_cast<std::size_t>(child)];
                bool const exact_clone = a_expected == read_field<std::uintptr_t>(item.data(),
                    Node_Info_Node_Clone_Offset);
                if (!exact_clone && a_expected != read_field<std::uintptr_t>(item.data(),
                        Node_Info_Template_Offset))
                    continue;
                if (match.index == child)
                    continue;
                if (match.count == 0)
                {
                    match.index = child;
                    match.exact_clone = exact_clone;
                }
                else
                {
                    match.index = -1;
                    match.count = 2;  // There is no unique active child.
                    return match;
                }
                ++match.count;
            }
            return match;
        }

        char const* chain_match_name(ChainChildMatch const& a_match) noexcept
        {
            if (a_match.count > 1)
                return "ambiguous";
            if (a_match.count == 0)
                return "missing";
            return a_match.exact_clone ? "clone" : "template";
        }

        struct ChainLogContext
        {
            std::string_view key;
            std::span<ChainNode const> nodes;
            std::span<std::int32_t const> indices;
            ParentChainTopology const& topology;
        };

        bool log_chain_state(ChainLogContext const& a_context, std::size_t a_index,
            std::uintptr_t a_node, ParentChainChildRange a_range) noexcept
        {
            std::int32_t current_id{};
            std::uint16_t packed_index{};
            ChainArray states{};
            ChainArray effects{};
            if (!read_chain_field(a_node, 0x80, current_id) ||
                !read_chain_field(a_node, 0x102, packed_index) ||
                !read_chain_array(a_node + 0x90, states) ||
                !read_chain_array(a_node + State_Machine_Active_Transitions_Offset, effects))
            {
                logger::info("TRANSITION PARENT CHAIN SM {} index={} status=unreadable", a_context.key, a_index);
                return false;
            }
            std::int32_t const state_index = packed_index >> 1;
            std::uintptr_t state_info{};
            std::uintptr_t state_vtable{};
            std::uintptr_t generator{};
            std::int32_t state_id{};
            std::array<char, Transition_Probe_Name_Length> state_name;
            bool const state_read = state_index < states.size &&
                read_chain_field(states.data, static_cast<std::size_t>(state_index) * sizeof(state_info), state_info) &&
                plausible_object_pointer(state_info) && read_chain_field(state_info, 0, state_vtable) &&
                state_vtable == RE::VTABLE_hkbStateMachine__StateInfo[0].address() &&
                read_chain_field(state_info, 0x58, generator) &&
                read_chain_field(state_info, 0x68, state_id) &&
                read_chain_string(state_info + 0x60, state_name);
            std::uintptr_t effect{};
            bool const effect_read = effects.size == 0 || read_chain_field(effects.data,
                static_cast<std::size_t>(effects.size - 1) * State_Machine_Active_Transitions_Stride, effect);
            std::uintptr_t const expected = effects.size > 0 ? effect : generator;
            ChainChildMatch const match = match_chain_child(expected, a_range, a_context.indices, a_context.nodes);
            char const* const status = !state_read || !effect_read ? "unreadable" :
                state_id != current_id ? "state-id-mismatch" : "ok";
            logger::info("TRANSITION PARENT CHAIN SM {} index={} current-state={} state-index={} state-info={:#x} "
                "state-id={} state-name='{}' state-generator={:#x} active-effects={} last-effect={:#x} "
                "match={} child={} on-path={} status={}",
                a_context.key, a_index, current_id, state_index, state_info, state_id,
                state_read ? std::string_view(state_name.data()) : std::string_view{}, generator,
                effects.size, effect, chain_match_name(match), match.index,
                match.index >= 0 && a_context.topology.path[static_cast<std::size_t>(match.index)], status);
            return state_read && effect_read && current_id == state_id && match.count == 1;
        }

        bool log_chain_selector(ChainLogContext const& a_context, std::size_t a_index,
            std::uintptr_t a_node, ParentChainChildRange a_range) noexcept
        {
            ChainArray generators{};
            std::int8_t selected{};
            std::int8_t current{};
            if (!read_chain_array(a_node + 0x48, generators) ||
                !read_chain_field(a_node, 0x58, selected) || !read_chain_field(a_node, 0x59, current))
            {
                logger::info("TRANSITION PARENT CHAIN SELECTOR {} index={} status=unreadable", a_context.key, a_index);
                return false;
            }
            std::uintptr_t generator{};
            bool const index_valid = current >= 0 && current < generators.size;
            bool const generator_read = index_valid && read_chain_field(generators.data,
                static_cast<std::size_t>(current) * sizeof(generator), generator);
            ChainChildMatch const match = match_chain_child(generator, a_range, a_context.indices, a_context.nodes);
            logger::info("TRANSITION PARENT CHAIN SELECTOR {} index={} generators={} selected={} current={} "
                "current-generator={:#x} match={} child={} on-path={} status={}",
                a_context.key, a_index, generators.size, static_cast<int>(selected), static_cast<int>(current),
                generator, chain_match_name(match), match.index,
                match.index >= 0 && a_context.topology.path[static_cast<std::size_t>(match.index)],
                !index_valid ? "invalid-index" : generator_read ? "ok" : "unreadable");
            return generator_read && match.count == 1;
        }

        char const* parent_chain_status_name(ParentChainStatus a_status) noexcept
        {
            switch (a_status)
            {
                case ParentChainStatus::e_ok: return "ok";
                case ParentChainStatus::e_overflow: return "overflow";
                case ParentChainStatus::e_invalid_range: return "invalid-range";
                case ParentChainStatus::e_invalid_index: return "invalid-index";
                case ParentChainStatus::e_cycle: return "cycle";
            }
            return "unknown";
        }

        // Shared v7/v8 seed: the unique active clip whose animation leaf matches the
        // segment family. Identity is the animation, not the node name, because a segment
        // can carry differently named clones of the same clip.
        char const* find_family_clip(std::uintptr_t a_behavior, char const* a_expected_leaf,
            std::uintptr_t& a_clip) noexcept
        {
            a_clip = 0;
            ChainArray nodes{};
            std::uintptr_t behavior_vtable{};
            std::uintptr_t node_head{};
            if (!plausible_object_pointer(a_behavior) ||
                !read_chain_field(a_behavior, 0, behavior_vtable) ||
                behavior_vtable != RE::VTABLE_hkbBehaviorGraph[0].address() ||
                !read_chain_field(a_behavior, Behavior_Graph_Active_Nodes_Offset, node_head) ||
                !read_chain_array(node_head, nodes))
                return "unavailable";
            if (nodes.size <= 0 ||
                nodes.size > static_cast<std::int32_t>(Parent_Chain_Active_Node_Limit))
                return nodes.size <= 0 ? "missing-seed" : "overflow";

            std::array<ChainNode, Parent_Chain_Active_Node_Limit> raw_nodes{};
            if (!read_chain_memory(nodes.data, raw_nodes.data(),
                    sizeof(ChainNode) * static_cast<std::size_t>(nodes.size)))
                return "unreadable";

            std::array<char, Transition_Probe_Name_Length> name{};
            std::size_t seeds = 0;
            for (std::size_t i = 0; i < static_cast<std::size_t>(nodes.size); ++i)
            {
                std::uintptr_t const clone = read_field<std::uintptr_t>(raw_nodes[i].data(),
                    Node_Info_Node_Clone_Offset);
                std::uintptr_t vtable{};
                if (!plausible_object_pointer(clone) || !read_chain_field(clone, 0, vtable))
                    return "unreadable-node";
                if (vtable != RE::VTABLE_hkbClipGenerator[0].address())
                    continue;
                if (!read_chain_string(clone + 0x48, name))
                    return "unreadable-clip";
                std::string_view const animation{ name.data() };
                std::size_t const separator = animation.find_last_of("\\/");
                std::string_view const leaf = separator == std::string_view::npos
                    ? animation : animation.substr(separator + 1);
                if (_stricmp(leaf.data(), a_expected_leaf) != 0)
                    continue;
                a_clip = clone;
                ++seeds;
            }
            if (seeds == 0)
                return "missing-seed";
            if (seeds != 1)
                return "ambiguous-seed";
            return "ok";
        }
    }

    void log_transition_parent_chain(RE::BShkbAnimationGraph const& a_graph,
        TransitionProbeRecord const& a_probe) noexcept
    {
        std::uintptr_t const behavior = reinterpret_cast<std::uintptr_t>(a_graph.characterInstance.behaviorGraph.get());
        std::string const key = fmt::format(
            "side=candidate segment=equip phase={} graph={:#x} behavior={:#x} actor={:08x} request={} commanded={} completed={}",
            transition_probe_phase_name(static_cast<TransitionProbePhase>(a_probe.phase)),
            a_probe.graph, behavior, a_probe.actor, a_probe.request, a_probe.commanded, a_probe.sequence);

        ChainArray nodes{};
        ChainArray indices{};
        std::size_t seeds = 0;
        std::size_t seed = 0;
        std::size_t unresolved = 0;
        std::size_t ordinary_blenders = 0;
        ParentChainTopology topology{};
        auto const summary = [&](char const* a_status) {
            logger::info("TRANSITION PARENT CHAIN SUMMARY {} status={} active-nodes={} child-slots={} "
                "seeds={} seed={} nodes={} edges={} roots={} empty-slots={} ordinary-blenders={} unresolved={}",
                key, a_status, nodes.size, indices.size, seeds, seeds == 1 ? static_cast<int>(seed) : -1,
                topology.node_count, topology.edge_count, topology.roots, topology.empty_slots,
                ordinary_blenders, unresolved);
        };

        std::uintptr_t behavior_vtable{};
        std::uintptr_t node_head{};
        std::uintptr_t index_head{};
        if (!plausible_object_pointer(behavior) ||
            !read_chain_field(behavior, 0, behavior_vtable) ||
            behavior_vtable != RE::VTABLE_hkbBehaviorGraph[0].address() ||
            !read_chain_field(behavior, Behavior_Graph_Active_Nodes_Offset, node_head) ||
            !read_chain_field(behavior, Graph_Child_Indices_Offset, index_head))
        {
            summary("unavailable");
            return;
        }
        if (!read_chain_array(node_head, nodes) || !read_chain_array(index_head, indices))
        {
            summary("invalid-array");
            return;
        }
        if (nodes.size > static_cast<std::int32_t>(Parent_Chain_Active_Node_Limit) ||
            indices.size > static_cast<std::int32_t>(Parent_Chain_Child_Index_Limit))
        {
            summary("overflow");
            return;
        }
        if (nodes.size == 0)
        {
            summary("missing-seed");
            return;
        }

        std::array<ChainNode, Parent_Chain_Active_Node_Limit> raw_nodes;
        std::array<std::int32_t, Parent_Chain_Child_Index_Limit> raw_indices;
        if (!read_chain_memory(nodes.data, raw_nodes.data(), sizeof(ChainNode) * static_cast<std::size_t>(nodes.size)) ||
            (indices.size != 0 && !read_chain_memory(indices.data, raw_indices.data(),
                 sizeof(std::int32_t) * static_cast<std::size_t>(indices.size))))
        {
            summary("unreadable");
            return;
        }
        std::span<ChainNode const> const node_snapshot{ raw_nodes.data(), static_cast<std::size_t>(nodes.size) };
        std::span<std::int32_t const> const index_snapshot{ raw_indices.data(), static_cast<std::size_t>(indices.size) };
        std::array<ParentChainChildRange, Parent_Chain_Active_Node_Limit> ranges;
        std::array<std::uintptr_t, Parent_Chain_Active_Node_Limit> vtables;
        std::array<char, Transition_Probe_Name_Length> name;
        for (std::size_t i = 0; i < node_snapshot.size(); ++i)
        {
            void const* const row = node_snapshot[i].data();
            ranges[i] = { read_field<std::int32_t>(row, Node_Info_Child_First_Offset),
                read_field<std::int32_t>(row, Node_Info_Child_Count_Offset) };
            std::uintptr_t const clone = read_field<std::uintptr_t>(row, Node_Info_Node_Clone_Offset);
            if (!plausible_object_pointer(clone) || !read_chain_field(clone, 0, vtables[i]))
            {
                summary("unreadable-node");
                return;
            }
            if (vtables[i] != RE::VTABLE_hkbClipGenerator[0].address())
                continue;
            if (!read_chain_string(clone + 0x38, name))
            {
                summary("unreadable-clip");
                return;
            }
            if (std::string_view(name.data()) == "DG_1hmEquip")
            {
                seed = i;
                ++seeds;
            }
        }
        if (seeds != 1)
        {
            summary(seeds == 0 ? "missing-seed" : "ambiguous-seed");
            return;
        }

        std::uintptr_t const clip = read_field<std::uintptr_t>(node_snapshot[seed].data(), Node_Info_Node_Clone_Offset);
        std::uintptr_t binding{};
        std::uint16_t binding_index{};
        std::uint32_t local_time_bits{};
        if (!read_chain_string(clip + 0x48, name) ||
            !read_chain_field(clip, 0xA0, binding) ||
            !read_chain_field(clip, 0x70, binding_index) ||
            !read_chain_field(clip, 0xF0, local_time_bits))
        {
            summary("unreadable-clip");
            return;
        }
        std::string_view const animation = name.data();
        std::size_t const separator = animation.find_last_of("\\/");
        std::string_view const leaf = separator == std::string_view::npos ? animation : animation.substr(separator + 1);
        bool const family_matches = _stricmp(leaf.data(), "1HM_Equip.hkx") == 0;
        logger::info("TRANSITION PARENT CHAIN CLIP {} index={} node={:#x} animation='{}' "
            "binding={:#x} binding-index={} local-time-bits={:08X} family={}",
            key, seed, clip, animation, binding, binding_index, local_time_bits, family_matches);
        if (!family_matches || !plausible_object_pointer(binding))
        {
            summary("clip-mismatch");
            return;
        }

        topology = trace_parent_chain(std::span<ParentChainChildRange const>{ ranges.data(), node_snapshot.size() },
            index_snapshot, seed);
        if (topology.status != ParentChainStatus::e_ok)
        {
            summary(parent_chain_status_name(topology.status));
            return;
        }

        ChainLogContext const context{ key, node_snapshot, index_snapshot, topology };
        for (std::size_t i = 0; i < node_snapshot.size(); ++i)
        {
            if (!topology.path[i])
                continue;
            void const* const row = node_snapshot[i].data();
            std::uintptr_t const node = read_field<std::uintptr_t>(row, Node_Info_Node_Clone_Offset);
            bool const alias = read_field<std::uint8_t>(row, Node_Info_Uses_Alias_Offset) != 0;
            std::int32_t const alias_index = alias ? read_field<std::int32_t>(row, Node_Info_Alias_Offset) : -1;
            bool const alias_valid = !alias || (alias_index >= 0 && alias_index < nodes.size);
            std::uint16_t id{};
            bool const node_read = read_chain_string(node + 0x38, name) && read_chain_field(node, 0x40, id);
            TransitionProbeNodeClass const node_class = classify(vtables[i]);
            logger::info("TRANSITION PARENT CHAIN NODE {} index={} class={} id={} name='{}' template={:#x} "
                "clone={:#x} owner={:#x} vtable={:#x} first={} count={} alias={} alias-index={} status={}",
                key, i, transition_probe_class_name(node_class), id,
                node_read ? std::string_view(name.data()) : std::string_view{},
                read_field<std::uintptr_t>(row, Node_Info_Template_Offset), node,
                read_field<std::uintptr_t>(row, Node_Info_Behavior_Offset), vtables[i], ranges[i].first,
                ranges[i].count, alias, alias_index, !node_read ? "unreadable" : alias_valid ? "ok" : "invalid-alias");
            if (!node_read || !alias_valid)
                ++unresolved;
            if (node_class == TransitionProbeNodeClass::state_machine)
            {
                if (!log_chain_state(context, i, node, ranges[i]))
                    ++unresolved;
            }
            else if (node_class == TransitionProbeNodeClass::manual_selector)
            {
                if (!log_chain_selector(context, i, node, ranges[i]))
                    ++unresolved;
            }
            else if (node_class == TransitionProbeNodeClass::blender)
            {
                ++ordinary_blenders;
                std::uint16_t flags{};
                bool const flags_read = read_chain_field(node, 0x5A, flags);
                if (!flags_read)
                    ++unresolved;
                logger::info("TRANSITION PARENT CHAIN BLENDER {} index={} node={:#x} flags={:04X} "
                    "weight-mode={} status={}", key, i, node, flags,
                    (flags & 0x10) != 0 ? "interval" : "child", flags_read ? "ok" : "unreadable");
            }
        }
        for (std::size_t i = 0; i < topology.edge_count; ++i)
        {
            ParentChainEdge const edge = topology.edges[i];
            logger::info("TRANSITION PARENT CHAIN EDGE {} parent={} child={} slot={}",
                key, edge.parent, edge.child, edge.slot);
        }
        summary(topology.edge_count == 0 ? "no-parent" : unresolved == 0 ? "ok" : "incomplete-identity");
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
                "TRANSITION PROBE SM side={} segment={} phase={} seq={} index={} node={:#x} vtable={:#x} name='{}' state={} transitions={} overflow={}",
                side, segment, phase, a_record.sequence, i, state_machine.node, state_machine.vtable,
                std::string_view{ state_machine.name, state_machine.name_length },
                state_machine.current_state, state_machine.transition_count, state_machine.overflow);
            log_transition_aux(state_machine, side, segment, phase, a_record.sequence, i);
        }

        std::size_t const effect_count = std::min<std::size_t>(a_record.effect_count,
            Transition_Probe_Effect_Capacity);
        for (std::size_t i = 0; i < effect_count; ++i)
        {
            TransitionProbeEffect const& effect = a_record.effects[i];
            logger::info(
                "TRANSITION PROBE EFFECT side={} segment={} phase={} seq={} sm-index={} index={} effect={:#x} vtable={:#x} class={} duration={:.6f} duration-bits={:08X} time-in={:.6f} time-in-bits={:08X} time-remaining={:.6f} time-remaining-bits={:08X} blend-curve={} end-mode={} flags={} self={} event={}",
                side, segment, phase, a_record.sequence, effect.state_machine_index, i, effect.effect, effect.vtable,
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
                "TRANSITION PROBE BLENDER side={} segment={} phase={} seq={} index={} node={:#x} vtable={:#x} name='{}' blend-param={:.6f} blend-param-bits={:08X} end-weight={:.6f} end-weight-bits={:08X} active-children={} begin={} end={} children={} internals={} overflow={}",
                side, segment, phase, a_record.sequence, i, blender.node, blender.vtable,
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
                    "TRANSITION PROBE CHILD side={} segment={} phase={} seq={} blender-index={} index={} generator={:#x} vtable={:#x} class={} weight={:.6f} weight-bits={:08X} wfm={:.6f} wfm-bits={:08X}",
                    side, segment, phase, a_record.sequence, i, child_index, child.generator, child.vtable,
                    transition_probe_class_name(static_cast<TransitionProbeNodeClass>(child.class_id)),
                    std::bit_cast<float>(child.weight_bits), child.weight_bits,
                    std::bit_cast<float>(child.world_from_model_bits), child.world_from_model_bits);
            }
            if (blender.internal_count > 0)
            {
                logger::info(
                    "TRANSITION PROBE INTERNAL side={} segment={} phase={} seq={} blender-index={} count={} values='{:04X},{:04X},{:04X},{:04X}'",
                    side, segment, phase, a_record.sequence, i, blender.internal_count,
                    blender.internals[0], blender.internals[1], blender.internals[2],
                    blender.internals[3]);
            }
        }
    }

    constexpr std::size_t Idle_Chain_Track_Limit = 256;
    constexpr std::size_t Clip_Control_Offset = 0x88;

    // C-G5-CHAIN v1: clip-generator detection by exact vtable (same rule as the K1 census).
    bool idle_chain_is_clip(RE::hkbNode const* a_node) noexcept
    {
        if (!a_node)
            return false;
        return *reinterpret_cast<std::uintptr_t const*>(a_node) == RE::VTABLE_hkbClipGenerator[0].address();
    }

    // C-G5-CHAIN v1: rotation angle in degrees between two quats (1-|dot| domain); infinity
    // marks non-finite or zero-norm input, matching pose_rotation_agreement_error's sentinel.
    float idle_chain_theta_degrees(std::array<float, 4> const& a_lhs,
        std::array<float, 4> const& a_rhs) noexcept
    {
        double left_length = 0.0;
        double right_length = 0.0;
        for (std::size_t i = 0; i < 4; ++i)
        {
            if (!std::isfinite(a_lhs[i]) || !std::isfinite(a_rhs[i]))
                return std::numeric_limits<float>::infinity();
            left_length += static_cast<double>(a_lhs[i]) * static_cast<double>(a_lhs[i]);
            right_length += static_cast<double>(a_rhs[i]) * static_cast<double>(a_rhs[i]);
        }
        if (left_length <= 0.0 || right_length <= 0.0)
            return std::numeric_limits<float>::infinity();
        double dot = 0.0;
        for (std::size_t i = 0; i < 4; ++i)
            dot += static_cast<double>(a_lhs[i]) * static_cast<double>(a_rhs[i]);
        double const cosine = std::min(1.0, std::abs(dot) /
            (std::sqrt(left_length) * std::sqrt(right_length)));
        return static_cast<float>(2.0 * std::acos(cosine) * 180.0 / 3.14159265358979323846);
    }

    // One graph = one side: resolve interest tracks by bone name, walk the active clip
    // generators, sample each clip at that side's own localTime and compare the authored
    // transforms against this side's poseLocal. Read-only; the sample lands in a probe-local
    // static buffer exactly like the sweep probe (display thread only).
    IdleChainSideSummary log_idle_chain_side(RE::BShkbAnimationGraph const& a_graph,
        char const* a_side, std::uint32_t a_actor, std::uint64_t a_request,
        std::uint64_t a_view, std::uint32_t a_call) noexcept
    {
        auto const status = [&](char const* a_status) {
            logger::info("IDLE CHAIN side={} actor={:08x} request={} view={} call={} status={}",
                a_side, a_actor, a_request, a_view, a_call, a_status);
            return IdleChainSideSummary{};
        };

        RE::hkbBehaviorGraph const* const behavior = a_graph.characterInstance.behaviorGraph.get();
        if (!behavior || !behavior->activeNodes)
            return status("unavailable");
        RE::hkbCharacter const& character = a_graph.characterInstance;
        if (character.poseLocal == nullptr || character.numPoseLocal <= 0 ||
            static_cast<std::size_t>(character.numPoseLocal) > Idle_Chain_Track_Limit)
            return status("invalid-pose");
        RE::hkaSkeleton const* const skeleton = character.setup
            ? character.setup->animationSkeleton.get() : nullptr;
        if (!skeleton || skeleton->bones.empty())
            return status("no-skeleton");

        std::size_t const bone_count = std::min<std::size_t>(
            static_cast<std::size_t>(character.numPoseLocal), skeleton->bones.size());
        std::array<std::int32_t, Idle_Chain_Interest_Names.size()> interest_bone{};
        interest_bone.fill(-1);
        std::size_t resolved = 0;
        for (std::size_t i = 0; i < bone_count; ++i)
        {
            char const* const raw = skeleton->bones.data()[i].name.c_str();
            if (!raw || !plausible_object_pointer(reinterpret_cast<std::uintptr_t>(raw)))
                continue;
            std::size_t const interest = idle_chain_interest_index(raw);
            if (interest != static_cast<std::size_t>(-1) && interest_bone[interest] < 0)
            {
                interest_bone[interest] = static_cast<std::int32_t>(i);
                ++resolved;
            }
        }
        if (resolved == 0)
            return status("no-interest-tracks");

        RE::NodeList const& nodes = *behavior->activeNodes;
        std::size_t const walked = std::min<std::size_t>(nodes.size(), 64);
        std::array<RE::hkbClipGenerator const*, 8> clips{};
        std::size_t clip_count = 0;
        std::size_t overflow = 0;
        for (std::size_t i = 0; i < walked; ++i)
        {
            RE::hkbNode const* const node = nodes.data()[i].nodeClone;
            if (!node || !plausible_object_pointer(reinterpret_cast<std::uintptr_t>(node)))
                continue;
            if (!idle_chain_is_clip(node))
                continue;
            if (clip_count >= clips.size())
            {
                ++overflow;
                break;
            }
            clips[clip_count++] = static_cast<RE::hkbClipGenerator const*>(node);
        }

        IdleChainSideSummary summary;
        std::size_t compared_clips = 0;
        for (std::size_t c = 0; c < clip_count; ++c)
        {
            RE::hkbClipGenerator const* const clip = clips[c];
            std::string const node_name{ read_node_name(reinterpret_cast<RE::hkbNode const*>(clip)) };
            std::uintptr_t control{};
            std::uintptr_t control_vtable{};
            if (!read_chain_field(reinterpret_cast<std::uintptr_t>(clip), Clip_Control_Offset, control) ||
                !plausible_object_pointer(control) ||
                !read_chain_field(control, 0, control_vtable) ||
                control_vtable != RE::VTABLE_hkaDefaultAnimationControl[0].address())
            {
                logger::info("IDLE CHAIN side={} actor={:08x} request={} view={} call={} clip='{}' node={:#x} status=no-control",
                    a_side, a_actor, a_request, a_view, a_call, node_name,
                    reinterpret_cast<std::uintptr_t>(clip));
                continue;
            }
            auto const* const animation_control = reinterpret_cast<RE::hkaDefaultAnimationControl const*>(control);
            RE::hkaAnimationBinding const* const binding = animation_control->binding;
            if (!binding || !plausible_object_pointer(reinterpret_cast<std::uintptr_t>(binding)))
            {
                logger::info("IDLE CHAIN side={} actor={:08x} request={} view={} call={} clip='{}' node={:#x} status=no-binding",
                    a_side, a_actor, a_request, a_view, a_call, node_name,
                    reinterpret_cast<std::uintptr_t>(clip));
                continue;
            }
            RE::hkaAnimation const* const animation = binding->animation.get();
            if (!animation)
            {
                logger::info("IDLE CHAIN side={} actor={:08x} request={} view={} call={} clip='{}' node={:#x} status=no-animation",
                    a_side, a_actor, a_request, a_view, a_call, node_name,
                    reinterpret_cast<std::uintptr_t>(clip));
                continue;
            }
            std::int32_t const tracks = animation->numberOfTransformTracks;
            if (tracks <= 0 || static_cast<std::size_t>(tracks) > Idle_Chain_Track_Limit)
            {
                logger::info("IDLE CHAIN side={} actor={:08x} request={} view={} call={} clip='{}' node={:#x} tracks={} status=invalid-tracks",
                    a_side, a_actor, a_request, a_view, a_call, node_name,
                    reinterpret_cast<std::uintptr_t>(clip), tracks);
                continue;
            }
            K5IdentityCache identity_cache;
            K5IdentityCache::Entry const identity = identity_cache.identity_for(binding);
            float const local_time = animation_control->localTime;
            std::span<std::int16_t const> const map{ binding->transformTrackToBoneIndices.data(),
                static_cast<std::size_t>(binding->transformTrackToBoneIndices.size()) };
            std::vector<AnimationTrackBinding> const track_bindings =
                resolve_animation_tracks(map, static_cast<std::size_t>(tracks), bone_count);
            if (track_bindings.empty())
                continue;

            std::array<std::int32_t, Idle_Chain_Interest_Names.size()> interest_track{};
            interest_track.fill(-1);
            for (std::size_t i = 0; i < Idle_Chain_Interest_Names.size(); ++i)
            {
                std::int32_t const bone = interest_bone[i];
                if (bone < 0 || bone >= character.numPoseLocal)
                    continue;
                auto const track = std::ranges::find(track_bindings, bone, &AnimationTrackBinding::bone);
                if (track != track_bindings.end())
                    interest_track[i] = track->track;
            }

            static std::array<RE::hkQsTransform, Idle_Chain_Track_Limit> sampled;
            std::memset(sampled.data(), 0, sizeof(RE::hkQsTransform) * static_cast<std::size_t>(tracks));
            animation->SamplePartialTracks(local_time, static_cast<std::uint32_t>(tracks),
                sampled.data(), 0, nullptr, nullptr);

            std::string readings;
            float max_drot = 0.0f;
            std::size_t diff_count = 0;
            std::size_t nan_count = 0;
            for (std::size_t i = 0; i < Idle_Chain_Interest_Names.size(); ++i)
            {
                IdleChainTrackReading reading{};
                reading.bone = interest_bone[i];
                if (reading.bone >= 0 && interest_track[i] >= 0)
                {
                    reading.resolved = true;
                    PoseSample const authored = capture_pose_sample(sampled[static_cast<std::size_t>(interest_track[i])]);
                    PoseSample const pose = capture_pose_sample(character.poseLocal[static_cast<std::size_t>(reading.bone)]);
                    reading.dpos = pose_component_delta(authored.translation, pose.translation);
                    reading.drot_degrees = idle_chain_theta_degrees(authored.rotation, pose.rotation);
                    if (reading.drot_degrees == std::numeric_limits<float>::infinity())
                        reading.non_finite = true;
                }
                summary.observe(i, reading);
                if (!reading.resolved)
                    continue;
                if (reading.non_finite)
                    ++nan_count;
                if (!readings.empty())
                    readings.push_back(',');
                readings += fmt::format("{}:{}:{:.4f}", reading.bone, reading.drot_degrees, reading.dpos);
                if (!reading.non_finite && reading.drot_degrees > max_drot)
                    max_drot = reading.drot_degrees;
                if (!reading.non_finite && reading.drot_degrees >= 0.01f)
                    ++diff_count;
            }
            ++compared_clips;
            logger::info("IDLE CHAIN side={} actor={:08x} request={} view={} call={} clip='{}' node={:#x} binding={:#x} "
                "tracks={} map-count={} identity-map={} data-fnv={:016X} map-fnv={:016X} duration-bits={:08X} "
                "local-time-bits={:08X} readings='{}' diff-count={} max-drot={:.4f} nan={} status=ok",
                a_side, a_actor, a_request, a_view, a_call, node_name,
                reinterpret_cast<std::uintptr_t>(clip), reinterpret_cast<std::uintptr_t>(binding),
                tracks, map.size(), map.empty(),
                identity.ok ? identity.identity.data_hash : std::uint64_t{ 0 },
                identity.ok ? identity.identity.map_hash : std::uint64_t{ 0 },
                identity.ok ? identity.identity.duration_bits : std::uint32_t{ 0 },
                std::bit_cast<std::uint32_t>(local_time),
                std::string_view{ readings }, diff_count, max_drot, nan_count);
        }

        logger::info("IDLE CHAIN SUMMARY side={} actor={:08x} request={} view={} call={} clips={} compared={} "
            "resolved-interest={} shoulder-exceeds5={} others-clean5={} status=ok",
            a_side, a_actor, a_request, a_view, a_call, clip_count, compared_clips,
            resolved, summary.shoulder_exceeds(5.0f), summary.others_below(5.0f) ? 1 : 0);
        return summary;
    }

    // Bounded active-node census for one graph (C-G5-CHAIN v1): class-by-vtable rows so the
    // offline pass can diff the two sides' active sets and name a candidate modification layer.
    void log_idle_chain_census(RE::BShkbAnimationGraph const& a_graph, char const* a_side,
        std::uint32_t a_actor, std::uint64_t a_request, std::uint64_t a_view,
        std::uint32_t a_call) noexcept
    {
        RE::hkbBehaviorGraph const* const behavior = a_graph.characterInstance.behaviorGraph.get();
        if (!behavior || !behavior->activeNodes)
        {
            logger::info("IDLE CHAIN CENSUS side={} actor={:08x} request={} view={} call={} status=unavailable",
                a_side, a_actor, a_request, a_view, a_call);
            return;
        }
        RE::NodeList const& nodes = *behavior->activeNodes;
        std::size_t const limit = std::min<std::size_t>(nodes.size(), 128);
        std::size_t unknown = 0;
        std::size_t branch_counts[5] = {};
        for (std::size_t i = 0; i < limit; ++i)
        {
            RE::hkbNode const* const node = nodes.data()[i].nodeClone;
            if (!node || !plausible_object_pointer(reinterpret_cast<std::uintptr_t>(node)))
                continue;
            std::uintptr_t const vtable = read_field<std::uintptr_t>(node, 0);
            TransitionProbeNodeClass const node_class = classify(vtable);
            if (node_class == TransitionProbeNodeClass::unknown)
                ++unknown;
            std::string_view const node_name = read_node_name(node);
            ++branch_counts[static_cast<std::size_t>(idle_chain_branch_of(node_name))];
            logger::info("IDLE CHAIN CENSUS side={} actor={:08x} request={} view={} call={} index={} node={:#x} vtable={:#x} class={} name='{}' id={}",
                a_side, a_actor, a_request, a_view, a_call, i,
                reinterpret_cast<std::uintptr_t>(node), vtable,
                transition_probe_class_name(node_class), node_name,
                static_cast<std::uint32_t>(node->id));
        }
        logger::info("IDLE CHAIN CENSUS SUMMARY side={} actor={:08x} request={} view={} call={} walked={} active-nodes={} unknown={}",
            a_side, a_actor, a_request, a_view, a_call, limit, nodes.size(), unknown);
        // C-G6 v1 (P2-V1): the branch timeline row. The per-node census rows above
        // are the classification authority; this aggregates them so the offline
        // pass can diff both sides' branch sets per snapshot.
        logger::info("IDLE CHAIN BRANCHES side={} actor={:08x} request={} view={} call={} b1hm={} bmt={} bmlh={} "
            "weapon-adj={} other={} walked={} active-nodes={} classification=heuristic-substring",
            a_side, a_actor, a_request, a_view, a_call,
            branch_counts[static_cast<std::size_t>(IdleChainBranch::e_1hm)],
            branch_counts[static_cast<std::size_t>(IdleChainBranch::e_mt)],
            branch_counts[static_cast<std::size_t>(IdleChainBranch::e_mlh)],
            branch_counts[static_cast<std::size_t>(IdleChainBranch::e_weapon_adjustment)],
            branch_counts[static_cast<std::size_t>(IdleChainBranch::e_other)], limit, nodes.size());
    }

    void log_idle_chain_probe(RE::BShkbAnimationGraph const& a_candidate,
        RE::BShkbAnimationGraph const& a_source, std::uint32_t a_actor, std::uint64_t a_request,
        std::uint64_t a_view, std::uint32_t a_call) noexcept
    {
        log_idle_chain_side(a_candidate, "candidate", a_actor, a_request, a_view, a_call);
        log_idle_chain_side(a_source, "source", a_actor, a_request, a_view, a_call);
        log_idle_chain_census(a_candidate, "candidate", a_actor, a_request, a_view, a_call);
        log_idle_chain_census(a_source, "source", a_actor, a_request, a_view, a_call);
    }
}
PLUGIN_NAMESPACE_END
