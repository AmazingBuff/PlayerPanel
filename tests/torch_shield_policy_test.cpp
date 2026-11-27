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

void bow_to_axe_and_torch_selects_the_axe_equip()
{
    using namespace test_plugin::animation;
    using test_plugin::panel_transition_bases;
    constexpr std::uint32_t bow = 1;
    constexpr std::uint32_t axe = 2;
    constexpr std::uint32_t torch = 3;
    const auto form_of = [](std::uint32_t id)
    {
        switch (id)
        {
        case bow: return WeaponForm::kBow;
        case axe: return WeaponForm::kOneHanded;
        case torch: return WeaponForm::kTorch;
        default: return WeaponForm::kNone;
        }
    };
    const auto equip = torch_equipment_transition({ bow, bow }, { axe, torch }, form_of);
    assert(equip.kind == ActionKind::e_equip && equip.form == WeaponForm::kOneHanded);
    assert(panel_transition_bases(equip.form, false).front() == "Animations\\1HM_Equip.hkx");
    assert(!needs_idle_preparation(idle_target(equip.form, false, Default_Unarmed_Idle), equip.kind));
    const auto unequip = torch_equipment_transition({ axe, torch }, { 0, torch }, form_of);
    assert(unequip.kind == ActionKind::e_unequip && unequip.form == WeaponForm::kOneHanded);
}

void only_a_left_torch_change_can_refresh_the_same_weapon_transition()
{
    using test_plugin::animation::left_torch_only_changed;
    assert(left_torch_only_changed({ 2, 0 }, { 2, 3 }, false, true));
    assert(left_torch_only_changed({ 2, 3 }, { 2, 0 }, true, false));
    assert(left_torch_only_changed({ 0, 0 }, { 0, 3 }, false, true));
    assert(!left_torch_only_changed({ 1, 1 }, { 2, 3 }, false, true));
    assert(!left_torch_only_changed({ 0, 3 }, { 2, 3 }, true, true));
    assert(!left_torch_only_changed({ 2, 3 }, { 2, 4 }, true, false));
}

int main()
{
    weather_flag_keeps_native_source_answer();
    left_shield_removal_keeps_weapon_and_previous_query_hands();
    bow_to_axe_and_torch_selects_the_axe_equip();
    only_a_left_torch_change_can_refresh_the_same_weapon_transition();
    std::cout << "4 torch/shield policy cases passed (engine-free boundary)\n";
}
