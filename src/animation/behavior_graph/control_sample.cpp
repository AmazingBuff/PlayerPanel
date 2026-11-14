//
// C-G5-CAL K2-C4 control B carrier (plan A, 2026-11-14).
//
// The oracle is the authored spline data decoded offline by HavokLib; this file
// only exercises the runtime SamplePartialTracks path and records raw float32
// bits. See control_sample_policy.h for the immutable registration.
#include "animation/behavior_graph/control_sample.h"
#include "animation/behavior_graph/control_sample_policy.h"
#include "animation/behavior_graph/observation_chain_policy.h"

#include "RE/H/hkaAnimation.h"
#include "RE/H/hkaAnimationBinding.h"
#include "RE/H/hkaSplineCompressedAnimation.h"
#include "RE/H/hkbBehaviorGraph.h"
#include "RE/H/hkbCharacter.h"
#include "RE/H/hkbClipGenerator.h"

#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstring>
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
    constexpr std::size_t Control_Node_Limit = 512;
    constexpr std::size_t Control_Candidate_Log_Limit = 8;

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

    bool contains_mt_idle(std::string const& a_name)
    {
        std::string folded = a_name;
        for (char& c : folded)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return folded.find("mt_idle") != std::string::npos;
    }

    std::string bits64(std::uint64_t a_value)
    {
        return fmt::format("{:016X}", a_value);
    }

    struct QualifiedCarrier
    {
        RE::hkbClipGenerator const* clip{};
        RE::hkaAnimationBinding const* binding{};
        RE::hkaAnimation const* animation{};
        std::string name;
        std::size_t node_index{};
        std::uint64_t map_hash{};
        std::uint32_t map_count{};
        std::uint32_t blend_hint{};
        bool map_match{};
    };

    bool spline_matches(RE::hkaAnimation const& a_animation)
    {
        if (static_cast<std::uint32_t>(a_animation.type.get()) !=
            static_cast<std::uint32_t>(RE::hkaAnimation::AnimationType::kSplineCompressedAnimation))
            return false;
        if (std::bit_cast<std::uint32_t>(a_animation.duration) != Control_Sample_Duration_Bits ||
            a_animation.numberOfTransformTracks != static_cast<std::int32_t>(Control_Sample_Track_Count) ||
            a_animation.numberOfFloatTracks != static_cast<std::int32_t>(Control_Sample_Float_Track_Count))
            return false;
        auto const* const spline = static_cast<RE::hkaSplineCompressedAnimation const*>(&a_animation);
        if (!readable(spline, sizeof(*spline)))
            return false;
        if (spline->numFrames != static_cast<std::int32_t>(Control_Sample_Num_Frames) ||
            spline->numBlocks != static_cast<std::int32_t>(Control_Sample_Num_Blocks) ||
            spline->maxFramesPerBlock != static_cast<std::int32_t>(Control_Sample_Max_Frames_Per_Block) ||
            std::bit_cast<std::uint32_t>(spline->blockDuration) != Control_Sample_Block_Duration_Bits ||
            std::bit_cast<std::uint32_t>(spline->blockInverseDuration) != Control_Sample_Block_Inverse_Duration_Bits ||
            std::bit_cast<std::uint32_t>(spline->frameDuration) != Control_Sample_Frame_Duration_Bits)
            return false;
        if (spline->data.size() != static_cast<std::int32_t>(Control_Sample_Data_Bytes) ||
            !readable(spline->data.data(), Control_Sample_Data_Bytes))
            return false;
        if (control_sample_fnv1a64(spline->data.data(), Control_Sample_Data_Bytes) != Control_Sample_Data_Fnv1a64)
            return false;
        if (spline->blockOffsets.size() != static_cast<std::int32_t>(Control_Sample_Num_Blocks) ||
            !readable(spline->blockOffsets.data(),
                Control_Sample_Num_Blocks * sizeof(std::uint32_t)))
            return false;
        return control_sample_fnv1a64(spline->blockOffsets.data(),
                   Control_Sample_Num_Blocks * sizeof(std::uint32_t)) == Control_Sample_Block_Offsets_Fnv1a64;
    }

    bool read_binding_identity(RE::hkaAnimationBinding const& a_binding, QualifiedCarrier& a_carrier)
    {
        auto const& map = a_binding.transformTrackToBoneIndices;
        a_carrier.map_count = static_cast<std::uint32_t>(std::max(map.size(), 0));
        a_carrier.blend_hint = static_cast<std::uint32_t>(a_binding.blendHint.get());
        if (a_carrier.map_count == 0)
        {
            a_carrier.map_hash = control_sample_fnv1a64(nullptr, 0);
            a_carrier.map_match = false;
            return true;
        }
        if (!readable(map.data(), static_cast<std::size_t>(a_carrier.map_count) * sizeof(std::int16_t)))
            return false;
        a_carrier.map_hash = control_sample_fnv1a64(map.data(),
            static_cast<std::size_t>(a_carrier.map_count) * sizeof(std::int16_t));
        a_carrier.map_match = a_carrier.map_count == Control_Sample_Binding_Map_Count &&
            a_carrier.map_hash == Control_Sample_Binding_Map_Fnv1a64 &&
            a_carrier.blend_hint == Control_Sample_Blend_Hint;
        return true;
    }

    void log_candidate(std::uint32_t a_actor_id, std::uint64_t a_request, std::size_t a_completed,
        QualifiedCarrier const& a_carrier, bool a_qualified, char const* a_reason)
    {
        logger::info(
            "CONTROL B actor={:08x} request={} phase=scan completed={} node={:#x} index={} clip='{}' "
            "binding={:#x} animation={:#x} map_count={} map_fnv1a64={} blend_hint={} map_match={} "
            "qualified={} reason={}",
            a_actor_id, a_request, a_completed, reinterpret_cast<std::uintptr_t>(a_carrier.clip),
            a_carrier.node_index, a_carrier.name, reinterpret_cast<std::uintptr_t>(a_carrier.binding),
            reinterpret_cast<std::uintptr_t>(a_carrier.animation), a_carrier.map_count,
            bits64(a_carrier.map_hash), a_carrier.blend_hint, a_carrier.map_match, a_qualified, a_reason);
    }
}

