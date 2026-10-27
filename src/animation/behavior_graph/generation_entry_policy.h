//
// Created by AmazingBuff on 2026/10/27.
//
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    struct GenerationEntryView
    {
        bool active;
        std::int32_t active_count;
        std::int32_t variable_count;
        std::int32_t pose_count;
        std::uintptr_t outer_behavior;
        std::uintptr_t character_behavior;
        std::uintptr_t active_nodes;
        std::uintptr_t active_data;
        std::uintptr_t variable_values;
        std::uintptr_t variable_data;
        std::uintptr_t pose;
        std::array<std::uintptr_t, 2> outputs;
        std::array<std::uintptr_t, 2> elements;
    };

    enum class GenerationEntryStatus : std::uint8_t
    {
        e_ready,
        e_behavior_missing,
        e_behavior_mismatch,
        e_behavior_alias,
        e_inactive,
        e_runtime_storage_missing,
        e_runtime_storage_alias,
        e_output_missing,
        e_output_alias
    };

    [[nodiscard]] inline GenerationEntryStatus generation_entry_status(
        GenerationEntryView const& candidate, GenerationEntryView const& source)
    {
        if (!candidate.outer_behavior || !candidate.character_behavior)
            return GenerationEntryStatus::e_behavior_missing;
        if (candidate.outer_behavior != candidate.character_behavior)
            return GenerationEntryStatus::e_behavior_mismatch;
        if (candidate.outer_behavior == source.outer_behavior || candidate.outer_behavior == source.character_behavior)
            return GenerationEntryStatus::e_behavior_alias;
        if (!candidate.active) return GenerationEntryStatus::e_inactive;
        if (!candidate.active_nodes || !candidate.active_data || candidate.active_count <= 0 ||
            !candidate.variable_values || !candidate.variable_data || candidate.variable_count <= 0 ||
            !candidate.pose || candidate.pose_count <= 0)
            return GenerationEntryStatus::e_runtime_storage_missing;
        if (candidate.active_nodes == source.active_nodes || candidate.active_data == source.active_data ||
            candidate.variable_values == source.variable_values || candidate.variable_data == source.variable_data ||
            candidate.pose == source.pose)
            return GenerationEntryStatus::e_runtime_storage_alias;
        for (std::size_t i = 0; i < candidate.outputs.size(); ++i)
        {
            if (!candidate.outputs[i] || !candidate.elements[i]) return GenerationEntryStatus::e_output_missing;
            for (std::size_t j = 0; j < source.outputs.size(); ++j)
            {
                if (candidate.outputs[i] == source.outputs[j] || candidate.elements[i] == source.elements[j])
                    return GenerationEntryStatus::e_output_alias;
            }
        }
        if (candidate.outputs[0] == candidate.outputs[1] || candidate.elements[0] == candidate.elements[1])
            return GenerationEntryStatus::e_output_alias;
        return GenerationEntryStatus::e_ready;
    }

    [[nodiscard]] inline std::string_view generation_entry_reason(GenerationEntryStatus status)
    {
        switch (status)
        {
        case GenerationEntryStatus::e_ready: return "ready";
        case GenerationEntryStatus::e_behavior_missing: return "behavior-missing";
        case GenerationEntryStatus::e_behavior_mismatch: return "outer-character-mismatch";
        case GenerationEntryStatus::e_behavior_alias: return "source-behavior-alias";
        case GenerationEntryStatus::e_inactive: return "behavior-inactive";
        case GenerationEntryStatus::e_runtime_storage_missing: return "runtime-storage-missing";
        case GenerationEntryStatus::e_runtime_storage_alias: return "source-runtime-storage-alias";
        case GenerationEntryStatus::e_output_missing: return "output-storage-missing";
        case GenerationEntryStatus::e_output_alias: return "output-storage-alias";
        default: return "unknown-guard-status";
        }
    }
}
PLUGIN_NAMESPACE_END
