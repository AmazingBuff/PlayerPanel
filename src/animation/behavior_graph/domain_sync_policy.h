//
// C-G6-CON v1 (2026-11-22): candidate domain-sync policy (CON-A, H-SYNC).
//
// The retained candidate's branch domain does not follow the source's draw/sheath
// switches (C-G6 v1 run1/run2: the clone stays in its replay-final domain while
// the source swaps 1HM+MLh <-> MT). The run-2 variable diff identified the driver
// words; this header pre-registers the sync set, the event budget and the
// clip-family normalization used by the offline convergence judgment. Nothing
// here touches the engine: the executor lives beside the display drive path and
// writes only the candidate's own variable set through the typed setters.
//
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // The pre-registered sync set (C-G6-CON v1 §2.1): the eight domain drivers
    // observed in the run-2 draw->sheath word diff plus the two weapon-type words
    // (SCAR dummy-variant alignment). All BOOL/INT — the typed-int setter path
    // supports exactly these. TimeDelta (per-graph clock) and REAL-typed words
    // are excluded by contract, not by omission.
    inline constexpr std::size_t Domain_Sync_Variable_Count = 10;
    inline constexpr std::array<std::string_view, Domain_Sync_Variable_Count> Domain_Sync_Variable_Names{
        "iState",
        "currentDefaultState",
        "iRightHandEquipped",
        "iLeftHandEquipped",
        "iMagicEquipped",
        "bMLh_Ready",
        "bMRh_Ready",
        "bIsInMT",
        "iRightHandType",
        "iLeftHandType",
    };

    // Sync events are user-paced domain switches; a runaway count means something
    // is oscillating, so the executor disables itself (log once, restart to reset).
    inline constexpr std::size_t Domain_Sync_Event_Budget = 32;

    // FNV-1a 64 over the ten source words in declared order: the change detector
    // compares signatures, not individual words, so one event covers one switch.
    [[nodiscard]] inline std::uint64_t domain_sync_signature(
        std::array<std::uint32_t, Domain_Sync_Variable_Count> const& a_words) noexcept
    {
        std::uint64_t hash = 0xcbf29ce484222325ull;
        for (std::uint32_t const word : a_words)
        {
            hash ^= word;
            hash *= 0x100000001b3ull;
        }
        return hash;
    }

    // Offline clip-family normalization (C-G6-CON v1 §3): clip names strip a
    // trailing ".HKX" (source nodes carry the suffix, candidate clones do not)
    // and collapse the SCAR_1HM_Ready_Dummy<N> ordinal, which differed between
    // the sides in every recorded snapshot. Order-sensitive calls must compare
    // through this helper, never the raw names.
    [[nodiscard]] inline std::string_view domain_sync_clip_family(std::string_view a_name) noexcept
    {
        while (!a_name.empty() && (a_name.front() == ' ' || a_name.front() == '\t'))
            a_name.remove_prefix(1);
        while (!a_name.empty() && (a_name.back() == ' ' || a_name.back() == '\t'))
            a_name.remove_suffix(1);
        if (a_name.size() >= 4 && a_name.substr(a_name.size() - 4) == ".HKX")
            a_name.remove_suffix(4);
        std::size_t const dummy = a_name.find("_Dummy");
        if (dummy != std::string_view::npos)
        {
            std::string_view const tail = a_name.substr(dummy + 6);
            bool digits = !tail.empty();
            for (char const ch : tail)
                digits = digits && ch >= '0' && ch <= '9';
            if (digits)
                a_name = a_name.substr(0, dummy);
        }
        return a_name;
    }
}
PLUGIN_NAMESPACE_END
