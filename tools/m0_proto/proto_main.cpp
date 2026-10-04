//
// Created by AmazingBuff on 2026/09/28.
//

#include "proto.h"
#include "proto_pinstance.h"

#include <RE/Skyrim.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>
#include <SKSE/Version.h>
#include <Windows.h>
#include <fmt/format.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "proto_config.h"

using namespace std::literals;
namespace logger = SKSE::log;

namespace CharacterPanelProto
{
    namespace
    {
        // F6 is bound in the user's game setup, so the panel lives on F7/F8
        // (the probe's F7/F8 never co-load with this DLL).
        constexpr std::uint32_t Panel_Toggle_Virtual_Key = VK_F7;
        constexpr std::uint32_t Dump_Virtual_Key = VK_F8;

        bool initialize_log() noexcept
        {
            try
            {
                std::optional<std::filesystem::path> path = SKSE::log::log_directory();
                if (!path)
                    return false;

                *path /= fmt::format("{}.log"sv, Name);
                std::shared_ptr<spdlog::sinks::basic_file_sink_mt> sink =
                    std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
                std::shared_ptr<spdlog::logger> log =
                    std::make_shared<spdlog::logger>("CharacterPanelProto"s, std::move(sink));
                log->set_level(spdlog::level::info);
                log->flush_on(spdlog::level::info);
                spdlog::set_default_logger(std::move(log));
                spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        // FR-05: the sink only observes the two panel keys; every other event
        // passes through untouched, and held/repeat events are rejected so a
        // single press toggles exactly once. The callback stays active while
        // InventoryMenu pauses the game, so the panel is operable exactly
        // where its content exists.
        class InputHandler final : public RE::BSTEventSink<RE::InputEvent*>
        {
        public:
            static InputHandler& instance()
            {
                static InputHandler s_instance;
                return s_instance;
            }

            RE::BSEventNotifyControl ProcessEvent(
                RE::InputEvent* const* events,
                RE::BSTEventSource<RE::InputEvent*>*) noexcept override
            {
                if (!events)
                    return RE::BSEventNotifyControl::kContinue;

                UINT const panel_scan = MapVirtualKeyA(Panel_Toggle_Virtual_Key, MAPVK_VK_TO_VSC);
                UINT const dump_scan = MapVirtualKeyA(Dump_Virtual_Key, MAPVK_VK_TO_VSC);
                for (RE::InputEvent* event = *events; event; event = event->next)
                {
                    RE::ButtonEvent* button = event->AsButtonEvent();
                    if (!button || button->device.get() != RE::INPUT_DEVICE::kKeyboard || !button->IsDown())
                        continue;

                    if (panel_scan != 0 && button->GetIDCode() == panel_scan)
                        Proto::instance().toggle_panel();
                    else if (dump_scan != 0 && button->GetIDCode() == dump_scan)
                        Proto::instance().request_dump();
                }
                return RE::BSEventNotifyControl::kContinue;
            }

        private:
            InputHandler() = default;
        };

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

        void message_handler(SKSE::MessagingInterface::Message* message) noexcept
        {
            if (!message)
                return;
            switch (message->type)
            {
                case SKSE::MessagingInterface::kDataLoaded:
                    Proto::instance().install();
                    // Run 49 (v6.15): from here on, idle frames auto-spawn P
                    // on the first unpaused world frame so the engine
                    // renderer-initializes its geometries (device buffers).
                    PInstance::instance().set_world_ready(true);
                    break;
                // Run 49 (v6.15): each completed save load also re-arms the
                // auto-spawn (kDataLoaded fires once per app session only).
                case SKSE::MessagingInterface::kPostLoadGame:
                    PInstance::instance().set_world_ready(true);
                    break;
                // FR-06: a loading screen or a fresh game is no valid preview
                // context; drop the panel so it cannot carry stale studio
                // content across a session change. Run-37 crash defense:
                // the loading screen also tears down HUD/menu state the
                // parked clone and the accumulator state depend on — a
                // force-close here is followed by a hard P kill so no
                // stale actor/graph survives the load either.
                case SKSE::MessagingInterface::kPreLoadGame:
                    PInstance::instance().set_world_ready(false);
                    Proto::instance().close_panel("save loading");
                    PInstance::instance().despawn();
                    break;
                case SKSE::MessagingInterface::kNewGame:
                    PInstance::instance().set_world_ready(false);
                    Proto::instance().close_panel("new game");
                    PInstance::instance().despawn();
                    break;
                // v6.31 (run 64): quitting with the panel open crashed — the
                // studio draw and composite kept running into the tearing-
                // down renderer, and the parked clone's graph unloads with
                // the world on quit-to-menu. CLib's MessagingInterface enum
                // stops at kDataLoaded; the SKSE API's continued values are
                // kShutdown=10, kExitGame=11, kQuitGame=12. Force-close and
                // kill P before the teardown proceeds (same defense as the
                // load path above).
                case 10:  // SKSE kShutdown
                case 11:  // SKSE kExitGame
                case 12:  // SKSE kQuitGame
                    PInstance::instance().set_world_ready(false);
                    Proto::instance().close_panel("game exit");
                    PInstance::instance().despawn();
                    break;
                default:
                    // v6.33: bounded diagnostics — the quit-path message
                    // values (expected 10/11/12) have not been observed
                    // firing; log the neighborhood to calibrate the values
                    // this SKSE build actually dispatches.
                    if (message->type >= 9 && message->type <= 15)
                        logger::info("SKSE message type={} sender={}", message->type,
                            message->sender ? message->sender : "(null)");
                    break;
            }
        }
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

        if (!supported_runtime())
        {
            logger::warn("Unsupported runtime; proto input remains disabled");
            return;
        }

        RE::BSInputDeviceManager* source = RE::BSInputDeviceManager::GetSingleton();
        if (!source)
        {
            logger::warn("Input device manager unavailable; panel hotkeys are disabled");
            return;
        }

        source->AddEventSink(&InputHandler::instance());
        // v6.39: the panel follows the inventory menu (open/close with it).
        if (auto* ui = RE::UI::GetSingleton())
            ui->AddEventSink(&MenuSink::instance());
        else
            logger::warn("UI singleton unavailable; the panel will not follow the inventory menu");
        m_installed = true;
        if (!install_pass_hooks())
        {
            logger::warn("Proto pass-hook install failed; the panel has no redirect effect");
            return;
        }
        if (!install_hook())
        {
            logger::warn("Proto DrawInterfaceStart hook failed; the panel has no effect");
            return;
        }
        m_capture_ready = true;
        logger::info("M0 proto v6.68 installed: run-102 fix — pointer caches cannot catch the transition "
                     "cutting the light rig out of the host (0/3 with everything matching); a stalled "
                     "wrapper fetch now re-creates the rig under the current scene. F7 = fallback, "
                     "F8 = dump");
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

    bool supported_runtime() noexcept
    {
        try
        {
            REL::Module& module = REL::Module::get();
            return REL::Module::IsAE() && module.version() == SKSE::RUNTIME_SSE_1_6_1170;
        }
        catch (...)
        {
            return false;
        }
    }
}

extern "C" __declspec(dllexport) bool SKSEPlugin_Load(SKSE::LoadInterface const* skse)
{
    REL::Module::reset();
    if (!CharacterPanelProto::initialize_log())
        return false;
    logger::info("{} v{} build={} loaded", CharacterPanelProto::Name, CharacterPanelProto::Version,
        CharacterPanelProto::Build_Identity);

    SKSE::Init(skse);
    if (!CharacterPanelProto::supported_runtime())
    {
        logger::warn("Unsupported runtime; expected Skyrim AE 1.6.1170, proto will not register input");
        return true;
    }
    SKSE::MessagingInterface const* messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(CharacterPanelProto::message_handler))
        return false;
    return true;
}

extern "C" __declspec(dllexport) constinit auto SKSEPlugin_Version = [] {
    SKSE::PluginVersionData version;
    version.PluginVersion(REL::Version(1, 2, 0, 0));
    version.PluginName(CharacterPanelProto::Name);
    version.AuthorName("CharacterPanel");
    version.CompatibleVersions({ SKSE::RUNTIME_SSE_1_6_1170 });
    version.MinimumRequiredXSEVersion(REL::Version(2, 2, 6, 0));
    return version;
}();

extern "C" __declspec(dllexport) bool SKSEPlugin_Query(SKSE::QueryInterface const*, SKSE::PluginInfo* info)
{
    info->infoVersion = SKSE::PluginInfo::kVersion;
    info->name = SKSEPlugin_Version.pluginName;
    info->version = SKSEPlugin_Version.pluginVersion;
    return true;
}
