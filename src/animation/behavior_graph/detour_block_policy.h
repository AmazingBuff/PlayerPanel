//
// Created by AmazingBuff on 2026/11/04.
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    inline constexpr std::size_t Detour_Original_Offset = 128;
    inline constexpr std::size_t Detour_Block_Size = 2 * Detour_Original_Offset;
    inline constexpr std::size_t Detour_Hook_Count = 2;
    inline constexpr std::size_t Detour_Pool_Size = Detour_Hook_Count * Detour_Block_Size;

    inline std::uint8_t* relocate_detour_head(std::uint8_t* a_block,
        std::span<std::uint8_t const> a_head, std::uintptr_t a_resume) noexcept
    {
        std::uint8_t* const original = a_block + Detour_Original_Offset;
        std::memcpy(original, a_head.data(), a_head.size());
        constexpr std::uint8_t s_jump[] = { 0xFF, 0x25, 0, 0, 0, 0 };
        std::memcpy(original + a_head.size(), s_jump, sizeof(s_jump));
        std::memcpy(original + a_head.size() + sizeof(s_jump), &a_resume, sizeof(a_resume));
        return original;
    }
}
PLUGIN_NAMESPACE_END
