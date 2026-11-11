//
// Created by AmazingBuff on 2026/11/11.
//
#pragma once

#include <array>
#include <string_view>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // C-G5-CAL D3 v2: the four words needed to identify the right-hand weapon branch at
    // window-open. Transient mode only logs changes, so these initial values must be read once.
    inline constexpr std::array<std::string_view, 4> Source_Timeline_Probe_Names{ {
        "iRightHandType",
        "iLeftHandType",
        "Is1HM",
        "i1HMState",
    } };
}
PLUGIN_NAMESPACE_END
