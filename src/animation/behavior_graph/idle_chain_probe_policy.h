//
// C-G5-CHAIN v1 (2026-11-21): idle chain-layer attribution policy.
//
// The offline noise-floor pass (docs/diagnostics/g5-bone-name-mapping-20261121/) showed the
// settled idle pose differs between the candidate replay and the live source on the right
// shoulder chain (RClv/RUar/RUt1/RUt2/R Pauldron) by a constant, axis-stable rotation, while
// both graphs sample the same clip data (data/map FNV identical) over the same reference pose.
// This probe compares each side's poseLocal against a direct SamplePartialTracks of that
// side's active clip at that side's own localTime, so the side that equals the authored clip
// sample is the unmodified one. Interest tracks are resolved by bone name at runtime (no
// hardcoded bone indices); the two control tracks are expected to show the candidate clock
// staircase shape and calibrate the comparison's sensitivity.
//
#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // Resolved by exact trimmed bone-name match against the animation skeleton. The right
    // shoulder chain carries the offline-measured difference; forearm/hand/weapon verify the
    // sub-chain; the attachment slot checks the translation-only difference; the last two are
    // controls expected to differ only through the candidate clock staircase.
    inline constexpr std::array<std::string_view, 14> Idle_Chain_Interest_Names{
        "NPC R Clavicle [RClv]",
        "NPC R UpperArm [RUar]",
        "NPC R UpperarmTwist1 [RUt1]",
        "NPC R UpperarmTwist2 [RUt2]",
        "NPC R Pauldron",
        "NPC R Forearm [RLar]",
        "NPC R ForearmTwist1 [RLt1]",
        "NPC R ForearmTwist2 [RLt2]",
        "NPC R Hand [RHnd]",
        "Weapon",
        "NPC L Hand [LHnd]",
        "Quiver",
        "NPC L Thigh [LThg]",
        "NPC Spine1 [Spn1]",
    };

    // Indexes into Idle_Chain_Interest_Names for the pre-registered verdict tracks.
    inline constexpr std::size_t Idle_Chain_Track_RClavicle = 0;
    inline constexpr std::size_t Idle_Chain_Track_RUpperArm = 1;
    inline constexpr std::size_t Idle_Chain_Track_RUt1 = 2;
    inline constexpr std::size_t Idle_Chain_Track_RUt2 = 3;
    inline constexpr std::size_t Idle_Chain_Track_RPauldron = 4;
    inline constexpr std::size_t Idle_Chain_Track_RForearm = 5;
    inline constexpr std::size_t Idle_Chain_Track_RHand = 8;
    inline constexpr std::size_t Idle_Chain_Track_Weapon = 9;
    inline constexpr std::size_t Idle_Chain_Track_LThigh = 12;
    inline constexpr std::size_t Idle_Chain_Track_Spine1 = 13;

    // Trimmed bone-name comparison: the census shows trailing spaces inside the bracketed
    // tags are part of the names, but leading/trailing whitespace outside them is not.
    [[nodiscard]] inline std::string_view idle_chain_trim(std::string_view a_name) noexcept
    {
        while (!a_name.empty() && (a_name.front() == ' ' || a_name.front() == '\t'))
            a_name.remove_prefix(1);
        while (!a_name.empty() && (a_name.back() == ' ' || a_name.back() == '\t'))
            a_name.remove_suffix(1);
        return a_name;
    }

    // Index of the interest name that equals a_name after trimming, or npos.
    [[nodiscard]] inline std::size_t idle_chain_interest_index(std::string_view a_name) noexcept
    {
        std::string_view const trimmed = idle_chain_trim(a_name);
        for (std::size_t i = 0; i < Idle_Chain_Interest_Names.size(); ++i)
        {
            if (Idle_Chain_Interest_Names[i] == trimmed)
                return i;
        }
        return static_cast<std::size_t>(-1);
    }

    // One compared interest track: bone index in poseLocal (-1 unresolved), rotation error in
    // degrees between the authored clip sample and the side's own poseLocal, translation delta,
    // and whether either quaternion was non-finite.
    struct IdleChainTrackReading
    {
        std::int32_t bone{ -1 };
        float        drot_degrees{};
        float        dpos{};
        bool         non_finite{};
        bool         resolved{};
    };

    // Bounded comparison verdict for one side and one snapshot. The runtime emits the raw
    // rows; this class only counts, so the offline pass can apply the pre-registered
    // V-P1/V-P2/V-P3 thresholds to the same numbers.
    class IdleChainSideSummary
    {
    public:
        void observe(std::size_t a_interest, IdleChainTrackReading const& a_reading)
        {
            if (a_interest >= m_readings.size())
                return;
            m_readings[a_interest] = a_reading;
        }

        [[nodiscard]] IdleChainTrackReading const& reading(std::size_t a_interest) const noexcept
        {
            return m_readings[a_interest];
        }

        // drot >= threshold (degrees) count over resolved, finite shoulder-chain tracks
        // (RClavicle..RPauldron, indexes 0-4).
        [[nodiscard]] std::size_t shoulder_exceeds(float a_threshold_degrees) const noexcept
        {
            std::size_t count = 0;
            for (std::size_t i = 0; i <= 4; ++i)
            {
                IdleChainTrackReading const& entry = m_readings[i];
                if (entry.resolved && !entry.non_finite && entry.drot_degrees >= a_threshold_degrees)
                    ++count;
            }
            return count;
        }

        // Every resolved, finite non-shoulder track stays under the threshold.
        [[nodiscard]] bool others_below(float a_threshold_degrees) const noexcept
        {
            for (std::size_t i = 5; i < m_readings.size(); ++i)
            {
                IdleChainTrackReading const& entry = m_readings[i];
                if (!entry.resolved || entry.non_finite)
                    continue;
                if (entry.drot_degrees >= a_threshold_degrees)
                    return false;
            }
            return true;
        }

    private:
        std::array<IdleChainTrackReading, Idle_Chain_Interest_Names.size()> m_readings{};
    };

    // C-G6 v1 (P2): branch-set bucket for the census walk. Substring heuristic over
    // the node's display name — the per-node census rows stay the authority, this
    // only aggregates them into the branch timeline. `1stP` counts as 1HM because
    // the C-G5-CHAIN run-1 census showed the 1HM branch carries 1stP-prefixed
    // modifier nodes that share no other token with it.
    enum class IdleChainBranch : std::uint8_t
    {
        e_other,
        e_weapon_adjustment,
        e_1hm,
        e_mt,
        e_mlh,
    };

    [[nodiscard]] inline IdleChainBranch idle_chain_branch_of(std::string_view a_name) noexcept
    {
        std::string_view const trimmed = idle_chain_trim(a_name);
        const auto contains = [&trimmed](std::string_view a_token) noexcept
        {
            return trimmed.find(a_token) != std::string_view::npos;
        };
        if (contains("WeaponAdjustment")) return IdleChainBranch::e_weapon_adjustment;
        if (contains("MLh")) return IdleChainBranch::e_mlh;
        if (contains("1HM") || contains("1stP")) return IdleChainBranch::e_1hm;
        if (contains("MT_") || trimmed.starts_with("MT")) return IdleChainBranch::e_mt;
        return IdleChainBranch::e_other;
    }

    [[nodiscard]] inline std::string_view idle_chain_branch_name(IdleChainBranch a_branch) noexcept
    {
        switch (a_branch)
        {
        case IdleChainBranch::e_weapon_adjustment: return "weapon-adjustment";
        case IdleChainBranch::e_1hm: return "1hm";
        case IdleChainBranch::e_mt: return "mt";
        case IdleChainBranch::e_mlh: return "mlh";
        default: return "other";
        }
    }
}
PLUGIN_NAMESPACE_END
