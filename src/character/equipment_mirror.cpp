//
// Created by AmazingBuff on 2026/10/14.
//

#include "character/equipment_mirror.h"

#include "RE/A/Actor.h"
#include "RE/A/ActorEquipManager.h"
#include "RE/B/BGSBipedObjectForm.h"
#include "RE/I/InventoryChanges.h"
#include "RE/I/InventoryEntryData.h"
#include "RE/I/ItemRemoveReason.h"
#include "RE/T/TESAmmo.h"
#include "RE/T/TESBoundObject.h"
#include "RE/T/TESForm.h"

#include <algorithm>

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // Every biped slot that carries the worn figure's appearance. The shield is excluded: it rides
    // the left-hand equip slot and the driver's hand poll mirrors it (with the draw/sheathe push that
    // belongs there), and a weapon is not a `BGSBipedObjectForm`, so neither set can reach into the
    // other. FX01 stays out too: it carries effect attachments, and the effect-object prune drops
    // their kin from the drawn graph anyway.
    constexpr RE::BGSBipedObjectForm::BipedObjectSlot Body_Slots[] =
    {
        RE::BGSBipedObjectForm::BipedObjectSlot::kHead,
        RE::BGSBipedObjectForm::BipedObjectSlot::kHair,
        RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
        RE::BGSBipedObjectForm::BipedObjectSlot::kHands,
        RE::BGSBipedObjectForm::BipedObjectSlot::kForearms,
        RE::BGSBipedObjectForm::BipedObjectSlot::kAmulet,
        RE::BGSBipedObjectForm::BipedObjectSlot::kRing,
        RE::BGSBipedObjectForm::BipedObjectSlot::kFeet,
        RE::BGSBipedObjectForm::BipedObjectSlot::kCalves,
        RE::BGSBipedObjectForm::BipedObjectSlot::kTail,
        RE::BGSBipedObjectForm::BipedObjectSlot::kLongHair,
        RE::BGSBipedObjectForm::BipedObjectSlot::kCirclet,
        RE::BGSBipedObjectForm::BipedObjectSlot::kEars,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModMouth,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModNeck,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModChestPrimary,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModChestSecondary,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModShoulder,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModBack,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModMisc1,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModPelvisPrimary,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModPelvisSecondary,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModArmLeft,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModArmRight,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModLegRight,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModLegLeft,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModFaceJewelry,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModMisc2,
    };

    // The actor's CURRENT ammunition, as the engine itself selects it (same virtual, same source
    // the quiver model is rendered from). Null when the actor has no current ammo.
    RE::TESAmmo* current_ammo(RE::Actor& a_actor)
    {
        return a_actor.GetCurrentAmmo();
    }

    void collect_worn(RE::Actor& a_actor, std::vector<RE::TESBoundObject*>& a_out)
    {
        a_out.clear();
        // `false` = initialise the container if it is not there yet. A freshly placed Actor's inventory
        // is created on demand, and asking for it with `true` handed back nothing at all - which is how
        // the 2026-10-14 strip reported `items=0` on a shell that was wearing an iron war axe.
        RE::InventoryChanges* const changes = a_actor.GetInventoryChanges(false);
        if (!changes || !changes->entryList)
            return;
        for (const RE::InventoryEntryData* entry : *changes->entryList)
        {
            if (!entry || !entry->IsWorn())
                continue;
            RE::TESBoundObject* const object = entry->object;
            if (object && equipment_is_body_biped(*object))
                a_out.push_back(object);
        }
    }
    // Takes an item off a figure and out of its inventory.
    //
    // Both halves are needed. Unequipping alone leaves the item carried, and a figure with no AI to
    // decide otherwise is exactly what the engine's own auto-equip serves: it puts the item straight
    // back on. And the equip API's own report is not to be trusted here - the 2026-10-14 session
    // logged `removed=0` on every weapon replacement while the old weapon stayed in the hand, so the
    // removal is done through the inventory as well and the caller re-reads to see what happened.
    void take_away(RE::Actor& a_actor, RE::TESBoundObject* a_object)
    {
        if (!a_object)
            return;
        if (RE::ActorEquipManager* const equip_manager = RE::ActorEquipManager::GetSingleton())
            equip_manager->UnequipObject(&a_actor, a_object, nullptr, 1, nullptr, false, true, false, true);

        RE::InventoryChanges* const changes = a_actor.GetInventoryChanges(false);
        const std::int16_t carried = changes ? changes->GetItemCount(a_object) : 0;
        // An equipped-but-uncarried item still has to go: the birth strip met exactly that state (an
        // iron war axe in the hand and an empty inventory).
        a_actor.RemoveItem(a_object, carried > 0 ? carried : 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
    }
}

