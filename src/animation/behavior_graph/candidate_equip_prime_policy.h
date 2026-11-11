//
// Created by AmazingBuff on 2026/11/11.
//
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // C-G5-CAL D2: the three equipment words the source wrote during the real 1H equip
    // (R run 2: iMagicEquipped 0->1, iRightHandEquipped 0->1, iLeftHandEquipped 0->9).
    // The order is the source's observed order; values are the observed post-equip state.
    struct CandidateEquipPrimeWrite
    {
        std::string_view name;
        std::int32_t value;
    };

    inline constexpr std::array<CandidateEquipPrimeWrite, 3> Candidate_Equip_Prime_Writes{ {
        { "iMagicEquipped", 1 },
        { "iRightHandEquipped", 1 },
        { "iLeftHandEquipped", 9 },
    } };

    // C-G5-CAL D3: captured from the source graph at window-open on 2026-11-11.
    // iRightHandType selects the root/Root/mg branch (1 = one-handed); iLeftHandType
    // selects the next branch (9 = the source's left-hand state). These are not universal
    // defaults; they belong to the frozen 1H + left-hand loadout of this run.
    inline constexpr std::array<CandidateEquipPrimeWrite, 2> Candidate_Equip_Prime_Branch_Writes{ {
        { "iRightHandType", 1 },
        { "iLeftHandType", 9 },
    } };
}
PLUGIN_NAMESPACE_END
