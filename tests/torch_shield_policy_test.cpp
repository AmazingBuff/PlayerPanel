//
// Created by AmazingBuff on 2026/10/22.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/idle_policy.h"
#include "animation/oar/policy.h"

#include <cassert>
#include <iostream>

void weather_flag_keeps_native_source_answer()
{
    const test_plugin::oar::PreviewState preview;
    assert(test_plugin::oar::forwards_source("CurrentWeatherHasFlag"));
    assert(!test_plugin::oar::standing_predicate("CurrentWeatherHasFlag", preview).has_value());
    assert(!test_plugin::oar::standing_value("CurrentWeatherHasFlag").has_value());
    assert(!test_plugin::oar::forwards_source("UnregisteredCondition"));
}

void left_shield_removal_keeps_weapon_and_previous_query_hands()
{
    using namespace test_plugin;
    const std::array<std::uint32_t, 2> weapon{ 0x123, 0 };
    const std::array<std::uint32_t, 2> shield{ 0x123, 0x456 };
    const auto equip = animation::left_shield_transition(weapon, shield, false, true);
    assert(equip && equip->kind == animation::ActionKind::e_equip && !equip->hand_override);
    const auto remove = animation::left_shield_transition(shield, weapon, true, false);
    assert(remove && remove->kind == animation::ActionKind::e_unequip && remove->hand_override == shield);
    assert(panel_transition_bases(animation::WeaponForm::kOneHanded,
        remove->kind == animation::ActionKind::e_unequip).front() == "Animations\\1HM_Unequip.hkx");
}

int main()
{
    weather_flag_keeps_native_source_answer();
    left_shield_removal_keeps_weapon_and_previous_query_hands();
    std::cout << "2 torch/shield policy cases passed (engine-free boundary)\n";
}
