//
// Created by AmazingBuff on 2026/09/28.
//

#include "panel/panel.h"

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
            instance().m_panel_open.store(true, std::memory_order_relaxed);
        else
            instance().m_panel_open.store(false, std::memory_order_relaxed);
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
}

bool PanelMonitor::is_menu_open() const
{
    return m_panel_open.load(std::memory_order_acquire);
}

PanelMonitor::PanelMonitor() : m_panel_open(false)
{
}

PanelMonitor::~PanelMonitor()
{

}

PLUGIN_NAMESPACE_END
