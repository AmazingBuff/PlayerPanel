//
// Created by AmazingBuff on 2026/11/11.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/candidate_equip_prime_policy.h"

#include <cassert>
#include <cstddef>
#include <iostream>
#include <string_view>

using test_plugin::animation::behavior_graph::Candidate_Equip_Prime_Writes;

namespace
{
    void exact_source_sequence()
    {
        assert(Candidate_Equip_Prime_Writes.size() == 3);
        assert(Candidate_Equip_Prime_Writes[0].name == "iMagicEquipped");
        assert(Candidate_Equip_Prime_Writes[0].value == 1);
        assert(Candidate_Equip_Prime_Writes[1].name == "iRightHandEquipped");
        assert(Candidate_Equip_Prime_Writes[1].value == 1);
        assert(Candidate_Equip_Prime_Writes[2].name == "iLeftHandEquipped");
        assert(Candidate_Equip_Prime_Writes[2].value == 9);
    }

    void names_are_unique()
    {
        for (std::size_t i = 0; i < Candidate_Equip_Prime_Writes.size(); ++i)
            for (std::size_t j = i + 1; j < Candidate_Equip_Prime_Writes.size(); ++j)
                assert(Candidate_Equip_Prime_Writes[i].name != Candidate_Equip_Prime_Writes[j].name);
    }

    void values_are_the_observed_post_equip_state()
    {
        for (auto const& write : Candidate_Equip_Prime_Writes)
            assert(write.value > 0);
    }
}

int main()
{
    exact_source_sequence();
    names_are_unique();
    values_are_the_observed_post_equip_state();
    std::cout << "equip prime policy: 3 cases passed\n";
    return 0;
}
