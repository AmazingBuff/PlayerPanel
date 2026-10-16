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
        explicit QuerySession(std::uint32_t seed = 0x50414E4C) : selection(seed) {}
        SelectionState selection;
        void reset()
        {
            selection.reset();
            m_owner = nullptr;
            m_names.clear();
        }

        template <class F>
        [[nodiscard]] std::span<std::string const> names(void const* owner, void const* data, std::size_t count, F&& read)
        {
            if (m_owner != owner || m_name_data != data || m_names.size() != count)
            {
                m_owner = owner;
                m_name_data = data;
                m_names.clear();
                m_names.reserve(count);
                for (std::size_t i = 0; i < count; ++i)
                    m_names.push_back(read(i));
            }
            return m_names;
        }

    private:
        void const* m_owner = nullptr;
        void const* m_name_data = nullptr;
        std::vector<std::string> m_names;
    };
}
PLUGIN_NAMESPACE_END
