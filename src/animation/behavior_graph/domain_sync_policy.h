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

    // C-G6-CON v2 (§2.1): the two domain-switch dispatch events from the
    // 2026-11-22 evidence session (draw = WeapEquip id 2, sheath = Unequip id
    // 144 on the source event table; ids resolve on the candidate's own table).
    // Payload and sender stay null, exactly like the G3-B replay dispatches.
    inline constexpr std::size_t Domain_Sync_Dispatch_Count = 2;
    inline constexpr std::array<std::string_view, Domain_Sync_Dispatch_Count> Domain_Sync_Dispatch_Event_Names{
        "WeapEquip",
        "Unequip",
    };
    inline constexpr std::size_t Domain_Sync_Dispatch_Draw = 0;
    inline constexpr std::size_t Domain_Sync_Dispatch_Sheath = 1;

    // C-G6-CON v2 (§2.2): the direction rule reads the (candidate, source) pair
    // of the domain master switch iState (sync-set position 0). Draw when the
    // candidate sheathed (0) and the source drawn (9); sheath on the mirrored
    // pair; every other combination — including equal domains — primes only and
    // dispatches nothing.
    enum class DomainSyncDirection : std::uint8_t
    {
        e_none,
        e_draw,
        e_sheath,
    };

    [[nodiscard]] inline DomainSyncDirection domain_sync_direction(std::uint32_t a_candidate_i_state,
        std::uint32_t a_source_i_state) noexcept
    {
        if (a_candidate_i_state == 0 && a_source_i_state == 9) return DomainSyncDirection::e_draw;
        if (a_candidate_i_state == 9 && a_source_i_state == 0) return DomainSyncDirection::e_sheath;
        return DomainSyncDirection::e_none;
    }

    [[nodiscard]] inline std::string_view domain_sync_direction_name(DomainSyncDirection a_direction) noexcept
    {
        switch (a_direction)
        {
        case DomainSyncDirection::e_draw: return "draw";
        case DomainSyncDirection::e_sheath: return "sheath";
        default: return "none";
        }
    }

    [[nodiscard]] inline std::size_t domain_sync_dispatch_index(DomainSyncDirection a_direction) noexcept
    {
        return a_direction == DomainSyncDirection::e_draw ? Domain_Sync_Dispatch_Draw :
            Domain_Sync_Dispatch_Sheath;
    }

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
