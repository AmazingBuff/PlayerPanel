//
// Created by AmazingBuff on 2026/10/9.
//

#include "character/animation_source.h"

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
#include <unordered_set>
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
    constexpr double Max_Clip_Step_Seconds = 0.1;
    constexpr size_t Max_String_Read = 128;
    constexpr size_t String_Guard_Window = 16;

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

    std::string guarded_string(RE::hkStringPtr const& value)
    {
        const char* const text = value.c_str();
        if (!text)
            return "-";
        std::string out;
        size_t verified = 0;
        for (size_t i = 0; i < Max_String_Read; ++i)
        {
            if (i >= verified)
            {
                const size_t span = std::min(String_Guard_Window, Max_String_Read - i);
                if (readable(text + i, span))
                {
                    verified = i + span;
                }
                else if (readable(text + i, 1))
                {
                    verified = i + 1;  // a string can legitimately end at a page edge; keep going byte-wise
                }
                else
                {
                    return "-";
                }
            }
            const char character = text[i];
            if (character == '\0')
                return out.empty() ? "-" : out;
            if (static_cast<unsigned char>(character) < 0x20 || static_cast<unsigned char>(character) > 0x7e)
                return "-";
            out += character;
        }
        return out.empty() ? "-" : out + "...";
    }

    std::uintptr_t clip_generator_vtable()
    {
        static const std::uintptr_t vtable = RE::hkbClipGenerator::VTABLE[0].address();
        return vtable;
    }
}

bool clip_generator_is_live(void const* object)
{
    if (!object || !readable(object, sizeof(void*)) ||
        *static_cast<const std::uintptr_t*>(object) != clip_generator_vtable())
        return false;
    return readable(object, sizeof(RE::hkbClipGenerator));
}

void collect_running_clip_generators(RE::TESObjectREFR& source, std::vector<ActiveClipEntry>& out)
{
    struct ClipIdentity
    {
        std::string name;
    };

    static std::unordered_map<const RE::hkbClipGenerator*, ClipIdentity> cache;

    RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
    if (!source.GetAnimationGraphManager(manager) || !manager)
        return;

    for (std::uint32_t i = 0; i < manager->graphs.size(); ++i)
    {
        RE::BShkbAnimationGraph* const graph = manager->graphs[i].get();
        if (!graph || !graph->holder || static_cast<RE::TESObjectREFR*>(graph->holder) != &source)
            continue;
        RE::hkbBehaviorGraph* const behavior = graph->characterInstance.behaviorGraph.get();
        if (!behavior || !behavior->activeNodes)
            continue;

        // Read the live typed activation list; inactive template generators carry no usable control.
        for (const RE::hkbNodeInfo& info : *behavior->activeNodes)
        {
            auto* const node = static_cast<const void*>(info.nodeClone);
            if (!clip_generator_is_live(node))
                continue;
            const auto* const generator = static_cast<const RE::hkbClipGenerator*>(node);
            auto known = cache.find(generator);
            if (known == cache.end())
            {
                const std::string name = guarded_string(generator->animationName);
                known = cache.emplace(generator, ClipIdentity{ name }).first;
            }
            out.push_back({ generator, known->second.name });
        }
    }

    if (!out.empty())
    {
        std::unordered_set<const RE::hkbClipGenerator*> running;
        running.reserve(out.size());
        for (const ActiveClipEntry& entry : out)
            running.insert(entry.generator);
        std::erase_if(cache, [&](const auto& entry) { return running.find(entry.first) == running.end(); });
    }
    else
    {
        cache.clear();
    }
}

size_t play_animation_clip(ClipPlayback const& clip, float seconds, RE::NiAVObject& copy_root, bool loop)
{
    if (!clip.valid())
        return 0;

    if (!animation_data_is_present(clip.animation))
        return 0;

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

    const size_t sample_count = clip.tracks.empty() ? 0 : static_cast<size_t>(clip.tracks.back()) + 1;
    if (sample_count == 0)
        return 0;
    std::vector<RE::hkQsTransform> sampled(sample_count);
    sample_tracks(*clip.animation, looped, static_cast<std::uint32_t>(sample_count), sampled.data(), nullptr, 0);

    size_t written = 0;
    for (size_t i = 0; i < clip.tracks.size(); ++i)
    {
        RE::NiAVObject* const node = clip.nodes[i];
        if (!node || static_cast<size_t>(clip.tracks[i]) >= sampled.size())
            continue;
        RE::NiTransform local = ni_local_from_pose(read_pose(sampled[clip.tracks[i]]));
        if (!std::isfinite(local.scale) || local.scale <= 0.0f)
            local.scale = node->local.scale;
        node->local = local;
        ++written;
    }
    recompute_subtree_worlds(copy_root, 0);
    return written;
}

