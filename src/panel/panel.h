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
    // True exactly once for each time the inventory opens, however many callers ask. The studio restarts
    // its animation clocks on this, and it must not depend on seeing the close as well: the render-side
    // hook that would observe it does not run in every frame, while the menu event always arrives.
    bool consume_panel_opened();
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
    std::atomic_bool m_panel_opened;
    MenuSink m_menu_sink;
};

PLUGIN_NAMESPACE_END
