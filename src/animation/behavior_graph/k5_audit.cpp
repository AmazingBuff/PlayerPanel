//
// C-G5-CAL K5 layer audit (2026-11-14 implementation pre-registration §10.4).
//
// Every read is a plain field read through CommonLibSSE-audited layouts; nodes
// and bindings are guarded with plausible_object_pointer before dereference.
// The classification table covers exactly the vtable constants CommonLibSSE
// ships; anything else lands in `unknown` with its vtable address, which is
// where hkbBlendingBehavior ends up (no audited layout -> explicit gap).
#include "animation/behavior_graph/k5_audit.h"

#include "RE/B/BShkbAnimationGraph.h"
#include "RE/H/hkbBehaviorGraph.h"
#include "RE/H/hkbClipGenerator.h"
#include "RE/H/hkbNode.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <string>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    namespace
    {
        bool node_is_clip_generator(RE::hkbNode const* a_node)
        {
            if (!a_node)
                return false;
            return *reinterpret_cast<std::uintptr_t const*>(a_node) ==
                RE::VTABLE_hkbClipGenerator[0].address();
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

        struct NodeClass
        {
            char const* name;
            bool has_node_layout;  // hkbNode-derived: name/id are readable at 0x38/0x40
        };

        // Exact vtable match, evaluated in order. The first hit wins; every entry is a
        // CommonLibSSE VTABLE constant, so no layout is guessed.
        NodeClass classify(std::uintptr_t a_vtable)
        {
            if (a_vtable == RE::VTABLE_hkbClipGenerator[0].address())
                return { "clip", true };
            if (a_vtable == RE::VTABLE_hkbStateMachine[0].address())
                return { "state-machine", true };
            if (a_vtable == RE::VTABLE_hkbManualSelectorGenerator[0].address())
                return { "manual-selector", true };
            if (a_vtable == RE::VTABLE_hkbModifierGenerator[0].address())
                return { "modifier-gen", true };
            if (a_vtable == RE::VTABLE_hkbPoseMatchingGenerator[0].address())
                return { "pose-match", true };
            if (a_vtable == RE::VTABLE_hkbBehaviorGraph[0].address())
                return { "behavior-graph", true };
            if (a_vtable == RE::VTABLE_hkbGenerator[0].address())
                return { "generator-unclassified", true };
            if (a_vtable == RE::VTABLE_hkbNode[0].address())
                return { "node-other", true };
            return { "unknown", false };
        }
    }

    void log_k5_layer_audit(RE::BShkbAnimationGraph const& a_graph, std::uint32_t a_actor_id,
        std::uint64_t a_request, char const* a_phase, std::uint32_t a_point)
    {
        K5IdentityCache audit_cache;
        RE::hkbBehaviorGraph const* const behavior = a_graph.characterInstance.behaviorGraph.get();
        if (!behavior || !behavior->activeNodes)
        {
            logger::info("K5 AUDIT actor={:08x} request={} phase={} point={} status=unavailable reason=no-active-nodes",
                a_actor_id, a_request, a_phase, a_point);
            return;
        }
        RE::NodeList const& nodes = *behavior->activeNodes;
        std::size_t const node_count = static_cast<std::size_t>(nodes.size());
        std::size_t const walked = std::min(node_count, K5_Audit_Node_Limit);
        std::size_t clips = 0;
        std::size_t cross_clips = 0;
        std::size_t unclassified = 0;
        std::size_t identity_failed = 0;
        for (std::size_t i = 0; i < walked; ++i)
        {
            RE::hkbNode* const node = nodes.data()[i].nodeClone;
            if (!node || !plausible_object_pointer(reinterpret_cast<std::uintptr_t>(node)))
                continue;
            std::uintptr_t const vtable = *reinterpret_cast<std::uintptr_t const*>(node);
            NodeClass const kind = classify(vtable);
            if (kind.name == std::string_view{ "unknown" })
                ++unclassified;
            bool const is_clip = node_is_clip_generator(node);
            if (is_clip)
            {
                ++clips;
                if (i < K5_Audit_Cross_Limit)
                    ++cross_clips;
            }
            if (!kind.has_node_layout)
            {
                logger::info("K5 LAYER actor={:08x} request={} phase={} point={} index={} node={:#x} class={} vtable={:#x}",
                    a_actor_id, a_request, a_phase, a_point, i,
                    reinterpret_cast<std::uintptr_t>(node), kind.name, vtable);
                continue;
            }
            std::string const name = read_string(node->name, 96);
            if (!is_clip)
            {
                logger::info("K5 LAYER actor={:08x} request={} phase={} point={} index={} node={:#x} class={} name='{}' id={}",
                    a_actor_id, a_request, a_phase, a_point, i,
                    reinterpret_cast<std::uintptr_t>(node), kind.name, name,
                    static_cast<std::uint32_t>(node->id));
                continue;
            }
            auto const* const clip = static_cast<RE::hkbClipGenerator const*>(node);
            RE::hkaAnimationBinding const* const binding =
                plausible_object_pointer(reinterpret_cast<std::uintptr_t>(clip->binding))
                ? clip->binding
                : nullptr;
            K5IdentityCache::Entry const entry = audit_cache.identity_for(binding);
            if (!entry.ok)
                ++identity_failed;
            AnimationResourceIdentity const& identity = entry.identity;
            logger::info(
                "K5 LAYER actor={:08x} request={} phase={} point={} index={} node={:#x} class=clip name='{}' id={} "
                "binding={:#x} binding-index={} local-time-bits={:08X} mode={} speed={:.6f} enforced={:.6f} crop={:.6f} "
                "duration-bits={:08X} data-fnv={:016X} blockoff-fnv={:016X} map-count={} map-fnv={:016X} identity={}",
                a_actor_id, a_request, a_phase, a_point, i,
                reinterpret_cast<std::uintptr_t>(node), name,
                static_cast<std::uint32_t>(node->id),
                reinterpret_cast<std::uintptr_t>(binding),
                static_cast<std::uint32_t>(clip->animationBindingIndex),
                std::bit_cast<std::uint32_t>(clip->localTime),
                static_cast<std::uint32_t>(clip->mode.get()),
                clip->playbackSpeed, clip->enforcedDuration,
                clip->cropStartAmountLocalTime + clip->cropEndAmountLocalTime,
                identity.duration_bits, identity.data_hash, identity.block_offsets_hash,
                identity.map_count, identity.map_hash, entry.ok ? 1 : 0);
        }
        std::size_t const missing = clips - cross_clips;
        logger::info(
            "K5 AUDIT actor={:08x} request={} phase={} point={} walked={} nodes={} clips={} cross-clips={} missing={} "
            "unclassified={} identity-failed={} overflow={}",
            a_actor_id, a_request, a_phase, a_point, walked, node_count, clips, cross_clips, missing,
            unclassified, identity_failed, node_count > K5_Audit_Node_Limit ? 1 : 0);
    }
}
PLUGIN_NAMESPACE_END
