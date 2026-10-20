//
// Created by AmazingBuff on 2026/10/9.
//

#include "animation/clip.h"

#include "character/snapshot_transform.h"

#include "RE/B/BSAnimationGraphManager.h"
#include "RE/B/BShkbAnimationGraph.h"
#include "RE/B/BSVisit.h"
#include "RE/H/hkaAnimation.h"
#include "RE/H/hkaAnimationBinding.h"
#include "RE/H/hkaAnimationControl.h"
#include "RE/H/hkaDefaultAnimationControl.h"
#include "RE/H/hkaBone.h"
#include "RE/H/hkaSkeleton.h"
#include "RE/H/hkaSplineCompressedAnimation.h"
#include "RE/H/hkbBehaviorGraph.h"
#include "RE/H/hkbCharacterSetup.h"
#include "RE/H/hkbClipGenerator.h"
#include "RE/T/TESObjectREFR.h"
#include "REL/Module.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// winnt.h defines macros of these names, which would rewrite the REX constants used below as
// `REX::W32::0x1000`; every Windows header this file needs is already parsed at this point.
#undef MEM_COMMIT
#undef PAGE_NOACCESS

PLUGIN_NAMESPACE_BEGIN

namespace
{
    constexpr std::int32_t Max_Tracks_Read = 4096;

    using NameIndex = std::unordered_map<std::string_view, RE::NiAVObject*>;

    NameIndex index_nodes_by_name(RE::NiAVObject& root)
    {
        NameIndex index;
        RE::BSVisit::TraverseScenegraphObjects(&root, [&](RE::NiAVObject* object)
        {
            const char* const name = object->name.c_str();
            index.emplace(name ? name : "", object);
            return RE::BSVisit::BSVisitControl::kContinue;
        });
        return index;
    }

    bool readable(void const* address, size_t bytes)
    {
        if (!address || bytes == 0)
            return false;
        REX::W32::MEMORY_BASIC_INFORMATION info{};
        if (REX::W32::VirtualQuery(address, &info, sizeof(info)) == 0)
            return false;
        if (info.state != REX::W32::MEM_COMMIT || info.protect == REX::W32::PAGE_NOACCESS)
            return false;
        const auto* const begin = static_cast<const std::uint8_t*>(address);
        const auto* const end = static_cast<const std::uint8_t*>(info.baseAddress) + info.regionSize;
        return begin + bytes <= end;
    }

    bool looks_like_object(void const* object)
    {
        if (!readable(object, sizeof(void*)))
            return false;
        const void* const vtable = *static_cast<void* const*>(object);
        if (!vtable)
            return false;
        const std::uintptr_t value = reinterpret_cast<std::uintptr_t>(vtable);
        for (const REL::Segment::Name name : { REL::Segment::rdata, REL::Segment::data })
        {
            const REL::Segment segment = REL::Module::get().segment(name);
            if (value >= segment.address() && value < segment.address() + segment.size())
                return true;
        }
        return false;
    }

    bool animation_data_is_present(RE::hkaAnimation const* animation)
    {
        if (!animation || !readable(animation, sizeof(RE::hkaAnimation)) || !looks_like_object(animation))
            return false;
        if (static_cast<std::uint32_t>(animation->type.get()) !=
            static_cast<std::uint32_t>(RE::hkaAnimation::AnimationType::kSplineCompressedAnimation))
            return true;

        const auto* const spline = static_cast<const RE::hkaSplineCompressedAnimation*>(animation);
        if (!readable(spline, sizeof(RE::hkaSplineCompressedAnimation)))
            return false;

        // Prefix sampling uses blockOffsets and compressed bytes, not transformOffsets.
        if (spline->numFrames <= 0 || spline->numBlocks <= 0)
            return false;
        if (spline->blockOffsets.size() <= 0 || !spline->blockOffsets.data() ||
            !readable(spline->blockOffsets.data(), sizeof(std::uint32_t) * static_cast<size_t>(spline->blockOffsets.size())))
            return false;
        const std::int32_t bytes = spline->data.size();
        if (bytes <= 0 || !spline->data.data() || !readable(spline->data.data(), static_cast<size_t>(bytes)))
            return false;
        return true;
    }

    void sample_tracks(RE::hkaAnimation const& animation, float time, std::uint32_t count, RE::hkQsTransform* out, float* float_out, std::uint32_t float_count)
    {
        animation.SamplePartialTracks(time, count, out, float_count, float_out, nullptr);
    }


}

PoseBuffer sample_animation_clip(ClipPlayback const& clip, float seconds, bool loop)
{
    if (!clip.valid())
        return {};

    if (!animation_data_is_present(clip.animation))
        return {};

    const float length = clip.duration > 0.05f ? clip.duration : 0.05f;
    float looped = seconds;
    if (loop)
    {
        looped = std::fmod(seconds, length);
        if (looped < 0.0f)
            looped += length;
    }
    else if (looped > length)
        looped = length;  // single-play: the engine blends the final frame out; hold it, never replay

    const std::int32_t sample_count = clip.animation->numberOfTransformTracks;
    if (sample_count <= 0 || sample_count > Max_Tracks_Read)
        return {};
    std::vector<RE::hkQsTransform> sampled(static_cast<std::size_t>(sample_count));
    sample_tracks(*clip.animation, looped, static_cast<std::uint32_t>(sample_count), sampled.data(), nullptr, 0);

    PoseBuffer result;
    for (size_t i = 0; i < clip.tracks.size(); ++i)
    {
        RE::NiAVObject* const node = clip.nodes[i];
        if (!node || static_cast<size_t>(clip.tracks[i]) >= sampled.size())
            continue;
        RE::NiTransform local = ni_local_from_pose(read_pose(sampled[clip.tracks[i]]));
        if (!std::isfinite(local.scale) || local.scale <= 0.0f)
            local.scale = node->local.scale;
        result.emplace_back(node->name.c_str(), local);
    }
    return result;
}