bool run_control_sample(RE::hkbCharacter& a_character, std::uint32_t a_actor_id,
    std::uint64_t a_request, std::size_t a_completed)
{
    logger::info(
        "CONTROL B actor={:08x} request={} phase=manifest completed={} resource='{}' bytes={} sha256={} "
        "duration_bits={:08X} tracks={} floats={} frames={} blocks={} max_frames_per_block={} "
        "block_duration_bits={:08X} block_inverse_bits={:08X} frame_duration_bits={:08X} "
        "data_bytes={} data_fnv1a64={} block_offsets_fnv1a64={} map_count={} map_fnv1a64={} blend_hint={} "
        "times={}",
        a_actor_id, a_request, a_completed, Control_Sample_Resource, Control_Sample_Resource_Bytes,
        Control_Sample_Resource_Sha256, Control_Sample_Duration_Bits, Control_Sample_Track_Count,
        Control_Sample_Float_Track_Count, Control_Sample_Num_Frames, Control_Sample_Num_Blocks,
        Control_Sample_Max_Frames_Per_Block, Control_Sample_Block_Duration_Bits,
        Control_Sample_Block_Inverse_Duration_Bits, Control_Sample_Frame_Duration_Bits,
        Control_Sample_Data_Bytes, bits64(Control_Sample_Data_Fnv1a64),
        bits64(Control_Sample_Block_Offsets_Fnv1a64), Control_Sample_Binding_Map_Count,
        bits64(Control_Sample_Binding_Map_Fnv1a64), Control_Sample_Blend_Hint, Control_Sample_Time_Count);

    RE::hkbBehaviorGraph* const graph = a_character.behaviorGraph.get();
    if (!graph || !graph->activeNodes)
    {
        logger::info("CONTROL B actor={:08x} request={} phase=result status=unavailable reason=no-active-nodes",
            a_actor_id, a_request);
        return false;
    }

    RE::NodeList const& nodes = *graph->activeNodes;
    std::size_t const node_count = static_cast<std::size_t>(nodes.size());
    if (node_count > Control_Node_Limit)
    {
        logger::info("CONTROL B actor={:08x} request={} phase=result status=invalid reason=node-overflow nodes={}",
            a_actor_id, a_request, node_count);
        return false;
    }

    std::vector<QualifiedCarrier> qualified;
    std::size_t logged = 0;
    for (std::size_t i = 0; i < node_count; ++i)
    {
        RE::hkbNode* const node = nodes.data()[i].nodeClone;
        if (!node_is_clip_generator(node))
            continue;
        auto const* const clip = static_cast<RE::hkbClipGenerator const*>(node);
        QualifiedCarrier carrier{};
        carrier.clip = clip;
        carrier.node_index = i;
        carrier.name = read_string(clip->animationName, 128);

        char const* reason = "qualified";
        bool ok = plausible_object_pointer(reinterpret_cast<std::uintptr_t>(clip->binding));
        if (ok)
        {
            carrier.binding = clip->binding;
            carrier.animation = carrier.binding->animation.get();
            ok = carrier.animation != nullptr &&
                plausible_object_pointer(reinterpret_cast<std::uintptr_t>(carrier.animation));
        }
        if (!ok)
        {
            reason = "bad-binding";
        }
        else if (!spline_matches(*carrier.animation))
        {
            ok = false;
            reason = "resource-mismatch";
        }
        else if (!read_binding_identity(*carrier.binding, carrier))
        {
            ok = false;
            reason = "binding-unreadable";
        }
        else if (!contains_mt_idle(carrier.name))
        {
            ok = false;
            reason = "name-mismatch";
        }

        if (ok)
            qualified.push_back(carrier);
        if (ok || logged < Control_Candidate_Log_Limit)
        {
            log_candidate(a_actor_id, a_request, a_completed, carrier, ok, reason);
            ++logged;
        }
    }

    if (qualified.size() != 1)
    {
        logger::info("CONTROL B actor={:08x} request={} phase=result status=unavailable reason=carrier-count qualified={}",
            a_actor_id, a_request, qualified.size());
        return false;
    }

    QualifiedCarrier const& carrier = qualified.front();
    logger::info(
        "CONTROL B actor={:08x} request={} phase=carrier completed={} node={:#x} index={} clip='{}' binding={:#x} "
        "binding_index={} animation={:#x} duration_bits={:08X} tracks={} data_fnv1a64={} map_count={} "
        "map_fnv1a64={} blend_hint={} map_match={}",
        a_actor_id, a_request, a_completed, reinterpret_cast<std::uintptr_t>(carrier.clip),
        carrier.node_index, carrier.name, reinterpret_cast<std::uintptr_t>(carrier.binding),
        static_cast<std::uint32_t>(carrier.clip->animationBindingIndex),
        reinterpret_cast<std::uintptr_t>(carrier.animation),
        std::bit_cast<std::uint32_t>(carrier.animation->duration), carrier.animation->numberOfTransformTracks,
        bits64(Control_Sample_Data_Fnv1a64), carrier.map_count, bits64(carrier.map_hash),
        carrier.blend_hint, carrier.map_match);

    std::vector<RE::hkQsTransform> first(Control_Sample_Track_Count);
    std::vector<RE::hkQsTransform> second(Control_Sample_Track_Count);
    std::uint64_t bits_hash = 0xcbf29ce484222325ull;
    std::size_t comparisons = 0;
    std::size_t equal = 0;
    std::size_t non_finite = 0;
    bool valid = true;

    for (std::size_t time_index = 0; time_index < Control_Sample_Time_Count; ++time_index)
    {
        std::uint32_t const time_bits = Control_Sample_Times[time_index];
        float const time = std::bit_cast<float>(time_bits);
        carrier.animation->SamplePartialTracks(time, Control_Sample_Track_Count, first.data(), 0, nullptr, nullptr);
        carrier.animation->SamplePartialTracks(time, Control_Sample_Track_Count, second.data(), 0, nullptr, nullptr);
        bool const read_equal = std::memcmp(first.data(), second.data(),
                                      Control_Sample_Track_Count * sizeof(RE::hkQsTransform)) == 0;
        ++comparisons;
        equal += read_equal ? 1u : 0u;
        if (!read_equal)
            valid = false;

        for (std::size_t track = 0; track < Control_Sample_Track_Count; ++track)
        {
            alignas(16) float translation[4];
            alignas(16) float rotation[4];
            alignas(16) float scale[4];
            _mm_store_ps(translation, first[track].translation.quad);
            _mm_store_ps(rotation, first[track].rotation.vec.quad);
            _mm_store_ps(scale, first[track].scale.quad);
            float const values[10] = { translation[0], translation[1], translation[2],
                rotation[0], rotation[1], rotation[2], rotation[3],
                scale[0], scale[1], scale[2] };
            std::string line = fmt::format(
                "CONTROL B actor={:08x} request={} phase=track completed={} time={} time_bits={:08X} track={} ",
                a_actor_id, a_request, a_completed, time_index, time_bits, track);
            for (std::size_t i = 0; i < 10; ++i)
            {
                std::uint32_t const raw = std::bit_cast<std::uint32_t>(values[i]);
                if (!std::isfinite(values[i]))
                    ++non_finite;
                line += fmt::format("{}{:08X}", i == 0 ? "bits=" : ",", raw);
                auto const* const bytes = reinterpret_cast<std::uint8_t const*>(&raw);
                for (std::size_t b = 0; b < sizeof(raw); ++b)
                {
                    bits_hash ^= bytes[b];
                    bits_hash *= 0x100000001b3ull;
                }
            }
            logger::info("{}", line);
        }
    }

    bool const hash_match = bits_hash == Control_Sample_Truth_Bits_Fnv1a64;
    valid = valid && non_finite == 0;
    logger::info(
        "CONTROL B actor={:08x} request={} phase=summary status={} times={} tracks={} reads={} two_read_equal={} "
        "nonfinite={} bits_fnv1a64={} expected={} hash_match={} map_match={}",
        a_actor_id, a_request, valid ? "complete" : "invalid", Control_Sample_Time_Count,
        Control_Sample_Track_Count, comparisons, equal, non_finite, bits64(bits_hash),
        bits64(Control_Sample_Truth_Bits_Fnv1a64), hash_match, carrier.map_match);
    return valid;
}
}
PLUGIN_NAMESPACE_END
