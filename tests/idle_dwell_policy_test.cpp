//
// C-IDLE v1 (2026-11-23): engine-free checks for the idle sequence dwell policy.
//
// The pre-registered criteria live in
// docs/diagnostics/idle-loop-contract-20261123/README.md.
//
// SCOPE CHANGE, recorded honestly: the contract originally proposed wiring
// SelectionState::dwell()/seek_dwell() into the accept paths, on the theory that they
// were a finished-but-unwired fix. A direct experiment falsified that: dwell() clears
// the PlayOnce flag instead of pinning the position, so it never leaves the entry
// member. The contract's direction was withdrawn (see README section 七).
//
// What this file does now is pin the behaviour that actually decides the defect:
// commit() dwells correctly ONLY when the entry variant carries _bPlayOnce. That is
// the regression guard for whatever fix eventually lands.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

// base/def.h derives a constexpr hash from Plugin::Plugin_Author, which the real build
// generates into plugin.h together with the CommonLibSSE include. This unit stays
// engine-free, so it supplies only the fact def.h actually needs.
namespace Plugin
{
    inline constexpr std::string_view Plugin_Author = "AmazingBuff";
}
#include "animation/variant_state.h"

#ifdef NDEBUG
#error This test requires assertions.
#endif

namespace
{
    using test_plugin::animation::SelectionState;
    using test_plugin::oar::DataScope;
    using test_plugin::oar::VariantEntry;
    using test_plugin::oar::VariantOptions;

    // Start -> Loop1 -> Loop2, the ordinary "entry move then dwell" authoring.
    constexpr std::uint64_t Clip_Key = 0x1111;
    constexpr std::uint16_t Start_Index = 9013;
    constexpr std::uint16_t Loop1_Index = 9014;
    constexpr std::uint16_t Loop2_Index = 9041;

    VariantOptions sequence(bool entry_play_once)
    {
        VariantOptions options;
        options.key = { DataScope::e_local, Clip_Key };
        options.sequential = true;
        options.sequence = {
            { Start_Index, entry_play_once, "Non-Combat idle Start" },
            { Loop1_Index, false, "Non-Combat idle Loop1" },
            { Loop2_Index, false, "Non-Combat idle Loop2" },
        };
        return options;
    }

    std::vector<std::uint16_t> walk(VariantOptions const& options, int steps)
    {
        SelectionState state;
        std::vector<std::uint16_t> seen;
        for (int i = 0; i < steps; ++i)
        {
            const auto pick = state.variant(options, Clip_Key);
            assert(pick.has_value());
            state.commit(options, *pick, Clip_Key);
            seen.push_back(pick->index);
        }
        return seen;
    }

    // The load-bearing fact: _bPlayOnce on the entry variant is what stops the replay.
    // OAR stores it at Variant::_bPlayOnce=0x29 (PDB-audited, docs/oar-hooks.md).
    void entry_flagged_play_once_dwells_on_the_loop_body()
    {
        const auto seen = walk(sequence(true), 8);
        assert(seen.front() == Start_Index);
        // Start appears exactly once; every later step is a loop body.
        int start_hits = 0;
        for (std::uint16_t index : seen)
            if (index == Start_Index) ++start_hits;
        assert(start_hits == 1);
        for (std::size_t i = 1; i < seen.size(); ++i)
            assert(seen[i] == Loop1_Index || seen[i] == Loop2_Index);
    }

    // The defect: without the flag the sequence restarts and the entry move replays
    // every cycle. This is the "start -> stand -> start again" the operator reported.
    void entry_without_play_once_replays_every_cycle()
    {
        const auto seen = walk(sequence(false), 9);
        // Exact cycle Start, Loop1, Loop2 repeated.
        assert(seen[0] == Start_Index && seen[1] == Loop1_Index && seen[2] == Loop2_Index);
        assert(seen[3] == Start_Index && seen[4] == Loop1_Index && seen[5] == Loop2_Index);
        assert(seen[6] == Start_Index && seen[7] == Loop1_Index && seen[8] == Loop2_Index);
    }

    // Both shapes preserve the authored order, so a fix must not skip the entry move.
    void authored_order_is_preserved_in_both_shapes()
    {
        for (bool flag : { true, false })
        {
            const auto seen = walk(sequence(flag), 3);
            assert(seen[0] == Start_Index);
            assert(seen[1] == Loop1_Index);
            assert(seen[2] == Loop2_Index);
        }
    }

    // An all-once sequence is free to restart from the top; the dwell branch does not apply.
    void all_once_sequence_restarts()
    {
        VariantOptions options = sequence(true);
        options.sequence = {
            { Start_Index, true, "Entry" },
            { Loop1_Index, true, "Second" },
        };
        const auto seen = walk(options, 4);
        assert(seen[0] == Start_Index && seen[1] == Loop1_Index);
        assert(seen[2] == Start_Index && seen[3] == Loop1_Index);
    }

    // dwell() must NOT be wired in: it clears the PlayOnce flag, so it pins the entry
    // member forever instead of dwelling on the loop body. This is the falsified fix,
    // kept as an executable record of why the contract direction was withdrawn.
    void dwell_pins_the_entry_member_forever()
    {
        SelectionState state;
        const VariantOptions options = sequence(true);
        for (int i = 0; i < 6; ++i)
        {
            const auto pick = state.variant(options, Clip_Key);
            assert(pick.has_value());
            state.dwell(options, *pick, Clip_Key);
            assert(pick->index == Start_Index);
        }
    }

    // seek_dwell() writes history directly; it must stay callable only with a real pick.
    void seek_dwell_moves_the_recheck_start()
    {
        SelectionState state;
        const VariantOptions options = sequence(true);
        const auto first = state.variant(options, Clip_Key);
        assert(first.has_value() && first->index == Start_Index);
        state.commit(options, *first, Clip_Key);
        // Land on the last member as if the panel opened mid-sequence.
        state.seek_dwell(options.key, Clip_Key, { Loop2_Index, 2 });
        const auto next = state.variant(options, Clip_Key);
        assert(next.has_value() && next->index == Loop2_Index);
    }

    // A clip with no variants at all yields no pick; the controller then falls back to
    // the base clip. This is the documented nullopt path in variant_state.h:67.
    void clip_without_variants_yields_no_pick()
    {
        SelectionState state;
        VariantOptions options = sequence(true);
        options.sequence.clear();
        assert(!state.variant(options, Clip_Key).has_value());
    }
}

int main()
{
    entry_flagged_play_once_dwells_on_the_loop_body();
    entry_without_play_once_replays_every_cycle();
    authored_order_is_preserved_in_both_shapes();
    all_once_sequence_restarts();
    dwell_pins_the_entry_member_forever();
    seek_dwell_moves_the_recheck_start();
    clip_without_variants_yields_no_pick();
    std::cout << "7 idle sequence dwell cases passed\n";
    return 0;
}
