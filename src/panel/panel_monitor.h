//
// Created by AmazingBuff on 2026/09/28.
//
#pragma once
#include <atomic>
#include <cstdint>
PLUGIN_NAMESPACE_BEGIN
class PanelMonitor
{
public:
    static PanelMonitor& instance();
    void install();
    [[nodiscard]] bool is_menu_open() const;
    [[nodiscard]] std::uint64_t revision() const;
private:
    class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(RE::MenuOpenCloseEvent const*,
            RE::BSTEventSource<RE::MenuOpenCloseEvent>*) noexcept override;
    };
    std::atomic_bool m_panel_open{ false };
    std::atomic_uint64_t m_revision{ 0 };
    bool m_installed = false;
    MenuSink m_menu_sink;
};
PLUGIN_NAMESPACE_END
