//
// Created by AmazingBuff on 2026/10/18.
//

#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "RE/N/NiSmartPointer.h"
#include "RE/T/TESEquipEvent.h"

PLUGIN_NAMESPACE_BEGIN

// Equip observation for ANY registered actor, not just the player (moved out of
// PanelMonitor, whose sink answered IsPlayerRef only). The engine fires
// TESEquipEvent on the ScriptEventSourceHolder for every actor that equips or
// unequips, so the watch is a per-actor revision counter over the same stream.
// Consumers poll the revision (the panel's per-frame request loop) or register
// an observer that fires once per matching change.
class EquipmentMonitor
{
public:
    using Observer = std::function<void(RE::TESObjectREFR& a_actor, const RE::TESEquipEvent& a_event)>;

    static EquipmentMonitor& instance();

    // Installs the ScriptEventSourceHolder sink (idempotent). Called from
    // PanelMonitor::install so the wiring stays where it is today.
    void install();

    // Begin watching an actor's equip stream. The player is always watched
    // (the panel's entering-stance equipment read depends on it), so callers
    // never register the player here.
    void watch(RE::Actor& a_actor);
    void unwatch(RE::Actor& a_actor);
    [[nodiscard]] bool watching(const RE::Actor& a_actor) const;

    // Monotonic per-actor change count; 0 when the actor is not watched. The
    // revision jumps on every equip/unequip event, equipped or not - the
    // consumer compares values, it does not interpret them.
    [[nodiscard]] std::uint64_t revision(const RE::Actor& a_actor) const;

    // A stable per-actor key for revision bookkeeping across respawns (the
    // engine can free and recreate a reference between sessions while the
    // Actor* slot is reused).
    [[nodiscard]] static std::uint32_t actor_key(const RE::Actor& a_actor);

    void add_observer(Observer a_observer);
    void clear_observers();

private:
    class Sink final : public RE::BSTEventSink<RE::TESEquipEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(RE::TESEquipEvent const* a_event,
            RE::BSTEventSource<RE::TESEquipEvent>*) noexcept override;
    };

    struct Watched
    {
        std::uint64_t revision = 0;
    };

    EquipmentMonitor() = default;
    ~EquipmentMonitor() = default;

    void on_equip_event(const RE::TESEquipEvent& a_event);

private:
    Sink m_sink;
    bool m_installed = false;
    // Guarded: the event stream runs on whatever thread the engine dispatches
    // script events on; watch/unwatch run on the main thread.
    mutable std::mutex m_mutex;
    std::unordered_map<std::uint32_t, Watched> m_watched;
    std::vector<Observer> m_observers;
};

PLUGIN_NAMESPACE_END
