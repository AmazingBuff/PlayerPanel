//
// C-G6-CON v1 (2026-11-22): engine-free checks for the domain-sync policy —
// the signature hash and the clip-family normalization the offline convergence
// judgment compares through.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/domain_sync_policy.h"

#include <cassert>
#include <iostream>
#include <string_view>

#ifdef NDEBUG
#error This test requires assertions.
#endif

namespace
{
    using test_plugin::animation::behavior_graph::Domain_Sync_Dispatch_Event_Names;
    using test_plugin::animation::behavior_graph::Domain_Sync_Event_Budget;
    using test_plugin::animation::behavior_graph::Domain_Sync_Variable_Count;
    using test_plugin::animation::behavior_graph::Domain_Sync_Variable_Names;
    using test_plugin::animation::behavior_graph::DomainSyncDirection;
    using test_plugin::animation::behavior_graph::domain_sync_signature;
    using test_plugin::animation::behavior_graph::domain_sync_clip_family;
    using test_plugin::animation::behavior_graph::Domain_Sync_Drawn;
    using test_plugin::animation::behavior_graph::Domain_Sync_Magic;
    using test_plugin::animation::behavior_graph::Domain_Sync_Sheathed;
    using test_plugin::animation::behavior_graph::domain_sync_direction;
    using test_plugin::animation::behavior_graph::domain_sync_dispatch_index;

    void sync_set_matches_the_preregistered_contract()
    {
        assert(Domain_Sync_Variable_Count == 10);
        assert(Domain_Sync_Variable_Names[0] == "iState");
        assert(Domain_Sync_Variable_Names[7] == "bIsInMT");
        assert(Domain_Sync_Variable_Names[9] == "iLeftHandType");
        assert(Domain_Sync_Event_Budget == 32);
        // v3 domain codes: currentDefaultState separates all four domains.
        assert(Domain_Sync_Sheathed == 0);
        assert(Domain_Sync_Drawn == 1);
        assert(Domain_Sync_Magic == 2);
    }

    void signature_is_order_sensitive_and_stable()
    {
        std::array<std::uint32_t, Domain_Sync_Variable_Count> draw{};
        draw[0] = 9; // iState
        draw[3] = 9; // iLeftHandEquipped
        std::array<std::uint32_t, Domain_Sync_Variable_Count> sheath = draw;
        sheath[0] = 0;
        sheath[7] = 1; // bIsInMT
        assert(domain_sync_signature(draw) == domain_sync_signature(draw));
        assert(domain_sync_signature(draw) != domain_sync_signature(sheath));
        std::array<std::uint32_t, Domain_Sync_Variable_Count> swapped = draw;
        swapped[0] = 0;
        swapped[7] = 1;
        swapped[7] = draw[7];
        swapped[0] = draw[0];
        assert(domain_sync_signature(swapped) == domain_sync_signature(draw));
    }

    void clip_family_normalization_matches_recorded_evidence()
    {
        // run1/run2: source MT_Idle.HKX vs candidate MT_Idle — same family.
        assert(domain_sync_clip_family("MT_Idle.HKX") == domain_sync_clip_family("MT_Idle"));
        // SCAR dummy ordinals differed on every recorded snapshot.
        assert(domain_sync_clip_family("SCAR_1HM_Ready_Dummy0") ==
            domain_sync_clip_family("SCAR_1HM_Ready_Dummy1"));
        // Non-dummy names and mid-name matches stay untouched.
        assert(domain_sync_clip_family("1HM_Idle") == "1HM_Idle");
        assert(domain_sync_clip_family("Dummy_Backup") == "Dummy_Backup");
        assert(domain_sync_clip_family("SCAR_1HM_Ready_DummyX") == "SCAR_1HM_Ready_DummyX");
        // Outer whitespace only.
        assert(domain_sync_clip_family(" MT_Idle.HKX\t") == "MT_Idle");
    }

    void direction_rule_matches_the_v3_contract()
    {
        // Evidenced pairs (contract §2.2).
        assert(domain_sync_direction(0, Domain_Sync_Drawn) == DomainSyncDirection::e_draw);
        assert(domain_sync_direction(Domain_Sync_Drawn, Domain_Sync_Magic) == DomainSyncDirection::e_magic_ready);
        assert(domain_sync_direction(Domain_Sync_Magic, Domain_Sync_Drawn) == DomainSyncDirection::e_weapon_swap);
        // The sheath row covers both 1->0 and the new 2->0 hypothesis.
        assert(domain_sync_direction(Domain_Sync_Drawn, Domain_Sync_Sheathed) == DomainSyncDirection::e_sheath);
        assert(domain_sync_direction(Domain_Sync_Magic, Domain_Sync_Sheathed) == DomainSyncDirection::e_sheath);
        // Equal domains, transient source (11), and unevidenced pairs prime only.
        assert(domain_sync_direction(0, 0) == DomainSyncDirection::e_none);
        assert(domain_sync_direction(Domain_Sync_Drawn, Domain_Sync_Drawn) == DomainSyncDirection::e_none);
        assert(domain_sync_direction(Domain_Sync_Magic, Domain_Sync_Magic) == DomainSyncDirection::e_none);
        assert(domain_sync_direction(Domain_Sync_Drawn, 11) == DomainSyncDirection::e_none);
        assert(domain_sync_direction(Domain_Sync_Magic, 11) == DomainSyncDirection::e_none);
        assert(domain_sync_direction(Domain_Sync_Sheathed, Domain_Sync_Magic) == DomainSyncDirection::e_none);
        assert(domain_sync_direction(Domain_Sync_Sheathed, 11) == DomainSyncDirection::e_none);
        // Dispatch table: four events, direction mapping.
        assert(domain_sync_dispatch_index(DomainSyncDirection::e_draw) == 0);
        assert(domain_sync_dispatch_index(DomainSyncDirection::e_sheath) == 1);
        assert(domain_sync_dispatch_index(DomainSyncDirection::e_magic_ready) == 2);
        assert(domain_sync_dispatch_index(DomainSyncDirection::e_weapon_swap) == 3);
        assert(domain_sync_dispatch_index(DomainSyncDirection::e_none) == 4);
        assert(Domain_Sync_Dispatch_Event_Names[0] == "WeapEquip");
        assert(Domain_Sync_Dispatch_Event_Names[1] == "Unequip");
        assert(Domain_Sync_Dispatch_Event_Names[2] == "MagicForceEquipRight");
        assert(Domain_Sync_Dispatch_Event_Names[3] == "WeapOutRightReplaceForceEquip");
    }
}

int main()
{
    sync_set_matches_the_preregistered_contract();
    signature_is_order_sensitive_and_stable();
    clip_family_normalization_matches_recorded_evidence();
    direction_rule_matches_the_v3_contract();
    std::cout << "4 g6 domain-sync policy cases passed" << std::endl;
    return 0;
}
