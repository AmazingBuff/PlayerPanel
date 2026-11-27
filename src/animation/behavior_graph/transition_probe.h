//
// C-G5-TRANS v1 (2026-11-18): read-only transition/blender state probe.
//
// The probe is called at the existing K5 candidate/source sample points. It
// copies a fixed-size snapshot of active state-machine transitions and Blender
// weights; it never writes a graph, pose or variable and adds no detour.
//
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <type_traits>

namespace RE
{
    class BShkbAnimationGraph;
}

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    inline constexpr std::size_t Transition_Probe_Name_Length = 96;
    inline constexpr std::size_t Transition_Probe_Node_Limit = 512;
    inline constexpr std::size_t Transition_Probe_State_Machine_Capacity = 32;
    inline constexpr std::size_t Transition_Probe_Effect_Capacity = 32;
    inline constexpr std::size_t Transition_Probe_Blender_Capacity = 8;
    inline constexpr std::size_t Transition_Probe_Child_Capacity = 8;
    inline constexpr std::size_t Transition_Probe_Internal_Capacity = 8;
    inline constexpr std::size_t Transition_Probe_Source_Capacity = 2560;
    inline constexpr std::size_t Transition_Probe_Record_Budget = 16384;
    inline constexpr std::uint16_t Transition_Probe_Version = 3;
    inline constexpr std::uint32_t Transition_Probe_Magic = 0x31525054u;  // "TPR1"

    // C-G5-TRANS v5: candidate-only behavior-reference subgraph probe. The
    // bounds are part of the pre-registration and are checked by the
    // engine-free test; exceeding any of them marks the batch overflowed.
    inline constexpr std::size_t Transition_Subgraph_Bfr_Capacity = 8;
    inline constexpr std::size_t Transition_Subgraph_Node_Capacity = 256;
    inline constexpr std::size_t Transition_Subgraph_State_Machine_Capacity = 8;
    inline constexpr std::size_t Transition_Subgraph_Blender_Capacity = 8;
    inline constexpr std::size_t Transition_Subgraph_Child_Capacity = 8;
    inline constexpr std::uint32_t Transition_Subgraph_Equip_Max_Commanded = 13;
    inline constexpr std::uint32_t Transition_Subgraph_Unequip_Max_Commanded = 12;

    enum class TransitionProbeSide : std::uint8_t
    {
        candidate = 0,
        source = 1,
    };

    enum class TransitionProbeSegment : std::uint8_t
    {
        equip = 0,
        unequip = 1,
        window = 2,
    };

    enum class TransitionProbePhase : std::uint8_t
    {
        post_update = 0,
        pre_sample = 1,
        post_sample = 2,
    };

    enum class TransitionProbeNodeClass : std::uint8_t
    {
        unknown = 0,
        clip,
        state_machine,
        blender,
        manual_selector,
        blending_transition,
        generator_transition,
        behavior_reference,
        modifier_generator,
        other,
    };

    enum class TransitionSubgraphStatus : std::uint8_t
    {
        ok = 0,
        unavailable,
        unknown_behavior_class,
        no_active_nodes,
        invalid_array,
        inactive,
    };

    inline constexpr std::uint16_t Transition_Probe_Flag_Unavailable = 1u << 0;
    inline constexpr std::uint16_t Transition_Probe_Flag_State_Machine_Overflow = 1u << 1;
    inline constexpr std::uint16_t Transition_Probe_Flag_Effect_Overflow = 1u << 2;
    inline constexpr std::uint16_t Transition_Probe_Flag_Blender_Overflow = 1u << 3;
    inline constexpr std::uint16_t Transition_Probe_Flag_Child_Overflow = 1u << 4;
    inline constexpr std::uint16_t Transition_Probe_Flag_Internal_Overflow = 1u << 5;
    inline constexpr std::uint16_t Transition_Probe_Flag_Name_Invalid = 1u << 6;
    inline constexpr std::uint16_t Transition_Probe_Flag_Overflow_Mask =
        Transition_Probe_Flag_State_Machine_Overflow |
        Transition_Probe_Flag_Effect_Overflow |
        Transition_Probe_Flag_Blender_Overflow |
        Transition_Probe_Flag_Child_Overflow |
        Transition_Probe_Flag_Internal_Overflow;

    struct TransitionProbeStateMachine
    {
        std::uintptr_t node;
        std::uintptr_t vtable;
        std::int32_t current_state;
        std::uint16_t transition_count;
        std::uint8_t name_length;
        std::uint8_t overflow;
        char name[Transition_Probe_Name_Length];
    };

    struct TransitionProbeEffect
    {
        std::uintptr_t effect;
        std::uintptr_t vtable;
        std::uint32_t duration_bits;
        std::uint32_t start_fraction_bits;
        std::uint32_t time_remaining_bits;
        std::uint32_t time_in_transition_bits;
        std::uint16_t flags;
        std::uint8_t class_id;
        std::uint8_t state_machine_index;
        std::uint8_t blend_curve;
        std::uint8_t end_mode;
        std::uint8_t self_mode;
        std::uint8_t event_mode;
        std::uint8_t pad[2];
    };

    struct TransitionProbeChild
    {
        std::uintptr_t generator;
        std::uintptr_t vtable;
        std::uint32_t weight_bits;
        std::uint32_t world_from_model_bits;
        std::uint8_t class_id;
        std::uint8_t pad[3];
    };

    struct TransitionProbeBlender
    {
        std::uintptr_t node;
        std::uintptr_t vtable;
        std::uint32_t blend_parameter_bits;
        std::uint32_t end_interval_weight_bits;
        std::int32_t num_active_children;
        std::int16_t begin_interval_index;
        std::int16_t end_interval_index;
        std::uint16_t children_count;
        std::uint16_t internal_count;
        std::uint8_t name_length;
        std::uint8_t overflow;
        char name[Transition_Probe_Name_Length];
        std::array<TransitionProbeChild, Transition_Probe_Child_Capacity> children;
        std::array<std::uint16_t, Transition_Probe_Internal_Capacity> internals;
    };

    struct TransitionProbeRecord
    {
        std::uint32_t magic;
        std::uint16_t version;
        std::uint8_t side;
        std::uint8_t segment;
        std::uint8_t phase;
        std::uint8_t reserved0;
        std::uint16_t flags;
        std::uint16_t state_machine_count;
        std::uint16_t effect_count;
        std::uint16_t blender_count;
        std::uint32_t sequence;
        std::uint32_t commanded;
        std::uint32_t probe_seq;
        std::uint32_t reserved1;
        std::uint64_t tsc;
        std::uintptr_t graph;
        std::uint32_t actor;
        std::uint32_t reserved;
        std::uint64_t request;
        std::uint32_t walked;
        std::uint32_t active_nodes;
        std::array<TransitionProbeStateMachine, Transition_Probe_State_Machine_Capacity> state_machines;
        std::array<TransitionProbeEffect, Transition_Probe_Effect_Capacity> effects;
        std::array<TransitionProbeBlender, Transition_Probe_Blender_Capacity> blenders;
    };

    static_assert(std::is_trivially_copyable_v<TransitionProbeRecord>);
    static_assert(sizeof(TransitionProbeRecord) <= Transition_Probe_Record_Budget);

    inline char const* transition_probe_side_name(TransitionProbeSide a_side) noexcept
    {
        return a_side == TransitionProbeSide::source ? "source" : "candidate";
    }

    inline char const* transition_probe_segment_name(TransitionProbeSegment a_segment) noexcept
    {
        switch (a_segment)
        {
            case TransitionProbeSegment::equip:
                return "equip";
            case TransitionProbeSegment::unequip:
                return "unequip";
            case TransitionProbeSegment::window:
                return "window";
        }
        return "unknown";
    }

    inline char const* transition_probe_phase_name(TransitionProbePhase a_phase) noexcept
    {
        switch (a_phase)
        {
            case TransitionProbePhase::post_update:
                return "post-update";
            case TransitionProbePhase::pre_sample:
                return "pre-sample";
            case TransitionProbePhase::post_sample:
                return "post-sample";
        }
        return "unknown";
    }

    inline char const* transition_probe_class_name(TransitionProbeNodeClass a_class) noexcept
    {
        switch (a_class)
        {
            case TransitionProbeNodeClass::clip:
                return "clip";
            case TransitionProbeNodeClass::state_machine:
                return "state-machine";
            case TransitionProbeNodeClass::blender:
                return "blender";
            case TransitionProbeNodeClass::manual_selector:
                return "manual-selector";
            case TransitionProbeNodeClass::blending_transition:
                return "blending-transition";
            case TransitionProbeNodeClass::generator_transition:
                return "generator-transition";
            case TransitionProbeNodeClass::behavior_reference:
                return "behavior-reference";
            case TransitionProbeNodeClass::modifier_generator:
                return "modifier-generator";
            case TransitionProbeNodeClass::other:
                return "other";
            case TransitionProbeNodeClass::unknown:
                break;
        }
        return "unknown";
    }

    inline char const* transition_subgraph_status_name(TransitionSubgraphStatus a_status) noexcept
    {
        switch (a_status)
        {
            case TransitionSubgraphStatus::ok:
                return "ok";
            case TransitionSubgraphStatus::unavailable:
                return "unavailable";
            case TransitionSubgraphStatus::unknown_behavior_class:
                return "unknown-behavior-class";
            case TransitionSubgraphStatus::no_active_nodes:
                return "no-active-nodes";
            case TransitionSubgraphStatus::invalid_array:
                return "invalid-array";
            case TransitionSubgraphStatus::inactive:
                return "inactive";
        }
        return "unavailable";
    }

    [[nodiscard]] constexpr bool transition_subgraph_window_matches(
        TransitionProbeSegment a_segment, std::uint32_t a_commanded) noexcept
    {
        switch (a_segment)
        {
            case TransitionProbeSegment::equip:
                return a_commanded >= 1 &&
                    a_commanded <= Transition_Subgraph_Equip_Max_Commanded;
            case TransitionProbeSegment::unequip:
                return a_commanded >= 1 &&
                    a_commanded <= Transition_Subgraph_Unequip_Max_Commanded;
            case TransitionProbeSegment::window:
                return false;
        }
        return false;
    }

    inline void transition_probe_copy_name(char (&a_dest)[Transition_Probe_Name_Length],
        std::string_view a_name) noexcept
    {
        std::size_t const length = a_name.size() < Transition_Probe_Name_Length - 1
            ? a_name.size() : Transition_Probe_Name_Length - 1;
        if (length > 0)
            std::memcpy(a_dest, a_name.data(), length);
        a_dest[length] = '\0';
    }

    [[nodiscard]] inline bool transition_probe_overflowed(TransitionProbeRecord const& a_record) noexcept
    {
        return (a_record.flags & Transition_Probe_Flag_Overflow_Mask) != 0;
    }

    // Read-only capture at the existing K5 candidate/source point. The caller supplies the
    // same sequence key as the K5 record (candidate completed / source seq). No allocation,
    // no logging and no graph mutation occurs here.
    void capture_transition_probe(RE::BShkbAnimationGraph const& a_graph,
        TransitionProbeSide a_side, TransitionProbeSegment a_segment,
        TransitionProbePhase a_phase, std::uint32_t a_sequence,
        std::uint32_t a_commanded, std::uint32_t a_probe_seq, std::uint64_t a_tsc,
        TransitionProbeRecord& a_out) noexcept;

    // Formatting and logging only; call on the candidate sample point or the source render drain.
    void log_transition_probe(TransitionProbeRecord const& a_record) noexcept;

    // Bounded candidate-only active-node census used to locate the equipment Blender.
    // Read-only; logs vtable/class/name/id and never reads an unknown class's fields.
    void log_transition_census(RE::BShkbAnimationGraph const& a_graph, char const* a_segment,
        std::uint32_t a_commanded) noexcept;

    // Candidate-only, log-only modifier/binding probe for the target-state scalar. Uses the
    // HKX2 hkbModifierGenerator layout (+0x48 modifier / +0x50 generator) with guarded reads;
    // it does not enter TransitionProbeRecord or the source ring.
    void log_transition_modifiers(RE::BShkbAnimationGraph const& a_graph, char const* a_segment,
        TransitionProbePhase a_phase, std::uint32_t a_sequence, std::uint32_t a_commanded) noexcept;

    // Candidate-only, log-only subgraph probe for the WeapEquipBFR route. It follows the
    // audited hkbBehaviorReferenceGenerator behavior pointer (+0x50) into the referenced
    // hkbBehaviorGraph and enumerates its activeNodes; every pointer is guarded and no
    // graph function is called. It does not enter TransitionProbeRecord or the source ring.
    void log_transition_subgraph(RE::BShkbAnimationGraph const& a_graph,
        TransitionProbeSegment a_segment, TransitionProbePhase a_phase,
        std::uint32_t a_sequence, std::uint32_t a_commanded) noexcept;

    // Candidate-only AE 1.6.1170 parent chain evidence from the existing K5 observations.
    void log_transition_parent_chain(RE::BShkbAnimationGraph const& a_graph,
        TransitionProbeRecord const& a_probe) noexcept;

    // C-G5-CHAIN v1 (2026-11-21, pre-registered in docs/diagnostics/g5-idle-chain-probe-20261121/):
    // idle-settle three-way comparison run symmetrically on the held candidate and the live
    // source graph at the two F7 display checkpoints. Per side: every active clip generator is
    // sampled at that side's own control localTime and compared against that side's poseLocal
    // on the name-resolved interest tracks, plus a bounded active-node census of both graphs.
    // Read-only; the sample buffers are probe-local; no graph, pose or variable is written.
    void log_idle_chain_probe(RE::BShkbAnimationGraph const& a_candidate,
        RE::BShkbAnimationGraph const& a_source, std::uint32_t a_actor, std::uint64_t a_request,
        std::uint64_t a_view, std::uint32_t a_call) noexcept;
}
PLUGIN_NAMESPACE_END
