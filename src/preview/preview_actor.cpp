//
// Created by AmazingBuff on 2026/09/21.
//

#include "preview_actor.h"

#include "config/config.h"

#include <algorithm>
#include <cmath>

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // Skyrim stores reference yaw in radians. The pinned source is the proof:
    // TESObjectREFR::GetHeadingAngle feeds the raw value of GetAngleZ() through rad_to_deg, and
    // TESObjectREFR::PlaceObjectAtMe passes GetAngle() to CreateReferenceAtLocation unchanged, so
    // GetAngle() and SetAngle() share one unit. See
    // extern/CommonLibSSE/src/RE/T/TESObjectREFR.cpp, TESObjectREFR::GetHeadingAngle.
    constexpr char const* state_name(PreviewActor::State state)
    {
        switch (state)
        {
        case PreviewActor::State::kIdle:
            return "idle";
        case PreviewActor::State::kReady:
            return "ready";
        case PreviewActor::State::kUnavailable:
            return "unavailable";
        }
        return "unknown";
    }

    // Places the preview PreviewDistance BEHIND the player and turns it to face the player's back.
    // The duplicate is a real reference in the world only because the engine loads and submits its
    // skinned 3D through the ordinary render path (a never-submitted skin carries a zero bone-matrix
    // count and the panel would lose the body), so it is kept where the player cannot see it: behind
    // the view direction, culled by the renderer within ~0.2s of the first submission. The yaw is
    // read and written in radians and measured from +Y towards +X, so the offset behind the player's
    // facing is -(sin(yaw), cos(yaw)) and the preview's own yaw is yaw (facing the player's back).
    void place_in_front(RE::TESObjectREFR& preview, RE::Actor const& player)
    {
        double const distance = Setting::instance().get_config().preview_distance;
        float const yaw = player.GetAngleZ();
        RE::NiPoint3 const player_position = player.GetPosition();
        RE::NiPoint3 const target{
            player_position.x - static_cast<float>(std::sin(yaw) * distance),
            player_position.y - static_cast<float>(std::cos(yaw) * distance),
            player_position.z
        };
        preview.SetPosition(target);
        preview.SetAngle(RE::NiPoint3{ 0.0f, 0.0f, yaw });
    }

    // Walks the player's own inventory changes and re-dresses the preview. The player's container is
    // only read; the preview's copy is added and equipped through the engine's own equip path, so it
    // carries no enchanted-instance data from the player's worn items (only the base object is worn).
    // Only what the biped form says is actually worn on the body is mirrored - the inventory's worn
    // flags also mark weapons, ammo, torches and lights, which must not appear on the preview (the
    // ghost-weapon report).
    bool sync_worn_equipment(RE::Actor& preview, RE::Actor& player)
    {
        // a_noInit avoids creating the player's container changes while only reading them.
        RE::InventoryChanges* const changes = player.GetInventoryChanges(true);
        if (!changes)
        {
            logger::warn("Player container changes are unavailable; the preview is not dressed");
            return false;
        }
        if (!changes->entryList)
            return true;

        // The engine's equip path (the AddWornItem virtual, its true name EquipManager::EquipItem in
        // the SKSE sources) looks the item's inventory entry and its extra data up in the actor's own
        // container: equipping a base object that was never added crashes inside
        // ExtraDataList::GetEnchantment on a garbage extra-list pointer. The fix is the order SKSE's
        // own EquipItemEx uses: add the object to the container first, then equip through
        // ActorEquipManager, which resolves the entry and its extra data itself.
        RE::ActorEquipManager* const equip_manager = RE::ActorEquipManager::GetSingleton();
        if (!equip_manager)
        {
            logger::warn("Actor equip manager is unavailable; the preview is not dressed");
            return false;
        }

        std::uint32_t added = 0;
        // The biped slots a portrait mirrors: the armour/clothing slots plus hair and circlet. The
        // weapon-bearing mod slots are deliberately absent: measured in game, the engine equips the
        // bow on kModBack, one-handed weapons on the pelvis slots and the torch on kModMisc1, so
        // mirroring those slots is what painted weapons, a shield and a bow onto a preview whose
        // player carried none. Clothing mods that claim a weapon slot are rarer than every weapon
        // mod, so the portrait errs toward showing exactly the body.
        using BipedSlot = RE::BGSBipedObjectForm::BipedObjectSlot;
        constexpr BipedSlot Mirrored_Slots[] = {
            BipedSlot::kBody,       BipedSlot::kHead,       BipedSlot::kHands,
            BipedSlot::kForearms,   BipedSlot::kAmulet,     BipedSlot::kRing,
            BipedSlot::kFeet,       BipedSlot::kCalves,     BipedSlot::kTail,
            BipedSlot::kLongHair,   BipedSlot::kCirclet,    BipedSlot::kEars,
            BipedSlot::kModMouth,   BipedSlot::kModNeck,    BipedSlot::kModChestPrimary,
            BipedSlot::kModChestSecondary, BipedSlot::kModShoulder, BipedSlot::kModArmLeft,
            BipedSlot::kModArmRight, BipedSlot::kModLegRight, BipedSlot::kModLegLeft,
            BipedSlot::kModFaceJewelry,
        };

        for (RE::InventoryEntryData* entry : *changes->entryList)
        {
            if (!entry)
                continue;
            // Read the public member directly: Windows.h defines GetObject as an object-like macro.
            RE::TESBoundObject* const object = entry->object;
            if (!object || !entry->IsWorn())
                continue;

            // Only objects that declare one of the mirrored biped slots are body-worn; a worn flag
            // on a weapon or a torch does not make it part of the outfit. The check is on the form
            // itself, so the exact worn variant (left/right hand) does not matter.
            RE::BGSBipedObjectForm* const biped = object->As<RE::BGSBipedObjectForm>();
            if (!biped)
                continue;
            bool const mirrored = std::any_of(std::begin(Mirrored_Slots), std::end(Mirrored_Slots),
                [biped](BipedSlot slot) { return biped->HasPartOf(slot); });
            if (!mirrored)
                continue;

            preview.AddObjectToContainer(object, nullptr, 1, nullptr);
            equip_manager->EquipObject(&preview, object, nullptr, 1, nullptr,
                false,  // a_queueEquip: applied in this call, not queued
                true,   // a_forceEquip: the preview has no AI to choose
                false,  // a_playSounds: a silent preview
                true);  // a_applyNow: dressed before the staged enable
            ++added;
        }

        logger::info("Preview wears {} worn items", added);
        return true;
    }
}