bool equipment_is_body_biped(RE::TESBoundObject& a_object)
{
    const RE::BGSBipedObjectForm* const biped = a_object.As<RE::BGSBipedObjectForm>();
    if (!biped)
        return false;
    return std::ranges::any_of(Body_Slots, [biped](RE::BGSBipedObjectForm::BipedObjectSlot a_slot)
    {
        return biped->HasPartOf(a_slot);
    });
}

void equipment_collect_worn(RE::Actor& a_actor, std::vector<RE::TESBoundObject*>& a_out)
{
    collect_worn(a_actor, a_out);
}

std::uint32_t equipment_equip_all_worn(RE::Actor& a_target, RE::Actor& a_source)
{
    RE::ActorEquipManager* const equip_manager = RE::ActorEquipManager::GetSingleton();
    if (!equip_manager)
        return 0;

    std::vector<RE::TESBoundObject*> worn;
    collect_worn(a_source, worn);

    RE::InventoryChanges* const target_changes = a_target.GetInventoryChanges(false);
    std::uint32_t equipped = 0;
    for (RE::TESBoundObject* const object : worn)
    {
        if (!object)
            continue;
        // The copy route's order, unchanged: the item has to exist in the target's own inventory
        // before the equip is asked for, and `applyNow` is what makes the biped change happen in this
        // call rather than on the actor's next update (a figure with no AI never gets that update).
        // It is added only when it is not already carried - adding a second copy is how a figure ends
        // up with two of the same item.
        if (target_changes && target_changes->GetItemCount(object) <= 0)
            a_target.AddObjectToContainer(object, nullptr, 1, nullptr);
        equip_manager->EquipObject(&a_target, object, nullptr, 1, nullptr, false, true, false, true);
        ++equipped;
    }
    return equipped;
}

EquipmentSync equipment_sync_worn(RE::Actor& a_target, RE::Actor& a_source)
{
    EquipmentSync sync;

    std::vector<RE::TESBoundObject*> wanted;
    collect_worn(a_source, wanted);
    sync.worn = static_cast<std::uint32_t>(wanted.size());

    RE::ActorEquipManager* const equip_manager = RE::ActorEquipManager::GetSingleton();
    if (!equip_manager)
        return sync;

    std::vector<RE::TESBoundObject*> current;
    collect_worn(a_target, current);

    const auto in = [](const std::vector<RE::TESBoundObject*>& a_set, RE::TESBoundObject* a_object)
    {
        return std::ranges::any_of(a_set, [a_object](RE::TESBoundObject* a_entry) { return a_entry == a_object; });
    };

    for (RE::TESBoundObject* const object : wanted)
    {
        if (!object || in(current, object))
            continue;
        RE::InventoryChanges* const changes = a_target.GetInventoryChanges(false);
        if (changes && changes->GetItemCount(object) <= 0)
            a_target.AddObjectToContainer(object, nullptr, 1, nullptr);
        equip_manager->EquipObject(&a_target, object, nullptr, 1, nullptr, false, true, false, true);
        ++sync.equipped;
    }

    for (RE::TESBoundObject* const object : current)
    {
        // Anything the target wears that the source no longer does comes off AND leaves the container:
        // carried-but-unwanted is what the engine's auto-equip picks back up.
        if (!object || in(wanted, object))
            continue;
        take_away(a_target, object);
        ++sync.removed;
    }

    return sync;
}

EquipmentPrune equipment_prune(RE::Actor& a_target, RE::Actor& a_source)
{
    EquipmentPrune out;

    // Everything the source is actually using: what it wears on the body, what its two hands hold,
    // and the one ammo its quiver renders from.
    std::vector<RE::TESBoundObject*> keep;
    collect_worn(a_source, keep);
    for (const bool left : { false, true })
    {
        const RE::InventoryEntryData* const held = a_source.GetEquippedEntryData(left);
        if (held && held->object)
            keep.push_back(held->object);
    }
    if (RE::TESAmmo* const ammo = current_ammo(a_source))
        keep.push_back(ammo);

    RE::InventoryChanges* const changes = a_target.GetInventoryChanges(false);
    if (!changes || !changes->entryList)
        return out;

    // Collected first: taking an item away mutates the list being walked.
    std::vector<RE::TESBoundObject*> extras;
    for (const RE::InventoryEntryData* entry : *changes->entryList)
    {
        if (!entry || !entry->object)
            continue;
        ++out.considered;
        if (std::ranges::any_of(keep, [object = entry->object](RE::TESBoundObject* a_kept) { return a_kept == object; }))
            continue;
        extras.push_back(entry->object);
    }

    for (RE::TESBoundObject* const object : extras)
    {
        if (out.sample.size() < 200)
        {
            const char* const name = object->GetName();
            out.sample += fmt::format("{}({:08x}) ", name && *name ? name : "?", object->GetFormID());
        }
        take_away(a_target, object);
        ++out.removed;
    }

    return out;
}

