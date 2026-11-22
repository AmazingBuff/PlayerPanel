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
    using test_plugin::animation::behavior_graph::Domain_Sync_Event_Budget;
    using test_plugin::animation::behavior_graph::Domain_Sync_Variable_Count;
    using test_plugin::animation::behavior_graph::Domain_Sync_Variable_Names;
    using test_plugin::animation::behavior_graph::domain_sync_signature;
    using test_plugin::animation::behavior_graph::domain_sync_clip_family;

    void sync_set_matches_the_preregistered_contract()
    {
        assert(Domain_Sync_Variable_Count == 10);
        assert(Domain_Sync_Variable_Names[0] == "iState");
        assert(Domain_Sync_Variable_Names[7] == "bIsInMT");
        assert(Domain_Sync_Variable_Names[9] == "iLeftHandType");
        assert(Domain_Sync_Event_Budget == 32);
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
}

int main()
{
    sync_set_matches_the_preregistered_contract();
    signature_is_order_sensitive_and_stable();
    clip_family_normalization_matches_recorded_evidence();
    std::cout << "3 g6 domain-sync policy cases passed" << std::endl;
    return 0;
}
