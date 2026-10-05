//
// Created by AmazingBuff on 2026/09/28.
//
// Panel controller TU: the Proto lifecycle singleton and the menu event
// sink that drives it (hotkeys live in input/input.cpp). (2026-10-05 src
// migration: split out of tools/m0_proto, behavior byte-identical)
//

#include "input/input.h"
#include "panel/panel.h"
#include "pinstance/pinstance.h"
#include "render/ui_render_hook.h"

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // v6.39: the panel's lifecycle IS the inventory menu's (user
    // decision — the panel depends on nothing but the inventory being
    // open). Opening the inventory opens the panel, closing it closes
    // the panel; F7 remains as a manual fallback. MenuOpenCloseEvent
    // fires on the game thread while the menu system is consistent, so
    // the panel state flips before the first menu frame renders — the
    // very first inventory frame is already bracketed.
    class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:
        static MenuSink& instance()
        {
            static MenuSink s_instance;
            return s_instance;
        }

        RE::BSEventNotifyControl ProcessEvent(
            const RE::MenuOpenCloseEvent* event,
            RE::BSTEventSource<RE::MenuOpenCloseEvent>*) noexcept override
        {
            if (!event)
                return RE::BSEventNotifyControl::kContinue;
            // Only the inventory drives the panel; other menus (map,
            // skills, containers, ...) keep their own behavior.
            if (event->menuName == RE::InventoryMenu::MENU_NAME)
            {
                if (event->opening)
                    Proto::instance().open_panel("inventory opened");
                else
                    // A USER close: write the evidence TGA first (v3.1
                    // "close = capture" — silently dropped when the
                    // MenuSink lifecycle landed in v6.39; restored in
                    // v6.62). Force-closes (load/teardown) stay silent.
                    Proto::instance().close_panel("inventory closed", true);
            }
            return RE::BSEventNotifyControl::kContinue;
        }

    private:
        MenuSink() = default;
    };
}

Proto::Proto() :
    m_installed(false), m_capture_ready(false), m_panel_open(false),
    m_release_pending(false), m_dump_requested(false), m_dump_on_close(false),
    m_panel_generation(0)
{
}

Proto& Proto::instance()
{
    static Proto s_instance;
    return s_instance;
}

void Proto::install()
{
    if (m_installed)
        return;

    if (!InputManager::install())
        return;

    // v6.39: the panel follows the inventory menu (open/close with it).
    if (auto* ui = RE::UI::GetSingleton())
        ui->AddEventSink(&MenuSink::instance());
    else
        logger::warn("UI singleton unavailable; the panel will not follow the inventory menu");
    m_installed = true;
    if (!install_ui_render_hooks())
        return;
    m_capture_ready = true;
    logger::info("M0 proto v6.70 installed: born-at-depth FALSIFIED (run 104 — the engine picks "
                 "character LOD by reference distance at load; the depth-spawned graph had zero "
                 "bind matrices and rendered scattered) — placement reverted to PlaceObjectAtMe, "
                 "the flash is fixed by the grace fade guard instead. F7 = fallback, F8 = dump");
}

void Proto::open_panel(std::string_view reason)
{
    if (!m_capture_ready)
    {
        logger::warn("Panel open ignored ({}): capture pipeline not installed", reason);
        return;
    }
    bool expected = false;
    if (!m_panel_open.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        return;  // already open (F7 may have opened it first) — idempotent
    // Clear release/dump requests that have not reached the render
    // thread yet, then bump the generation so the bracket resets its
    // per-open counters. Store order matters for the bracket: the
    // generation must be visible by the time panel_frame_active()
    // returns true.
    m_release_pending.store(false, std::memory_order_relaxed);
    m_dump_on_close.store(false, std::memory_order_relaxed);
    m_panel_generation.fetch_add(1, std::memory_order_release);
    // Stage 2: the panel's content is the independent display instance
    // P — spawn it with the panel.
    PInstance::instance().spawn();
    // Stage-2b: the per-open home-hosting check (U2) — logs the verdict
    // for a relocated graph, no-op otherwise.
    PInstance::instance().note_panel_open();
    logger::info("Panel opened ({}): every menu frame is now bracketed for studio redirection", reason);
}

void Proto::toggle_panel()
{
    if (!m_capture_ready)
    {
        logger::warn("Panel toggle ignored: capture pipeline not installed");
        return;
    }
    bool expected = false;
    if (m_panel_open.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
    {
        m_release_pending.store(false, std::memory_order_relaxed);
        m_dump_on_close.store(false, std::memory_order_relaxed);
        m_panel_generation.fetch_add(1, std::memory_order_release);
        // Stage 2: the panel's content is the independent display
        // instance P — spawn it with the panel, despawn with the close.
        toggle_p_instance();
        logger::info("Panel opened (F7 fallback): every menu frame is now bracketed for studio "
                     "redirection (F7 closes, F8 dumps)");
        return;
    }
    m_panel_open.store(false, std::memory_order_release);
    m_dump_requested.store(false, std::memory_order_release);
    m_dump_on_close.store(true, std::memory_order_release);
    m_release_pending.store(true, std::memory_order_release);
    toggle_p_instance();
    logger::info("Panel closed: studio redirection stops, one studio dump is written before the target "
                 "is released on the render thread");
}

void Proto::request_dump()
{
    if (!m_capture_ready)
    {
        logger::warn("Dump ignored: capture pipeline not installed");
        return;
    }
    if (!m_panel_open.load(std::memory_order_acquire))
    {
        logger::warn("Dump ignored: panel is closed");
        return;
    }
    m_dump_requested.store(true, std::memory_order_release);
    logger::info("Studio target dump requested (written when the current bracket closes)");
}

void Proto::close_panel(std::string_view reason, bool a_dump_evidence)
{
    bool expected = true;
    if (m_panel_open.compare_exchange_strong(expected, false, std::memory_order_acq_rel))
    {
        m_dump_requested.store(false, std::memory_order_release);
        if (a_dump_evidence)
            m_dump_on_close.store(true, std::memory_order_release);
        m_release_pending.store(true, std::memory_order_release);
        toggle_p_instance();
        logger::info("Panel force-closed ({})", reason);
    }
}

void Proto::toggle_p_instance()
{
    // Game thread: P's lifecycle is no longer bound to the panel
    // (run 49). P auto-spawns on the first unpaused world frame after a
    // load and parks disabled once renderer-initialized; a build that
    // ran inside the paused inventory can never be initialized (the
    // rd=9 failure), so F7-open only tops up a missing instance and
    // close keeps the built instance for the whole session. despawn()
    // stays bound to load/new-game teardown.
    if (m_panel_open.load(std::memory_order_acquire))
    {
        PInstance::instance().spawn();
        // Stage-2b: same per-open home-hosting check as open_panel (U2).
        PInstance::instance().note_panel_open();
    }
}

bool Proto::panel_frame_active()
{
    return m_panel_open.load(std::memory_order_acquire);
}

bool Proto::take_dump()
{
    bool expected = true;
    return m_dump_requested.compare_exchange_strong(expected, false, std::memory_order_acq_rel);
}

bool Proto::take_release_pending()
{
    bool expected = true;
    return m_release_pending.compare_exchange_strong(expected, false, std::memory_order_acq_rel);
}

bool Proto::take_dump_on_close()
{
    bool expected = true;
    return m_dump_on_close.compare_exchange_strong(expected, false, std::memory_order_acq_rel);
}

std::uint32_t Proto::panel_generation()
{
    return m_panel_generation.load(std::memory_order_acquire);
}

PLUGIN_NAMESPACE_END
