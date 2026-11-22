//
// C-G6 v1 (2026-11-22): engine-free checks for the P2 branch-set classifier.
// The expected families come from the C-G5-CHAIN run-1/run-2 census evidence
// (1HM branch carries 1stP-prefixed modifiers; MT branch names appear mid-path;
// WeaponAdjustment only exists on the source side).
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/idle_chain_probe_policy.h"

#include <cassert>
#include <iostream>
#include <string_view>

#ifdef NDEBUG
#error This test requires assertions.
#endif

namespace
{
    using test_plugin::animation::behavior_graph::IdleChainBranch;
    using test_plugin::animation::behavior_graph::idle_chain_branch_of;
    using test_plugin::animation::behavior_graph::idle_chain_branch_name;

    void branch_classification_matches_the_registered_census_families()
    {
        assert(idle_chain_branch_of("MT_BehaviorGraph") == IdleChainBranch::e_mt);
        assert(idle_chain_branch_of("IdleBFR/MT_BehaviorBFR") == IdleChainBranch::e_mt);
        assert(idle_chain_branch_of("1HM_Behavior") == IdleChainBranch::e_1hm);
        assert(idle_chain_branch_of("1HM_1stPUpperBody") == IdleChainBranch::e_1hm);
        assert(idle_chain_branch_of("1stP ReadiedModifier") == IdleChainBranch::e_1hm);
        assert(idle_chain_branch_of("SCAR_1HM_Ready_Dummy0") == IdleChainBranch::e_1hm);
        assert(idle_chain_branch_of("MLh_BehaviorGraph") == IdleChainBranch::e_mlh);
        assert(idle_chain_branch_of("WeaponAdjustment_OffsetAnimationGen") == IdleChainBranch::e_weapon_adjustment);
    }

    void non_branch_names_and_outer_whitespace_stay_other()
    {
        assert(idle_chain_branch_of("H2H_Idle") == IdleChainBranch::e_other);
        assert(idle_chain_branch_of("ArmsBlend") == IdleChainBranch::e_other);
        assert(idle_chain_branch_of("NPC R Hand [RHnd]") == IdleChainBranch::e_other);
        assert(idle_chain_branch_of("  MT_BehaviorGraph\t") == IdleChainBranch::e_mt);
        assert(idle_chain_branch_of("") == IdleChainBranch::e_other);
    }

    void precedence_is_weapon_adjustment_then_mlh_then_1hm_then_mt()
    {
        assert(idle_chain_branch_of("MLh_1HM_Mixed") == IdleChainBranch::e_mlh);
        assert(idle_chain_branch_of("1HM_MT_Mixed") == IdleChainBranch::e_1hm);
        assert(idle_chain_branch_of("WeaponAdjustment_MT_Mixed") == IdleChainBranch::e_weapon_adjustment);
    }

    void branch_names_are_stable_row_labels()
    {
        assert(idle_chain_branch_name(IdleChainBranch::e_1hm) == "1hm");
        assert(idle_chain_branch_name(IdleChainBranch::e_mt) == "mt");
        assert(idle_chain_branch_name(IdleChainBranch::e_mlh) == "mlh");
        assert(idle_chain_branch_name(IdleChainBranch::e_weapon_adjustment) == "weapon-adjustment");
        assert(idle_chain_branch_name(IdleChainBranch::e_other) == "other");
    }
}

int main()
{
    branch_classification_matches_the_registered_census_families();
    non_branch_names_and_outer_whitespace_stay_other();
    precedence_is_weapon_adjustment_then_mlh_then_1hm_then_mt();
    branch_names_are_stable_row_labels();
    std::cout << "4 g6 branch-classification cases passed" << std::endl;
    return 0;
}
