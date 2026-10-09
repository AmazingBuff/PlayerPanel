//
// Created by AmazingBuff on 2026/10/9.
//

#pragma once

#include "RE/H/hkQsTransform.h"
#include "RE/N/NiAVObject.h"
#include "RE/N/NiMatrix3.h"
#include "RE/N/NiTransform.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <xmmintrin.h>

namespace RE
{
    class TESObjectREFR;
}

PLUGIN_NAMESPACE_BEGIN

// One pose sample as plain numbers: Havok stores its vectors as SSE quads with no element accessor,
// and keeping the conversion in plain floats is what lets it be unit-tested without a game.
struct PoseSample
{
    float position[3];
    float quaternion[4];  // x, y, z, w
    float scale;
};

[[nodiscard]] inline PoseSample read_pose(RE::hkQsTransform const& pose)
{
    alignas(16) float translation[4];
    alignas(16) float rotation[4];
    alignas(16) float scale[4];
    _mm_store_ps(translation, pose.translation.quad);
    _mm_store_ps(rotation, pose.rotation.vec.quad);
    _mm_store_ps(scale, pose.scale.quad);

    PoseSample sample{};
    sample.position[0] = translation[0];
    sample.position[1] = translation[1];
    sample.position[2] = translation[2];
    sample.quaternion[0] = rotation[0];
    sample.quaternion[1] = rotation[1];
    sample.quaternion[2] = rotation[2];
    sample.quaternion[3] = rotation[3];
    sample.scale = scale[0];
    return sample;
}

// The rotation a unit quaternion (x, y, z, w) describes, in the convention this codebase measured
// rather than assumed: A * B applies B first, so a rotation multiplies a column vector and
// entry[row][col] is that row and column.
[[nodiscard]] inline RE::NiMatrix3 ni_matrix_from_quaternion(float x, float y, float z, float w)
{
    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    const float xy = x * y;
    const float xz = x * z;
    const float yz = y * z;
    const float xw = x * w;
    const float yw = y * w;
    const float zw = z * w;

    RE::NiMatrix3 matrix;
    matrix.entry[0][0] = 1.0f - 2.0f * (yy + zz);
    matrix.entry[0][1] = 2.0f * (xy - zw);
    matrix.entry[0][2] = 2.0f * (xz + yw);
    matrix.entry[1][0] = 2.0f * (xy + zw);
    matrix.entry[1][1] = 1.0f - 2.0f * (xx + zz);
    matrix.entry[1][2] = 2.0f * (yz - xw);
    matrix.entry[2][0] = 2.0f * (xz - yw);
    matrix.entry[2][1] = 2.0f * (yz + xw);
    matrix.entry[2][2] = 1.0f - 2.0f * (xx + yy);
    return matrix;
}

// A pose sample as the local transform of the node it drives. `transposed` builds the inverse
// rotation instead: the replay writes both candidates and reports which one lands the copy on the
// source, so the engine's quaternion convention is measured at capture rather than assumed here.
[[nodiscard]] inline RE::NiTransform ni_local_from_pose(PoseSample const& sample, bool transposed)
{
    RE::NiTransform local;
    local.rotate = ni_matrix_from_quaternion(sample.quaternion[0], sample.quaternion[1], sample.quaternion[2], sample.quaternion[3]);
    if (transposed)
        local.rotate = local.rotate.Transpose();
    local.translate = RE::NiPoint3{ sample.position[0], sample.position[1], sample.position[2] };
    local.scale = sample.scale;
    return local;
}

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
// `bone_node_out` receives the node index each bone resolved to (-1 when it did not) and may be
// empty when only the counts are wanted.
[[nodiscard]] inline SkeletonAlignment align_skeleton(
    std::span<std::string_view const> bone_names,
    std::span<std::int16_t const> bone_parents,
    std::span<std::string_view const> node_names,
    std::span<std::int32_t const> node_parents,
    std::span<std::int32_t> bone_node_out,
    size_t max_names_reported)
{
    SkeletonAlignment alignment{};

    for (std::int32_t& slot : bone_node_out)
        slot = -1;

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
        if (i < bone_node_out.size())
            bone_node_out[i] = bone_node[i];
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

// Whether a candidate track-to-bone table is structurally valid for a skeleton of `bone_count`
// bones: at least one track, and every entry either names a bone of this skeleton or is -1 for a
// track without one. This is what tells a real animation binding from a misread pointer without
// calling anything on the candidate.
[[nodiscard]] inline bool track_indices_are_valid(std::span<std::int16_t const> track_to_bone, std::int32_t bone_count)
{
    if (track_to_bone.empty() || bone_count <= 0)
        return false;
    for (std::int16_t index : track_to_bone)
    {
        if (index < -1 || index >= bone_count)
            return false;
    }
    return true;
}

// Read-only reconnaissance of the source character's animation graphs: which graphs exist, which of
// them holds the graph that was captured, what each one's animation skeleton is, and how that
// skeleton resolves against the copy. Runs at capture, while the game is paused; it writes nothing,
// neither into the source character nor into the engine's graphs.
void report_animation_source(RE::TESObjectREFR& source, RE::NiAVObject& source_root, RE::NiAVObject& copy_root);

// Writes the source character's own current pose into the copy and measures how far the copy lands
// from the source, then puts the captured pose back. The engine holds that pose in
// hkbCharacter::poseLocal, so this needs no clip, no binding set and no untyped layout, and it
// exercises the whole write path the sampling route will use: bone → node by name, quaternion →
// NiMatrix3, local write, one downward world recompute.
//
// Every candidate is compared with one instrument, including a control posed away from the source by
// a known amount: without it a zero delta would only say the measurement is insensitive. The
// candidates are the two orders the pose could be indexed in (the animation skeleton's, and the
// engine's own boneNodes table, which the alignment round proved is NOT the skeleton's order) times
// the two quaternion conventions (direct and transposed) - the verdict names the one that reproduces
// the source.
void verify_pose_replay(RE::TESObjectREFR& source, RE::NiAVObject& source_root, RE::NiAVObject& copy_root);

// Reads what the character's own animation list says and validates the two candidate layouts of the
// binding set, without believing either: the engine's animation *names* are typed
// (hkbCharacterStringData::animationNames), the binding elements are not, so each candidate is
// checked structurally - its animation pointer has to look like a live object, its duration has to
// be a plausible clip length, and its track-to-bone table has to name only bones this skeleton has -
// before anything is read through it. Nothing virtual is called on a candidate and nothing is
// written, so a misread pointer reports nonsense instead of crashing.
void report_animation_catalogue(RE::TESObjectREFR& source, RE::NiAVObject& source_root, RE::NiAVObject& copy_root);

PLUGIN_NAMESPACE_END
