//
// Created by AmazingBuff on 2026/10/27.
//
#pragma once

#include <cstdint>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // Read-only observation hardening (audit24, 2026-10-27).
    //
    // The v16-era g3-gates row read `[ctx+0x50]` while `sizeof(RE::hkbContext)` is
    // 0x50, so its first hop was already outside the context object and the stale
    // stack slot there decided at random whether a wild pointer was followed. v16,
    // v18 and v19 happened to read zero and short-circuited; v20's stack layout read
    // a non-null value and the process faulted at `CharacterPanel.dll+0x28067`
    // (`independent_graph.cpp:955`, `mov r8, [rbx+0x88]`).
    //
    // Observation code therefore checks every intermediate before dereferencing it.
    // This filter only rejects values that cannot be a user-mode object address in
    // this process (null, tiny, unaligned, non-canonical); it is not a lifetime,
    // ownership or type proof, and it must not be used to justify a native call.
    [[nodiscard]] inline bool plausible_object_pointer(std::uintptr_t const value)
    {
        // Windows x64 user-mode objects live below the canonical limit; 8-byte
        // alignment covers every hkReferencedObject-derived type read here.
        constexpr std::uintptr_t kMinimum = 0x10000;
        constexpr std::uintptr_t kCanonicalUserLimit = 0x0000800000000000ull;
        return value >= kMinimum && value < kCanonicalUserLimit && (value & 0x7) == 0;
    }
}
PLUGIN_NAMESPACE_END
