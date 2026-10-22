//
// Created by AmazingBuff on 2026/10/22.
//
#pragma once

#include <cstring>
#include <string_view>

PLUGIN_NAMESPACE_BEGIN
namespace dac
{
    [[nodiscard]] inline bool event_matches(std::string_view rule, std::string_view event)
    {
        return rule.size() == event.size() &&
            (rule.empty() || ::_strnicmp(rule.data(), event.data(), rule.size()) == 0);
    }
}
PLUGIN_NAMESPACE_END
