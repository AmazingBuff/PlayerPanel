//
// Created by AmazingBuff on 2026/10/20.
//
#pragma once
#include "dac/effects.h"
namespace RE { class Actor; }
PLUGIN_NAMESPACE_BEGIN
namespace dac
{
    // Returns nullptr while native DataLoaded initialization is unfinished or the
    // original profiled module is unavailable. No native processing code is copied.
    [[nodiscard]] std::vector<RuleScope> const* native_rules();
    void native_dispatch(RE::Actor& source, std::string const& event);
}
PLUGIN_NAMESPACE_END
