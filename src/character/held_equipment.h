//
// Created by AmazingBuff on 2026/10/20.
//
#pragma once
#include <span>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "RE/N/NiTransform.h"
namespace RE { class Actor; class NiAVObject; class NiNode; }
PLUGIN_NAMESPACE_BEGIN
enum class HeldWeaponState : std::uint8_t { e_equipped, e_retained, e_hidden };

[[nodiscard]] constexpr HeldWeaponState held_weapon_state(HeldWeaponState previous, std::uint32_t weapon,
    std::uint32_t equipped, std::uint32_t unequipping)
{
    if (previous == HeldWeaponState::e_hidden) return previous;
    if (weapon == equipped) return HeldWeaponState::e_equipped;
    return weapon == unequipping ? HeldWeaponState::e_retained : HeldWeaponState::e_hidden;
}

// A mapped equipment model with both parents it may live under. The model is mounted on the
// hand at capture; the transition mount parks it on its captured sheath parent until the equip
// clip reaches its weaponDraw event. A removed weapon stays attached to its own copy until
// its Unequip handoff or supersession, then the studio stops submitting its geometry.
struct HeldCopyMount
{
    RE::NiAVObject* model = nullptr;
    RE::NiNode* hand = nullptr;
    RE::NiNode* sheath_parent = nullptr;
    RE::NiTransform local{};
    std::uint32_t weapon = 0;
    std::size_t slot = 0;
    HeldWeaponState state = HeldWeaponState::e_equipped;
};
struct HeldDiagnosticIds
{
    std::uint64_t request;
    std::uint64_t accepted_request;
    std::uint32_t capture;
};
void hold_copy_equipment(RE::Actor& source, std::span<RE::NiAVObject* const> originals,
    std::span<RE::NiAVObject* const> copies, std::vector<HeldCopyMount>* mounts = nullptr, HeldDiagnosticIds ids = {});
PLUGIN_NAMESPACE_END
