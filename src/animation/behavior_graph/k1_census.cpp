//
// C-G5-CAL K1 identity census (2026-11-14).
//
// Fills K1-C2 graph identity, K1-C3 skeleton/mapping, and K1-C4 resource/clip
// instance identity without sampling or advancing anything.
#include "animation/behavior_graph/k1_census.h"
#include "animation/behavior_graph/animation_resource_identity.h"
#include "animation/behavior_graph/observation_chain_policy.h"

#include "RE/B/BShkbAnimationGraph.h"
#include "RE/H/hkaAnimation.h"
#include "RE/H/hkaAnimationBinding.h"
#include "RE/H/hkaSkeleton.h"
#include "RE/H/hkbBehaviorGraph.h"
#include "RE/H/hkbCharacter.h"
#include "RE/H/hkbClipGenerator.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include <xmmintrin.h>

// winnt.h defines macros of these names, which would rewrite the REX constants
// as `REX::W32::0x1000`; every Windows header this file needs is already parsed.
#undef MEM_COMMIT
#undef PAGE_NOACCESS

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
namespace
{
    constexpr std::size_t K1_Node_Limit = 512;
    constexpr std::size_t K1_Clip_Limit = 64;
    constexpr std::size_t K1_Bone_Limit = 512;

    bool readable(void const* a_address, std::size_t a_bytes)
    {
        if (!a_address || a_bytes == 0)
            return false;
        REX::W32::MEMORY_BASIC_INFORMATION info{};
        if (REX::W32::VirtualQuery(a_address, &info, sizeof(info)) == 0)
            return false;
        if (info.state != REX::W32::MEM_COMMIT || info.protect == REX::W32::PAGE_NOACCESS)
            return false;
        auto const* const begin = static_cast<std::uint8_t const*>(a_address);
        auto const* const end = static_cast<std::uint8_t const*>(info.baseAddress) + info.regionSize;
        return begin + a_bytes <= end;
    }

    bool node_is_clip_generator(RE::hkbNode const* a_node)
    {
        if (!a_node)
            return false;
        return *reinterpret_cast<std::uintptr_t const*>(a_node) == RE::VTABLE_hkbClipGenerator[0].address();
    }

    std::string read_string(RE::hkStringPtr const& a_text, std::size_t a_max_length)
    {
        char const* const raw = a_text.c_str();
        if (!plausible_object_pointer(reinterpret_cast<std::uintptr_t>(raw)))
            return std::string("unreadable");
        std::size_t length = 0;
        while (length < a_max_length && raw[length] != '\0')
            ++length;
        return std::string(raw, length);
    }

    std::string read_cstring(char const* a_text, std::size_t a_max_length)
    {
        if (!plausible_object_pointer(reinterpret_cast<std::uintptr_t>(a_text)))
            return std::string("unreadable");
        std::size_t length = 0;
        while (length < a_max_length && a_text[length] != '\0')
            ++length;
        return std::string(a_text, length);
    }

    std::string bits32(float a_value)
    {
        return fmt::format("{:08X}", std::bit_cast<std::uint32_t>(a_value));
    }

    std::string bits64(std::uint64_t a_value)
    {
        return fmt::format("{:016X}", a_value);
    }

    std::size_t active_node_count(RE::hkbCharacter const& a_character)
    {
        RE::hkbBehaviorGraph* const graph = a_character.behaviorGraph.get();
        if (!graph || !graph->activeNodes)
            return 0;
        return static_cast<std::size_t>(graph->activeNodes->size());
    }

    void log_graph_identity(RE::BShkbAnimationGraph const& a_graph, char const* a_side,
        std::uint32_t a_actor_id, std::uint64_t a_request, char const* a_phase)
    {
        RE::hkbCharacter const& character = a_graph.characterInstance;
        RE::hkaSkeleton const* const skeleton = character.setup
            ? character.setup->animationSkeleton.get() : nullptr;
        logger::info(
            "K1 CENSUS actor={:08x} request={} side={} phase={} graph={:#x} holder={:#x} root={:#x} project='{}' "
            "project-db={:#x} behavior={:#x} char-behavior={:#x} setup={:#x} bindings={:#x} pose-local={:#x}/{} "
            "bone-nodes={} num-anim-bones={} active-nodes={} skeleton={:#x} bones={}",
            a_actor_id, a_request, a_side, a_phase,
            reinterpret_cast<std::uintptr_t>(&a_graph),
            reinterpret_cast<std::uintptr_t>(a_graph.holder),
            reinterpret_cast<std::uintptr_t>(a_graph.rootNode),
            read_cstring(a_graph.projectName.c_str(), 64),
            reinterpret_cast<std::uintptr_t>(a_graph.projectDBData),
            reinterpret_cast<std::uintptr_t>(a_graph.behaviorGraph),
            reinterpret_cast<std::uintptr_t>(character.behaviorGraph.get()),
            reinterpret_cast<std::uintptr_t>(character.setup.get()),
            reinterpret_cast<std::uintptr_t>(character.animationBindingSet.get()),
            reinterpret_cast<std::uintptr_t>(character.poseLocal), character.numPoseLocal,
            a_graph.boneNodes.size(), a_graph.numAnimBones, active_node_count(character),
            reinterpret_cast<std::uintptr_t>(skeleton), skeleton ? skeleton->bones.size() : 0);
    }

