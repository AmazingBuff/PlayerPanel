//
// C-G5-E3 engine-free cases: the tri-state timeline setting, the settle/step pairing rule, the
// bounded producer spin, and the settle ring the ledger drains.
//
// Build:
//   cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests\probe_liveness_e3_test.cpp
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/probe_liveness_policy.h"
#include "animation/behavior_graph/source_timeline_policy.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string_view>

using namespace test_plugin::animation::behavior_graph;

namespace
{
    void timeline_setting_is_tri_state_and_rejects_typos()
    {
        assert(parse_source_timeline("concurrent") == SourceTimelineMode::e_concurrent);
        assert(parse_source_timeline("Concurrent") == SourceTimelineMode::e_concurrent);
        assert(parse_source_timeline("on") == SourceTimelineMode::e_on);
        assert(parse_source_timeline("TRUE") == SourceTimelineMode::e_on);
        assert(parse_source_timeline("1") == SourceTimelineMode::e_on);
        assert(parse_source_timeline("off") == SourceTimelineMode::e_off);
        assert(parse_source_timeline("false") == SourceTimelineMode::e_off);
        assert(parse_source_timeline("") == SourceTimelineMode::e_off);
        // A typo must not arm a window: nullopt, never a silent fallback to a live state.
        assert(!parse_source_timeline("concurren").has_value());
        assert(!parse_source_timeline("on ").has_value());
        assert(!parse_source_timeline("yes").has_value());
    }

    void pairing_takes_the_first_step_at_or_after_each_settle_without_reuse()
    {
        // Each settle pairs with the first step at or after it; a step is consumed once, so the
        // second settle at 20 takes 30 rather than borrowing the step the first one already used.
        std::uint64_t const settles[] = { 10, 20, 20, 95 };
        std::uint64_t const steps[] = { 5, 20, 30, 40, 100 };
        ProbeSettlePairing const pairing = probe_pair_settles(settles, 4, steps, 5);
        assert(pairing.settles == 4);
        assert(pairing.paired == 4);
        assert(pairing.unpaired == 0);

        // A settle after the last step stays missing instead of snapping to a neighbour.
        std::uint64_t const late[] = { 10, 150 };
        std::uint64_t const one_step[] = { 20 };
        ProbeSettlePairing const partial = probe_pair_settles(late, 2, one_step, 1);
        assert(partial.paired == 1);
        assert(partial.unpaired == 1);

        // Empty inputs and a missing step timeline are counted, never guessed.
        ProbeSettlePairing const empty = probe_pair_settles(nullptr, 0, nullptr, 0);
        assert(empty.settles == 0 && empty.paired == 0 && empty.unpaired == 0);
        std::uint64_t const only_settle[] = { 1 };
        ProbeSettlePairing const no_steps = probe_pair_settles(only_settle, 1, nullptr, 0);
        assert(no_steps.paired == 0 && no_steps.unpaired == 1);
    }

    void push_gate_spins_then_counts_one_contention()
    {
        ProbePushGate gate;
        assert(gate.try_enter());
        // Same thread cannot re-enter: the bounded spin exhausts and the failure is counted once,
        // not once per pause attempt.
        assert(!gate.try_enter());
        assert(gate.contended() == 1);
        gate.leave();
        assert(gate.try_enter());
        gate.leave();
        assert(gate.contended() == 1);
        gate.reset();
        assert(gate.contended() == 0);
    }

    void settle_ring_keeps_order_and_counts_overflow()
    {
        SourceSettleRing ring;
        assert(ring.size() == 0);
        for (std::uint64_t i = 0; i < Source_Timeline_Settle_Ring; ++i)
            assert(ring.push(i * 10));
        assert(ring.size() == Source_Timeline_Settle_Ring);
        assert(!ring.push(999));
        assert(ring.dropped() == 1);
        for (std::uint64_t i = 0; i < Source_Timeline_Settle_Ring; ++i)
        {
            std::uint64_t value = 0;
            assert(ring.pop(value));
            assert(value == i * 10);
        }
        std::uint64_t empty = 0;
        assert(!ring.pop(empty));
        assert(ring.push(7));
        assert(ring.size() == 1);
        ring.reset();
        assert(ring.size() == 0);
        assert(ring.dropped() == 0);
    }

    void graph_selector_matches_by_project_name_and_reports_no_pick()
    {
        // C-G5-E3 v2 §八: one actor can own two managed graphs (463/36 and 652/106 on 2026-11-10).
        // The selector must pick by project name, ignore case, and never silently fall back to a
        // different graph when the name does not match.
        std::string_view const names[] = { "DefaultFemale", "DefaultFemaleAlternate" };
        assert(source_timeline_pick(names, 2, "DefaultFemale") == 0);
        assert(source_timeline_pick(names, 2, "defaultfemalealternate") == 1);
        assert(source_timeline_pick(names, 2, "DEFFAULTFEMALE") == Source_Timeline_No_Pick);
        assert(source_timeline_pick(names, 2, "") == Source_Timeline_No_Pick);
        assert(source_timeline_pick(names, 0, "DefaultFemale") == Source_Timeline_No_Pick);
        assert(source_timeline_pick(nullptr, 2, "DefaultFemale") == Source_Timeline_No_Pick);
        // Exact length matters: a prefix is not a match.
        assert(source_timeline_pick(names, 2, "Default") == Source_Timeline_No_Pick);
        assert(source_timeline_name_matches("DefaultFemale", "defaultfemale"));
        assert(!source_timeline_name_matches("DefaultFemale", "DefaultFemale "));
    }

    void first_person_filter_matches_both_spellings_case_insensitively()
    {
        // C-G5-E3 v3: the census showed `FirstPerson` 463/36 and `DefaultFemale` 652/106 on the
        // same actor, while every historical filter matched only the lowercase "1stperson" - so the
        // first-person graph slipped through and the source instruments watched it.
        assert(source_graph_is_first_person("FirstPerson"));
        assert(source_graph_is_first_person("firstperson"));
        assert(source_graph_is_first_person("FIRSTPERSON"));
        assert(source_graph_is_first_person("1stperson"));
        assert(source_graph_is_first_person("DefaultFemale_FirstPerson"));
        assert(!source_graph_is_first_person("DefaultFemale"));
        assert(!source_graph_is_first_person(""));
        assert(!source_graph_is_first_person("First"));
        assert(!source_graph_is_first_person("person"));
    }
}

int main()
{
    timeline_setting_is_tri_state_and_rejects_typos();
    pairing_takes_the_first_step_at_or_after_each_settle_without_reuse();
    push_gate_spins_then_counts_one_contention();
    settle_ring_keeps_order_and_counts_overflow();
    graph_selector_matches_by_project_name_and_reports_no_pick();
    first_person_filter_matches_both_spellings_case_insensitively();
    std::puts("6 probe-liveness e3 cases passed (engine-free)");
}
