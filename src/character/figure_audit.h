//
// Created by AmazingBuff on 2026/10/13.
//
#pragma once
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
    [[nodiscard]] std::uint32_t signature() const
    {
        return geometries * 1000003u ^ skinned * 10007u ^ bound_nodes * 101u ^ alloc_slots;
    }
};
[[nodiscard]] FigureAssembly figure_measure_assembly(RE::NiAVObject& root);
PLUGIN_NAMESPACE_END
