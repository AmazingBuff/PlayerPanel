//
// Created by AmazingBuff on 2026/09/21.
//

#include "preview_actor.h"

#include "config/config.h"

#include <cmath>

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // Skyrim stores reference yaw in radians. The pinned source is the proof:
    // TESObjectREFR::GetHeadingAngle feeds the raw value of GetAngleZ() through rad_to_deg, and
    // TESObjectREFR::PlaceObjectAtMe passes GetAngle() to CreateReferenceAtLocation unchanged, so
    // GetAngle() and SetAngle() share one unit. See
    // extern/CommonLibSSE/src/RE/T/TESObjectREFR.cpp, TESObjectREFR::GetHeadingAngle.
    constexpr float Pi = 3.14159265358979323846f;

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

    // Places the preview PreviewDistance in front of the player and turns it back to face the
    // player. The yaw is read and written in radians and measured from +Y towards +X, so the
    // forward offset is (sin(yaw), cos(yaw)) and the preview's own yaw is yaw + Pi.
    void place_in_front(RE::TESObjectREFR& preview, RE::Actor const& player)
    {
        double const distance = Setting::instance().get_config().preview_distance;
        float const yaw = player.GetAngleZ();
        RE::NiPoint3 const player_position = player.GetPosition();
        RE::NiPoint3 const target{
            player_position.x + static_cast<float>(std::sin(yaw) * distance),
            player_position.y + static_cast<float>(std::cos(yaw) * distance),
            player_position.z
        };
        preview.SetPosition(target);
        preview.SetAngle(RE::NiPoint3{ 0.0f, 0.0f, yaw + Pi });
    }

    // Walks the player's own inventory changes and re-dresses the preview; the player's container is
    // only read, no entry is materialised into a copy and no equip event is sent or replayed.
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

        std::uint32_t added = 0;
        std::uint32_t refused = 0;
        for (RE::InventoryEntryData* entry : *changes->entryList)
        {
            if (!entry)
                continue;
            // Read the public member directly: Windows.h defines GetObject as an object-like macro.
            RE::TESBoundObject* const object = entry->object;
            if (!object || !entry->IsWorn())
                continue;
            if (preview.AddWornItem(object, 1, true, 0, 0))
                ++added;
            else
                ++refused;
        }

        if (refused > 0)
            logger::warn("Preview could not wear {} of {} worn items", refused, added + refused);
        else
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

    // Staged hide/dress/show so no partially dressed frame ever reaches the renderer.
    placed->Disable();
    place_in_front(*placed, *player);
    if (!sync_worn_equipment(*preview, *player))
    {
        destroy();
        set_state(State::kUnavailable, "worn equipment is unavailable for synchronization");
        return false;
    }
    placed->Enable(false);

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
