//
// Created by AmazingBuff on 2026/10/26.
//
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
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
        e_post_sample,
        e_count
    };

    inline constexpr std::array<std::uint32_t, 6> Frame_Trace_Steps{ 0, 1, 2, 59, 60, 119 };
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

    // The arm block rewrites the smaller timer away from its {0,-MAXFLT} reset value.
    [[nodiscard]] inline bool arm_observed(float t0, float t1)
    {
        const float smaller = t0 < t1 ? t0 : t1;
        return smaller > std::numeric_limits<float>::lowest();
    }

    class FrameTraceBudget
    {
    public:
        FrameTraceBudget() : m_seen{}, m_rows(0), m_summary(false) {}

        [[nodiscard]] bool take(std::uint32_t step, FrameStage stage)
        {
            const std::size_t stage_index = static_cast<std::size_t>(stage);
            if (stage_index >= Frame_Stage_Names.size()) return false;
            for (std::size_t i = 0; i < Frame_Trace_Steps.size(); ++i)
            {
                if (step != Frame_Trace_Steps[i]) continue;
                const std::uint8_t bit = static_cast<std::uint8_t>(1u << stage_index);
                if ((m_seen[i] & bit) != 0) return false;
                m_seen[i] |= bit;
                ++m_rows;
                return true;
            }
            return false;
        }

        [[nodiscard]] bool take_summary()
        {
            if (m_summary) return false;
            m_summary = true;
            return true;
        }

        [[nodiscard]] std::uint32_t rows() const { return m_rows; }

    private:
        std::array<std::uint8_t, Frame_Trace_Steps.size()> m_seen;
        std::uint32_t m_rows;
        bool m_summary;
    };

    class RawPoseProgress
    {
    public:
        explicit RawPoseProgress(std::optional<std::uint64_t> baseline) :
            m_baseline(baseline), m_previous(baseline), m_completed(0), m_available(0), m_missing(0),
            m_changes(0), m_first_changed(std::nullopt) {}

        void observe(std::optional<std::uint64_t> hash)
        {
            ++m_completed;
            if (!hash) { ++m_missing; return; }
            ++m_available;
            if (m_previous && *m_previous != *hash)
            {
                ++m_changes;
                if (!m_first_changed) m_first_changed = m_completed;
            }
            m_previous = hash;
        }

        [[nodiscard]] std::optional<std::uint64_t> baseline() const { return m_baseline; }
        [[nodiscard]] std::uint32_t completed() const { return m_completed; }
        [[nodiscard]] std::uint32_t available() const { return m_available; }
        [[nodiscard]] std::uint32_t missing() const { return m_missing; }
        [[nodiscard]] std::uint32_t changes() const { return m_changes; }
        [[nodiscard]] std::optional<std::uint32_t> first_changed() const { return m_first_changed; }

    private:
        std::optional<std::uint64_t> m_baseline;
        std::optional<std::uint64_t> m_previous;
        std::uint32_t m_completed;
        std::uint32_t m_available;
        std::uint32_t m_missing;
        std::uint32_t m_changes;
        std::optional<std::uint32_t> m_first_changed;
    };
}
PLUGIN_NAMESPACE_END
