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
            // Keep the clone alive in the high-priority update radius but stop
            // it from wandering, combat or dialogue.
            clone->SetActorValue(RE::ActorValue::kAggression, 0.0f);
            clone->SetActorValue(RE::ActorValue::kConfidence, 0.0f);
            clone->SetActorValue(RE::ActorValue::kAssistance, 0.0f);
            clone->GetActorRuntimeData().boolFlags.set(RE::Actor::BOOL_FLAGS::kMovementBlocked);
        }

        // worn/equipped mirroring that cannot throw the game down: skips
        // weapons (dual-wield slot logic is crash-prone off the equip UI) and
        // only dresses armor/clothing, which is what the spike needs to see.
        void mirror_worn_equipment(RE::Actor* clone, RE::PlayerCharacter* player)
        {
            auto* changes = player->GetInventoryChanges();
            if (!changes || !changes->entryList)
                return;
            std::uint32_t worn = 0;
            std::uint32_t failed = 0;
            for (RE::InventoryEntryData* entry : *changes->entryList)
            {
                if (!entry || !entry->IsWorn())
                    continue;
                RE::TESBoundObject* object = entry->object;  // GetObject is macro-clashed by Windows.h
                if (!object)
                    continue;
                auto* armor = object->As<RE::TESObjectARMO>();
                if (!armor)
                    continue;  // spike scope: armor/clothing only
                ++worn;
                if (!clone->AddWornItem(object, 1, true, 0, 0))
                    ++failed;
            }
            logger::info("[spike] worn armor mirrored: {} ok, {} failed", worn - failed, failed);
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

        // The 3D tree appears a few frames after placement and after the worn
        // items are equipped; collect geometries once the root exists and at
        // least one geometry is present.
        if (m_actor && !m_dressed)
        {
            position_and_dress();
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
        logger::info("[spike] clone despawned");
    }
}

PLUGIN_NAMESPACE_END
