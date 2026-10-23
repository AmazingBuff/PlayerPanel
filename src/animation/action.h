//
// Created by AmazingBuff on 2026/10/19.
//
#pragma once
#include <cstdint>
#include <algorithm>
#include <span>
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
    // are read), two one-handed weapons are the dual stance, a torch in either hand is its own
    // stance, except a right one-handed weapon with a left torch composes a separate arm pose, and
    // a lone one-handed weapon with a shield or spell in the other hand is still the one-handed
    // stance.
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
        if (a_right == HandKind::kOneHandedWeapon && a_left == HandKind::kTorch)
            return WeaponForm::kOneHanded;
        if (a_right == HandKind::kTorch || a_left == HandKind::kTorch)
            return WeaponForm::kTorch;
        if (a_right == HandKind::kOneHandedWeapon && a_left == HandKind::kOneHandedWeapon)
            return WeaponForm::kDualWield;
        if (a_right == HandKind::kOneHandedWeapon || a_left == HandKind::kOneHandedWeapon)
            return WeaponForm::kOneHanded;
        if (a_right == HandKind::kSpell || a_left == HandKind::kSpell)
            return WeaponForm::kMagic;
        if (a_right == HandKind::kShield || a_left == HandKind::kShield)
            return WeaponForm::kShield;
        return WeaponForm::kNone;
    }


    enum class ActionKind : std::uint8_t { e_standing_idle, e_weapon_idle, e_equip, e_unequip };
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
    // Vanilla "held" names, taken from the engine's own animation-name table (OAR log
    // "original animation" lines); the first round of this backend used invented names
    // (bow_idle/crossbow_idle/torch/dual) that are absent from the table, so the proposal
    // could not be built at all and the panel stayed empty for those families.
    case Form::kBow: return { "Animations\\Bow_IdleHeld.hkx" };
    case Form::kCrossbow: return { "Animations\\DLC01\\CrossBow_IdleHeld.hkx", "Animations\\Bow_IdleHeld.hkx" };
    case Form::kDualWield: return { "Animations\\DW1HM1HMIdle.hkx", "Animations\\1hm_idle.hkx" };
    case Form::kMagic: return { "Animations\\MRh_Idle.hkx", "Animations\\MLh_Idle.hkx" };
    case Form::kStaff: return { "Animations\\StaffRight_Idle.hkx", "Animations\\StaffLeft_Idle.hkx" };
    case Form::kTorch:
        // The world's standing graph routes a held torch to the TOR_Idle clip, and the torch-aware
        // packs branch there (Malignis/Ashe Idle_Torch under their torch conditions); the idle
        // query takes the first base it finds, so TOR_Idle alone is the faithful route.
        return { "Animations\\TOR_Idle.hkx" };
    case Form::kShield: return { "Animations\\1hm_idle.hkx" };
    default: return { "Animations\\H2H_Idle.hkx", "Animations\\MT_Idle.hkx" };
    }
}

// The world-side draw/sheathe move for the stance. Same naming rule as the idle table: exact
// engine names, with the pre-canonical lowercase variants kept as fallbacks for packs that
// registered their own copies. A one-shot transition is played before the idle after an
// in-inventory equipment change (the world shows Equip -> Start -> Loop; the panel has no
// Start/Loop playlist, so the transition hands over to the OAR-selected loop idle).
[[nodiscard]] inline std::vector<std::string_view> panel_transition_bases(animation::WeaponForm form, bool unequip)
{
    using Form = animation::WeaponForm;
    switch (form)
    {
    case Form::kTwoHanded:
        return unequip ? std::vector<std::string_view>{ "Animations\\2HM_Unequip.hkx", "Animations\\2hc_unequip.hkx" }
                       : std::vector<std::string_view>{ "Animations\\2HM_Equip.hkx", "Animations\\2hc_equip.hkx" };
    case Form::kTwoHandedAxe:
        return unequip ? std::vector<std::string_view>{ "Animations\\2HW_Unequip.hkx", "Animations\\2hw_unequip.hkx" }
                       : std::vector<std::string_view>{ "Animations\\2HW_Equip.hkx", "Animations\\2hw_equip.hkx" };
    case Form::kBow:
        return unequip ? std::vector<std::string_view>{ "Animations\\Bow_Unequip.hkx", "Animations\\bow_unequip.hkx" }
                       : std::vector<std::string_view>{ "Animations\\Bow_Equip.hkx", "Animations\\bow_equip.hkx" };
    case Form::kCrossbow:
        return unequip ? std::vector<std::string_view>{ "Animations\\DLC01\\CrossBow_Unequip.hkx", "Animations\\crossbow_unequip.hkx" }
                       : std::vector<std::string_view>{ "Animations\\DLC01\\CrossBow_Equip.hkx", "Animations\\crossbow_equip.hkx" };
    case Form::kMagic:
        return unequip ? std::vector<std::string_view>{ "Animations\\MRh_Unequip.hkx", "Animations\\MLh_Unequip.hkx" }
                       : std::vector<std::string_view>{ "Animations\\MRh_Equip.hkx", "Animations\\MLh_Equip.hkx" };
    case Form::kStaff:
        return unequip ? std::vector<std::string_view>{ "Animations\\StaffRight_Unequip.hkx", "Animations\\1hm_unequip.hkx" }
                       : std::vector<std::string_view>{ "Animations\\StaffRight_Equip.hkx", "Animations\\1hm_equip.hkx" };
    case Form::kNone: return {};
    case Form::kTorch: return {}; // a torch equip has no equip move of its own; go straight to the torch idle
    default:
        return unequip ? std::vector<std::string_view>{ "Animations\\1HM_Unequip.hkx", "Animations\\1hm_unequip.hkx" }
                       : std::vector<std::string_view>{ "Animations\\1HM_Equip.hkx", "Animations\\1hm_equip.hkx" };
    }
}

[[nodiscard]] inline std::span<std::string_view const> panel_torch_arm_bases()
{
    static constexpr std::string_view s_names[]{ "Animations\\Tor_1HMPose.hkx" };
    return s_names;
}

[[nodiscard]] inline std::span<std::string_view const> panel_base_animation_names()
{
    static const std::vector<std::string_view> s_names = []
    {
        std::vector<std::string_view> names;
        const auto append = [&](std::vector<std::string_view> const& bases)
        {
            for (std::string_view base : bases)
                if (std::ranges::find(names, base) == names.end()) names.push_back(base);
        };
        for (std::uint8_t value = 0; value <= static_cast<std::uint8_t>(animation::WeaponForm::kTwoHandedAxe); ++value)
        {
            const animation::WeaponForm form = static_cast<animation::WeaponForm>(value);
            append(panel_idle_bases(form));
            append(panel_transition_bases(form, false));
            append(panel_transition_bases(form, true));
        }
        for (std::string_view base : panel_torch_arm_bases()) names.push_back(base);
        return names;
    }();
    return s_names;
}

PLUGIN_NAMESPACE_END
