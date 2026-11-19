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
#include "RE/H/hkbBehaviorGraphData.h"
#include "RE/H/hkbBehaviorGraphStringData.h"
#include "RE/H/hkbBlendingTransitionEffect.h"
#include "RE/H/hkbCharacter.h"
#include "RE/H/hkbNode.h"
#include "RE/H/hkbStateMachine.h"
#include "RE/H/hkbVariableValueSet.h"

#include <algorithm>
#include <bit>
#include <cstring>
#include <string>

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
}
PLUGIN_NAMESPACE_END
