//
// Created by AmazingBuff on 2026/11/20.
//
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    inline constexpr std::size_t Parent_Chain_Active_Node_Limit = 256;
    inline constexpr std::size_t Parent_Chain_Child_Index_Limit = 4096;
    inline constexpr std::size_t Parent_Chain_Node_Limit = 64;
    inline constexpr std::size_t Parent_Chain_Edge_Limit = 128;

    struct ParentChainChildRange
    {
        std::int32_t first;
        std::int32_t count;
    };

    struct ParentChainEdge
    {
        std::uint16_t parent;
        std::int32_t child;
        std::uint16_t slot;
    };

    enum class ParentChainStatus : std::uint8_t
    {
        e_ok,
        e_overflow,
        e_invalid_range,
        e_invalid_index,
        e_cycle,
    };

    struct ParentChainTopology
    {
        std::array<bool, Parent_Chain_Active_Node_Limit> path{};
        std::array<ParentChainEdge, Parent_Chain_Edge_Limit> edges{};
        std::size_t node_count{};
        std::size_t edge_count{};
        std::size_t empty_slots{};
        std::size_t roots{};
        ParentChainStatus status{ ParentChainStatus::e_ok };
    };

    // The path includes every incoming edge to the seed or to one of its
    // ancestors. Output aliases are deliberately outside this input.
    [[nodiscard]] inline ParentChainTopology trace_parent_chain(
        std::span<ParentChainChildRange const> a_ranges,
        std::span<std::int32_t const> a_children, std::size_t a_seed) noexcept
    {
        ParentChainTopology out{};
        if (a_ranges.size() > Parent_Chain_Active_Node_Limit ||
            a_children.size() > Parent_Chain_Child_Index_Limit)
        {
            out.status = ParentChainStatus::e_overflow;
            return out;
        }
        if (a_seed >= a_ranges.size())
        {
            out.status = ParentChainStatus::e_invalid_index;
            return out;
        }

        // Build incoming links once. A slot can be in more than one recorded
        // range, so use a separate entry for each parent/slot occurrence.
        struct IncomingEdge
        {
            std::uint16_t parent;
            std::uint16_t slot;
            std::uint16_t next;
        };
        constexpr std::uint16_t no_edge = 0xFFFF;
        std::array<std::uint16_t, Parent_Chain_Active_Node_Limit> incoming;
        incoming.fill(no_edge);
        std::array<IncomingEdge, Parent_Chain_Child_Index_Limit> links;
        std::size_t link_count = 0;

        for (std::size_t parent = 0; parent < a_ranges.size(); ++parent)
        {
            ParentChainChildRange const range = a_ranges[parent];
            if (range.count == 0 && range.first == -1)
                continue;
            if (range.first < 0 || range.count < 0 ||
                static_cast<std::size_t>(range.first) > a_children.size() ||
                static_cast<std::size_t>(range.count) >
                    a_children.size() - static_cast<std::size_t>(range.first))
            {
                out.status = ParentChainStatus::e_invalid_range;
                return out;
            }
            std::size_t const start = static_cast<std::size_t>(range.first);
            std::size_t const end = start + static_cast<std::size_t>(range.count);
            for (std::size_t slot = start; slot < end; ++slot)
            {
                std::int32_t const child = a_children[slot];
                if (child == -1)
                {
                    ++out.empty_slots;
                    continue;
                }
                if (child < 0 || static_cast<std::size_t>(child) >= a_ranges.size())
                {
                    out.status = ParentChainStatus::e_invalid_index;
                    return out;
                }
                if (link_count == links.size())
                {
                    out.status = ParentChainStatus::e_overflow;
                    return out;
                }
                std::size_t const child_index = static_cast<std::size_t>(child);
                links[link_count] = { static_cast<std::uint16_t>(parent),
                    static_cast<std::uint16_t>(slot), incoming[child_index] };
                incoming[child_index] = static_cast<std::uint16_t>(link_count++);
            }
        }

        std::array<std::uint16_t, Parent_Chain_Node_Limit> pending;
        pending[0] = static_cast<std::uint16_t>(a_seed);
        out.node_count = 1;
        out.path[a_seed] = true;
        for (std::size_t i = 0; i < out.node_count; ++i)
        {
            std::uint16_t const child = pending[i];
            if (incoming[child] == no_edge)
                ++out.roots;
            for (std::uint16_t link = incoming[child]; link != no_edge; link = links[link].next)
            {
                IncomingEdge const& edge = links[link];
                if (out.edge_count == out.edges.size() ||
                    (!out.path[edge.parent] && out.node_count == pending.size()))
                {
                    out.status = ParentChainStatus::e_overflow;
                    return out;
                }
                out.edges[out.edge_count++] = { edge.parent, child, edge.slot };
                if (!out.path[edge.parent])
                {
                    out.path[edge.parent] = true;
                    pending[out.node_count++] = edge.parent;
                }
            }
        }

        // Verify that every discovered ancestor can be reached from a root.
        std::array<std::uint16_t, Parent_Chain_Active_Node_Limit> parent_counts{};
        for (std::size_t i = 0; i < out.edge_count; ++i)
            ++parent_counts[static_cast<std::size_t>(out.edges[i].child)];
        std::size_t ready = 0;
        for (std::size_t i = 0; i < a_ranges.size(); ++i)
        {
            if (out.path[i] && parent_counts[i] == 0)
                pending[ready++] = static_cast<std::uint16_t>(i);
        }
        for (std::size_t i = 0; i < ready; ++i)
        {
            for (std::size_t j = 0; j < out.edge_count; ++j)
            {
                ParentChainEdge const& edge = out.edges[j];
                if (edge.parent == pending[i] && --parent_counts[static_cast<std::size_t>(edge.child)] == 0)
                    pending[ready++] = static_cast<std::uint16_t>(edge.child);
            }
        }
        if (ready != out.node_count)
            out.status = ParentChainStatus::e_cycle;
        return out;
    }
}
PLUGIN_NAMESPACE_END