    void log_skeleton(RE::BShkbAnimationGraph const& a_graph, char const* a_side,
        std::uint32_t a_actor_id, std::uint64_t a_request, char const* a_phase)
    {
        RE::hkbCharacter const& character = a_graph.characterInstance;
        RE::hkaSkeleton const* const skeleton = character.setup
            ? character.setup->animationSkeleton.get() : nullptr;
        if (!skeleton)
        {
            logger::info("K1 CENSUS actor={:08x} request={} side={} phase={} status=unavailable reason=no-skeleton",
                a_actor_id, a_request, a_side, a_phase);
            return;
        }
        std::size_t const bones = static_cast<std::size_t>(skeleton->bones.size());
        std::size_t const parents = static_cast<std::size_t>(skeleton->parentIndices.size());
        std::size_t const reference = static_cast<std::size_t>(skeleton->referencePose.size());
        logger::info(
            "K1 CENSUS actor={:08x} request={} side={} phase={} skeleton={:#x} name='{}' bones={} parents={} "
            "reference={} floats={} slots={}",
            a_actor_id, a_request, a_side, a_phase,
            reinterpret_cast<std::uintptr_t>(skeleton), read_string(skeleton->name, 64),
            bones, parents, reference, skeleton->referenceFloats.size(), skeleton->floatSlots.size());

        std::size_t const count = std::min(bones, K1_Bone_Limit);
        std::size_t recorded = 0;
        std::size_t invalid_parent = 0;
        std::size_t nonfinite = 0;
        for (std::size_t i = 0; i < count; ++i)
        {
            int const parent = i < parents ? static_cast<int>(skeleton->parentIndices.data()[i]) : -999;
            if (parent < -1 || (parent >= 0 && static_cast<std::size_t>(parent) >= bones))
                ++invalid_parent;
            if (i >= reference)
            {
                logger::info("K1 CENSUS actor={:08x} request={} side={} phase=bone index={} name='{}' parent={} status=no-reference",
                    a_actor_id, a_request, a_side, i, read_string(skeleton->bones.data()[i].name, 64), parent);
                continue;
            }
            RE::hkQsTransform const& pose = skeleton->referencePose.data()[i];
            alignas(16) float translation[4];
            alignas(16) float rotation[4];
            alignas(16) float scale[4];
            _mm_store_ps(translation, pose.translation.quad);
            _mm_store_ps(rotation, pose.rotation.vec.quad);
            _mm_store_ps(scale, pose.scale.quad);
            float const values[10] = { translation[0], translation[1], translation[2],
                rotation[0], rotation[1], rotation[2], rotation[3],
                scale[0], scale[1], scale[2] };
            for (float const value : values)
                nonfinite += std::isfinite(value) ? 0u : 1u;
            logger::info(
                "K1 CENSUS actor={:08x} request={} side={} phase=bone index={} name='{}' parent={} bits={},{},{},{},{},{},{},{},{},{}",
                a_actor_id, a_request, a_side, i, read_string(skeleton->bones.data()[i].name, 64), parent,
                bits32(values[0]), bits32(values[1]), bits32(values[2]), bits32(values[3]), bits32(values[4]),
                bits32(values[5]), bits32(values[6]), bits32(values[7]), bits32(values[8]), bits32(values[9]));
            ++recorded;
        }
        logger::info(
            "K1 CENSUS actor={:08x} request={} side={} phase=skeleton-summary bones={} recorded={} invalid-parent={} "
            "nonfinite={} overflow={}",
            a_actor_id, a_request, a_side, bones, recorded, invalid_parent, nonfinite,
            bones > K1_Bone_Limit);
    }

