//
// C-G5-CAL K2-C4 control B carrier (plan A, 2026-11-14).
//
// Read-only: finds the active MT_Idle clip whose animation bytes match the
// pre-registered authored resource, then samples that animation directly at the
// fixed registered times. It never writes the graph, never advances it and never
// reads poseLocal as an oracle.
#pragma once

#include <cstddef>
#include <cstdint>

namespace RE
{
    class hkbCharacter;
    class hkaAnimationBinding;
}

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // Read-only identity of one loaded animation binding. Used by the control B
    // carrier check and by the K1 clip/resource census; spline payload hashes are
    // filled only when the animation is spline-compressed and the buffers are
    // readable.
    struct AnimationResourceIdentity
    {
        bool readable{};
        bool spline{};
        std::uint32_t type{};
        std::uint32_t duration_bits{};
        std::int32_t transform_tracks{};
        std::int32_t float_tracks{};
        std::uint32_t num_frames{};
        std::uint32_t num_blocks{};
        std::uint32_t max_frames_per_block{};
        std::uint32_t block_duration_bits{};
        std::uint32_t block_inverse_duration_bits{};
        std::uint32_t frame_duration_bits{};
        std::uint32_t data_bytes{};
        std::uint64_t data_hash{};
        std::uint64_t block_offsets_hash{};
        std::uint32_t map_count{};
        std::uint64_t map_hash{};
        std::uint32_t blend_hint{};
    };

    [[nodiscard]] bool read_animation_resource_identity(RE::hkaAnimationBinding const* a_binding,
        AnimationResourceIdentity& a_out);

    // Returns true only when exactly one qualified carrier was found and every
    // registered sample was read twice with bitwise-equal results.
    [[nodiscard]] bool run_control_sample(RE::hkbCharacter& a_character, std::uint32_t a_actor_id,
        std::uint64_t a_request, std::size_t a_completed);
}
PLUGIN_NAMESPACE_END
