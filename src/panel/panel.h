//
// Created by AmazingBuff on 2026/09/28.
//

#pragma once

PLUGIN_NAMESPACE_BEGIN

class PanelMonitor
{
public:
    static PanelMonitor& instance();

    void install();
    bool is_menu_open() const;
private:
    class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(
            const RE::MenuOpenCloseEvent* event,
            RE::BSTEventSource<RE::MenuOpenCloseEvent>*) noexcept override;
    };
private:
    PanelMonitor();
    ~PanelMonitor();
private:
    std::atomic_bool m_panel_open;
    MenuSink m_menu_sink;
};

PLUGIN_NAMESPACE_END
