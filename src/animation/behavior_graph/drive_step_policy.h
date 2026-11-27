//
// Created by AmazingBuff on 2026/10/28.
//
#pragma once

#include "animation/behavior_graph/candidate_window.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    enum class FrameStage : std::uint8_t
    {
        e_pre_pipeline,
        e_post_pipeline,
        e_post_update,
        e_pre_sample,
        e_post_sample
    };

    inline constexpr std::array<std::string_view, 5> Frame_Stage_Names{
        "pre-pipeline", "post-pipeline", "post-update", "pre-sample", "post-sample"
    };

    // v19 contract: the eye point is the candidate root's position shifted along Z
    // by more than the arm block's 800-unit cap, which yields its maximum 0.25 step.
    inline constexpr float Eye_Offset_Z = 900.0F;

    struct EyePoint
    {
        float x;
        float y;
        float z;
    };

    [[nodiscard]] inline EyePoint make_eye_point(std::optional<std::array<float, 3>> root_position)
    {
        if (!root_position) return EyePoint{ 0.0F, 0.0F, 0.0F };
        return EyePoint{ (*root_position)[0], (*root_position)[1], (*root_position)[2] + Eye_Offset_Z };
    }

    template <class Graph>
    class ScopedGraphWorld
    {
    public:
        ScopedGraphWorld(Graph& graph, Graph const& source) : m_ref_graph(graph), m_previous(graph.physicsWorld)
        {
            m_ref_graph.physicsWorld = source.physicsWorld;
        }

        ~ScopedGraphWorld() { m_ref_graph.physicsWorld = m_previous; }
        ScopedGraphWorld(ScopedGraphWorld const&) = delete;
        ScopedGraphWorld& operator=(ScopedGraphWorld const&) = delete;

    private:
        Graph& m_ref_graph;
        decltype(Graph::physicsWorld) m_previous;
    };

    // Native side effects are supplied at their boundary; both callers retain the same step.
    // The C-G4-A window guard arms for the step's native calls: OAR evaluate detours count
    // their hits only while candidate activity is actually executing.
    template <class Graph, class Pipeline, class Update, class Generate, class Observe>
    void drive_graph_step(Graph& graph, Graph const& source, float timestep, EyePoint const* eye,
        Pipeline const& pipeline, Update const& update, Generate const& generate, Observe const& observe)
    {
        ScopedCandidateWindow const candidate_window;
        ScopedGraphWorld<Graph> const world(graph, source);
        observe(FrameStage::e_pre_pipeline);
        pipeline(graph);
        observe(FrameStage::e_post_pipeline);

        graph.characterInstance.behaviorGraph->updateActiveNodes = true;
        // Audited AE1170 BSAnimationUpdateData: dt@0, optionalEyePosition@0x18, modes@0x2A.
        alignas(8) std::array<std::byte, 0x30> record{};
        std::memcpy(record.data(), &timestep, sizeof(timestep));
        std::memcpy(record.data() + 0x18, &eye, sizeof(eye));
        record[0x2B] = std::byte{ 1 };
        record[0x2C] = std::byte{ 1 };
        record[0x2E] = std::byte{ 1 };
        update(graph, record.data());
        observe(FrameStage::e_post_update);

        graph.numAnimBones = static_cast<std::uint16_t>(graph.characterInstance.numPoseLocal);
        observe(FrameStage::e_pre_sample);
        generate(graph);
        observe(FrameStage::e_post_sample);
    }
}
PLUGIN_NAMESPACE_END
