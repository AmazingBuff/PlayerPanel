//
// Created by AmazingBuff on 2026/10/14.
//

#pragma once

#include <cstdint>
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
// The biped slots worth mirroring. Every body slot except the two hand slots: those hold the
// weapons the hand poll owns.
[[nodiscard]] bool equipment_is_body_biped(RE::TESBoundObject& a_object);

// Every worn body-slot item the actor carries. `IsWorn()` is a pure `ExtraWorn`/`ExtraWornLeft`
// extra-data flag test - it reads no biped slot and no 3D - so the result is what the actor's own
// inventory says is on the body, which is the only honest source for a figure with no AI.
void equipment_collect_worn(RE::Actor& a_actor, std::vector<RE::TESBoundObject*>& a_out);

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
    std::string sample;            // `name(formID)` of a few entries, for the log

    [[nodiscard]] bool changed() const { return removed != 0 || unequipped != 0; }
};

// Takes EVERYTHING off an actor: every worn body item, both hands, and every inventory entry.
//
// This exists because a freshly placed Actor is not empty. Its base record carries what the record
// itself was authored with, and the 2026-10-14 session showed the result on the panel figure: a bow,
// a shield and an axe - none of them the player's, and none of them anything this plugin equipped -
// sitting on a puppet whose base is a copy of the PLAYER's NPC record, on top of the equipment the
// mirror had just put there. A figure that is supposed to wear exactly the player's items must start
// from nothing, so the driver strips it once when the reference is born and again before dressing.
[[nodiscard]] EquipmentStrip equipment_strip(RE::Actor& a_actor);

PLUGIN_NAMESPACE_END
