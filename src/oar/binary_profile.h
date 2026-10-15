#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
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
            0x57, 0x48, 0x8D, 0x68, 0xA1, 0x48, 0x81 }
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
            0x70, 0x20, 0x55, 0x57, 0x41, 0x56, 0x48 }
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
