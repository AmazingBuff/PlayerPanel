#pragma once

#include "oar/context.h"

#include <algorithm>
#include <optional>
#include <string_view>

PLUGIN_NAMESPACE_BEGIN
namespace oar
{
    // Animation family only. The source actor's draw state is read independently of its equipment.
    enum class WeaponForm : std::uint8_t
    {
        kNone = 0,   // nothing drawn / nothing legible: the plain standing idle
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

    [[nodiscard]] inline std::optional<bool> standing_predicate(std::string_view name, PreviewState const& state)
    {
        if (name == "IsWeaponDrawn")
            return state.weapon_drawn;
        constexpr std::string_view inactive[] = {
            "IsAttacking", "IsRunning", "IsSneaking", "IsSprinting", "IsInAir", "IsInCombat",
            "IsBlocking", "IsOnMount", "IsRiding", "IsRidingHasKeyword", "IsBeingRidden",
            "IsBeingRiddenBy", "IsSwimming", "IsStaggered", "IsTalking", "IsGreetingPlayer",
            "IsInScene", "IsInSpecifiedScene", "IsDoingFavor", "IsOnStairs", "MOUNT"
        };
        if (std::ranges::find(inactive, name) != std::end(inactive))
            return false;
        return std::nullopt;
    }

    [[nodiscard]] inline std::optional<float> standing_value(std::string_view name)
    {
        constexpr std::string_view zero[] = {
            "CurrentMovementSpeed", "SubmergeLevel", "SitSleepState", "AttackState"
        };
        if (std::ranges::find(zero, name) != std::end(zero))
            return 0.0f;
        return std::nullopt;
    }

    [[nodiscard]] inline bool forwards_source(std::string_view name)
    {
        constexpr std::string_view names[] = {
            "OR", "AND", "XOR", "PRESET", "TARGET", "PLAYER",
            "IsForm", "IsActorBase", "IsRace", "IsFemale", "IsChild", "IsUnique", "IsClass",
            "IsVoiceType", "IsPlayerTeammate", "IsInFaction", "FactionRank", "HasKeyword",
            "HasPerk", "HasSpell", "HasMagicEffect", "HasMagicEffectWithKeyword",
            "IsEquipped", "IsEquippedType", "IsEquippedHasKeyword", "IsEquippedPower",
            "IsEquippedShout", "IsWorn", "IsWornHasKeyword", "IsWornInSlot", "IsWornInSlotHasKeyword",
            "IsEquippedHasEnchantment", "IsEquippedHasEnchantmentWithKeyword", "HasBoundWeaponEquipped",
            "Level", "CompareValues", "Scale", "Height", "Weight", "InventoryCount", "InventoryWeight",
            "CurrentWeather", "CurrentGameTime", "IsInInterior", "IsInLocation", "IsParentCell",
            "IsWorldSpace", "IsMenuOpen", "IsReplacerEnabled"
        };
        return std::ranges::find(names, name) != std::end(names);
    }

    [[nodiscard]] inline bool finish_predicate(bool value, bool disabled, bool negated)
    {
        return disabled || (negated ? !value : value);
    }
}
PLUGIN_NAMESPACE_END
