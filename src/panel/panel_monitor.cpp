//
// Created by AmazingBuff on 2026/09/28.
//
#include "panel/panel_monitor.h"
PLUGIN_NAMESPACE_BEGIN
PanelMonitor& PanelMonitor::instance() { static PanelMonitor s_instance; return s_instance; }
void PanelMonitor::install()
{
    if (!m_installed)
        if (RE::UI* ui = RE::UI::GetSingleton())
        {
            ui->AddEventSink(&m_menu_sink);
            m_installed = true;
            logger::info("Panel menu sink installed");
        }
}
bool PanelMonitor::is_menu_open() const { return m_panel_open.load(std::memory_order_acquire); }
std::uint64_t PanelMonitor::revision() const { return m_revision.load(std::memory_order_acquire); }
RE::BSEventNotifyControl PanelMonitor::MenuSink::ProcessEvent(RE::MenuOpenCloseEvent const* event,
    RE::BSTEventSource<RE::MenuOpenCloseEvent>*) noexcept
{
    if (event && event->menuName == RE::InventoryMenu::MENU_NAME)
    {
        PanelMonitor& monitor = instance();
        monitor.m_panel_open.store(event->opening, std::memory_order_release);
        monitor.m_revision.fetch_add(1, std::memory_order_release);
        logger::info("Panel menu opening={} revision={}", event->opening, monitor.revision());
    }
    return RE::BSEventNotifyControl::kContinue;
}
PLUGIN_NAMESPACE_END
