//
// Created by AmazingBuff on 2026/10/14.
//

#include "character/equipment_mirror.h"

#include "RE/A/Actor.h"
#include "RE/A/ActorEquipManager.h"
#include "RE/B/BGSBipedObjectForm.h"
#include "RE/I/InventoryChanges.h"
#include "RE/I/InventoryEntryData.h"
#include "RE/T/TESBoundObject.h"

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // Every biped slot that carries the worn figure's appearance. The hand slots are excluded: those
    // hold the weapons the driver's own hand poll mirrors (with the draw/sheathe push that belongs to
    // them), and a weapon is not a `BGSBipedObjectForm`, so neither set can reach into the other.
    constexpr RE::BGSBipedObjectForm::BipedObjectSlot Body_Slots[] =
    {
        RE::BGSBipedObjectForm::BipedObjectSlot::kBody,
        RE::BGSBipedObjectForm::BipedObjectSlot::kHead,
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
        RE::BGSBipedObjectForm::BipedObjectSlot::kModArmLeft,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModArmRight,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModLegRight,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModLegLeft,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModFaceJewelry,
    };

    void collect_worn(RE::Actor& a_actor, std::vector<RE::TESBoundObject*>& a_out)
    {
        a_out.clear();
        RE::InventoryChanges* const changes = a_actor.GetInventoryChanges(true);
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

    std::uint32_t equipped = 0;
    for (RE::TESBoundObject* const object : worn)
    {
        if (!object)
            continue;
        // The copy route's order, unchanged: the item has to exist in the target's own inventory
        // before the equip is asked for, and `applyNow` is what makes the biped change happen in this
        // call rather than on the actor's next update (a figure with no AI never gets that update).
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
        a_target.AddObjectToContainer(object, nullptr, 1, nullptr);
        equip_manager->EquipObject(&a_target, object, nullptr, 1, nullptr, false, true, false, true);
        ++sync.equipped;
    }

    for (RE::TESBoundObject* const object : current)
    {
        // Anything the target wears that the source no longer does comes off. The item stays in the
        // target's container: it is invisible inventory (the figure is dressed from the worn flags,
        // not from what it carries) and the set is bounded by the distinct items the player wore.
        if (!object || in(wanted, object))
            continue;
        if (equip_manager->UnequipObject(&a_target, object, nullptr, 1, nullptr, false, true, false, true))
            ++sync.removed;
    }

    return sync;
}

PLUGIN_NAMESPACE_END
