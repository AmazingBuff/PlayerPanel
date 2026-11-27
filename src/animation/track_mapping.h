//
// Created by AmazingBuff on 2026/11/27.
//
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

PLUGIN_NAMESPACE_BEGIN

struct AnimationTrackBinding
{
    std::uint16_t track;
    std::uint16_t bone;
};

[[nodiscard]] constexpr bool has_unmapped_track_overflow(
    std::span<std::int16_t const> table, std::size_t track_count, std::size_t bone_count)
{
    return table.empty() && track_count > bone_count;
}

[[nodiscard]] inline std::vector<AnimationTrackBinding> resolve_animation_tracks(
    std::span<std::int16_t const> table, std::size_t track_count, std::size_t bone_count)
{
    if (has_unmapped_track_overflow(table, track_count, bone_count))
        return {};

    const std::size_t count = table.empty() ? track_count : std::min(track_count, table.size());
    std::vector<AnimationTrackBinding> bindings;
    bindings.reserve(count);
    for (std::size_t track = 0; track < count; ++track)
    {
        const std::int32_t bone = table.empty() ? static_cast<std::int32_t>(track) : table[track];
        if (bone < 0 || static_cast<std::size_t>(bone) >= bone_count)
            continue;
        bindings.push_back({ static_cast<std::uint16_t>(track), static_cast<std::uint16_t>(bone) });
    }
    return bindings;
}

PLUGIN_NAMESPACE_END
