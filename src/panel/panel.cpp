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

void PanelMonitor::observe_idle_clip(std::string_view name, float weight)
{
    if (m_panel_open.load(std::memory_order_acquire))
        return;  // world-frame observation only
    // OAR-style (user direction): the NAME is the interface. One line per clip change — this is
    // the recording the panel's weapon-switch sequence is authored from. Weight never decides
    // anything here; it only feeds the crossfade measurement below.
    if (!name.empty() && name != m_idle_clip)
    {
        logger::info("SCOPY WORLD clip '{}'", name);
        m_idle_clip.assign(name);
    }
    // The engine's crossfade is the span the strongest clip weight spends below 1.0 between two
    // full-weight clips (oar-idle3 fifth round, problem 3: the panel's blend must match the
    // game, not a picked constant). Frame-time integration over the below-full span.
    constexpr float frame_seconds = 1.0f / 60.0f;  // world-frame cadence; a coarse clock is fine
    if (weight < 0.999f)
    {
        m_fading = true;
        m_fade_elapsed += frame_seconds;
    }
    else if (m_fading)
    {
        m_fading = false;
        if (m_fade_elapsed > 0.05f && m_fade_elapsed < 2.0f)
        {
            m_crossfade_seconds = m_fade_elapsed;
            logger::info("SCOPY PANEL measured world crossfade {:.2f}s (clip '{}')",
                m_crossfade_seconds, m_idle_clip);
        }
        m_fade_elapsed = 0.0f;
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