RE::hkbClipGenerator const* pick_running_clip(RE::TESObjectREFR& source)
{
    std::vector<ActiveClipEntry> running;
    collect_running_clip_generators(source, running);
    RE::hkbClipGenerator const* best = nullptr;
    float best_weight = 0.0f;
    for (const ActiveClipEntry& entry : running)
    {
        RE::hkbClipGenerator const* const generator = entry.generator;
        if (!generator || !readable(generator, sizeof(RE::hkbClipGenerator)) || !looks_like_object(generator))
            continue;
        const RE::hkaDefaultAnimationControl* const control = generator->animationControl.get();
        if (!control || !readable(control, sizeof(RE::hkaAnimationControl)) || !looks_like_object(control))
            continue;
        if (control->weight > best_weight)
        {
            best_weight = control->weight;
            best = generator;
        }
    }
    return best;
}

bool resolve_clip_playback(RE::TESObjectREFR& source, RE::NiAVObject& copy_root, RE::hkbClipGenerator const* generator,
    ClipPlayback& out, std::string& out_mapping, std::string& out_coverage)
{
    if (!generator || !readable(generator, sizeof(RE::hkbClipGenerator)) || !looks_like_object(generator))
    {
        logger::info("SCOPY PUPPET resolve UNAVAILABLE reason=generator-lost");
        return false;
    }

    const char* const raw_name = generator->animationName.c_str();
    const std::string clip_name = raw_name ? raw_name : "";
    const RE::hkaDefaultAnimationControl* const control = generator->animationControl.get();
    if (!control || !readable(control, sizeof(RE::hkaAnimationControl)) || !looks_like_object(control))
    {
        logger::info("SCOPY PUPPET resolve UNAVAILABLE reason=no-live-control clip='{}'", clip_name);
        return false;
    }

    const RE::hkaAnimationBinding* const binding = generator->binding;
    if (!binding || !readable(binding, sizeof(RE::hkaAnimationBinding)) || !looks_like_object(binding))
    {
        logger::info("SCOPY PUPPET resolve UNAVAILABLE reason=no-live-binding clip='{}'", clip_name);
        return false;
    }
    RE::hkaAnimation* const animation = binding->animation.get();
    if (!animation || !readable(animation, sizeof(RE::hkaAnimation)) || !looks_like_object(animation))
    {
        logger::info("SCOPY PUPPET resolve UNAVAILABLE reason=no-animation clip='{}'", clip_name);
        return false;
    }
    const std::int32_t track_count = animation->numberOfTransformTracks;
    if (track_count <= 0 || track_count > Max_Tracks_Read || !animation_data_is_present(animation))
    {
        logger::info("SCOPY PUPPET resolve UNAVAILABLE reason=animation-data clip='{}' tracks={}", clip_name, track_count);
        return false;
    }

    RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
    const RE::hkaSkeleton* skeleton = nullptr;
    std::uint32_t bone_nodes = 0;
    std::uint32_t pose_count = 0;
    if (source.GetAnimationGraphManager(manager) && manager)
    {
        for (const RE::BSTSmartPointer<RE::BShkbAnimationGraph>& holder : manager->graphs)
        {
            RE::BShkbAnimationGraph* const graph = holder.get();
            if (!graph || !graph->holder || !graph->rootNode)
                continue;
            if (static_cast<const RE::TESObjectREFR*>(graph->holder) != &source)
                continue;
            skeleton = graph->characterInstance.setup.get()
                ? graph->characterInstance.setup->animationSkeleton.get()
                : nullptr;
            bone_nodes = static_cast<std::uint32_t>(graph->boneNodes.size());
            pose_count = static_cast<std::uint32_t>(std::max(graph->characterInstance.numPoseLocal, 0));
            break;
        }
    }
    if (!skeleton || skeleton->bones.empty())
    {
        logger::info("SCOPY PUPPET resolve UNAVAILABLE reason=no-animation-skeleton clip='{}'", clip_name);
        return false;
    }

    // Empty replacement tables retain the existing unverified identity fallback.
    const RE::hkArray<std::int16_t>& table = binding->transformTrackToBoneIndices;
    std::vector<std::uint16_t> tracks;
    if (!table.empty() && readable(table.begin(), static_cast<std::size_t>(table.size()) * sizeof(std::int16_t)))
    {
        const std::int32_t count = std::min<std::int32_t>(track_count, static_cast<std::int32_t>(table.size()));
        tracks.reserve(static_cast<std::size_t>(count));
        for (std::int32_t i = 0; i < count; ++i)
        {
            const std::int16_t bone = table[static_cast<std::size_t>(i)];
            if (bone < 0 || static_cast<std::uint32_t>(bone) >= static_cast<std::uint32_t>(skeleton->bones.size()))
                continue;
            tracks.push_back(static_cast<std::uint16_t>(bone));
        }
        out_mapping = "table";
    }
    else
    {
        const std::int32_t count = std::min<std::int32_t>(track_count, static_cast<std::int32_t>(skeleton->bones.size()));
        tracks.reserve(static_cast<std::size_t>(count));
        for (std::int32_t i = 0; i < count; ++i)
            tracks.push_back(static_cast<std::uint16_t>(i));
        out_mapping = "identity-unverified";
    }

    const NameIndex copy_by_name = index_nodes_by_name(copy_root);
    out = ClipPlayback{};
    out.animation = animation;
    out.name = clip_name;
    out.duration = animation->duration;
    out.tracks = tracks;
    out.nodes.assign(tracks.size(), nullptr);
    size_t resolved = 0;
    for (size_t i = 0; i < tracks.size(); ++i)
    {
        const char* const bone_name = skeleton->bones[tracks[i]].name.c_str();
        if (!bone_name)
            continue;
        const auto found = copy_by_name.find(bone_name);
        if (found == copy_by_name.end() || found->second == &copy_root)
            continue;
        out.nodes[i] = found->second;
        ++resolved;
    }
    if (resolved == 0)
    {
        logger::info("SCOPY PUPPET resolve UNAVAILABLE reason=no-copy-nodes clip='{}' tracks={}", clip_name, tracks.size());
        return false;
    }
    out_coverage = fmt::format("tracks={} resolved={} bone-nodes={} pose={}", tracks.size(), resolved, bone_nodes, pose_count);
    return true;
}