    void log_clips(RE::BShkbAnimationGraph const& a_graph, char const* a_side,
        std::uint32_t a_actor_id, std::uint64_t a_request, char const* a_phase)
    {
        RE::hkbCharacter const& character = a_graph.characterInstance;
        RE::hkbBehaviorGraph* const graph = character.behaviorGraph.get();
        if (!graph || !graph->activeNodes)
        {
            logger::info("K1 CENSUS actor={:08x} request={} side={} phase={} status=unavailable reason=no-active-nodes",
                a_actor_id, a_request, a_side, a_phase);
            return;
        }
        RE::NodeList const& nodes = *graph->activeNodes;
        std::size_t const node_count = static_cast<std::size_t>(nodes.size());
        std::size_t const node_limit = std::min(node_count, K1_Node_Limit);
        std::size_t clips = 0;
        std::size_t missing = 0;
        std::size_t overflow = node_count > K1_Node_Limit ? 1u : 0u;
        std::vector<std::string> stable_ids;
        for (std::size_t i = 0; i < node_limit; ++i)
        {
            RE::hkbNode* const node = nodes.data()[i].nodeClone;
            if (!node_is_clip_generator(node))
                continue;
            ++clips;
            if (clips > K1_Clip_Limit)
            {
                overflow = 1;
                continue;
            }
            auto const* const clip = static_cast<RE::hkbClipGenerator const*>(node);
            RE::hkaAnimationBinding const* const binding =
                plausible_object_pointer(reinterpret_cast<std::uintptr_t>(clip->binding)) ? clip->binding : nullptr;
            AnimationResourceIdentity identity{};
            bool const identity_ok = read_animation_resource_identity(binding, identity);
            if (!identity_ok)
                ++missing;
            std::string stable_id = fmt::format("{:x}:{:x}:{}:{:016X}",
                reinterpret_cast<std::uintptr_t>(clip), reinterpret_cast<std::uintptr_t>(binding),
                static_cast<std::uint32_t>(clip->animationBindingIndex), identity.data_hash);
            bool duplicate = false;
            for (std::string const& id : stable_ids)
                duplicate = duplicate || id == stable_id;
            if (!duplicate)
                stable_ids.push_back(stable_id);

            logger::info(
                "K1 CENSUS actor={:08x} request={} side={} phase=clip slot={} node={:#x} index={} name='{}' "
                "binding={:#x} animation={:#x} binding-index={} mode={} local-time-bits={:08X} speed={:.6f} enforced={:.6f} "
                "crop={:.6f} identity={} type={} duration-bits={:08X} tracks={} floats={} spline={} frames={} blocks={} "
                "max-frames={} block-duration-bits={:08X} block-inverse-bits={:08X} frame-duration-bits={:08X} "
                "data-bytes={} data-fnv={} blockoff-fnv={} map-count={} map-fnv={} blend={} stable-id='{}'",
                a_actor_id, a_request, a_side, clips - 1,
                reinterpret_cast<std::uintptr_t>(clip), i, read_string(clip->animationName, 128),
                reinterpret_cast<std::uintptr_t>(binding),
                reinterpret_cast<std::uintptr_t>(binding ? binding->animation.get() : nullptr),
                static_cast<std::uint32_t>(clip->animationBindingIndex),
                static_cast<std::uint32_t>(clip->mode.get()), std::bit_cast<std::uint32_t>(clip->localTime),
                clip->playbackSpeed, clip->enforcedDuration,
                clip->cropStartAmountLocalTime + clip->cropEndAmountLocalTime,
                identity_ok, identity.type, identity.duration_bits, identity.transform_tracks, identity.float_tracks,
                identity.spline, identity.num_frames, identity.num_blocks, identity.max_frames_per_block,
                identity.block_duration_bits, identity.block_inverse_duration_bits, identity.frame_duration_bits,
                identity.data_bytes, bits64(identity.data_hash), bits64(identity.block_offsets_hash),
                identity.map_count, bits64(identity.map_hash), identity.blend_hint, stable_id);
        }
        logger::info(
            "K1 CENSUS actor={:08x} request={} side={} phase=clips-summary nodes={} clips={} unique={} "
            "missing={} overflow={}",
            a_actor_id, a_request, a_side, node_count, clips, stable_ids.size(), missing, overflow);
    }
}

void log_k1_census(RE::BShkbAnimationGraph const& a_graph, char const* a_side,
    std::uint32_t a_actor_id, std::uint64_t a_request, char const* a_phase,
    bool a_dump_skeleton)
{
    log_graph_identity(a_graph, a_side, a_actor_id, a_request, a_phase);
    if (a_dump_skeleton)
        log_skeleton(a_graph, a_side, a_actor_id, a_request, a_phase);
    log_clips(a_graph, a_side, a_actor_id, a_request, a_phase);
}
}
PLUGIN_NAMESPACE_END
