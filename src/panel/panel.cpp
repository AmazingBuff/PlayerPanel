//
// Created by AmazingBuff on 2026/09/28.
//

#include "panel/panel.h"

#include "character/animation_source.h"
#include "oar/query_policy.h"

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
            // The open edge is the last chance to see the move the player is IN: the periodic
            // world-frame recorder runs every 10th frame, so a short draw/sheathe move could be
            // missed and the stance read would answer from the previous idle (round 17). Sample
            // before the panel flag is set, so observe_idle_clip still accepts world frames.
            if (RE::PlayerCharacter* const player = RE::PlayerCharacter::GetSingleton())
            {
                const RunningIdleSample sample = strongest_idle_clip(*player);
                if (!sample.any_name.empty())
                    instance().observe_idle_clip(sample.any_name,
                        sample.any_weight > 0.0f ? sample.any_weight : sample.weight);
                // The world's own decision for the entry action: the running binding and its
                // phase, from the strongest clip that is NOT a hand idle (the entry drives the
                // whole body). The panel replays it from that phase instead of re-asking OAR
                // with a frozen menu state (round 18).
                if (sample.body_index != 0xFFFF)
                {
                    instance().m_entry_action.valid = true;
                    instance().m_entry_action.index = sample.body_index;
                    instance().m_entry_action.local_time = sample.body_time;
                    instance().m_entry_action.looping = sample.body_looping;
                }
            }
            instance().m_panel_open.store(true, std::memory_order_relaxed);
            instance().m_panel_opened.store(true, std::memory_order_relaxed);
            // The weaponState BIT is not sampled here: the engine clears it before this event
            // fires (oar-idle3 verdict — the rising-edge read reported sheathed for a
            // drawn-then-opened player). The clip name recorded above decides the stance.
        }
        else
        {
            instance().m_panel_open.store(false, std::memory_order_relaxed);
            instance().m_entry_action = {};  // a closed session never replays a stale entry move
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
    // is the interface). Walk the recent clip history BACKWARDS. A draw/sheathe move is
    // decisive on its own: the engine clears weaponState before the menu event and freezes it
    // for the whole menu session, so entering during 1HM_Equip used to answer sheathed (and show
    // MT_Idle) while entering during 1HM_Unequip answered drawn (and showed a weapon idle) —
    // round 17. An idle decides by family; anything else (attack, locomotion) keeps looking back.
    for (auto it = m_clip_history.rbegin(); it != m_clip_history.rend(); ++it)
    {
        switch (oar::clip_stance(*it))
        {
        case oar::ClipStance::e_drawn:
            return { true, true };
        case oar::ClipStance::e_sheathed:
            return { true, false };
        case oar::ClipStance::e_undecided:
            break;
        }
    }
    // No decisive clip in the recent history: the last unpaused weaponState read is the only
    // remaining source.
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
    // game, not a picked constant). This observation runs every 10th world frame, so the span is
    // integrated from the real steady-clock delta between samples — a fixed 1/60 s per sample
    // measured the fade ~10x short (round 16).
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if (weight < 0.999f)
    {
        if (m_fading)
            m_fade_elapsed += std::chrono::duration<float>(now - m_fade_sample).count();
        else
        {
            m_fading = true;
            m_fade_elapsed = 0.0f;
        }
    }
    else if (m_fading)
    {
        m_fading = false;
        m_fade_elapsed += std::chrono::duration<float>(now - m_fade_sample).count();
        if (m_fade_elapsed > 0.05f && m_fade_elapsed < 2.0f)
        {
            m_crossfade_seconds = m_fade_elapsed;
            logger::info("SCOPY PANEL measured world crossfade {:.2f}s (clip '{}')",
                m_crossfade_seconds, m_idle_clip);
        }
        m_fade_elapsed = 0.0f;
    }
    m_fade_sample = now;
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

PanelMonitor::EntryAction PanelMonitor::take_entry_action()
{
    // Consumed exactly once: the first animation request of the panel session replays the move
    // the player was frozen in; later requests are equipment/recapture updates.
    const EntryAction action = m_entry_action;
    m_entry_action = {};
    return action;
}

PanelMonitor::PanelMonitor() : m_panel_open(false), m_panel_opened(false)
{
}

PanelMonitor::~PanelMonitor()
{

}

PLUGIN_NAMESPACE_END
