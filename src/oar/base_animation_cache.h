//
// Created by AmazingBuff on 2026/10/21.
//

#pragma once

#include "animation/action.h"

#include <array>
#include <cctype>
#include <optional>
#include <string>
#include <utility>

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

    struct BaseAnimation
    {
        std::uint16_t index;
        std::string name;
    };

    struct AnimationTableIdentity
    {
        void const* owner;
        void const* data;
        std::size_t count;
        bool operator==(AnimationTableIdentity const&) const = default;
    };

    class BaseAnimationProject
    {
    public:
        explicit BaseAnimationProject(AnimationTableIdentity identity) : m_identity(identity)
        {
            for (std::string_view name : panel_base_animation_names())
            {
                std::string key = canonical_animation_path(name);
                if (std::ranges::find(m_entries, key, &Entry::key) == m_entries.end())
                    m_entries.push_back({ std::move(key), {}, {} });
            }
        }

        [[nodiscard]] AnimationTableIdentity identity() const { return m_identity; }
        [[nodiscard]] std::size_t requested() const { return m_entries.size(); }
        [[nodiscard]] std::size_t resolved() const
        {
            return static_cast<std::size_t>(std::ranges::count_if(m_entries,
                [](Entry const& entry) { return entry.exact || entry.fallback; }));
        }

        [[nodiscard]] std::optional<BaseAnimation> find(std::span<std::string_view const> candidates) const
        {
            for (std::string_view candidate : candidates)
            {
                const std::string key = canonical_animation_path(candidate);
                const auto at = std::ranges::find(m_entries, key, &Entry::key);
                if (at != m_entries.end())
                {
                    if (at->exact) return at->exact;
                    if (at->fallback) return at->fallback;
                }
            }
            return std::nullopt;
        }

        template <class F>
        void resolve(F&& read)
        {
            for (std::size_t index = 0; index < m_identity.count; ++index)
            {
                const std::string_view name = read(index);
                std::array<char, 1024> storage;
                if (name.empty() || name.size() >= storage.size()) continue;
                for (std::size_t i = 0; i < name.size(); ++i)
                {
                    const unsigned char character = static_cast<unsigned char>(name[i]);
                    storage[i] = character == '/' ? '\\' : static_cast<char>(std::tolower(character));
                }
                std::string_view path(storage.data(), name.size());
                if (path.contains("openanimationreplacer") || path.contains("dynamicanimationreplacer")) continue;
                const auto exact = std::ranges::find(m_entries, path, &Entry::key);
                if (exact != m_entries.end() && !exact->exact)
                {
                    exact->exact = BaseAnimation{ static_cast<std::uint16_t>(index), std::string(name) };
                    exact->fallback.reset();
                }
                for (std::string_view gender : { "animations\\female\\", "animations\\male\\" })
                {
                    const std::size_t at = path.find(gender);
                    if (at != std::string_view::npos)
                    {
                        const std::size_t begin = at + std::string_view("animations\\").size();
                        const std::size_t end = at + gender.size();
                        std::move(storage.begin() + end, storage.begin() + path.size(), storage.begin() + begin);
                        path = { storage.data(), path.size() - (end - begin) };
                    }
                }
                for (Entry& entry : m_entries)
                {
                    if (entry.exact || entry.fallback) continue;
                    if (path == entry.key || (path.size() > entry.key.size() && path.ends_with(entry.key) &&
                        path[path.size() - entry.key.size() - 1] == '\\'))
                        entry.fallback = BaseAnimation{ static_cast<std::uint16_t>(index), std::string(name) };
                }
            }
        }

    private:
        struct Entry
        {
            std::string key;
            std::optional<BaseAnimation> exact;
            std::optional<BaseAnimation> fallback;
        };
        AnimationTableIdentity m_identity;
        std::vector<Entry> m_entries;
    };

    class BaseAnimationCache
    {
    public:
        struct Preparation
        {
            BaseAnimationProject const* project;
            bool initialized;
        };

        template <class F>
        [[nodiscard]] Preparation prepare(AnimationTableIdentity identity, F&& read)
        {
            const auto found = std::ranges::find(m_projects, identity, &BaseAnimationProject::identity);
            if (found != m_projects.end()) return { &*found, false };
            std::erase_if(m_projects, [&](BaseAnimationProject const& project)
            {
                return project.identity().owner == identity.owner || project.identity().data == identity.data;
            });
            if (m_projects.size() == Max_Projects) m_projects.erase(m_projects.begin());
            BaseAnimationProject& project = m_projects.emplace_back(identity);
            project.resolve(std::forward<F>(read));
            return { &project, true };
        }

        void invalidate() { m_projects.clear(); }

        template <class F>
        [[nodiscard]] static std::string binding_name(std::size_t count, std::uint16_t index, F&& read)
        {
            return index < count ? std::string(read(index)) : std::string{};
        }

    private:
        static constexpr std::size_t Max_Projects = 4;
        std::vector<BaseAnimationProject> m_projects;
    };
}
PLUGIN_NAMESPACE_END
