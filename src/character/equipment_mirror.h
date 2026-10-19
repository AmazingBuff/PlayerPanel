//
// Created by AmazingBuff on 2026/10/14.
//

#pragma once

#include "RE/B/BGSEquipSlot.h"
#include "RE/T/TESObjectWEAP.h"
#include "animation/action.h"

#include <cstdint>
#include <array>
#include <string>
#include <vector>

namespace RE
{
    class Actor;
    class TESBoundObject;
}

PLUGIN_NAMESPACE_BEGIN

// Worn-equipment mirroring, shared by the two routes that carry a second figure: the copy route
// dressed its clone with this walk from the start (character_clone.cpp) and the puppet now needs
// exactly the same thing - the player's armor and clothing on the panel's figure, not only the
// weapons the hand slot carries.
//
// The division of labour is deliberate and stays: this module mirrors the BIPED body items
// (`BGSBipedObjectForm` occupying a body slot), while the weapon/shield in the two hands is handled
// by the driver's own hand poll, which also has to push the draw/sheathe state. A weapon is not a
// biped form, so the two sets do not overlap.
//
// The biped slots worth mirroring: every armor-bearing slot except the shield - the shield rides
// the left-hand equip slot and belongs to the hand poll - and except FX01, which carries effect
// attachments (the effect-object prune drops their kin from the drawn graph anyway).
[[nodiscard]] bool equipment_is_body_biped(RE::TESBoundObject& a_object);

// Every worn body-slot item the actor carries. `IsWorn()` is a pure `ExtraWorn`/`ExtraWornLeft`
// extra-data flag test - it reads no biped slot and no 3D - so the result is what the actor's own
// inventory says is on the body, which is the only honest source for a figure with no AI.
void equipment_collect_worn(RE::Actor& a_actor, std::vector<RE::TESBoundObject*>& a_out);

// The weapon stance the two hands carry, as the panel's standing figure answers OAR with
// (state-idle v3): the panel figure always stands, so the weapon form is the only thing that
// changes the idle OAR selects for it. Both hands are read, because the engine reports a
// two-handed weapon - and a bow - in whichever hand entry it last touched.
[[nodiscard]] animation::WeaponForm equipment_weapon_form(RE::Actor& a_actor);
[[nodiscard]] std::array<RE::FormID, 2> equipment_hand_ids(RE::Actor& actor);

// Puts everything `a_source` wears onto `a_target`. Used for the puppet's initial dressing, before
// its figure is parked out of the world: the engine re-derives a biped change asynchronously, and
// doing it while the reference still stands beside the player is the order the copy route validated.
[[nodiscard]] std::uint32_t equipment_equip_all_worn(RE::Actor& a_target, RE::Actor& a_source);

// What one reconciliation changed. `worn` is the source's worn body-item count, i.e. what the
// target should now carry.
struct EquipmentSync
{
    std::uint32_t equipped = 0;
    std::uint32_t removed = 0;
    std::uint32_t worn = 0;

    [[nodiscard]] bool changed() const { return equipped != 0 || removed != 0; }
};

// Brings `a_target`'s worn body items in line with `a_source`: equips what the source wears and the
// target does not, unequips what the target wears and the source does not. Stateless - both sets are
// read fresh from the two inventories every call - so a missed frame cannot desynchronise it and
// nothing has to be invalidated across a 3D generation change.
[[nodiscard]] EquipmentSync equipment_sync_worn(RE::Actor& a_target, RE::Actor& a_source);

// What one strip took off a figure.
struct EquipmentStrip
{
    std::uint32_t items = 0;       // inventory entries the actor carried
    std::uint32_t removed = 0;     // ... of which were removed
    std::uint32_t unequipped = 0;  // worn items taken off before that
    std::string sample;            // `name(formID)` of a few inventory entries, for the log
    std::string unequipped_sample; // `name(formID)` of the worn items taken off

    [[nodiscard]] bool changed() const { return removed != 0 || unequipped != 0; }
};

// Takes everything off an actor: every worn body item, both hands, and every inventory entry.
//
// This exists because a freshly placed Actor is not empty. Its base record carries what the record
// itself was authored with - on this machine that is a starting kit added to the player's NPC record,
// and 2026-10-14 measured the figure born wearing an iron war axe (00013790) - and the puppet's base
// is a copy of that record. A figure that is supposed to wear exactly the player's items must start
// from nothing, so the driver strips it once when the reference is born and again before dressing.
[[nodiscard]] EquipmentStrip equipment_strip(RE::Actor& a_actor);

// What one prune took off a figure.
struct EquipmentPrune
{
    std::uint32_t considered = 0;  // inventory entries the actor carried
    std::uint32_t removed = 0;     // ... of which were not the source's and were taken away
    std::string sample;            // `name(formID)` of a few of them, for the log

