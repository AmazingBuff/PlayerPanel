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

    // C-G6-CON v2/v3: the domain-switch dispatch events from the 2026-11-22
    // evidence sessions. v2: WeapEquip (draw, id 2) and Unequip (sheath, id 144)
    // from the draw/sheath session; v3 adds MagicForceEquipRight (magic ready,
    // id 476) and WeapOutRightReplaceForceEquip (magic -> weapon, id 417) from
    // the magic-domain session. Ids resolve on the candidate's own event table.
    // Payload and sender stay null, exactly like the G3-B replay dispatches.
    inline constexpr std::size_t Domain_Sync_Dispatch_Count = 4;
    inline constexpr std::array<std::string_view, Domain_Sync_Dispatch_Count> Domain_Sync_Dispatch_Event_Names{
        "WeapEquip",
        "Unequip",
        "MagicForceEquipRight",
        "WeapOutRightReplaceForceEquip",
    };
    inline constexpr std::size_t Domain_Sync_Dispatch_Draw = 0;
    inline constexpr std::size_t Domain_Sync_Dispatch_Sheath = 1;
    inline constexpr std::size_t Domain_Sync_Dispatch_Magic = 2;
    inline constexpr std::size_t Domain_Sync_Dispatch_WeaponSwap = 3;

    // C-G6-CON v3 (§2.1): settled domain codes of currentDefaultState — the
    // domain master switch that separates all four domains (iState cannot:
    // drawn 1HM and magic-ready share iState=9). Equip transition 11 is a
    // ~0.6 s transient and never drives a dispatch.
    inline constexpr std::uint32_t Domain_Sync_Sheathed = 0;
    inline constexpr std::uint32_t Domain_Sync_Drawn = 1;
    inline constexpr std::uint32_t Domain_Sync_Magic = 2;

    // C-G6-CON v3 (§2.2): the direction rule reads the (candidate, source) pair
    // of currentDefaultState (sync-set position 1). Evidenced pairs dispatch
    // their pre-registered event; equal domains, a transient source state, and
    // unevidenced pairs prime only and dispatch nothing.
    enum class DomainSyncDirection : std::uint8_t
    {
        e_none,
        e_draw,        // sheathed -> drawn 1HM
        e_sheath,      // drawn or magic -> sheathed
        e_magic_ready, // drawn -> magic-ready
        e_weapon_swap, // magic-ready -> drawn 1HM
    };

    [[nodiscard]] inline DomainSyncDirection domain_sync_direction(std::uint32_t a_candidate_state,
        std::uint32_t a_source_state) noexcept
    {
        if (a_candidate_state == a_source_state) return DomainSyncDirection::e_none;
        if (a_source_state != Domain_Sync_Sheathed && a_source_state != Domain_Sync_Drawn &&
            a_source_state != Domain_Sync_Magic)
            return DomainSyncDirection::e_none;
        if (a_source_state == Domain_Sync_Sheathed) return DomainSyncDirection::e_sheath;
        if (a_candidate_state == Domain_Sync_Sheathed && a_source_state == Domain_Sync_Drawn)
            return DomainSyncDirection::e_draw;
        if (a_candidate_state == Domain_Sync_Drawn && a_source_state == Domain_Sync_Magic)
            return DomainSyncDirection::e_magic_ready;
        if (a_candidate_state == Domain_Sync_Magic && a_source_state == Domain_Sync_Drawn)
            return DomainSyncDirection::e_weapon_swap;
        return DomainSyncDirection::e_none;
    }

    [[nodiscard]] inline std::size_t domain_sync_dispatch_index(DomainSyncDirection a_direction) noexcept
    {
        switch (a_direction)
        {
        case DomainSyncDirection::e_draw: return Domain_Sync_Dispatch_Draw;
        case DomainSyncDirection::e_sheath: return Domain_Sync_Dispatch_Sheath;
        case DomainSyncDirection::e_magic_ready: return Domain_Sync_Dispatch_Magic;
        case DomainSyncDirection::e_weapon_swap: return Domain_Sync_Dispatch_WeaponSwap;
        default: return Domain_Sync_Dispatch_Count;
        }
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
