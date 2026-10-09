//
// Created by AmazingBuff on 2026/10/9.
//

#pragma once

#include "RE/N/NiAVObject.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace RE
{
    class TESObjectREFR;
}

PLUGIN_NAMESPACE_BEGIN

// The engine's animation skeleton lined up against the copy's node tree: the mapping the S2
// engine-animation route (docs/s2-animation-handoff.md) has to establish before anything samples a
// clip into the copy. Bones resolve to nodes by exact name, because the copy carries several
// skeletons plus modded hair and cloth chains whose node names contain the same words, and the S2
// probe already drove the wrong skeleton by matching loosely.
//
// The skeleton's own parent relation is reported, not required: this rig's pelvis and spine hang
// under different parents (CME LBody / CME UBody), so a skeleton hierarchy that disagrees with the
// node hierarchy is expected here and says nothing about whether the names resolved. What the write
// path needs is that every bone has a node; it writes local transforms and lets the node hierarchy
// compose them.
struct SkeletonAlignment
{
    size_t bones;              // bones in the animation skeleton
    size_t matched;            // bones whose name is exactly a node name of the copy
    size_t ambiguous;          // of the matched, bones whose name is shared by more than one node
    size_t duplicate_nodes;    // nodes sharing their name with another node
    size_t parent_ancestors;   // matched bones whose parent bone is an ancestor of that node
    std::string missing;       // unmatched bone names, up to the report cap
    std::string wrong_parent;  // matched bones whose parent bone resolves to a non-ancestor node
};

// Whether one node sits strictly below another in the node hierarchy. Bounded by the table size, so
// a cycle in the data cannot hang the walk.
[[nodiscard]] inline bool node_has_ancestor(std::span<std::int32_t const> node_parents, std::int32_t node, std::int32_t ancestor)
{
    for (size_t step = 0; step < node_parents.size(); ++step)
    {
        if (node < 0 || static_cast<size_t>(node) >= node_parents.size())
            return false;
        node = node_parents[static_cast<size_t>(node)];
        if (node == ancestor)
            return true;
    }
    return false;
}

// Pure mapping rules, unit-tested without a running game. Both tables are indexed like their names
// and hold -1 at a root; an out-of-range parent reads as a root. An unnamed bone cannot resolve and
// is not printed, so `bones - matched` is the number of unresolved bones including those.
[[nodiscard]] inline SkeletonAlignment align_skeleton(
    std::span<std::string_view const> bone_names,
    std::span<std::int16_t const> bone_parents,
    std::span<std::string_view const> node_names,
    std::span<std::int32_t const> node_parents,
    size_t max_names_reported)
{
    SkeletonAlignment alignment{};

    std::unordered_map<std::string_view, std::vector<std::int32_t>> nodes_by_name;
    nodes_by_name.reserve(node_names.size());
    for (size_t i = 0; i < node_names.size(); ++i)
        nodes_by_name[node_names[i]].push_back(static_cast<std::int32_t>(i));
    for (auto const& entry : nodes_by_name)
    {
        if (entry.second.size() > 1)
            alignment.duplicate_nodes += entry.second.size();
    }

    alignment.bones = bone_names.size();
    std::vector<std::int32_t> bone_node(bone_names.size(), -1);
    size_t missing_reported = 0;
    for (size_t i = 0; i < bone_names.size(); ++i)
    {
        const auto found = bone_names[i].empty() ? nodes_by_name.end() : nodes_by_name.find(bone_names[i]);
        if (found == nodes_by_name.end())
        {
            if (!bone_names[i].empty() && missing_reported < max_names_reported)
            {
                if (!alignment.missing.empty())
                    alignment.missing += ", ";
                alignment.missing += bone_names[i];
                ++missing_reported;
            }
            continue;
        }
        if (found->second.size() > 1)
            ++alignment.ambiguous;
        bone_node[i] = found->second.front();
        ++alignment.matched;
    }

    size_t wrong_parent_reported = 0;
    for (size_t i = 0; i < bone_names.size(); ++i)
    {
        if (bone_node[i] < 0)
            continue;
        const std::int32_t parent_bone = i < bone_parents.size() ? bone_parents[i] : -1;
        // A root, an out-of-range index and an unresolved parent cannot contradict the hierarchy.
        if (parent_bone < 0 || static_cast<size_t>(parent_bone) >= bone_node.size() || bone_node[parent_bone] < 0)
        {
            ++alignment.parent_ancestors;
            continue;
        }
        if (node_has_ancestor(node_parents, bone_node[i], bone_node[parent_bone]))
        {
            ++alignment.parent_ancestors;
            continue;
        }
        if (wrong_parent_reported < max_names_reported)
        {
            if (!alignment.wrong_parent.empty())
                alignment.wrong_parent += ", ";
            alignment.wrong_parent += bone_names[i];
            ++wrong_parent_reported;
        }
    }

    return alignment;
}

// The gate the sampling route rests on: every bone of the skeleton names a node of the copy.
[[nodiscard]] inline bool alignment_resolves_all_bones(SkeletonAlignment const& alignment)
{
    return alignment.bones != 0 && alignment.matched == alignment.bones;
}

// Read-only reconnaissance of the source character's animation graphs: which graphs exist, which of
// them holds the graph that was captured, what each one's animation skeleton is, and how that
// skeleton resolves against the copy. Runs at capture, while the game is paused; it writes nothing,
// neither into the source character nor into the engine's graphs.
void report_animation_source(RE::TESObjectREFR& source, RE::NiAVObject& source_root, RE::NiAVObject& copy_root);

PLUGIN_NAMESPACE_END
