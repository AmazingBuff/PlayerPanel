//
// Created by AmazingBuff on 2026/09/28.
//

#pragma once

#include <string>
#include <string_view>
#include <vector>

PLUGIN_NAMESPACE_BEGIN

class PanelMonitor
{
public:
    struct StanceSnapshot
    {
        bool valid = false;
        bool drawn = false;
    };
    static PanelMonitor& instance();

    void install();
    bool is_menu_open() const;
    // World-frame weapon stance: the last unpaused read wins (the menu-event read loses to the
    // engine's pre-menu sheathe). Sheathed entries answer the plain standing idle.
    void observe_stance(bool weapon_drawn);
    [[nodiscard]] StanceSnapshot stance_at_open() const;
    // The name rule on top (user direction, sixth-round evidence): the demonstrated SEQUENCE's
    // last stable idle decides the stance better than the weaponState bit — the bit is false
    // while the draw animation is still playing (1HM_Equip spans it), so a player who draws and
    // opens the menu within the animation reads sheathed. The last world clip being a weapon
    // idle (an idle that is not the unarmed base family) is the honest "武器在手" answer.
    void observe_stance_from_clip();
    // World-frame animation observation, OAR-style (user direction): the clip NAME is the only
    // interface. The observer logs every clip change the world side shows (the demonstration
    // recording the panel's weapon-switch order is authored from), and the panel loads a clip
    // by feeding that name back through the same OAR query path any standing base uses — index,
    // replacement and variant decisions are OAR's own, never ours. The weight argument serves
    // only the crossfade measurement (how long the engine blends), not any decision.
    void observe_idle_clip(std::string_view name, float weight);
    // The stable idle the player was entering with: the first idle-NAMED clip found walking
    // the recent history backwards (the last clip is often Equip/Unequip/attack, which carry
    // no "idle"). Empty when the recent history has no idle at all.
    [[nodiscard]] std::string last_idle_clip() const;
    [[nodiscard]] float measured_crossfade_seconds() const { return m_crossfade_seconds; }
    // True exactly once for each time the inventory opens, however many callers ask. The studio restarts
    // its animation clocks on this, and it must not depend on seeing the close as well: the render-side
    // hook that would observe it does not run in every frame, while the menu event always arrives.
    bool consume_panel_opened();
    [[nodiscard]] std::uint64_t equipment_revision() const;
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
    // World-frame animation observations. Written on unpaused frames only, read by the panel.
    // Plain members: the render hook and the panel tick both run on the main thread.
    std::string m_idle_clip;
    // Recent clip history (chronological, capped). The stance read walks it backwards: when the
    // LAST clip is a transition/attack move (1HM_Equip, BFCO_Attack1 — no "idle" in the name),
    // the stance the player is ENTERING is the stable idle BEFORE it, which the last-clip rule
    // misses entirely (the tenth-round report: Equip/Unequip/attack entries all answered
    // standing).
    std::vector<std::string> m_clip_history;
    float m_crossfade_seconds = 0.0f;
    // Crossfade measurement: the fade is the span between the frame the strongest idle weight
    // dropped below 1.0 and the frame it returned there.
    float m_fade_elapsed = 0.0f;
    bool m_fading = false;
    MenuSink m_menu_sink;
    EquipmentSink m_equipment_sink;
    std::atomic_uint64_t m_equipment_revision{ 0 };
};

PLUGIN_NAMESPACE_END
