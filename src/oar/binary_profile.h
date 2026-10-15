#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

PLUGIN_NAMESPACE_BEGIN
namespace oar
{
    // NumericValue::GetValue reads its own type at +0x80 and the graph variable's type and
    // name at +0xF0/+0xF8 in both verified builds; confirmed by disassembling GetValue in
    // each DLL (docs/oar-hooks.md). The public condition API header is byte-identical
    // between 3.1.6 and 3.2.1, so the mirrored interfaces hold for both.
    inline constexpr std::size_t Numeric_Type_Offset = 0x80;
    inline constexpr std::size_t Graph_Type_Offset = 0xF0;
    inline constexpr std::size_t Graph_Name_Offset = 0xF8;

    // The active-query chain (verified on 3.1.6 on 2026-10-16 from the installed DLL/PDB, the same
    // binary the two condition hooks are verified against). Asking OAR what it would choose is
    // what makes the panel's idle independent of what the engine happened to activate: the
    // decision function is OAR's own, and the conditions it evaluates run through the hooks above,
    // so the standing answers apply. A zero RVA means "no verified offsets for this build": the
    // query is disabled and the two condition hooks keep working (3.2.1 is in that state).
    struct QueryEntry
    {
        std::uintptr_t rva;
        std::array<std::uint8_t, 16> prefix;
    };

    struct QueryProfile
    {
        // Order: GetSingleton, ForEachReplacerProjectData, ReplacerProjectData::GetAnimationReplacements,
        // AnimationReplacements::EvaluateConditionsAndGetReplacementAnimation.
        std::array<QueryEntry, 4> entries;
    };

    inline constexpr QueryProfile Query_3_1_6{
        QueryEntry{ 0x0F7CD0, { 0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x8B, 0x0D, 0x3C, 0x91, 0x29, 0x00, 0x65, 0x48, 0x8B, 0x04 } },
        QueryEntry{ 0x199780, { 0x48, 0x89, 0x5C, 0x24, 0x18, 0x55, 0x56, 0x57, 0x48, 0x83, 0xEC, 0x50, 0x48, 0x8B, 0x05, 0xAD } },
        QueryEntry{ 0x1EBEA0, { 0x48, 0xB8, 0x25, 0x23, 0x22, 0x84, 0xE4, 0x9C, 0xF2, 0xCB, 0x44, 0x0F, 0xB6, 0xC2, 0x4C, 0x33 } },
        QueryEntry{ 0x1A5720, { 0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0xB8, 0xFF, 0xFF, 0x00, 0x00, 0x41, 0x0F, 0xB7, 0xD8, 0x66 } },
    };

    inline constexpr QueryProfile Query_3_2_1{
        QueryEntry{ 0, {} }, QueryEntry{ 0, {} }, QueryEntry{ 0, {} }, QueryEntry{ 0, {} },
    };

    // The query half of the identity check: every entry with a non-zero RVA must carry the exact
    // bytes the profile recorded, so a build whose query offsets are unverified never runs one.
    [[nodiscard]] inline bool matches_query(QueryProfile const& a_profile, std::uint32_t a_timestamp,
        std::uint32_t a_image_size, std::span<std::uint8_t const, 16> a_guid, std::uint32_t a_age,
        std::span<std::uint8_t const> a_image)
    {
        (void)a_timestamp;
        (void)a_image_size;
        (void)a_guid;
        (void)a_age;
        for (QueryEntry const& entry : a_profile.entries)
        {
            if (entry.rva == 0)
                continue;
            if (entry.rva + entry.prefix.size() > a_image.size())
                return false;
            if (std::memcmp(a_image.data() + entry.rva, entry.prefix.data(), entry.prefix.size()) != 0)
                return false;
        }
        return true;
    }

    struct BinaryProfile
    {
        std::array<std::uint16_t, 4> version;
        std::uint32_t timestamp;
        std::uint32_t image_size;
        std::array<std::uint8_t, 16> pdb_guid;
        std::uint32_t pdb_age;
        std::uintptr_t evaluate_rva;
        std::uintptr_t numeric_rva;
        std::array<std::uint8_t, 16> evaluate_prefix;
        std::array<std::uint8_t, 16> numeric_prefix;
        QueryProfile query;
    };

    // Nexus main file, DLL/PDB verified on 2026-10-15 and installed in the test environment.
    inline constexpr BinaryProfile Profile_3_1_6{
        { 3, 1, 6, 0 },
        0x6A40292A,
        0x3B3000,
        { 0x73, 0x4F, 0x5B, 0x73, 0xD0, 0xD7, 0x6C, 0x4E,
            0xAA, 0x8D, 0x3E, 0x9F, 0x83, 0x84, 0xC6, 0xB5 },
        1,
        0x1217B0,
        0x1FF8B0,
        { 0x48, 0x83, 0xEC, 0x28, 0x80, 0x79, 0x10, 0x00,
            0x74, 0x07, 0xB0, 0x01, 0x48, 0x83, 0xC4, 0x28 },
        { 0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x18, 0x55, 0x56,
            0x57, 0x48, 0x8D, 0x68, 0xA1, 0x48, 0x81 },
        Query_3_1_6
    };

    // Nexus main file, DLL/PDB verified on 2026-10-15. See docs/oar-hooks.md.
    inline constexpr BinaryProfile Profile_3_2_1{
        { 3, 2, 1, 0 },
        0x6A95A9E5,
        0x2F6000,
        { 0xF2, 0x65, 0xEB, 0x15, 0xDE, 0x91, 0x98, 0x4A,
            0x99, 0xCE, 0x9C, 0xA2, 0x19, 0x2E, 0x6A, 0xF4 },
        1,
        0x68640,
        0x1417F0,
        { 0x48, 0x83, 0xEC, 0x28, 0x80, 0x79, 0x10, 0x00,
            0x74, 0x07, 0xB0, 0x01, 0x48, 0x83, 0xC4, 0x28 },
        { 0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x18, 0x48, 0x89,
            0x70, 0x20, 0x55, 0x57, 0x41, 0x56, 0x48 },
        Query_3_2_1
    };

    inline constexpr std::array<BinaryProfile, 2> Profiles{ Profile_3_1_6, Profile_3_2_1 };

    [[nodiscard]] inline bool matches(BinaryProfile const& profile, std::uint32_t timestamp,
        std::uint32_t image_size, std::span<std::uint8_t const> guid, std::uint32_t age,
        std::span<std::uint8_t const> evaluate, std::span<std::uint8_t const> numeric)
    {
        return timestamp == profile.timestamp && image_size == profile.image_size && age == profile.pdb_age &&
            std::ranges::equal(guid, profile.pdb_guid) && std::ranges::equal(evaluate, profile.evaluate_prefix) &&
            std::ranges::equal(numeric, profile.numeric_prefix);
    }
}
PLUGIN_NAMESPACE_END
