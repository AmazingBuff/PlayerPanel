//
// Created by AmazingBuff on 2026/10/19.
//
#pragma once
#include <cstdint>
#include <string_view>
#include <vector>
PLUGIN_NAMESPACE_BEGIN
namespace animation
{
    // The equipped hand combination selects the panel idle family.
    enum class WeaponForm : std::uint8_t
    {
        kNone = 0,   // no displayable equipped hand item
        kOneHanded,
        kTwoHanded,
        kBow,
        kDualWield,
        kMagic,   // a hand holds a spell
        kStaff,
        kTorch,
        kShield,
        kCrossbow,
        kTwoHandedAxe
    };

    [[nodiscard]] inline std::string_view weapon_form_name(WeaponForm a_form)
    {
        switch (a_form)
        {
        case WeaponForm::kNone: return "none";
        case WeaponForm::kOneHanded: return "1h";
        case WeaponForm::kTwoHanded: return "2h";
        case WeaponForm::kBow: return "bow";
        case WeaponForm::kDualWield: return "dual";
        case WeaponForm::kMagic: return "magic";
        case WeaponForm::kStaff: return "staff";
        case WeaponForm::kTorch: return "torch";
        case WeaponForm::kShield: return "shield";
        case WeaponForm::kCrossbow: return "crossbow";
        case WeaponForm::kTwoHandedAxe: return "2hw";
        default: return "?";
        }
    }

    // The panel's standing figure answers with the form the two hands carry: a two-handed weapon or
    // a bow occupies both hands (the engine reports it in whichever entry it last touched, so both
    // are read), two one-handed weapons are the dual stance, and a lone one-handed weapon with a
    // shield, torch or spell in the other hand is still the one-handed stance.
    enum class HandKind : std::uint8_t
    {
        kEmpty = 0,
        kOneHandedWeapon,
        kTwoHandedWeapon,
        kStaff,
        kSpell,
        kTorch,
        kShield,
        kBow,
        kCrossbow,
        kTwoHandedAxe
    };

    [[nodiscard]] inline WeaponForm weapon_form_from_hands(HandKind a_right, HandKind a_left)
    {
        if (a_right == HandKind::kBow || a_left == HandKind::kBow)
            return WeaponForm::kBow;
        if (a_right == HandKind::kCrossbow || a_left == HandKind::kCrossbow)
            return WeaponForm::kCrossbow;
        if (a_right == HandKind::kTwoHandedAxe || a_left == HandKind::kTwoHandedAxe)
            return WeaponForm::kTwoHandedAxe;
        if (a_right == HandKind::kTwoHandedWeapon || a_left == HandKind::kTwoHandedWeapon)
            return WeaponForm::kTwoHanded;
        if (a_right == HandKind::kStaff || a_left == HandKind::kStaff)
            return WeaponForm::kStaff;
        if (a_right == HandKind::kOneHandedWeapon && a_left == HandKind::kOneHandedWeapon)
            return WeaponForm::kDualWield;
        if (a_right == HandKind::kOneHandedWeapon || a_left == HandKind::kOneHandedWeapon)
            return WeaponForm::kOneHanded;
        if (a_right == HandKind::kSpell || a_left == HandKind::kSpell)
            return WeaponForm::kMagic;
        if (a_right == HandKind::kTorch || a_left == HandKind::kTorch)
            return WeaponForm::kTorch;
        if (a_right == HandKind::kShield || a_left == HandKind::kShield)
            return WeaponForm::kShield;
        return WeaponForm::kNone;
    }


    enum class ActionKind : std::uint8_t { e_standing_idle, e_weapon_idle };
    struct ActionPrototype
    {
        ActionKind kind;
        WeaponForm form;
        bool loop;
    };
}
[[nodiscard]] inline std::vector<std::string_view> panel_idle_bases(animation::WeaponForm form)
{
    using Form = animation::WeaponForm;
    switch (form)
    {
    case Form::kOneHanded: return { "Animations\\1hm_idle.hkx" };
    case Form::kTwoHanded: return { "Animations\\2hm_idle.hkx" };
    case Form::kTwoHandedAxe: return { "Animations\\2hw_idle.hkx", "Animations\\2hm_idle.hkx" };
    case Form::kBow: return { "Animations\\bow_idle.hkx" };
    case Form::kCrossbow: return { "Animations\\crossbow_idle.hkx", "Animations\\bow_idle.hkx" };
    case Form::kDualWield: return { "Animations\\dw1hm_idle.hkx", "Animations\\1hm_idle.hkx" };
    case Form::kMagic: return { "Animations\\MRh_Idle.hkx", "Animations\\MLh_Idle.hkx" };
    case Form::kStaff: return { "Animations\\StaffRight_Idle.hkx", "Animations\\StaffLeft_Idle.hkx" };
    case Form::kTorch: return { "Animations\\MT_IdleTorch.hkx", "Animations\\1hm_idle.hkx" };
    case Form::kShield: return { "Animations\\1hm_idle.hkx" };
    default: return { "Animations\\h2h_idle.hkx" };
    }
}

PLUGIN_NAMESPACE_END
