//
// Created by AmazingBuff on 2026/11/27.
//
#include "animation/behavior_graph/animation_resource_identity.h"

#include "animation/behavior_graph/observation_chain_policy.h"

#include "RE/H/hkaAnimation.h"
#include "RE/H/hkaAnimationBinding.h"
#include "RE/H/hkaSplineCompressedAnimation.h"

#include <algorithm>
#include <bit>
#include <cstddef>

// Keep REX's named constants available when winnt.h has defined the same macros.
#undef MEM_COMMIT
#undef PAGE_NOACCESS

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
namespace
{
    bool readable(void const* a_address, std::size_t a_bytes)
    {
        if (!a_address || a_bytes == 0)
            return false;
        REX::W32::MEMORY_BASIC_INFORMATION info{};
        if (REX::W32::VirtualQuery(a_address, &info, sizeof(info)) == 0)
            return false;
        if (info.state != REX::W32::MEM_COMMIT || info.protect == REX::W32::PAGE_NOACCESS)
            return false;
        auto const* const begin = static_cast<std::uint8_t const*>(a_address);
        auto const* const end = static_cast<std::uint8_t const*>(info.baseAddress) + info.regionSize;
        return begin + a_bytes <= end;
    }

    [[nodiscard]] std::uint64_t fnv1a64(void const* a_data, std::size_t a_size)
    {
        auto const* const bytes = static_cast<std::uint8_t const*>(a_data);
        std::uint64_t hash = 0xcbf29ce484222325ull;
        for (std::size_t i = 0; i < a_size; ++i)
        {
            hash ^= bytes[i];
            hash *= 0x100000001b3ull;
        }
        return hash;
    }
}

bool read_animation_resource_identity(RE::hkaAnimationBinding const* a_binding,
    AnimationResourceIdentity& a_out)
{
    a_out = {};
    if (!a_binding || !plausible_object_pointer(reinterpret_cast<std::uintptr_t>(a_binding)))
        return false;

    a_out.blend_hint = static_cast<std::uint32_t>(a_binding->blendHint.get());
    auto const& map = a_binding->transformTrackToBoneIndices;
    a_out.map_count = static_cast<std::uint32_t>(std::max(map.size(), 0));
    if (a_out.map_count != 0)
    {
        if (!readable(map.data(), static_cast<std::size_t>(a_out.map_count) * sizeof(std::int16_t)))
            return false;
        a_out.map_hash = fnv1a64(map.data(),
            static_cast<std::size_t>(a_out.map_count) * sizeof(std::int16_t));
    }

    RE::hkaAnimation const* const animation = a_binding->animation.get();
    if (!animation || !plausible_object_pointer(reinterpret_cast<std::uintptr_t>(animation)) ||
        !readable(animation, sizeof(*animation)))
        return false;
    a_out.type = static_cast<std::uint32_t>(animation->type.get());
    a_out.duration_bits = std::bit_cast<std::uint32_t>(animation->duration);
    a_out.transform_tracks = animation->numberOfTransformTracks;
    a_out.float_tracks = animation->numberOfFloatTracks;
    if (a_out.type != static_cast<std::uint32_t>(RE::hkaAnimation::AnimationType::kSplineCompressedAnimation))
    {
        a_out.readable = true;
        return true;
    }

    auto const* const spline = static_cast<RE::hkaSplineCompressedAnimation const*>(animation);
    if (!readable(spline, sizeof(*spline)))
        return false;
    a_out.spline = true;
    a_out.num_frames = static_cast<std::uint32_t>(spline->numFrames);
    a_out.num_blocks = static_cast<std::uint32_t>(spline->numBlocks);
    a_out.max_frames_per_block = static_cast<std::uint32_t>(spline->maxFramesPerBlock);
    a_out.block_duration_bits = std::bit_cast<std::uint32_t>(spline->blockDuration);
    a_out.block_inverse_duration_bits = std::bit_cast<std::uint32_t>(spline->blockInverseDuration);
    a_out.frame_duration_bits = std::bit_cast<std::uint32_t>(spline->frameDuration);
    if (spline->data.size() < 0 || !readable(spline->data.data(), static_cast<std::size_t>(spline->data.size())))
        return false;
    a_out.data_bytes = static_cast<std::uint32_t>(spline->data.size());
    a_out.data_hash = fnv1a64(spline->data.data(), a_out.data_bytes);
    if (spline->blockOffsets.size() < 0 ||
        !readable(spline->blockOffsets.data(), static_cast<std::size_t>(spline->blockOffsets.size()) * sizeof(std::uint32_t)))
        return false;
    a_out.block_offsets_hash = fnv1a64(spline->blockOffsets.data(),
        static_cast<std::size_t>(spline->blockOffsets.size()) * sizeof(std::uint32_t));
    a_out.readable = true;
    return true;
}
}
PLUGIN_NAMESPACE_END
