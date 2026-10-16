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
    [[nodiscard]] std::uint64_t equipment_revision() const;
    // The player's weapon stance as it was when the inventory OPENED. The engine sheathes the
    // weapon while the menu is up, so reads after the open edge report sheathed for a player
    // who entered the menu with the weapon drawn (oar-idle2 report, problem 2: every query of
    // the session said drawn=false). Sampled by the menu sink on the rising edge only.
    struct StanceSnapshot
    {
        bool valid = false;
        bool drawn = false;
    };
    [[nodiscard]] StanceSnapshot stance_at_open() const;
private:
    class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(
            const RE::MenuOpenCloseEvent* event,
            RE::BSTEventSource<RE::MenuOpenCloseEvent>*) noexcept override;
    };
    class EquipmentSink final : public RE::BSTEventSink<RE::TESEquipEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(RE::TESEquipEvent const* event,
            RE::BSTEventSource<RE::TESEquipEvent>*) noexcept override;
    };
private:
    PanelMonitor();
    ~PanelMonitor();
private:
    std::atomic_bool m_panel_open;
    std::atomic_bool m_panel_opened;
    std::atomic_bool m_stance_valid{ false };
    std::atomic_bool m_stance_at_open{ false };
    MenuSink m_menu_sink;
    EquipmentSink m_equipment_sink;
    std::atomic_uint64_t m_equipment_revision{ 0 };
};

PLUGIN_NAMESPACE_END
