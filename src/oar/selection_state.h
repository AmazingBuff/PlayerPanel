//
// Created by AmazingBuff on 2026/10/16.
//

#pragma once

#include "base/def.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

PLUGIN_NAMESPACE_BEGIN
namespace oar
{
    enum class DataScope : std::uint8_t
    {
        e_local = 1, e_submod = 2, e_replacer_mod = 4, e_reference = 8
    };

    struct StateKey
    {
        DataScope scope = DataScope::e_local;
        std::uint64_t owner = 0;
        std::uint64_t group = 0;
        bool operator==(StateKey const&) const = default;
    };

    struct StateKeyHash
    {
        std::size_t operator()(StateKey const& key) const
        {
            size_t seed = Amazing_Hash;
            seed = hash_combine(seed, key.owner);
            seed = hash_combine(seed, key.group);
            return hash_combine(seed, key.scope);
        }
    };

    struct VariantEntry
    {
        std::uint16_t index;
        bool once = false;
        std::string path;
    };

    struct VariantOptions
    {
        StateKey key;
        bool sequential = false;
        bool reset_on_loop = true;
        bool share_history = false;
        std::vector<VariantEntry> sequence;
        std::vector<std::uint16_t> random;
        std::vector<std::string> random_paths;
        std::vector<float> cumulative;
    };

    struct VariantPick
    {
        std::uint16_t index = 0xFFFF;
        std::size_t position = std::numeric_limits<std::size_t>::max();
    };

    class ConditionRandomState
    {
    public:
        explicit ConditionRandomState(std::uint32_t seed = 0x50414E4C) : m_random(seed) {}
        void reset() { m_conditions.clear(); }
        [[nodiscard]] std::optional<float> random(StateKey key, std::uint64_t condition,
            std::uint64_t clip, bool reset_on_loop, float minimum, float maximum)
        {
            if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum > maximum)
                return std::nullopt;
            const RandomKey lookup{ key, condition };
            auto [entry, added] = m_conditions.try_emplace(lookup);
            if (added)
            {
                entry->second.value = static_cast<float>(static_cast<double>(minimum) +
                    (static_cast<double>(maximum) - minimum) * unit_random());
                entry->second.reset_on_loop = reset_on_loop;
            }
            register_clip(entry->second.clips, clip);
            return entry->second.value;
        }

        void loop(std::uint64_t clip)
        {
            std::erase_if(m_conditions, [&](auto const& entry)
            {
                return entry.second.reset_on_loop && std::ranges::find(entry.second.clips, clip) != entry.second.clips.end();
            });
        }
    private:
        struct RandomKey
        {
            StateKey key;
            std::uint64_t condition;
            bool operator==(RandomKey const&) const = default;
        };
        struct RandomKeyHash
        {
            std::size_t operator()(RandomKey const& key) const
            {
                return hash_combine(StateKeyHash{}(key.key), key.condition);
            }
        };
        struct RandomState
        {
            float value = 0.0f;
            bool reset_on_loop = false;
            std::vector<std::uint64_t> clips;
        };
        static void register_clip(std::vector<std::uint64_t>& clips, std::uint64_t clip)
        {
            if (std::ranges::find(clips, clip) == clips.end()) clips.push_back(clip);
        }
        double unit_random() { return static_cast<double>(m_random() >> 8) / 16777216.0; }
        std::mt19937 m_random;
        std::unordered_map<RandomKey, RandomState, RandomKeyHash> m_conditions;
    };
}
PLUGIN_NAMESPACE_END
