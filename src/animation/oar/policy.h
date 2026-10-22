#pragma once

#include "animation/oar/context.h"

#include <algorithm>
#include <optional>
#include <string_view>

PLUGIN_NAMESPACE_BEGIN
namespace oar
{
    [[nodiscard]] inline std::optional<bool> standing_predicate(std::string_view name, PreviewState const& state)
    {
        if (name == "IsWeaponDrawn")
            return state.weapon_drawn;
        // The preview figure stands still, so every locomotion/state predicate answers false.
        // IsMovementDirection belongs here: OAR's numeric component carries the REQUESTED
        // direction ("Direction", normally 0 = forward), not the actor's motion, so reading that
        // component can never answer "is the actor moving that way". Answering false is also what
        // the Smooth non-combat branches need - they add
        // `OR( HasMagicEffect(loop marker), NOT IsMovementDirection(0) )`, and a standing figure
        // satisfies the negated arm, which is how the panel reaches the Non-Combat Equip/Unequip
        // variants the world plays while the weapon is sheathed.
        constexpr std::string_view inactive[] = {
            "IsAttacking", "IsRunning", "IsSneaking", "IsSprinting", "IsInAir", "IsInCombat",
            "IsBlocking", "IsOnMount", "IsRiding", "IsRidingHasKeyword", "IsBeingRidden",
            "IsBeingRiddenBy", "IsSwimming", "IsStaggered", "IsTalking", "IsGreetingPlayer",
            "IsInScene", "IsInSpecifiedScene", "IsDoingFavor", "IsOnStairs", "MOUNT",
            "IsMovementDirection"
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
            "CurrentWeather", "CurrentWeatherHasFlag", "CurrentGameTime", "IsInInterior", "IsInLocation", "IsParentCell",
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
