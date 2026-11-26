//
// Created by AmazingBuff on 2026/11/26.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/dac/effect_presence.h"

#include <array>
#include <cassert>
#include <iostream>

using test_plugin::dac::dispelled_effect_corrections;
using test_plugin::dac::EffectCorrection;
using test_plugin::dac::ObservedEffectPresence;

void dispelled_managed_effect_is_excluded()
{
    constexpr std::uint32_t effect = 0x12000804;
    std::array effects{ ObservedEffectPresence{ effect, true, true, false } };

    const std::vector<EffectCorrection> expected{ { effect, true, true } };
    assert(dispelled_effect_corrections(effects) == expected);
}

void live_same_effect_from_another_spell_still_counts()
{
    constexpr std::uint32_t effect = 0x12000804;
    std::array effects{
        ObservedEffectPresence{ effect, true, true, false },
        ObservedEffectPresence{ effect, false, false, false }
    };

    assert(dispelled_effect_corrections(effects).empty());
}

int main()
{
    dispelled_managed_effect_is_excluded();
    live_same_effect_from_another_spell_still_counts();
    std::cout << "2 DAC effect presence cases passed\n";
}
