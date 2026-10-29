//
// Created by AmazingBuff on 2026/10/28.
//
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    struct DisplaySourceIdentity
    {
        std::uint32_t actor_id;
        std::uint64_t generation;
        std::uintptr_t graph;
        std::uintptr_t owner;
    };

    enum class DisplaySourceRejection : std::uint8_t
    {
        e_none,
        e_unbound_retained,
        e_actor_mismatch,
        e_generation_mismatch,
        e_graph_mismatch,
        e_owner_mismatch,
        e_inactive_view,
        e_call_graph_mismatch,
        e_candidate_unavailable
    };

    [[nodiscard]] inline DisplaySourceRejection display_source_rejection(DisplaySourceIdentity const& retained,
        DisplaySourceIdentity const& current)
    {
        if (retained.graph == 0 || retained.owner == 0) return DisplaySourceRejection::e_unbound_retained;
        if (retained.actor_id != current.actor_id) return DisplaySourceRejection::e_actor_mismatch;
        if (retained.generation != current.generation) return DisplaySourceRejection::e_generation_mismatch;
        if (retained.graph != current.graph) return DisplaySourceRejection::e_graph_mismatch;
        if (retained.owner != current.owner) return DisplaySourceRejection::e_owner_mismatch;
        return DisplaySourceRejection::e_none;
    }

    [[nodiscard]] inline char const* display_source_rejection_name(DisplaySourceRejection reason)
    {
        switch (reason)
        {
        case DisplaySourceRejection::e_none: return "none";
        case DisplaySourceRejection::e_unbound_retained: return "unbound-retained";
        case DisplaySourceRejection::e_actor_mismatch: return "actor-mismatch";
        case DisplaySourceRejection::e_generation_mismatch: return "source-generation-mismatch";
        case DisplaySourceRejection::e_graph_mismatch: return "source-graph-mismatch";
        case DisplaySourceRejection::e_owner_mismatch: return "controller-owner-mismatch";
        case DisplaySourceRejection::e_inactive_view: return "inactive-view";
        case DisplaySourceRejection::e_call_graph_mismatch: return "call-graph-mismatch";
        case DisplaySourceRejection::e_candidate_unavailable: return "candidate-unavailable";
        }
        return "unknown";
    }

    [[nodiscard]] inline bool same_display_source(DisplaySourceIdentity const& retained,
        DisplaySourceIdentity const& current)
    {
        return display_source_rejection(retained, current) == DisplaySourceRejection::e_none;
    }

    inline bool invalidate_display_source(DisplaySourceIdentity& retained, std::uintptr_t owner)
    {
        if (retained.owner != owner) return false;
        retained = {};
        return true;
    }

    class DisplayWindow
    {
    public:
        DisplayWindow() : m_source{}, m_request(0), m_view(0), m_steps(0), m_rows(0), m_active(false) {}

        bool bind(DisplaySourceIdentity source, std::uint64_t request)
        {
            if (m_active && same_display_source(m_source, source) && m_request == request) return false;
            m_source = source;
            m_request = request;
            m_view = 0;
            m_steps = 0;
            m_rows = 0;
            m_active = true;
            return true;
        }

        void retarget() { ++m_view; m_steps = 0; m_rows = 0; }
        void end() { m_active = false; m_steps = 0; }
        void advanced() { ++m_steps; }
        [[nodiscard]] bool accepts(DisplaySourceIdentity const& retained) const
        {
            return m_active && same_display_source(retained, m_source);
        }
        [[nodiscard]] DisplaySourceRejection rejection(DisplaySourceIdentity const& retained, std::uintptr_t call_graph) const
        {
            if (!m_active) return DisplaySourceRejection::e_inactive_view;
            DisplaySourceRejection const reason = display_source_rejection(retained, m_source);
            if (reason != DisplaySourceRejection::e_none) return reason;
            if (m_source.graph != call_graph) return DisplaySourceRejection::e_call_graph_mismatch;
            return DisplaySourceRejection::e_none;
        }
        [[nodiscard]] bool reject()
        {
            bool const report = take_row();
            end();
            return report;
        }
        [[nodiscard]] bool take_row()
        {
            if (!m_active || m_rows >= 18) return false;
            ++m_rows;
            return true;
        }
        [[nodiscard]] DisplaySourceIdentity const& source() const { return m_source; }
        [[nodiscard]] std::uint64_t request() const { return m_request; }
        [[nodiscard]] std::uint64_t view() const { return m_view; }
        [[nodiscard]] std::uint32_t steps() const { return m_steps; }
        [[nodiscard]] bool active() const { return m_active; }

    private:
        DisplaySourceIdentity m_source;
        std::uint64_t m_request;
        std::uint64_t m_view;
        std::uint32_t m_steps;
        std::uint32_t m_rows;
        bool m_active;
    };

    struct DisplayApplicationCoverage
    {
        std::size_t requested;
        std::size_t applied;
        std::size_t missing;
        std::size_t skipped_root;
        std::size_t missing_name_count;
        std::array<std::string, 8> missing_names;
    };

    template <class Pose, class Nodes>
    [[nodiscard]] DisplayApplicationCoverage apply_named_display_pose(Pose const& pose, Nodes const& nodes,
        std::string_view excluded_root_name)
    {
        DisplayApplicationCoverage coverage{};
        for (auto const& [name, local] : pose)
        {
            ++coverage.requested;
            if (auto const target = nodes.find(name); target != nodes.end())
            {
                target->second->local = local;
                ++coverage.applied;
            }
            else if (!excluded_root_name.empty() && std::string_view(name) == excluded_root_name)
                ++coverage.skipped_root;
            else
            {
                ++coverage.missing;
                if (coverage.missing_name_count < coverage.missing_names.size())
                    coverage.missing_names[coverage.missing_name_count++] = name;
            }
        }
        return coverage;
    }
}
PLUGIN_NAMESPACE_END
