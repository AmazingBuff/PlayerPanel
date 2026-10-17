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
    // The demonstrated sequence decides at READ time (sixth/seventh round direction: the name
    // is the interface). Walk the recent clip history BACKWARDS: the last clip is often a
    // transition/attack move with no "idle" in its name (1HM_Equip, 1HM_Unequip,
    // BFCO_Attack1 — the tenth-round report), and the stance the player is entering is the
    // stable idle BEFORE it. The first idle-named clip found from the tail answers; a
    // weapon/hand idle (not the unarmed base family) promotes drawn=true. The weaponState bit
    // keeps answering the sheathed side.
    for (auto it = m_clip_history.rbegin(); it != m_clip_history.rend(); ++it)
    {
        std::string lowered;
        lowered.reserve(it->size());
        for (unsigned char c : *it)
            lowered.push_back(static_cast<char>(std::tolower(c)));
        if (lowered.find("idle") == std::string::npos)
            continue;  // transition/attack/locomotion move — keep looking back
        const bool base_idle = lowered.find("mt_idle") != std::string::npos ||
            lowered.find("sneakmtidle") != std::string::npos ||
            lowered.find("mt_sprint") != std::string::npos ||
            lowered.find("mt_walk") != std::string::npos ||
            lowered.find("mt_run") != std::string::npos ||
            lowered.find("mt_jump") != std::string::npos ||
            lowered.find("mag_") != std::string::npos;
        if (base_idle)
            break;  // reached the unarmed base idle: the sequence rooted in the sheathed stance
        return { true, true };
    }
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
        // Keep the recent order for the stance read: the last clip may be a transition/attack
        // move whose "current stance" is the stable idle before it.
        m_clip_history.emplace_back(m_idle_clip);
        if (m_clip_history.size() > 8)
            m_clip_history.erase(m_clip_history.begin());
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

void PanelMonitor::observe_stance_from_clip()
{
    // Kept for the log trail only — the decision moved to the read side (stance_at_open): the
    // write-side order races the engine's own pre-menu sheathe. The open input clears the
    // weaponState bit AND fires the menu event in the same frame, and the render hook can run
    // one frame after that with menu_open still unset — the bit's last write would be false
    // even though the world spent the last seconds showing a weapon idle. Reading the recorded
    // clip name at decision time cannot lose that race.
}

std::string PanelMonitor::last_idle_clip() const
{
    // Same backward walk as the stance read, with one sequence-semantics skip: a START-named
    // idle is a one-shot entry member (Equip → Start → Loop). Once an attack or an Unequip has
    // happened, the Start the walk meets first has already played out — the world was dwelling
    // in the Loop by then (eleventh-round report: attack/Unequip entries landed on Start).
    // Skip Start-named members and answer the Loop the sequence settled into; when the walk
    // finds no Loop either (the sequence had not finished), the entry move itself is the best
    // answer and falls through to the plain idle rule below.
    std::string_view first_plain_idle;
    for (auto it = m_clip_history.rbegin(); it != m_clip_history.rend(); ++it)
    {
        std::string lowered;
        lowered.reserve(it->size());
        for (unsigned char c : *it)
            lowered.push_back(static_cast<char>(std::tolower(c)));
        if (lowered.find("idle") == std::string::npos)
            continue;
        const bool start_member = lowered.find("start") != std::string::npos;
        if (!start_member)
            return *it;
        if (first_plain_idle.empty())
            first_plain_idle = *it;  // remember the Start in case the whole history is entry moves
    }
    return std::string(first_plain_idle);
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
