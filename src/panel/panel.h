//
// Created by AmazingBuff on 2026/09/28.
//

#pragma once

#include <string>
#include <string_view>

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
    // Per-frame last-known weapon stance, sampled on every UNPAUSED world frame by the render
    // hook. The engine clears weaponState before the menu-open event fires (oar-idle3 verdict:
    // the rising-edge snapshot read sheathed for a player who drew-then-opened), so the menu
    // event cannot sample the entering stance; the last unpaused frame's read is the stance the
    // player entered the menu with ("武器已在手上"). Sheathed entries stay sheathed — the panel
    // answers a sheathed player with the plain standing idle (fifth-round report problem 1).
    void observe_stance(bool weapon_drawn);
    // The entering stance: the last unpaused observation, valid once anything was observed.
    [[nodiscard]] StanceSnapshot stance_at_open() const;
    // The running clip the world side showed on the last unpaused frames: name of the strongest
    // idle-named clip and its blend weight. The panel prefers THIS clip for the weapon stance
    // (fifth-round report problem 2: the game's own choice is the answer, the OAR sequence query
    // is only the fallback), and the weight history measures the engine's crossfade length
    // (problem 3): the fade is the span weight spends below 1.0 between two full-weight clips.
    void observe_idle_clip(std::string_view name, float weight);
    [[nodiscard]] std::string const& last_idle_clip() const { return m_idle_clip; }
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
