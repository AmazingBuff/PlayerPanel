//
// Created by AmazingBuff on 2026/11/27.
//
#pragma once

#include <cstdint>

namespace RE
{
    class hkaAnimationBinding;
}

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
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
}
PLUGIN_NAMESPACE_END
