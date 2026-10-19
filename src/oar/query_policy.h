//
// Created by AmazingBuff on 2026/10/16.
//

#pragma once

#include "oar/selection_state.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

PLUGIN_NAMESPACE_BEGIN
namespace oar
{
    [[nodiscard]] inline std::string canonical_animation_path(std::string_view path)
    {
        std::string result;
        result.reserve(path.size());
        for (unsigned char character : path)
            result.push_back(character == '/' ? '\\' : static_cast<char>(std::tolower(character)));
        return result;
    }

    [[nodiscard]] inline std::optional<std::uint16_t> find_base_animation(
        std::span<std::string const> names, std::span<std::string_view const> candidates)
    {
        for (std::string_view candidate : candidates)
        {
            const std::string wanted = canonical_animation_path(candidate);
            for (std::size_t i = 0; i < names.size() && i < 0xFFFF; ++i)
            {
                if (canonical_animation_path(names[i]) == wanted)
                    return static_cast<std::uint16_t>(i);
            }
            for (std::size_t i = 0; i < names.size() && i < 0xFFFF; ++i)
            {
                std::string path = canonical_animation_path(names[i]);
                if (path.find("openanimationreplacer") != std::string::npos ||
                    path.find("dynamicanimationreplacer") != std::string::npos)
                    continue;
                for (std::string_view gender : { "animations\\female\\", "animations\\male\\" })
                {
                    const std::size_t at = path.find(gender);
                    if (at != std::string::npos)
                        path.replace(at, gender.size(), "animations\\");
                }
                if (path == wanted || path.ends_with("\\" + wanted))
                    return static_cast<std::uint16_t>(i);
            }
        }
        return std::nullopt;
    }

    class QuerySession
    {
    public:
        explicit QuerySession(std::uint32_t seed = 0x50414E4C) : conditions(seed) {}
        ConditionRandomState conditions;
        void reset()
        {
            conditions.reset();
            m_owner = nullptr;
            m_names.clear();
        }

        template <class F>
        [[nodiscard]] std::span<std::string const> names(void const* owner, void const* data, std::size_t count, F&& read)
        {
            if (m_generation != project_generation() || m_owner != owner || m_name_data != data || m_names.size() != count)
            {
                m_generation = project_generation();
                m_owner = owner;
                m_name_data = data;
                // The catalogue is pure derived data of the source project's name table and
                // outlives panel sessions: rebuilding it on the inventory-open frame walked
                // ~15k entries on the main thread and stalled the panel for seconds
                // (oar-idle2 report, problem 5). The session-local previous behaviour remains
                // as the miss path; the process cache only removes the rebuild.
                m_names.clear();
                if (Catalogue const* const cached = project_catalogue(owner, data, count))
                {
                    m_names = cached->names;
                }
                else
                {
                    m_names.reserve(count);
                    for (std::size_t i = 0; i < count; ++i)
                        m_names.push_back(read(i));
                    remember_catalogue(owner, data, count, m_names);
                }
            }
            return m_names;
        }

        static void invalidate_projects() { ++project_generation(); catalogues().clear(); }
    private:
        static std::uint64_t& project_generation() { static std::uint64_t s_generation = 0; return s_generation; }
        std::uint64_t m_generation = 0;
        struct Catalogue
        {
            void const* owner = nullptr;
            void const* data = nullptr;
            std::size_t count = 0;
            std::vector<std::string> names;
        };

        // The table is engine-owned and replaced at a load boundary. A replacement usually gets a
        // NEW owner/data pointer pair, so the old entry cannot be matched and updated; keep the
        // cache bounded instead of appending one full ~15k-name table per load (round 16).
        static constexpr std::size_t Max_Catalogues = 4;

        [[nodiscard]] static Catalogue const* project_catalogue(void const* owner, void const* data, std::size_t count)
        {
            for (Catalogue& entry : catalogues())
            {
                if (entry.owner == owner && entry.data == data && entry.count == count)
                    return &entry;
            }
            return nullptr;
        }

        static void remember_catalogue(void const* owner, void const* data, std::size_t count,
            std::vector<std::string> const& names)
        {
            for (Catalogue& entry : catalogues())
            {
                if (entry.owner == owner || entry.data == data)
                {
                    entry.owner = owner;
                    entry.data = data;
                    entry.count = count;
                    entry.names = names;
                    return;
                }
            }
            if (catalogues().size() >= Max_Catalogues)
                catalogues().erase(catalogues().begin());
            catalogues().push_back({ owner, data, count, names });
        }

        static std::vector<Catalogue>& catalogues()
        {
            static std::vector<Catalogue> s_catalogues;
            return s_catalogues;
        }

        void const* m_owner = nullptr;
        void const* m_name_data = nullptr;
        std::vector<std::string> m_names;
    };
}
PLUGIN_NAMESPACE_END
