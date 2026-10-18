//
// Created by AmazingBuff on 2026/09/28.
//

#pragma once

#include <chrono>
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
    // The clip the player was IN when the inventory opened — the world's own decision, recorded
    // before the engine freezes the menu session. The panel replays it from local_time as its
    // entry action and then hands over to the idle sequence (round 18). Consumed once by the
    // first animation request of the session.
    struct EntryAction
    {
        bool valid = false;
        std::uint16_t index = 0xFFFF;
        float local_time = 0.0f;
        bool looping = false;
    };
    static PanelMonitor& instance();

    void install();
    bool is_menu_open() const;
    [[nodiscard]] EntryAction take_entry_action();
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
    // The TESEquipEvent sink moved to EquipmentMonitor (src/character), which
    // watches ANY registered actor; PanelMonitor consumes that stream through
    // an observer to keep the player-only equipment_revision semantics.
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
    // Crossfade measurement: the fade is the wall-clock span the strongest idle weight spends
    // below full. The observation cadence is every 10th world frame, so the span is measured with
    // a steady clock between observations — a fixed 1/60 s per sample measured the fade ~10x
    // short (round 16).
    std::chrono::steady_clock::time_point m_fade_sample{};
    float m_fade_elapsed = 0.0f;
    bool m_fading = false;
    EntryAction m_entry_action;
    MenuSink m_menu_sink;
    std::atomic_uint64_t m_equipment_revision{ 0 };
};

PLUGIN_NAMESPACE_END
