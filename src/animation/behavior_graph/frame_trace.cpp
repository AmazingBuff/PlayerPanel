//
// Created by AmazingBuff on 2026/10/26.
//
#include "animation/behavior_graph/frame_trace.h"

#include "RE/B/BShkbAnimationGraph.h"
#include "RE/H/hkbCharacter.h"

#include <bit>
#include <cstring>
#include <string>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
namespace
{
    template <class T>
    T scalar_at(void const* base, std::size_t offset)
    {
        T value;
        std::memcpy(&value, static_cast<std::byte const*>(base) + offset, sizeof(value));
        return value;
    }

    std::optional<std::uint64_t> raw_pose_hash(RE::hkbCharacter const& character)
    {
        if (!character.poseLocal || character.numPoseLocal <= 0) return std::nullopt;
        std::uint64_t hash = 0xcbf29ce484222325ull;
        std::byte const* const bytes = reinterpret_cast<std::byte const*>(character.poseLocal);
        const std::size_t size = static_cast<std::size_t>(character.numPoseLocal) * 0x30;
        for (std::size_t i = 0; i < size; ++i)
        {
            hash ^= std::to_integer<std::uint8_t>(bytes[i]);
            hash *= 0x100000001b3ull;
        }
        return hash;
    }

    std::string slot_fields(RE::BShkbAnimationGraph const& graph, std::size_t index)
    {
        void const* const output = graph.generatorOutputs[index];
        if (!output) return "status=missing-output";
        void const* const element = scalar_at<void const*>(output, 0);
        if (!element) return "status=missing-element";
        const float fraction = scalar_at<float>(element, 0x38);
        return fmt::format("status=present element={:#x} numTracks={} numData={} onFraction={:.9g} bits={:#010x}",
            reinterpret_cast<std::uintptr_t>(element), scalar_at<std::uint32_t>(element, 0x4),
            scalar_at<std::uint16_t>(element, 0x32), fraction, std::bit_cast<std::uint32_t>(fraction));
    }
}

FrameTrace::FrameTrace(bool enabled, std::uint32_t actor_id, std::uint64_t request, RE::hkbCharacter const& baseline, void const* eye) :
    m_enabled(enabled), m_actor_id(actor_id), m_request(request), m_budget(),
    m_progress(enabled ? raw_pose_hash(baseline) : std::nullopt), m_eye(eye), m_arm_observed(false) {}

void FrameTrace::phase(RE::BShkbAnimationGraph const& graph, std::uint32_t step, FrameStage stage)
{
    if (!m_enabled || !m_budget.take(step, stage)) return;
    const float t0 = scalar_at<float>(&graph, 0x1E8);
    const float t1 = scalar_at<float>(&graph, 0x1EC);
    if (stage == FrameStage::e_post_update && arm_observed(t0, t1)) m_arm_observed = true;
    logger::info("GRAPH FRAME schema=v20 entry=63589 mode=0 intent=outer-generation-apply actor={:08x} request={} step={} stage={} completed={} graph={:#x} numAnim={} w245={:#x} b246={} b249={} t0={:.9g}/{:#010x} t1={:.9g}/{:#010x} root218={:#x} b248={} eye={:#x} slot0[{}] slot1[{}]",
        m_actor_id, m_request, step, Frame_Stage_Names[static_cast<std::size_t>(stage)], m_progress.completed(),
        reinterpret_cast<std::uintptr_t>(&graph), scalar_at<std::uint16_t>(&graph, 0x240),
        scalar_at<std::uint16_t>(&graph, 0x245), scalar_at<std::uint8_t>(&graph, 0x246),
        scalar_at<std::uint8_t>(&graph, 0x249), t0, std::bit_cast<std::uint32_t>(t0),
        t1, std::bit_cast<std::uint32_t>(t1),
        reinterpret_cast<std::uintptr_t>(scalar_at<void const*>(&graph, 0x218)),
        scalar_at<std::uint8_t>(&graph, 0x248), reinterpret_cast<std::uintptr_t>(m_eye),
        slot_fields(graph, 0), slot_fields(graph, 1));
}

void FrameTrace::sampled_pose(RE::hkbCharacter const& character)
{
    if (m_enabled) m_progress.observe(raw_pose_hash(character));
}

void FrameTrace::summary()
{
    if (!m_enabled || !m_budget.take_summary()) return;
    const std::optional<std::uint64_t> baseline = m_progress.baseline();
    const std::optional<std::uint32_t> first = m_progress.first_changed();
    logger::info("GRAPH FRAME schema=v20 entry=63589 mode=0 intent=outer-generation-apply actor={:08x} request={} stage=summary phase-rows={} total-rows={} completed={} pose-available={} pose-missing={} raw-hash-changes={} first-changed-completed={} baseline={} arm-observed={} fingerprint=raw-buffer-padding-sensitive generator-payload=unobserved native-context=unobserved acceptance=unjudged",
        m_actor_id, m_request, m_budget.rows(), m_budget.rows() + 1, m_progress.completed(),
        m_progress.available(), m_progress.missing(), m_progress.changes(),
        first ? fmt::format("{}", *first) : "none-observed",
        baseline ? fmt::format("{:016x}", *baseline) : "unavailable", m_arm_observed ? "yes" : "no");
}
}
PLUGIN_NAMESPACE_END
