//
// Created by AmazingBuff on 2026/10/30.
//
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // C-VC1: a read-only census of one behaviour graph's variable table. The caller reads the RE
    // fields; this header only classifies, bounds and aligns plain values so the rules stay
    // engine-free and testable. Nothing here writes a graph, calls a native function, keeps a
    // native reference or interprets a role as a writer.
    inline constexpr std::int16_t Census_Output_Flag = 1 << 4;
    inline constexpr std::size_t Census_Max_Entries = 8192;

    [[nodiscard]] constexpr std::string_view census_type_name(std::int8_t a_type) noexcept
    {
        switch (a_type)
        {
        case 0: return "BOOL";
        case 1: return "INT8";
        case 2: return "INT16";
        case 3: return "INT32";
        case 4: return "REAL";
        case 5: return "POINTER";
        case 6: return "VECTOR3";
        case 7: return "VECTOR4";
        case 8: return "QUATERNION";
        default: return "INVALID";
        }
    }

    // Word-sized variables keep their value in the shared word array. Pointers and quad-sized
    // variables keep an index into the variant/quad arrays there, so their raw word is evidence
    // only and must not be read as a value.
    [[nodiscard]] constexpr bool census_word_value(std::int8_t a_type) noexcept
    {
        return a_type >= 0 && a_type <= 4;
    }

    // hkbRoleAttribute::FLAG_OUTPUT is metadata about the authored role, not proof of who writes
    // the variable. It is logged as a hint and never used as an input/output verdict.
    [[nodiscard]] constexpr bool census_output(std::int16_t a_flags) noexcept
    {
        return (a_flags & Census_Output_Flag) != 0;
    }

    struct CensusVariable
    {
        std::string_view name;  // empty when the string table has no entry for this index
        std::int8_t type{ -1 };
        std::int16_t role{};
        std::int16_t flags{};
        std::uint32_t word{};
        bool has_word{ false };  // false when the value set or its word array is unavailable
        std::optional<std::uint32_t> initial;
        std::optional<std::pair<std::uint32_t, std::uint32_t>> bounds;
    };

    struct CensusCounts
    {
        std::size_t total{};
        std::size_t named{};
        std::size_t boolean{};
        std::size_t integer{};
        std::size_t real{};
        std::size_t pointer{};
        std::size_t quad{};
        std::size_t unknown{};
    };

    [[nodiscard]] inline CensusCounts census_counts(std::span<CensusVariable const> a_variables)
    {
        CensusCounts counts;
        counts.total = a_variables.size();
        for (CensusVariable const& variable : a_variables)
        {
            if (!variable.name.empty()) ++counts.named;
            switch (variable.type)
            {
            case 0: ++counts.boolean; break;
            case 1:
            case 2:
            case 3: ++counts.integer; break;
            case 4: ++counts.real; break;
            case 5: ++counts.pointer; break;
            case 6:
            case 7:
            case 8: ++counts.quad; break;
            default: ++counts.unknown; break;
            }
        }
        return counts;
    }

    // The item table is bounded by the type table and the word array only. A missing or shorter
    // name table leaves those rows unnamed instead of erasing them; a name table is evidence,
    // not a length authority.
    [[nodiscard]] inline std::size_t census_bound(std::size_t a_infos, std::size_t a_words)
    {
        return std::min(std::min(a_infos, a_words), Census_Max_Entries);
    }

    struct CensusMatch
    {
        std::size_t source_index{};
        std::size_t candidate_index{};
        std::int8_t source_type{ -1 };
        std::int8_t candidate_type{ -1 };
    };

    struct CensusAlignment
    {
        std::vector<CensusMatch> matched;
        std::vector<std::size_t> source_only;
        std::vector<std::size_t> candidate_only;
    };

    // Pairs the two tables by name. Slot indices are not comparable between projects, so an
    // index match without a name match is deliberately not reported as a match.
    [[nodiscard]] inline CensusAlignment census_align(std::span<CensusVariable const> a_source,
        std::span<CensusVariable const> a_candidate)
    {
        CensusAlignment alignment;
        std::vector<bool> used(a_candidate.size(), false);
        std::unordered_map<std::string_view, std::size_t> by_name;
        by_name.reserve(a_candidate.size());
        for (std::size_t index = 0; index < a_candidate.size(); ++index)
            if (!a_candidate[index].name.empty())
                by_name.try_emplace(a_candidate[index].name, index);
        for (std::size_t index = 0; index < a_source.size(); ++index)
        {
            std::string_view const name = a_source[index].name;
            auto const found = name.empty() ? by_name.end() : by_name.find(name);
            if (found == by_name.end())
            {
                alignment.source_only.push_back(index);
                continue;
            }
            used[found->second] = true;
            alignment.matched.push_back(
                { index, found->second, a_source[index].type, a_candidate[found->second].type });
        }
        for (std::size_t index = 0; index < a_candidate.size(); ++index)
            if (!used[index])
                alignment.candidate_only.push_back(index);
        return alignment;
    }
}
PLUGIN_NAMESPACE_END
