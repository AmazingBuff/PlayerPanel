//
// Created by AmazingBuff on 2026/10/13.
//
#pragma once
#include "base/def.h"
#include "RE/N/NiAVObject.h"
#include <string_view>
PLUGIN_NAMESPACE_BEGIN
[[nodiscard]] inline bool figure_is_collision_helper(std::string_view a_name)
{
    if (a_name.empty())
        return false;
    constexpr std::string_view tokens[] = { "Collision", "VirtualGround", "3BCA_", "Stopper" };
    for (const std::string_view token : tokens)
        if (a_name.find(token) != std::string_view::npos)
            return true;
    return false;
}

struct FigureAssembly
{
    std::uint32_t geometries = 0;
    std::uint32_t skinned = 0;
    std::uint32_t alloc_slots = 0;
    std::uint32_t bound_nodes = 0;
    [[nodiscard]] size_t signature() const
    {
        size_t seed = Amazing_Hash;
        seed = hash_combine(seed, geometries);
        seed = hash_combine(seed, skinned);
        seed = hash_combine(seed, bound_nodes);
        return hash_combine(seed, alloc_slots);
    }
};
[[nodiscard]] FigureAssembly figure_measure_assembly(RE::NiAVObject& root);
PLUGIN_NAMESPACE_END