PreviewActor& PreviewActor::instance()
{
    static PreviewActor s_instance;
    return s_instance;
}

PreviewActor::PreviewActor() : m_handle(), m_ref_base(nullptr), m_state(State::kIdle), m_toggle_requested(false)
{
}

void PreviewActor::request_toggle()
{
    m_toggle_requested = true;
}

void PreviewActor::on_frame()
{
    try
    {
        instance().service_request();
    }
    catch (...)
    {
        try
        {
            instance().destroy();
            logger::error("Preview toggle failed; the preview state was reset");
        }
        catch (...) {}
    }
}

void PreviewActor::destroy()
{
    if (m_handle)
    {
        RE::NiPointer<RE::TESObjectREFR> reference = m_handle.get();
        if (reference)
        {
            reference->Disable();
            reference->SetDelete(true);
        }
        else
        {
            logger::warn("Preview reference no longer resolves; its handle is discarded");
        }
    }

    // The duplicated base record is deliberately left to the engine: its ownership is one of the
    // unverified M1 questions, so this batch releases the reference and records the question.
    m_handle.reset();
    m_ref_base = nullptr;
    m_toggle_requested = false;
    set_state(State::kIdle, "preview released");
}

PreviewActor::State PreviewActor::state() const
{
    return m_state;
}

RE::TESObjectREFR* PreviewActor::current_reference() const
{
    if (m_state != State::kReady || !m_handle)
        return nullptr;

    RE::NiPointer<RE::TESObjectREFR> reference = m_handle.get();
    return reference.get();
}

void PreviewActor::service_request()
{
    if (!m_toggle_requested)
        return;
    m_toggle_requested = false;
    if (m_state == State::kReady)
        destroy();
    else
        create();
}

bool PreviewActor::create()
{
    RE::PlayerCharacter* const player = RE::PlayerCharacter::GetSingleton();
    if (!player)
    {
        set_state(State::kUnavailable, "no player character in this session");
        return false;
    }

    RE::TESNPC* const source = player->GetActorBase();
    if (!source)
    {
        set_state(State::kUnavailable, "the player base record is unavailable");
        return false;
    }

    RE::TESForm* const copy = source->CreateDuplicateForm(true, nullptr);
    RE::TESNPC* const duplicate = copy ? copy->As<RE::TESNPC>() : nullptr;
    if (!duplicate)
    {
        set_state(State::kUnavailable, "duplicating the player base record failed");
        return false;
    }
    // FaceGen data and the record's own sub-arrays stay owned by the player's base record.
    duplicate->faceNPC = source;
    // The duplicate also carries its base's default outfit, which is not the player's equipment: a
    // placed preview was seen wearing an unrecognised daedric sword and scabbard. Null the outfit
    // before placement so the only equipment the preview ever wears is what sync_worn_equipment
    // applies from the player's own worn list.
    duplicate->defaultOutfit = nullptr;

    // a_forcePersist=false keeps the preview out of the save game.
    RE::NiPointer<RE::TESObjectREFR> placed = player->PlaceObjectAtMe(duplicate, false);
    if (!placed)
    {
        set_state(State::kUnavailable, "placing the preview reference failed");
        return false;
    }

    m_handle = placed->GetHandle();
    m_ref_base = duplicate;

    RE::Actor* const preview = placed->As<RE::Actor>();
    if (!preview || !m_handle)
    {
        destroy();
        set_state(State::kUnavailable, "the placed reference is not a usable actor");
        return false;
    }

    // The preview stays enabled: the placed reference's initial 3D attach is queued by the placement,
    // and a disable placed in the same frame cancels it - an enable afterwards does not re-queue it,
    // so the actor would never load a 3D root and the panel would have nothing to draw. The equips
    // below complete synchronously, well before the asynchronous 3D build, so the first 3D frame is
    // already fully dressed and no partially dressed frame reaches the renderer. The world copy is
    // kept out of sight by the renderer's per-frame cull instead.
    place_in_front(*placed, *player);
    if (!sync_worn_equipment(*preview, *player))
    {
        destroy();
        set_state(State::kUnavailable, "worn equipment is unavailable for synchronization");
        return false;
    }

    set_state(State::kReady, "preview placed, dressed and enabled");
    return true;
}

void PreviewActor::set_state(State state, std::string_view reason)
{
    if (m_state == state)
        return;
    m_state = state;
    logger::info("Preview state {}: {}", state_name(state), reason);
}

PLUGIN_NAMESPACE_END
