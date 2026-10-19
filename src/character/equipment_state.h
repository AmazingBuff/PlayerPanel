//
// Created by AmazingBuff on 2026/10/19.
//
#pragma once
#include "animation/action.h"
#include <array>
namespace RE { class Actor; }
PLUGIN_NAMESPACE_BEGIN
[[nodiscard]] animation::WeaponForm equipment_weapon_form(RE::Actor& source);
[[nodiscard]] std::array<std::uint32_t, 2> equipment_hand_ids(RE::Actor& source);
PLUGIN_NAMESPACE_END
