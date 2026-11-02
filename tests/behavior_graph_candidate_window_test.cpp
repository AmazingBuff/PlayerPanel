//
// C-G4-A4 regression (2026-11-02): the numeric-accessor type histogram must count every armed
// numeric hit, not just the rows that survive the OAR CONTEXT detail budget. The A3 run emitted
// 32 detail rows for 72 numeric hits and read the type only on the emitted rows, so its "all
// type 0" claim covered 7 rows. These cases pin the accounting rules:
//   1. hits past the detail-row budget still count;
//   2. the histogram total equals the numeric site count (reconciliation figure);
//   3. take() zeroes every counter and keeps the window metadata, and hits outside an armed
//      scope are ignored;
//   4. a fresh window starts clean (no residue from the previous one);
//   5. a foreign-actor take neither consumes nor clears the armed window;
//   6. type-3 attribution rows are bounded per window and refill after a new arm.
//
// The production contract counted here: counting needs both an armed tally (which only resets
// counters) and a live ScopedCandidateWindow depth guard - drive_graph_step and the two
// pre-registered dispatches establish it around the native candidate activity.
//
#include <cassert>
#include <cstdint>
#include <iostream>

#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/candidate_window.h"

#ifdef NDEBUG
#error This test requires assertions.
#endif

namespace
{
    using test_plugin::animation::behavior_graph::CandidateWindowTally;
    using test_plugin::animation::behavior_graph::OarHitSite;
    using test_plugin::animation::behavior_graph::ScopedCandidateWindow;
    using test_plugin::animation::behavior_graph::arm_candidate_window;
    using test_plugin::animation::behavior_graph::candidate_window_numeric_sum;
    using test_plugin::animation::behavior_graph::note_candidate_numeric_type;
    using test_plugin::animation::behavior_graph::note_candidate_window_hit;
    using test_plugin::animation::behavior_graph::take_candidate_window_tally;
    using test_plugin::animation::behavior_graph::take_candidate_window_tally_for;
    using test_plugin::animation::behavior_graph::take_numeric_type3_row;

    // The production per-window detail budget (hooks.cpp Oar_Context_Hit_Row_Cap).
    constexpr std::uint32_t Detail_Row_Cap = 32;

    std::uint32_t numeric_site_count(CandidateWindowTally const& tally)
    {
        return tally.sites[static_cast<std::size_t>(OarHitSite::e_numeric)];
    }

    void one_armed_window(std::uint32_t const actor_id)
    {
        arm_candidate_window(actor_id);
    }

    // Case 1 + 2: the A3 blind spot. Forty evaluate hits exhaust the detail budget, then three
    // numeric hits land after it - each one must still reach the histogram and the site count.
    void numeric_type_counts_survive_the_detail_row_budget()
    {
        one_armed_window(0x14);
        ScopedCandidateWindow const guard;
        for (std::uint32_t index = 0; index < Detail_Row_Cap + 8; ++index)
            note_candidate_window_hit(OarHitSite::e_evaluate, 0, 0);
        assert(take_numeric_type3_row());  // the type-3 row budget is independent of this

        // Post-budget numeric hits: 30 graph-variable-free (type 0), one graph-variable
        // accessor (type 3), one unnamed type (5 -> overflow bucket) and one unreadable
        // accessor (negative -> unclassified).
        for (std::uint32_t index = 0; index < 30; ++index)
        {
            note_candidate_window_hit(OarHitSite::e_numeric, 0, 0);
            note_candidate_numeric_type(0);
        }
        note_candidate_window_hit(OarHitSite::e_numeric, 0, 0);
        note_candidate_numeric_type(3);
        note_candidate_window_hit(OarHitSite::e_numeric, 0, 0);
        note_candidate_numeric_type(5);
        note_candidate_window_hit(OarHitSite::e_numeric, 0, 0);
        note_candidate_numeric_type(-1);

        const CandidateWindowTally tally = take_candidate_window_tally();
        assert(numeric_site_count(tally) == 33);
        assert(tally.numeric_types[0] == 30);
        assert(tally.numeric_types[1] == 0);
        assert(tally.numeric_types[2] == 0);
        assert(tally.numeric_types[3] == 1);
        assert(tally.numeric_type_other == 1);
        assert(tally.numeric_unclassified == 1);
        // The reconciliation figure: the packet must be able to compare this with sites n:.
        assert(candidate_window_numeric_sum(tally) == numeric_site_count(tally));
        assert(candidate_window_numeric_sum(tally) == 33);
        assert(tally.evaluations == Detail_Row_Cap + 8 + 33);
    }

