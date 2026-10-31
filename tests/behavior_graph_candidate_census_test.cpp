//
// Created by AmazingBuff on 2026/10/30.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/candidate_variable_census.h"

#include <cassert>
#include <string_view>
#include <vector>

#ifdef NDEBUG
#error This test requires assertions.
#endif

using test_plugin::animation::behavior_graph::CensusVariable;
using test_plugin::animation::behavior_graph::census_align;
using test_plugin::animation::behavior_graph::census_bound;
using test_plugin::animation::behavior_graph::census_counts;
using test_plugin::animation::behavior_graph::census_output;
using test_plugin::animation::behavior_graph::census_type_name;

namespace
{
    CensusVariable make(std::string_view a_name, std::int8_t a_type, std::int16_t a_flags = 0)
    {
        CensusVariable variable;
        variable.name = a_name;
        variable.type = a_type;
        variable.flags = a_flags;
        return variable;
    }
}

void counts_group_types_and_separate_named_entries()
{
    std::vector<CensusVariable> const variables{
        make("iState", 3),
        make("iRightHandEquipped", 3),
        make("bEquipUnequip", 0),
        make("fBlend", 4),
        make("", 5),
        make("", 7),
        make("", -1),
        make("", 9),
    };
    auto const counts = census_counts(variables);
    assert(counts.total == 8);
    assert(counts.named == 4);
    assert(counts.boolean == 1);
    assert(counts.integer == 2);
    assert(counts.real == 1);
    assert(counts.pointer == 1);
    assert(counts.quad == 1);
    assert(counts.unknown == 2);
    assert(census_type_name(3) == "INT32");
    assert(census_type_name(-1) == "INVALID");
    assert(census_output(0x10));
    assert(!census_output(0x0F));
    // The item table never exceeds the type table or the word array.
    assert(census_bound(652, 652) == 652);
    assert(census_bound(652, 463) == 463);
    assert(census_bound(0, 652) == 0);
}

void alignment_pairs_by_name_and_reports_both_unmatched_sides()
{
    std::vector<CensusVariable> const source{
        make("iState", 3),
        make("bEquipUnequip", 0),
        make("", 3),
    };
    std::vector<CensusVariable> const candidate{
        make("iState", 3),
        make("fExtra", 4),
        make("bEquipUnequip", 2),
    };
    auto const alignment = census_align(source, candidate);
    assert(alignment.matched.size() == 2);
    assert(alignment.matched[0].source_index == 0);
    assert(alignment.matched[0].candidate_index == 0);
    assert(alignment.matched[0].source_type == 3);
    assert(alignment.matched[0].candidate_type == 3);
    // Same name, different runtime type: paired, but the row must stay comparable.
    assert(alignment.matched[1].source_index == 1);
    assert(alignment.matched[1].candidate_index == 2);
    assert(alignment.matched[1].source_type == 0);
    assert(alignment.matched[1].candidate_type == 2);
    // An unnamed source row cannot pair by name, and the candidate-only row is reported too.
    assert(alignment.source_only.size() == 1);
    assert(alignment.source_only[0] == 2);
    assert(alignment.candidate_only.size() == 1);
    assert(alignment.candidate_only[0] == 1);
}

int main()
{
    counts_group_types_and_separate_named_entries();
    alignment_pairs_by_name_and_reports_both_unmatched_sides();
    return 0;
}
