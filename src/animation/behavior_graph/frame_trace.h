//
// Created by AmazingBuff on 2026/10/26.
//
#pragma once

#include "animation/behavior_graph/frame_trace_policy.h"

namespace RE
{
    class BShkbAnimationGraph;
    class hkbCharacter;
}

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    class FrameTrace
    {
    public:
        FrameTrace(bool enabled, std::uint32_t actor_id, std::uint64_t request, RE::hkbCharacter const& baseline);

        void phase(RE::BShkbAnimationGraph const& graph, std::uint32_t step, FrameStage stage);
        void sampled_pose(RE::hkbCharacter const& character);
        void summary();

    private:
        bool m_enabled;
        std::uint32_t m_actor_id;
        std::uint64_t m_request;
        FrameTraceBudget m_budget;
        RawPoseProgress m_progress;
    };
}
PLUGIN_NAMESPACE_END
