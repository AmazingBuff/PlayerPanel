//
// Created by AmazingBuff on 2026/10/18.
//

#include "equipment_monitor.h"

#include "RE/B/BSTEvent.h"
#include "RE/F/FormTypes.h"
#include "RE/P/PlayerCharacter.h"
#include "RE/S/ScriptEventSourceHolder.h"

#include <algorithm>
#include <vector>

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // TESEquipEvent::actor is the equipping reference. Character and
    // PlayerCharacter share FormType::ActorCharacter; everything else that
    // fires this event without that form type cannot carry the Actor state
    // consumers want, so it is dropped here.
    RE::Actor* equip_event_actor(const RE::TESEquipEvent& a_event)
    {
        RE::TESObjectREFR* const refr = a_event.actor.get();
        if (!refr || !refr->Is(RE::FormType::ActorCharacter))
            return nullptr;
        return static_cast<RE::Actor*>(refr);
    }
}

EquipmentMonitor& EquipmentMonitor::instance()
{
    static EquipmentMonitor s_instance;
    return s_instance;
}

void EquipmentMonitor::install()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_installed)
        return;
    if (RE::ScriptEventSourceHolder* events = RE::ScriptEventSourceHolder::GetSingleton())
    {
        events->AddEventSink(&m_sink);
        m_installed = true;
        logger::info("Equipment monitor installed (per-actor equip watch)");
    }
}

void EquipmentMonitor::watch(RE::Actor& a_actor)
{
    const std::uint32_t key = actor_key(a_actor);
    std::lock_guard<std::mutex> lock(m_mutex);
    // The player is implicitly watched (the panel's entering-stance equipment
    // read depends on the stream even when no clone exists); a Watched entry
    // for the player is harmless but pointless.
    if (a_actor.IsPlayerRef())
        return;
    m_watched.try_emplace(key, Watched{});
}

void EquipmentMonitor::unwatch(RE::Actor& a_actor)
{
    const std::uint32_t key = actor_key(a_actor);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_watched.erase(key);
}

bool EquipmentMonitor::watching(const RE::Actor& a_actor) const
{
    const std::uint32_t key = actor_key(a_actor);
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_watched.contains(key) || a_actor.IsPlayerRef();
}

std::uint64_t EquipmentMonitor::revision(const RE::Actor& a_actor) const
{
    const std::uint32_t key = actor_key(a_actor);
    std::lock_guard<std::mutex> lock(m_mutex);
    if (const auto it = m_watched.find(key); it != m_watched.end())
        return it->second.revision;
    // The player has no Watched entry (implicitly watched, never registered)
    // and the count lives in the map only for registered actors, so an
    // unregistered read is simply "no changes observed".
    return 0;
}

std::uint32_t EquipmentMonitor::actor_key(const RE::Actor& a_actor)
{
    // Local references (dynamic spawns, ff-formIDs) have no stable base ID, but
    // the runtime formID is unique for the reference's lifetime, which is the
    // lifetime this map cares about. Reused slots after a free are acceptable:
    // the revision is relative, every consumer re-reads it from 0.
    return a_actor.GetFormID();
}

void EquipmentMonitor::add_observer(Observer a_observer)
{
    if (!a_observer)
        return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_observers.push_back(std::move(a_observer));
}

void EquipmentMonitor::clear_observers()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_observers.clear();
}

RE::BSEventNotifyControl EquipmentMonitor::Sink::ProcessEvent(RE::TESEquipEvent const* a_event,
    RE::BSTEventSource<RE::TESEquipEvent>*) noexcept
{
    if (a_event)
        instance().on_equip_event(*a_event);
    return RE::BSEventNotifyControl::kContinue;
}

void EquipmentMonitor::on_equip_event(const RE::TESEquipEvent& a_event)
{
    // The engine dispatches equip events during the actor's own update. Copy
    // what the handlers need under the lock, run the handlers outside it - an
    // observer that calls back into watch()/revision() must not deadlock, and
    // a handler that throws must not leave the sink in a broken state.
    RE::Actor* actor = nullptr;
    std::vector<Observer> observers;
    bool watched = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_installed)
            return;
        actor = equip_event_actor(a_event);
        if (!actor)
            return;
        const bool player = actor->IsPlayerRef();
        const std::uint32_t key = actor_key(*actor);
        const auto it = m_watched.find(key);
        if (it != m_watched.end())
        {
            it->second.revision += 1;
            watched = true;
        }
        if (player)
            watched = true;  // the player is implicitly always watched
        if (watched && !m_observers.empty())
            observers = m_observers;
    }
    if (!watched)
        return;
    for (Observer& observer : observers)
        observer(*actor, a_event);
}

PLUGIN_NAMESPACE_END
