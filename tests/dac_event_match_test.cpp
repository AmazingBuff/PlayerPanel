//
// Created by AmazingBuff on 2026/10/22.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/dac/event_match.h"

#include <cassert>
#include <iostream>

void mixed_case_annotation_matches_native_rule()
{
    assert(test_plugin::dac::event_matches("Smooth_loop", "Smooth_Loop"));
}

void loop_off_event_remains_distinct()
{
    assert(!test_plugin::dac::event_matches("Smooth_loop", "Smooth_Loop_off"));
}

int main()
{
    mixed_case_annotation_matches_native_rule();
    loop_off_event_remains_distinct();
    std::cout << "2 native DAC event matching cases passed (matching boundary only)\n";
}
