//
// Created by AmazingBuff on 2026/10/22.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/diagnostic_trace.h"

#include <cassert>
#include <iostream>

void request_budget_bounds_rearming_sampling_and_stale_windows()
{
    test_plugin::animation::PoseTraceBudget budget;
    for (int window = 0; window < 4; ++window)
    {
        assert(budget.arm(7));
        assert(budget.sample(0.0) == 0);
        assert(budget.sample(0.2) == 1);
        assert(!budget.sample(0.21));
    }
    assert(!budget.arm(7));
    assert(!budget.sample(0.5));
    assert(budget.exhausted_notice() && !budget.exhausted_notice());
    for (int window = 0; window < 2; ++window)
    {
        assert(budget.arm(8));
        for (double elapsed : test_plugin::animation::Trace_Sample_Times) assert(budget.sample(elapsed));
    }
    assert(budget.exhausted_notice() && !budget.exhausted_notice());
    budget.request(8);
    assert(!budget.arm(8) && !budget.sample(0.0));
    assert(budget.arm(9) && budget.sample(0.0));
    assert(!budget.sample(4.2) && !budget.sample(4.0));
}

int main()
{
    request_budget_bounds_rearming_sampling_and_stale_windows();
    std::cout << "1 request-wide diagnostic budget case passed (engine-free boundary)\n";
}