    [[nodiscard]] bool changed() const { return removed != 0; }
};

// Removes everything the source is NOT using: any carried item that is neither worn on its body nor
// held in one of its hands. Ammunition is held to a stricter rule than plain exemption: only the
// source's CURRENT ammo survives, every other carried round goes - the quiver's model comes from the
// current ammo, so a residual stack left the figure rendering a quiver the player is not wearing.
//
// Equipping the source's set is not enough on its own, because of who else equips. The engine equips
// best-fit gear for an actor from that actor's own inventory, and this figure has no AI to decide
// otherwise, so an item left carried comes straight back on - which is how a weapon that should have
// been replaced stayed in the hand (2026-10-14). Pruning the inventory is what removes the engine's
// opportunity: after a prune the figure carries only the source's own set, so nothing else can be
// equipped on it.
[[nodiscard]] EquipmentPrune equipment_prune(RE::Actor& a_target, RE::Actor& a_source);

// What one ammunition reconciliation changed. `wanted` is the source's current ammo (0 when it has
// none - a player with a drawn bow always has one; a melee player may have none).
struct EquipmentAmmo
{
    std::uint32_t wanted = 0;    // the source's current ammo form id
    std::uint32_t equipped = 0;  // target ammo entries left carried (0 or 1)
    std::uint32_t removed = 0;   // target ammo entries taken away

    [[nodiscard]] bool changed() const { return equipped != 0 || removed != 0; }
};

// Brings the target's ammunition in line with the source's CURRENT ammo: the current ammo is added
// (and equipped, so the engine renders its quiver) and every other carried ammo is taken away. The
// engine picks an actor's current ammo from its own inventory, so a residual stack - the record's
// own starting arrows - would otherwise decide the quiver the figure wears.
[[nodiscard]] EquipmentAmmo equipment_sync_ammo(RE::Actor& a_target, RE::Actor& a_source);

// The hand reconcile's two questions, as pure decisions (the driver's hand poll owns the engine
// calls, mirroring and pruning share the rule). A TWO-HANDED weapon is reported in whichever hand
// entry the engine last touched, and the reading flips between polls, so it counts as held from
// either entry. Comparing the entries one by one unequipped and re-equipped the same weapon every
// frame - and let the prune take that weapon away while the hand poll was putting it back - until
// the engine crashed inside BipedAnim with a null biped object (2026-10-15 22:45:27, BiPolar Blade).
[[nodiscard]] inline bool hand_item_is_wanted(RE::TESBoundObject* a_held, RE::TESBoundObject* a_wanted,
    RE::TESBoundObject* a_two_handed)
{
    return a_held != nullptr && (a_held == a_wanted || a_held == a_two_handed);
}

// The removal loop's question: "is there something in this hand the figure must take off?" An EMPTY
// hand has nothing to take off even when the player does hold something - the equip loop adds that
// below - so a false here must not fall through to UnequipObject/RemoveItem on a null item, which
// crashed inside the inlined form-type check (2026-10-15 23:02:27, "Crimson Tide", puppet_driver
// slot_for dereferencing the null held while the equip poll ran from the first dressing).
[[nodiscard]] inline bool hand_removal_needed(RE::TESBoundObject* a_held, RE::TESBoundObject* a_wanted,
    RE::TESBoundObject* a_two_handed)
{
    return a_held != nullptr && !hand_item_is_wanted(a_held, a_wanted, a_two_handed);
}

[[nodiscard]] inline bool hand_object_equipped(RE::TESBoundObject* a_object, RE::TESBoundObject* a_two_handed,
    RE::TESBoundObject* a_this_entry, RE::TESBoundObject* a_other_entry)
{
    return a_this_entry == a_object || (a_object == a_two_handed && a_other_entry == a_object);
}

// Whether the object is a weapon the engine holds with BOTH hands. Decided from the weapon's own
// animation type - the data the engine itself drives its two-handed behaviour from. The equip slot
// is deliberately NOT consulted: DEFAULT_OBJECT::kEitherHandEquip is the ONE-HANDED either-hand slot
// (vanilla 0x00013f44), while every vanilla two-handed melee weapon and bow carries the two-handed
// slot 0x00013f45, so a slot comparison against the default object never matched and the reconcile
// kept fighting the engine's own recording until it died inside BipedAnim (2026-10-15 23:22 churn,
// 23:55:47 crash, Dwarven Battleaxe). Staves are excluded on purpose: vanilla carries them on
// one-hand slots. This install also legitimately equips such weapons into the LEFT-hand entry, and
// the pair rules below accept them from either entry accordingly.
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

PLUGIN_NAMESPACE_END
