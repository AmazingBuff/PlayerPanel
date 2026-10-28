//
// Created by AmazingBuff on 2026/10/28.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/drive_step_policy.h"
#include "animation/behavior_graph/graph_observation_policy.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

#ifdef NDEBUG
#error This test requires assertions.
#endif

namespace
{
    using test_plugin::animation::behavior_graph::EyePoint;
    using test_plugin::animation::behavior_graph::FrameStage;
    using test_plugin::animation::behavior_graph::drive_graph_step;
    using test_plugin::animation::behavior_graph::observation_scalar;
    using test_plugin::animation::behavior_graph::observe_graph_activity;
    using test_plugin::animation::behavior_graph::observe_output_slot;

    struct Behavior
    {
        bool isActive;
        bool updateActiveNodes;
    };

    struct Character
    {
        Behavior* behaviorGraph;
        std::int32_t numPoseLocal;
    };

    struct Graph
    {
        std::array<std::byte, 0x130> outer;
        Behavior* behaviorGraph;
        Character characterInstance;
        void* physicsWorld;
        std::uint16_t numAnimBones;
        float clip_time;
        float pose;
    };

    void a_display_step_advances_pose_and_restores_its_borrowed_world()
    {
        Behavior behavior{ true, false };
        int source_world = 0;
        int previous_world = 0;
        Graph source{};
        source.physicsWorld = &source_world;
        Graph candidate{};
        candidate.behaviorGraph = &behavior;
        candidate.characterInstance = Character{ &behavior, 116 };
        candidate.physicsWorld = &previous_world;
        EyePoint const eye{ 0.0F, 0.0F, 0.0F };

        const auto pipeline = [](Graph& graph) { graph.numAnimBones = 0; };
        const auto update = [&](Graph& graph, void const* record) {
            assert(graph.physicsWorld == source.physicsWorld);
            assert(graph.characterInstance.behaviorGraph->updateActiveNodes);
            assert(observation_scalar<EyePoint const*>(record, 0x18) == &eye);
            assert(observation_scalar<std::uint8_t>(record, 0x2A) == 0);
            assert(observation_scalar<std::uint8_t>(record, 0x2B) == 1);
            assert(observation_scalar<std::uint8_t>(record, 0x2C) == 1);
            assert(observation_scalar<std::uint8_t>(record, 0x2D) == 0);
            assert(observation_scalar<std::uint8_t>(record, 0x2E) == 1);
            graph.clip_time += observation_scalar<float>(record, 0);
            graph.characterInstance.behaviorGraph->updateActiveNodes = false;
        };
        const auto generate = [](Graph& graph) {
            assert(graph.numAnimBones == graph.characterInstance.numPoseLocal);
            graph.pose = std::sin(graph.clip_time);
        };
        const auto observe = [](FrameStage) {};

        drive_graph_step(candidate, source, 1.0F / 60.0F, &eye, pipeline, update, generate, observe);
        assert(candidate.pose > 0.0F);
        assert(candidate.physicsWorld == &previous_world);
        assert(source.clip_time == 0.0F && source.pose == 0.0F);
    }

    void observations_follow_behavior_and_output_element_objects()
    {
        Behavior behavior{ true, true };
        Graph graph{};
        graph.outer[0x12C] = std::byte{ 17 };
        graph.outer[0x12E] = std::byte{ 0 };
        graph.behaviorGraph = &behavior;
        const auto activity = observe_graph_activity(graph);
        assert(activity.active && activity.update_active_nodes);

        std::array<std::byte, 0x40> output{};
        std::array<std::byte, 0x40> element{};
        void const* const element_pointer = element.data();
        std::uint16_t const invalid_num_data = 62016;
        std::uint32_t const tracks = 21;
        std::uint16_t const bones = 116;
        float const fraction = 1.0F;
        std::memcpy(output.data(), &element_pointer, sizeof(element_pointer));
        std::memcpy(output.data() + 0x32, &invalid_num_data, sizeof(invalid_num_data));
        std::memcpy(element.data() + 0x4, &tracks, sizeof(tracks));
        std::memcpy(element.data() + 0x32, &bones, sizeof(bones));
        std::memcpy(element.data() + 0x38, &fraction, sizeof(fraction));
        const auto slot = observe_output_slot(output.data());
        assert(slot && slot->element == element_pointer);
        assert(slot->num_tracks == tracks && slot->num_data == bones && slot->on_fraction == fraction);
    }
}

int main()
{
    a_display_step_advances_pose_and_restores_its_borrowed_world();
    observations_follow_behavior_and_output_element_objects();
    std::cout << "2 display-step regression cases passed (engine-free)\n";
}
