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

    const std::int32_t sample_count = clip.animation->numberOfTransformTracks;
    if (sample_count <= 0 || sample_count > Max_Tracks_Read)
        return 0;
    std::vector<RE::hkQsTransform> sampled(static_cast<std::size_t>(sample_count));
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

RE::hkbClipGenerator const* pick_idle_running_clip(RE::TESObjectREFR& source)
{
    std::vector<ActiveClipEntry> running;
    collect_running_clip_generators(source, running);
    RE::hkbClipGenerator const* best = nullptr;
    float best_weight = 0.0f;
    for (const ActiveClipEntry& entry : running)
    {
        // The state-idle preference (state-idle decision, 2026-10-15): the strongest running clip
        // while MOVING is a locomotion cycle, but the panel wants the state's IDLE - the engine's
        // own idle family is recognisable by name (sneakIdle, 1hm_idle, MTn_IdleHorse, the
        // hand-idle family *Lh_Idle/*Rh_Idle the old classifier gap named). Attack and equip
        // one-shots carry other names and lose. No idle-named clip running (e.g. mid-sprint)
        // returns null and the caller falls back to the strongest clip, logging the pick.
        std::string lowered;
        lowered.reserve(entry.name.size());
        std::transform(entry.name.begin(), entry.name.end(), std::back_inserter(lowered),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lowered.find("idle") == std::string::npos)
            continue;
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

RunningIdleSample strongest_idle_clip(RE::TESObjectREFR& source)
{
    RunningIdleSample sample;
    std::vector<ActiveClipEntry> running;
    collect_running_clip_generators(source, running);
    float best_weight = 0.0f;
    float best_any_weight = 0.0f;
    float best_body_weight = 0.0f;
    for (const ActiveClipEntry& entry : running)
    {
        std::string lowered;
        lowered.reserve(entry.name.size());
        std::transform(entry.name.begin(), entry.name.end(), std::back_inserter(lowered),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        RE::hkbClipGenerator const* const generator = entry.generator;
        if (!generator || !readable(generator, sizeof(RE::hkbClipGenerator)) || !looks_like_object(generator))
            continue;
        const RE::hkaDefaultAnimationControl* const control = generator->animationControl.get();
        if (!control || !readable(control, sizeof(RE::hkaAnimationControl)) || !looks_like_object(control))
            continue;
        // Two answers in one walk: the strongest IDLE (the panel's weapon-stance preference)
        // and the strongest clip of ANY name (draw/sheathe/attack one-shots included) — the
        // world-side sequence recorder needs the full order of the player's own demonstration,
        // and those clips never carry "idle" in the name.
        if (control->weight > best_any_weight)
        {
            best_any_weight = control->weight;
            sample.any_name = entry.name;
            sample.any_weight = control->weight;
            sample.any_index = generator->animationBindingIndex;
            sample.any_time = std::isfinite(control->localTime) ? control->localTime : 0.0f;
            sample.any_looping = generator->mode == RE::hkbClipGenerator::PlaybackMode::kModeLooping;
        }
        // The body-entry answer excludes hand idles: the entry action drives the whole body, and
        // a hand-layer member out-weighing the still-blending body clips must not become it.
        const bool hand_idle = lowered.find("mlh_") != std::string::npos || lowered.find("mrh_") != std::string::npos;
        if (!hand_idle && control->weight > best_body_weight)
        {
            best_body_weight = control->weight;
            sample.body_name = entry.name;
            sample.body_index = generator->animationBindingIndex;
            sample.body_time = std::isfinite(control->localTime) ? control->localTime : 0.0f;
            sample.body_looping = generator->mode == RE::hkbClipGenerator::PlaybackMode::kModeLooping;
        }
        if (lowered.find("idle") != std::string::npos && control->weight > best_weight)
        {
            best_weight = control->weight;
            sample.name = entry.name;
            sample.weight = control->weight;
        }
    }
    return sample;
}

bool resolve_binding_playback(RE::BShkbAnimationGraph& graph, RE::NiAVObject& copy_root,
    RE::hkaAnimationBinding* binding, std::string const& clip_name, ClipPlayback& out,
    std::string& out_mapping, std::string& out_coverage, std::string_view weapon_hint)
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
    const auto matches_attachment = [](std::string_view bone)
    {
        return bone == "Weapon" || bone == "Quiver" || bone == "Shield";
    };
    for (size_t i = 0; i < bindings.size(); ++i)
    {
        out.tracks.push_back(bindings[i].track);
        const char* const bone_raw = skeleton->bones[bindings[i].bone].name.c_str();
        if (!bone_raw)
            continue;
        const std::string_view bone_name = trimmed(bone_raw);
        auto found = copy_by_name.find(bone_name);
        if (found == copy_by_name.end() || found->second == &copy_root)
        {
            // Animation-skeleton attachment bones carry the GENERIC name ("Weapon", "Quiver",
            // "Shield"); the NIF layer carries one node per variant ("WeaponSword", "WeaponAxe",
            // …). The in-game evidence: trim alone resolved the tracks (resolved 95/96) but the
            // longest-prefix pick landed on the WRONG variant (WeaponSwordLeft / Weapon L Calf)
            // and the visible weapon never moved — the caller's weapon-form hint names the exact
            // hand-attachment node ("WeaponSword", "WeaponBow", …). Hint first, then prefix.
            if (matches_attachment(bone_name))
            {
                RE::NiAVObject* best = nullptr;
                if (!weapon_hint.empty())
                {
                    const auto hinted = copy_by_name.find(weapon_hint);
                    if (hinted != copy_by_name.end() && hinted->second != &copy_root)
                        best = hinted->second;
                }
                if (!best)
                {
                    // Only the hint-less fallback walks the map. It really takes the LONGEST
                    // prefix: the old `!best` guard stopped at the first map-order match, which
                    // is how a generic Weapon bone ended up on WeaponSwordLeft (round 16).
                    size_t best_length = 0;
                    for (const auto& [name, node] : copy_by_name)
                    {
                        if (name.size() > bone_name.size() && name.size() > best_length && name.starts_with(bone_name))
                        {
                            best = node;
                            best_length = name.size();
                        }
                    }
                }
                if (best)
                {
                    out.nodes[i] = best;
                    ++resolved;
                    continue;
                }
            }
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

bool resolve_clip_playback(RE::TESObjectREFR& source, RE::NiAVObject& copy_root, RE::hkbClipGenerator const* generator,
    ClipPlayback& out, std::string& out_mapping, std::string& out_coverage)
{
    if (!generator || !readable(generator, sizeof(*generator)) || !looks_like_object(generator))
        return false;
    RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
    if (!source.GetAnimationGraphManager(manager) || !manager)
        return false;
    for (RE::BSTSmartPointer<RE::BShkbAnimationGraph> const& graph : manager->graphs)
    {
        if (graph && graph->holder == &source && graph->rootNode &&
            resolve_binding_playback(*graph, copy_root, generator->binding,
                guarded_string(generator->animationName), out, out_mapping, out_coverage))
            return true;
    }
    return false;
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

void ClipPlayer::set_clip(ClipPlayback clip, bool loop, double start_seconds)
{
    m_clip = std::move(clip);
    m_loop = loop;
    m_time = std::max(start_seconds, 0.0);
    m_last_tick = std::chrono::steady_clock::now();
}

void ClipPlayer::retarget(ClipPlayback clip)
{
    m_clip = std::move(clip);
    m_last_tick = std::chrono::steady_clock::now();
}

void ClipPlayer::restart(double start_seconds)
{
    m_time = std::max(start_seconds, 0.0);
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
    return play_animation_clip(m_clip, static_cast<float>(m_time), copy_root, m_loop);
}

PLUGIN_NAMESPACE_END
