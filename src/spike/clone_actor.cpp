#include "clone_actor.h"

#include <algorithm>
#include <cmath>
#include <numbers>

PLUGIN_NAMESPACE_BEGIN

namespace spike
{
    namespace
    {
        constexpr float SpawnDistance = 120.0f; // game units in front of the player
        // Frames to wait after PlaceObjectAtMe before touching the clone with
        // virtual calls; the engine finishes its secondary-base construction
        // and AI-process init asynchronously.
        constexpr std::uint32_t Grace_Frames = 60;

        std::vector<RE::BSGeometry*> collect_geometries(RE::NiAVObject* root)
        {
            std::vector<RE::BSGeometry*> result;
            if (!root)
                return result;
            RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* geometry) {
                result.push_back(geometry);
                return RE::BSVisit::BSVisitControl::kContinue;
            });
            return result;
        }

        void make_inert(RE::Actor* clone)
        {
            // No SetActorValue here: it dispatches through the ActorValueOwner
            // secondary base, whose vtable the engine populates long after the
            // actor's primary vtable and AI process are live (two in-game
            // crashes at call [rax+0x38], 60 frames apart, proved the
            // secondary-base window is not bounded by any practical grace
            // period). The duplicate already inherits the player's calm
            // values; kMovementBlocked is a plain flag write, no dispatch.
            clone->GetActorRuntimeData().boolFlags.set(RE::Actor::BOOL_FLAGS::kMovementBlocked);
        }

        // Body-worn mirroring following the order SKSE's own EquipItemEx uses:
        // AddObjectToContainer first, then equip through ActorEquipManager.
        // Calling AddWornItem with a base object that was never in the clone's
        // container crashes inside ExtraDataList::GetEnchantment on a garbage
        // extra-list pointer (documented engine behaviour, see the in-game
        // verified reference implementation). Only biped body slots are
        // mirrored; the worn flag also marks weapons, ammo and torches.
        void mirror_worn_equipment(RE::Actor* clone, RE::PlayerCharacter* player)
        {
            RE::InventoryChanges* changes = player->GetInventoryChanges(true);
            if (!changes || !changes->entryList)
            {
                logger::warn("[spike] player container changes unavailable; clone not dressed");
                return;
            }
            RE::ActorEquipManager* equip_manager = RE::ActorEquipManager::GetSingleton();
            if (!equip_manager)
            {
                logger::warn("[spike] equip manager unavailable; clone not dressed");
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
                bool body_worn = std::any_of(std::begin(Body_Slots), std::end(Body_Slots),
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
            logger::info("[spike] body-worn items mirrored: {}", added);
        }
    }

    CloneActor& CloneActor::instance()
    {
        static CloneActor s_instance;
        return s_instance;
    }

    void CloneActor::request_spawn()
    {
        m_request.store(Request::kSpawn, std::memory_order_release);
    }

    void CloneActor::request_despawn()
    {
        m_request.store(Request::kDespawn, std::memory_order_release);
    }

    void CloneActor::on_frame()
    {
        Request request = m_request.exchange(Request::kNone, std::memory_order_acq_rel);
        switch (request)
        {
            case Request::kSpawn:
                if (!m_actor)
                    spawn();
                break;
            case Request::kDespawn:
                if (m_actor)
                    despawn();
                break;
            case Request::kNone:
                break;
            default:
                break;
        }

        // The engine finishes constructing the clone's secondary bases (AI
        // process, ActorValueOwner vtables) and character controller a few
        // frames after PlaceObjectAtMe returns. Calling virtuals earlier blew
        // up inside SetActorValue (crash: call [rax+0x38] on a garbage
        // secondary-vtable). Wait out a fixed grace period first.
        if (m_actor && !m_dressed)
        {
            if (++m_frames_since_place >= Grace_Frames)
            {
                position_and_dress();
            }
        }
        else if (m_actor && m_dressed && m_geometries.empty())
        {
            if (RE::NiAVObject* root = m_actor->GetCurrent3D())
            {
                std::vector<RE::BSGeometry*> geometries = collect_geometries(root);
                if (!geometries.empty())
                {
                    m_geometries = std::move(geometries);
                    logger::info("[spike] collected {} geometries from the clone 3D", m_geometries.size());
                }
            }
        }
    }

    void CloneActor::spawn()
    {
        RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->GetActorBase() || !player->GetSequencer())
        {
            logger::warn("[spike] no player character or AI process; clone not spawned");
            return;
        }

        RE::TESNPC* playerBase = player->GetActorBase();
        RE::TESForm* duplicate = playerBase->CreateDuplicateForm(false, nullptr);
        auto* cloneBase = duplicate ? duplicate->As<RE::TESNPC>() : nullptr;
        if (!cloneBase)
        {
            logger::warn("[spike] CreateDuplicateForm failed");
            return;
        }
        cloneBase->faceNPC = playerBase;

        RE::NiPointer<RE::TESObjectREFR> placed = player->PlaceObjectAtMe(cloneBase, false);
        auto* clone = placed ? placed->As<RE::Actor>() : nullptr;
        if (!clone)
        {
            logger::warn("[spike] PlaceObjectAtMe failed or not an actor");
            return;
        }

        m_handle = RE::ObjectRefHandle(clone);
        m_actor = clone;
        m_frames_since_place = 0;

        // Let the engine initialize the clone's AI process at the placement
        // spot first; reposition and dress it next frame when the process
        // and character controller exist. Touching SetPosition/AddWornItem in
        // the same tick as placement crashed the game in the first run.
        logger::info("[spike] clone placed; positioning next frame");
    }

    void CloneActor::position_and_dress()
    {
        RE::Actor* clone = m_actor;
        RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
        if (!clone || !player || !clone->GetSequencer())
        {
            logger::warn("[spike] clone not ready for positioning; retrying");
            return;
        }

        // Place in front of the player, facing them. Facing uses the angle
        // convention of GetAngle/SetAngle (degrees, Z = yaw).
        RE::NiPoint3 playerPos = player->GetPosition();
        RE::NiPoint3 playerAngle = player->GetAngle();
        float yawRad = playerAngle.z * std::numbers::pi_v<float> / 180.0f;
        RE::NiPoint3 forward{ std::sin(yawRad), std::cos(yawRad), 0.0f };
        RE::NiPoint3 spawnPos = playerPos + forward * SpawnDistance;
        clone->SetPosition(spawnPos, true);
        RE::NiPoint3 toPlayer = playerPos - spawnPos;
        float facing = std::atan2(toPlayer.x, toPlayer.y) * 180.0f / std::numbers::pi_v<float>;
        clone->SetHeading(facing);

        make_inert(clone);
        mirror_worn_equipment(clone, player);

        m_dressed = true;
        logger::info("[spike] clone positioned at ({:.1f}, {:.1f}, {:.1f}) and dressed", spawnPos.x, spawnPos.y,
            spawnPos.z);
    }

    void CloneActor::despawn()
    {
        if (m_actor)
        {
            m_actor->Disable();
            m_actor->SetDelete(true);
        }
        m_actor = nullptr;
        m_handle.reset();
        m_geometries.clear();
        m_frames_since_place = 0;
        logger::info("[spike] clone despawned");
    }
}

PLUGIN_NAMESPACE_END
