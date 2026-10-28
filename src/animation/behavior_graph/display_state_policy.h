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

    [[nodiscard]] inline bool same_display_source(DisplaySourceIdentity const& retained,
        DisplaySourceIdentity const& current)
    {
        return retained.graph != 0 && retained.owner != 0 && retained.actor_id == current.actor_id &&
            retained.generation == current.generation && retained.graph == current.graph && retained.owner == current.owner;
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
        std::array<std::string, 4> missing_names;
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
