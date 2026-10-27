//
// Created by AmazingBuff on 2026/10/27.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/observation_chain_policy.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

#ifdef NDEBUG
#error This test requires assertions.
#endif

void observation_may_only_dereference_plausible_objects()
{
    using test_plugin::animation::behavior_graph::plausible_object_pointer;
    // Values the plugin has actually seen or could plausibly see: heap/stack objects
    // of this process are accepted, null/tiny/unaligned/non-canonical values and the
    // v20 crash value are rejected before any dereference.
    const std::vector<std::pair<std::uintptr_t, bool>> rows{
        { 0x0, false },
        { 0x10, false },
        { 0xFFFF, false },
        { 0x10000, true },
        { 0x1F473324500, true },
        { 0x1F613977880, true },
        { 0x1F473324507, false },
        { 0x00007FFFFFFF0000, true },
        { 0x0000800000000000, false },
        { 0x7BD6CD336F4B8024, false },
        { 0xFFFFFFFFFFFFFFFF, false }
    };
    for (std::pair<std::uintptr_t, bool> const& row : rows)
        assert(plausible_object_pointer(row.first) == row.second);
    std::cout << "1 parameterized observation-chain policy case passed (" << rows.size()
              << " value rows, engine-free)\n";
}

int main()
{
    observation_may_only_dereference_plausible_objects();
}
