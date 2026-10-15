#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

PLUGIN_NAMESPACE_BEGIN
namespace oar::profile_3_2_1
{
    // Nexus main file, DLL/PDB verified on 2026-10-15. See docs/oar-hooks.md.
    inline constexpr std::uint32_t Timestamp = 0x6A95A9E5;
    inline constexpr std::uint32_t Image_Size = 0x2F6000;
    inline constexpr std::array<std::uint8_t, 16> Pdb_Guid{
        0xF2, 0x65, 0xEB, 0x15, 0xDE, 0x91, 0x98, 0x4A,
        0x99, 0xCE, 0x9C, 0xA2, 0x19, 0x2E, 0x6A, 0xF4
    };
    inline constexpr std::uint32_t Pdb_Age = 1;
    inline constexpr std::uintptr_t Evaluate_Rva = 0x68640;
    inline constexpr std::uintptr_t Numeric_Rva = 0x1417F0;
    inline constexpr std::array<std::uint8_t, 16> Evaluate_Prefix{
        0x48, 0x83, 0xEC, 0x28, 0x80, 0x79, 0x10, 0x00,
        0x74, 0x07, 0xB0, 0x01, 0x48, 0x83, 0xC4, 0x28
    };
    inline constexpr std::array<std::uint8_t, 16> Numeric_Prefix{
        0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x18, 0x48,
        0x89, 0x70, 0x20, 0x55, 0x57, 0x41, 0x56, 0x48
    };
    inline constexpr std::size_t Numeric_Type_Offset = 0x80;
    inline constexpr std::size_t Graph_Type_Offset = 0xF0;
    inline constexpr std::size_t Graph_Name_Offset = 0xF8;

    [[nodiscard]] inline bool matches(std::uint32_t timestamp, std::uint32_t image_size,
        std::span<std::uint8_t const> guid, std::uint32_t age,
        std::span<std::uint8_t const> evaluate, std::span<std::uint8_t const> numeric)
    {
        return timestamp == Timestamp && image_size == Image_Size && age == Pdb_Age &&
            std::ranges::equal(guid, Pdb_Guid) && std::ranges::equal(evaluate, Evaluate_Prefix) &&
            std::ranges::equal(numeric, Numeric_Prefix);
    }
}
PLUGIN_NAMESPACE_END