EquipmentAmmo equipment_sync_ammo(RE::Actor& a_target, RE::Actor& a_source)
{
    EquipmentAmmo sync;
    RE::TESAmmo* const wanted = current_ammo(a_source);
    RE::TESAmmo* const held = current_ammo(a_target);
    sync.wanted = wanted ? wanted->GetFormID() : 0;
    if (held == wanted)
        return sync;   // the quiver already follows the player's, or both are empty

    RE::ActorEquipManager* const equip_manager = RE::ActorEquipManager::GetSingleton();
    if (!equip_manager)
        return sync;

    // The held one comes off first, by the same route everything else does - it may be
    // equipped-but-uncarried, the state the birth strip measured on the record's iron war axe,
    // which no inventory walk can see.
    if (held)
    {
        take_away(a_target, held);
        ++sync.removed;
    }

    // Collected after that removal: the remaining carried ammo the player is not using.
    std::vector<RE::TESBoundObject*> stale;
    RE::InventoryChanges* const changes = a_target.GetInventoryChanges(false);
    if (changes && changes->entryList)
    {
        for (const RE::InventoryEntryData* entry : *changes->entryList)
        {
            if (!entry || !entry->object || !entry->object->IsAmmo())
                continue;
            if (wanted && entry->object == wanted)
                continue;
            if (held && entry->object == held)
                continue;   // already taken above
            stale.push_back(entry->object);
        }
    }

    // The wanted ammo has to be carried AND equipped: the engine picks an actor's current ammo from
    // what it carries, so an unequipped stack leaves the quiver to whatever else is in there.
    if (wanted)
    {
        if (changes && changes->GetItemCount(wanted) <= 0)
            a_target.AddObjectToContainer(wanted, nullptr, 1, nullptr);
        equip_manager->EquipObject(&a_target, wanted, nullptr, 1, nullptr, false, true, false, true);
        ++sync.equipped;
    }

    for (RE::TESBoundObject* const object : stale)
    {
        take_away(a_target, object);
        ++sync.removed;
    }

    return sync;
}

EquipmentStrip equipment_strip(RE::Actor& a_actor)
{
    EquipmentStrip out;

    const auto note = [&out](RE::TESBoundObject* a_object)
    {
        if (!a_object || out.unequipped_sample.size() >= 200)
            return;
        const char* const name = a_object->GetName();
        out.unequipped_sample += fmt::format("{}({:08x}) ", name && *name ? name : "?", a_object->GetFormID());
    };

    // Worn items come off first, and by the same route everything else does: the equip API alone was
    // measured leaving a hand occupied (`removed=0` on a replacement), so the item is unequipped and
    // then removed rather than trusted to a return value.
    for (const RE::BGSBipedObjectForm::BipedObjectSlot slot : Body_Slots)
    {
        RE::TESObjectARMO* const worn = a_actor.GetWornArmor(slot);
        if (!worn)
            continue;
        take_away(a_actor, worn);
        ++out.unequipped;
        note(worn);
    }
    // A two-handed weapon is reported in BOTH hand entries, and taking the same object away twice
    // would count it twice and hand the removal a count it no longer carries.
    std::vector<RE::TESBoundObject*> stripped_hands;
    for (const bool left : { false, true })
    {
        const RE::InventoryEntryData* const held = a_actor.GetEquippedEntryData(left);
        RE::TESBoundObject* const object = held && held->object ? held->object : nullptr;
        if (!object || std::ranges::find(stripped_hands, object) != stripped_hands.end())
            continue;
        stripped_hands.push_back(object);
        take_away(a_actor, object);
        ++out.unequipped;
        note(object);
    }

    // `false`: initialise the container if it is not there. With `true` a freshly placed Actor handed
    // back nothing at all, which is how the first strip reported `items=0` on a shell that was wearing
    // an iron war axe - the record's own kit then arrived later and nothing ever took it off.
    RE::InventoryChanges* const changes = a_actor.GetInventoryChanges(false);
    if (!changes || !changes->entryList)
        return out;

    // Collected first, removed afterwards: the removal mutates the list being walked.
    std::vector<RE::TESBoundObject*> entries;
    for (const RE::InventoryEntryData* entry : *changes->entryList)
        if (entry && entry->object && entry->countDelta != 0)
            entries.push_back(entry->object);
    out.items = static_cast<std::uint32_t>(entries.size());

    for (RE::TESBoundObject* const object : entries)
    {
        if (out.sample.size() < 200)
        {
            const char* const name = object->GetName();
            out.sample += fmt::format("{}({:08x}) ", name && *name ? name : "?", object->GetFormID());
        }
        take_away(a_actor, object);
        ++out.removed;
    }
    return out;
}

PLUGIN_NAMESPACE_END
