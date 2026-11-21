//
// C-G5-CHAIN v1 engine-free policy cases (2026-11-21): interest-name trimming and
// resolution, and the bounded side summary counts behind the pre-registered
// V-P1/V-P2/V-P3 criteria (docs/diagnostics/g5-idle-chain-probe-20261121/).
//
// Build:
//   cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests\idle_chain_probe_test.cpp
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/idle_chain_probe_policy.h"

#include <cassert>
#include <cstddef>
#include <limits>
#include <string_view>

using namespace test_plugin::animation::behavior_graph;

namespace
{
    void interest_names_match_the_census_strings()
    {
        assert(Idle_Chain_Interest_Names.size() == 14);
        // The K1 census bone names, verbatim (tags carry internal padding).
        assert(Idle_Chain_Interest_Names[Idle_Chain_Track_RUpperArm] == "NPC R UpperArm [RUar]");
        assert(Idle_Chain_Interest_Names[Idle_Chain_Track_RUt1] == "NPC R UpperarmTwist1 [RUt1]");
        assert(Idle_Chain_Interest_Names[Idle_Chain_Track_RPauldron] == "NPC R Pauldron");
        assert(Idle_Chain_Interest_Names[Idle_Chain_Track_Weapon] == "Weapon");
        assert(Idle_Chain_Interest_Names[Idle_Chain_Track_Spine1] == "NPC Spine1 [Spn1]");
    }

    void trimming_keeps_internal_and_drops_outer_whitespace()
    {
        assert(idle_chain_trim("NPC R UpperArm [RUar]") == "NPC R UpperArm [RUar]");
        assert(idle_chain_trim("  Weapon ") == "Weapon");
        assert(idle_chain_trim("\tNPC R Pauldron\t") == "NPC R Pauldron");
        // Internal padding inside a bracket tag is part of the name and must survive.
        assert(idle_chain_trim("NPC L Thigh [LThg] ") == "NPC L Thigh [LThg]");
    }

    void interest_index_resolves_census_names()
    {
        assert(idle_chain_interest_index("NPC R UpperArm [RUar]") == Idle_Chain_Track_RUpperArm);
        assert(idle_chain_interest_index("NPC R Clavicle [RClv]") == Idle_Chain_Track_RClavicle);
        assert(idle_chain_interest_index("NPC R Hand [RHnd]") == Idle_Chain_Track_RHand);
        assert(idle_chain_interest_index("Quiver") == std::size_t{ 11 });
        assert(idle_chain_interest_index("  NPC L Thigh [LThg]") == Idle_Chain_Track_LThigh);
        assert(idle_chain_interest_index("NPC Root [Root]") == static_cast<std::size_t>(-1));
        // Case-sensitive: census names are upper-camel.
        assert(idle_chain_interest_index("weapon") == static_cast<std::size_t>(-1));
    }

    void shoulder_count_uses_resolved_finite_entries_only()
    {
        IdleChainSideSummary summary;
        // Four shoulder tracks over the 5-degree line, one under, one unresolved, one nan.
        auto entry = [](float a_degrees, bool a_resolved, bool a_nan) {
            IdleChainTrackReading reading{};
            reading.bone = 0;
            reading.resolved = a_resolved;
            reading.non_finite = a_nan;
            reading.drot_degrees = a_nan ? std::numeric_limits<float>::infinity() : a_degrees;
            return reading;
        };
        summary.observe(Idle_Chain_Track_RClavicle, entry(10.2f, true, false));
        summary.observe(Idle_Chain_Track_RUpperArm, entry(30.3f, true, false));
        summary.observe(Idle_Chain_Track_RUt1, entry(17.3f, true, false));
        summary.observe(Idle_Chain_Track_RUt2, entry(8.5f, true, false));
        summary.observe(Idle_Chain_Track_RPauldron, entry(4.9f, true, false));
        // Non-shoulder tracks: one large (must not enter the shoulder count), one clean.
        summary.observe(Idle_Chain_Track_RHand, entry(60.0f, true, false));
        summary.observe(Idle_Chain_Track_Weapon, entry(0.0f, true, false));
        // Unresolved and non-finite entries must be excluded from both counts.
        summary.observe(Idle_Chain_Track_LThigh, entry(99.0f, false, false));
        summary.observe(Idle_Chain_Track_Spine1, entry(std::numeric_limits<float>::infinity(), true, true));

        assert(summary.shoulder_exceeds(5.0f) == 4);
        assert(summary.shoulder_exceeds(11.0f) == 2);
        assert(summary.shoulder_exceeds(31.0f) == 0);
        // RHand at 60 degrees fails the "others below" gate even though the
        // shoulder set is over the line.
        assert(summary.others_below(5.0f) == false);
    }

    void others_below_ignores_unresolved_and_non_finite()
    {
        IdleChainSideSummary summary;
        auto entry = [](float a_degrees, bool a_resolved, bool a_nan) {
            IdleChainTrackReading reading{};
            reading.bone = 0;
            reading.resolved = a_resolved;
            reading.non_finite = a_nan;
            reading.drot_degrees = a_nan ? std::numeric_limits<float>::infinity() : a_degrees;
            return reading;
        };
        summary.observe(Idle_Chain_Track_RClavicle, entry(0.0f, true, false));
        summary.observe(Idle_Chain_Track_RUpperArm, entry(0.0f, true, false));
        summary.observe(Idle_Chain_Track_RUt1, entry(0.0f, true, false));
        summary.observe(Idle_Chain_Track_RUt2, entry(0.0f, true, false));
        summary.observe(Idle_Chain_Track_RPauldron, entry(0.0f, true, false));
        summary.observe(Idle_Chain_Track_RHand, entry(0.0f, false, false));
        summary.observe(Idle_Chain_Track_Weapon, entry(std::numeric_limits<float>::infinity(), true, true));
        // Nothing resolved and finite is over the line in the non-shoulder set.
        assert(summary.others_below(5.0f) == true);
    }
}

int main()
{
    interest_names_match_the_census_strings();
    trimming_keeps_internal_and_drops_outer_whitespace();
    interest_index_resolves_census_names();
    shoulder_count_uses_resolved_finite_entries_only();
    others_below_ignores_unresolved_and_non_finite();
    return 0;
}
