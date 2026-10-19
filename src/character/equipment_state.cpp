//
// Created by AmazingBuff on 2026/10/19.
//
#include "character/equipment_state.h"
#include "RE/A/Actor.h"
#include "RE/T/TESObjectWEAP.h"
PLUGIN_NAMESPACE_BEGIN
namespace
{
[[nodiscard]] inline bool hand_weapon_occupies_both_hands(RE::TESBoundObject* a_object)
{
    const RE::TESObjectWEAP* const weapon = a_object ? a_object->As<RE::TESObjectWEAP>() : nullptr;
    if (!weapon)
        return false;
    switch (weapon->GetWeaponType())
    {
    case RE::WEAPON_TYPE::kTwoHandSword:
    case RE::WEAPON_TYPE::kTwoHandAxe:
    case RE::WEAPON_TYPE::kBow:
    case RE::WEAPON_TYPE::kCrossbow:
        return true;
    default:
        return false;
    }
}

// The source's both-hands weapon, if either hand entry reports one: the caller normalises the pair
// with it (and uses it for the two rules above) so a flip between entries is not read as a change.
[[nodiscard]] inline RE::TESBoundObject* hand_two_handed_weapon(RE::TESBoundObject* a_right,
    RE::TESBoundObject* a_left)
{
    if (hand_weapon_occupies_both_hands(a_right))
        return a_right;
    return hand_weapon_occupies_both_hands(a_left) ? a_left : nullptr;
}

}
animation::WeaponForm equipment_weapon_form(RE::Actor& a_actor)
{
    // Only the reading is engine work: which kind each hand carries. The form itself is decided by
    // animation::weapon_form_from_hands, a pure rule the panel's regression covers.
    const auto kind = [](RE::TESForm const* a_form) -> animation::HandKind
    {
        if (!a_form)
            return animation::HandKind::kEmpty;
        if (const RE::TESObjectWEAP* const weapon = a_form->As<RE::TESObjectWEAP>())
        {
            switch (weapon->GetWeaponType())
            {
            case RE::WEAPON_TYPE::kTwoHandSword:
                return animation::HandKind::kTwoHandedWeapon;
            case RE::WEAPON_TYPE::kTwoHandAxe:
                return animation::HandKind::kTwoHandedAxe;
            case RE::WEAPON_TYPE::kBow:
                return animation::HandKind::kBow;
            case RE::WEAPON_TYPE::kCrossbow:
                return animation::HandKind::kCrossbow;
            case RE::WEAPON_TYPE::kStaff:
                return animation::HandKind::kStaff;
            default:
                return animation::HandKind::kOneHandedWeapon;
            }
        }
        if (a_form->As<RE::TESObjectLIGH>())
            return animation::HandKind::kTorch;
        if (a_form->IsArmor())
            return animation::HandKind::kShield;  // a shield is the only armour a hand equips
        if (a_form->Is(RE::FormType::Spell) || a_form->Is(RE::FormType::Scroll) || a_form->Is(RE::FormType::Shout))
            return animation::HandKind::kSpell;
        return animation::HandKind::kEmpty;
    };

    return animation::weapon_form_from_hands(kind(a_actor.GetEquippedObject(false)), kind(a_actor.GetEquippedObject(true)));
}

std::array<RE::FormID, 2> equipment_hand_ids(RE::Actor& actor)
{
    RE::TESForm* const right = actor.GetEquippedObject(false);
    RE::TESForm* const left = actor.GetEquippedObject(true);
    RE::TESBoundObject* const both = hand_two_handed_weapon(right ? right->As<RE::TESObjectWEAP>() : nullptr,
        left ? left->As<RE::TESObjectWEAP>() : nullptr);
    if (both)
        return { both->GetFormID(), both->GetFormID() };
    return { right ? right->GetFormID() : 0, left ? left->GetFormID() : 0 };
}
PLUGIN_NAMESPACE_END