    // Case 3: take() zeroes every counter, keeps the metadata, and hits outside an armed scope
    // are ignored (the depth guard is what makes a hit countable - see the header contract).
    void take_zeroes_counters_and_ignores_hits_outside_a_scope()
    {
        const CandidateWindowTally taken = take_candidate_window_tally();
        assert(taken.evaluations == 0);
        assert(taken.overlaps == 0);
        assert(taken.cand_active == 0 && taken.src_active == 0 && taken.neither == 0);
        assert(taken.sites[static_cast<std::size_t>(OarHitSite::e_evaluate)] == 0);
        assert(candidate_window_numeric_sum(taken) == 0);
        assert(taken.numeric_type3_rows == 0);
        // The metadata copy is what the display path compares before emitting its row.
        assert(taken.source_actor_id == 0x14);

        note_candidate_window_hit(OarHitSite::e_numeric, 0, 0);
        note_candidate_numeric_type(3);
        const CandidateWindowTally disarmed = take_candidate_window_tally();
        assert(disarmed.evaluations == 0);
        assert(candidate_window_numeric_sum(disarmed) == 0);
    }

    // Case 4: a fresh window starts clean - the previous window's types cannot leak into it.
    void a_second_window_starts_clean()
    {
        one_armed_window(0x14);
        ScopedCandidateWindow const guard;
        note_candidate_window_hit(OarHitSite::e_numeric, 0, 0);
        note_candidate_numeric_type(0);
        const CandidateWindowTally second = take_candidate_window_tally();
        assert(second.numeric_types[0] == 1);
        assert(second.numeric_types[3] == 0);
        assert(second.numeric_type_other == 0);
        assert(second.numeric_unclassified == 0);
        assert(candidate_window_numeric_sum(second) == 1);
    }

    // Case 5: one DisplaySession per registered actor - a non-owner take must return zeros
    // without touching or consuming the armed window.
    void a_foreign_take_neither_consumes_nor_clears()
    {
        one_armed_window(0x14);
        ScopedCandidateWindow const guard;
        for (std::uint32_t index = 0; index < 2; ++index)
        {
            note_candidate_window_hit(OarHitSite::e_numeric, 0, 0);
            note_candidate_numeric_type(3);
        }
        const CandidateWindowTally foreign = take_candidate_window_tally_for(0x99);
        assert(foreign.source_actor_id == 0x14);
        assert(foreign.evaluations == 0);
        assert(candidate_window_numeric_sum(foreign) == 0);
        assert(foreign.sites[static_cast<std::size_t>(OarHitSite::e_numeric)] == 0);

        const CandidateWindowTally owner = take_candidate_window_tally_for(0x14);
        assert(owner.numeric_types[3] == 2);
        assert(numeric_site_count(owner) == 2);
        assert(candidate_window_numeric_sum(owner) == numeric_site_count(owner));
    }

    // Case 6: type-3 attribution rows are bounded per window and refill after a new arm.
    void type3_attribution_rows_are_bounded_per_window()
    {
        one_armed_window(0x14);
        for (std::uint32_t index = 0; index < test_plugin::animation::behavior_graph::Numeric_Type3_Row_Cap; ++index)
            assert(take_numeric_type3_row());
        assert(!take_numeric_type3_row());
        assert(take_candidate_window_tally().numeric_type3_rows ==
               test_plugin::animation::behavior_graph::Numeric_Type3_Row_Cap);

        one_armed_window(0x14);
        assert(take_numeric_type3_row());
        assert(take_candidate_window_tally().numeric_type3_rows == 1);
    }
}

int main()
{
    numeric_type_counts_survive_the_detail_row_budget();
    take_zeroes_counters_and_ignores_hits_outside_a_scope();
    a_second_window_starts_clean();
    a_foreign_take_neither_consumes_nor_clears();
    type3_attribution_rows_are_bounded_per_window();
    std::cout << "6 candidate-window numeric-type cases passed (engine-free)\n";
}
