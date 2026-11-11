//
// Created by AmazingBuff on 2026/11/11.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/source_timeline_probe_policy.h"

#include <cassert>
#include <cstddef>
#include <iostream>

using test_plugin::animation::behavior_graph::Source_Timeline_Probe_Names;

namespace
{
    void exact_probe_names()
    {
        assert(Source_Timeline_Probe_Names.size() == 4);
        assert(Source_Timeline_Probe_Names[0] == "iRightHandType");
        assert(Source_Timeline_Probe_Names[1] == "iLeftHandType");
        assert(Source_Timeline_Probe_Names[2] == "Is1HM");
        assert(Source_Timeline_Probe_Names[3] == "i1HMState");
    }

    void names_are_unique()
    {
        for (std::size_t i = 0; i < Source_Timeline_Probe_Names.size(); ++i)
            for (std::size_t j = i + 1; j < Source_Timeline_Probe_Names.size(); ++j)
                assert(Source_Timeline_Probe_Names[i] != Source_Timeline_Probe_Names[j]);
    }
}

int main()
{
    exact_probe_names();
    names_are_unique();
    std::cout << "source timeline probe policy: 2 cases passed\n";
    return 0;
}
