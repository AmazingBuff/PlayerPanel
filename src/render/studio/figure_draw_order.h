//
// Created by AmazingBuff on 2026/10/15.
//

#pragma once

#include <algorithm>
#include <cmath>
#include <span>

namespace RE
{
    class BSGeometry;
}

PLUGIN_NAMESPACE_BEGIN

struct FigureGeometry
{
    bool alpha_blend;
    bool alpha_test;
    bool no_sort;
    float depth;
    RE::BSGeometry* geometry;
};

inline void order_figure_geometry(std::span<FigureGeometry> geometries)
{
    const auto blended = std::stable_partition(geometries.begin(), geometries.end(), [](FigureGeometry const& geometry)
    {
        return !geometry.alpha_blend;
    });
    const auto farther_first = [](FigureGeometry const& left, FigureGeometry const& right)
    {
        const float left_depth = std::isfinite(left.depth) ? left.depth : 0.0f;
        const float right_depth = std::isfinite(right.depth) ? right.depth : 0.0f;
        return left_depth > right_depth;
    };

    // A No Sorter surface keeps its insertion position within the alpha stage. Sorting must not
    // move another surface across that author-specified boundary.
    auto first = blended;
    for (auto current = blended; current != geometries.end(); ++current)
    {
        if (current->no_sort)
        {
            std::stable_sort(first, current, farther_first);
            first = current + 1;
        }
    }
    std::stable_sort(first, geometries.end(), farther_first);
}

PLUGIN_NAMESPACE_END
