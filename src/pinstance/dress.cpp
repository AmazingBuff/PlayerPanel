//
// Build-window dressing: mirror the player's body-worn equipment onto the
// clone. (2026-10-05 src migration: split out of tools/m0_proto, behavior byte-identical)
//

#include "pinstance/pinstance_detail.h"

PLUGIN_NAMESPACE_BEGIN
// Body-worn mirroring in the order SKSE's EquipItemEx uses (spike
// verified: AddObjectToContainer first, then ActorEquipManager;
// AddWornItem without container membership crashed in
// ExtraDataList::GetEnchantment). Only biped body slots.
void mirror_worn_equipment(RE::Actor* clone, RE::PlayerCharacter* player)
{
    RE::InventoryChanges* changes = player->GetInventoryChanges(true);
    RE::ActorEquipManager* equip_manager = RE::ActorEquipManager::GetSingleton();
    if (!changes || !changes->entryList || !equip_manager)
    {
        logger::warn("Proto P dressing unavailable (changes={} equip={})", static_cast<void*>(changes),
            static_cast<void*>(equip_manager));
        return;
    }

    using BipedSlot = RE::BGSBipedObjectForm::BipedObjectSlot;
    constexpr BipedSlot Body_Slots[] = {
        BipedSlot::kBody,     BipedSlot::kHead,     BipedSlot::kHands,
        BipedSlot::kForearms, BipedSlot::kAmulet,   BipedSlot::kRing,
        BipedSlot::kFeet,     BipedSlot::kCalves,   BipedSlot::kTail,
        BipedSlot::kLongHair, BipedSlot::kCirclet,  BipedSlot::kEars,
        BipedSlot::kModMouth, BipedSlot::kModNeck,  BipedSlot::kModChestPrimary,
        BipedSlot::kModChestSecondary, BipedSlot::kModShoulder, BipedSlot::kModArmLeft,
        BipedSlot::kModArmRight, BipedSlot::kModLegRight, BipedSlot::kModLegLeft,
        BipedSlot::kModFaceJewelry,
    };

    std::uint32_t added = 0;
    for (RE::InventoryEntryData* entry : *changes->entryList)
    {
        if (!entry)
            continue;
        RE::TESBoundObject* object = entry->object;  // GetObject is macro-clashed by Windows.h
        if (!object || !entry->IsWorn())
            continue;
        RE::BGSBipedObjectForm* biped = object->As<RE::BGSBipedObjectForm>();
        if (!biped)
            continue;
        const bool body_worn = std::any_of(std::begin(Body_Slots), std::end(Body_Slots),
            [biped](BipedSlot slot) { return biped->HasPartOf(slot); });
        if (!body_worn)
            continue;

        clone->AddObjectToContainer(object, nullptr, 1, nullptr);
        equip_manager->EquipObject(clone, object, nullptr, 1, nullptr,
            false,  // a_queueEquip: applied in this call
            true,   // a_forceEquip: the clone has no AI to choose
            false,  // a_playSounds: silent
            true);  // a_applyNow: dressed immediately
        ++added;
    }
    logger::info("Proto P body-worn items mirrored: {}", added);
}
PLUGIN_NAMESPACE_END