float live_clip_time(RE::hkbClipGenerator const* generator)
{
    if (!generator || !readable(generator, sizeof(RE::hkbClipGenerator)) || !looks_like_object(generator))
        return -1.0f;
    const RE::hkaDefaultAnimationControl* const control = generator->animationControl.get();
    if (!control || !readable(control, sizeof(RE::hkaAnimationControl)) || !looks_like_object(control))
        return -1.0f;
    const float time = control->localTime;
    return std::isfinite(time) ? time : -1.0f;
}

void ClipPlayer::reset()
{
    m_clip = ClipPlayback{};
    m_time = 0.0;
    m_last_tick = std::chrono::steady_clock::now();
}

void ClipPlayer::set_clip(ClipPlayback clip)
{
    m_clip = std::move(clip);
    m_time = 0.0;
    m_last_tick = std::chrono::steady_clock::now();
}

void ClipPlayer::restart()
{
    m_time = 0.0;
    m_last_tick = std::chrono::steady_clock::now();
}

size_t ClipPlayer::advance(RE::NiAVObject& copy_root)
{
    if (!m_clip.valid())
        return 0;

    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    const double step = std::min(std::chrono::duration<double>(now - m_last_tick).count(), Max_Clip_Step_Seconds);
    m_last_tick = now;
    m_time += std::max(step, 0.0);
    return play_animation_clip(m_clip, static_cast<float>(m_time), copy_root);
}

PLUGIN_NAMESPACE_END
