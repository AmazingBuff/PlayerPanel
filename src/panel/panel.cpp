//
// Created by AmazingBuff on 2026/09/28.
//

#include "panel/panel.h"

#include "RE/P/PlayerCharacter.h"
#include "RE/S/ScriptEventSourceHolder.h"
#include "RE/T/TESEquipEvent.h"

PLUGIN_NAMESPACE_BEGIN

RE::BSEventNotifyControl PanelMonitor::MenuSink::ProcessEvent(const RE::MenuOpenCloseEvent* event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) noexcept
{
    if (!event)
        return RE::BSEventNotifyControl::kContinue;
    // Only the inventory drives the panel; other menus (map,
    // skills, containers, ...) keep their own behavior.
    if (event->menuName == RE::InventoryMenu::MENU_NAME)
    {
        if (event->opening)
        {
            instance().m_panel_open.store(true, std::memory_order_relaxed);
            instance().m_panel_opened.store(true, std::memory_order_relaxed);
            // The entering stance is NOT sampled here: the engine clears weaponState before this
            // event fires (oar-idle3 verdict — the rising-edge read reported sheathed for a
            // drawn-then-opened player). observe_stance() holds the last unpaused world frame's
            // read, which IS the entering stance.
        }
        else
        {
            instance().m_panel_open.store(false, std::memory_order_relaxed);
        }
    }
    return RE::BSEventNotifyControl::kContinue;
}

PanelMonitor& PanelMonitor::instance()
{
    static PanelMonitor s_instance;
    return s_instance;
}

void PanelMonitor::install()
{
    if (RE::UI* ui = RE::UI::GetSingleton())
    {
        ui->AddEventSink(&m_menu_sink);
        logger::info("Menu sink installed");
    }
    if (RE::ScriptEventSourceHolder* events = RE::ScriptEventSourceHolder::GetSingleton())
        events->AddEventSink(&m_equipment_sink);
}

RE::BSEventNotifyControl PanelMonitor::EquipmentSink::ProcessEvent(RE::TESEquipEvent const* event,
    RE::BSTEventSource<RE::TESEquipEvent>*) noexcept
{
    if (event && event->actor && event->actor->IsPlayerRef())
        instance().m_equipment_revision.fetch_add(1, std::memory_order_release);
    return RE::BSEventNotifyControl::kContinue;
}

std::uint64_t PanelMonitor::equipment_revision() const
{
    return m_equipment_revision.load(std::memory_order_acquire);
}

bool PanelMonitor::is_menu_open() const
{
    return m_panel_open.load(std::memory_order_acquire);
}

PanelMonitor::StanceSnapshot PanelMonitor::stance_at_open() const
{
    if (!m_stance_valid.load(std::memory_order_acquire))
        return {};
    return { true, m_stance_at_open.load(std::memory_order_relaxed) };
}

void PanelMonitor::observe_stance(bool weapon_drawn)
{
    // Menu frames must not write: the engine froze weaponState for the menu session, so a menu
    // read says sheathed regardless of the entering stance. World frames own the signal.
    if (!m_panel_open.load(std::memory_order_acquire))
    {
        m_stance_at_open.store(weapon_drawn, std::memory_order_relaxed);
        m_stance_valid.store(true, std::memory_order_release);
    }
}

bool PanelMonitor::consume_panel_opened()
{
    return m_panel_opened.exchange(false, std::memory_order_acq_rel);
}

PanelMonitor::PanelMonitor() : m_panel_open(false), m_panel_opened(false)
{
}

PanelMonitor::~PanelMonitor()
{

}

PLUGIN_NAMESPACE_END