bool resolve_binding_playback(RE::BShkbAnimationGraph& graph, RE::NiAVObject& copy_root,
    RE::hkaAnimationBinding* binding, std::string const& clip_name, ClipPlayback& out,
    std::string& out_mapping, std::string& out_coverage)
{
    if (!binding || !readable(binding, sizeof(*binding)) || !looks_like_object(binding))
        return false;
    RE::hkaAnimation* const animation = binding->animation.get();
    if (!animation || !readable(animation, sizeof(*animation)) || !looks_like_object(animation))
        return false;
    const std::int32_t track_count = animation->numberOfTransformTracks;
    if (track_count <= 0 || track_count > Max_Tracks_Read || !animation_data_is_present(animation) ||
        !std::isfinite(animation->duration) || animation->duration <= 0.0f)
        return false;
    RE::hkaSkeleton const* const skeleton = graph.characterInstance.setup
        ? graph.characterInstance.setup->animationSkeleton.get() : nullptr;
    if (!skeleton || skeleton->bones.empty())
        return false;
    const std::uint32_t bone_nodes = static_cast<std::uint32_t>(graph.boneNodes.size());
    const std::uint32_t pose_count = static_cast<std::uint32_t>(std::max(graph.characterInstance.numPoseLocal, 0));
    // Empty replacement tables retain the existing unverified identity fallback.
    const RE::hkArray<std::int16_t>& table = binding->transformTrackToBoneIndices;
    std::span<std::int16_t const> track_table;
    if (!table.empty())
    {
        if (!readable(table.begin(), static_cast<std::size_t>(table.size()) * sizeof(std::int16_t)))
        {
            logger::info("SCOPY PUPPET resolve UNAVAILABLE reason=track-table-unreadable clip='{}'", clip_name);
            return false;
        }
        track_table = { table.begin(), static_cast<std::size_t>(table.size()) };
        out_mapping = "table";
    }
    else
    {
        out_mapping = "identity-unverified";
    }
    const auto bindings = resolve_animation_tracks(track_table, static_cast<std::size_t>(track_count),
        static_cast<std::size_t>(skeleton->bones.size()));

    const NameIndex copy_by_name = index_nodes_by_name(copy_root);
    out = ClipPlayback{};
    out.animation = animation;
    out.binding_owner.reset(binding);
    out.name = clip_name;
    out.duration = animation->duration;
    out.tracks.reserve(bindings.size());
    out.nodes.assign(bindings.size(), nullptr);
    size_t resolved = 0;
    std::string unresolved;
    // Havok bone names carry stray whitespace ("x_NPC Translate [Pos ]" — the in-game set was
    // full of them), and the third round proved the attachment bones never hit their exact
    // alias either: the unresolved print kept saying 'Weapon' while no variant node matched.
    // Trim before comparing, and quote the print so the next round cannot hide this again.
    const auto trimmed = [](std::string_view name)
    {
        while (!name.empty() && (name.front() == ' ' || name.front() == '\t'))
            name.remove_prefix(1);
        while (!name.empty() && (name.back() == ' ' || name.back() == '\t'))
            name.remove_suffix(1);
        return name;
    };
    for (size_t i = 0; i < bindings.size(); ++i)
    {
        out.tracks.push_back(bindings[i].track);
        const char* const bone_raw = skeleton->bones[bindings[i].bone].name.c_str();
        if (!bone_raw)
            continue;
        const std::string_view bone_name = trimmed(bone_raw);
        std::string_view target_name = bone_name;
        if (bone_name == "Weapon") target_name = "WEAPON";
        else if (bone_name == "Shield") target_name = "SHIELD";
        else if (bone_name == "Quiver") target_name = "QUIVER";
        auto found = copy_by_name.find(target_name);
        if (found == copy_by_name.end() || found->second == &copy_root)
        {
            if (unresolved.size() < 160)
                unresolved.append(fmt::format("{}'{:.60}' ", unresolved.empty() ? "" : "|", bone_name));
            continue;
        }
        out.nodes[i] = found->second;
        ++resolved;
    }
    if (resolved == 0)
    {
        logger::info("SCOPY PUPPET resolve UNAVAILABLE reason=no-copy-nodes clip='{}' tracks={}", clip_name, bindings.size());
        return false;
    }
    out_coverage = fmt::format("tracks={} resolved={} bone-nodes={} pose={} unresolved='{}'",
        bindings.size(), resolved, bone_nodes, pose_count, unresolved);
    return true;
}

PLUGIN_NAMESPACE_END
