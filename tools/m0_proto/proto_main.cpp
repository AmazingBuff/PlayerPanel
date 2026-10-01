//
// Created by AmazingBuff on 2026/09/28.
//

#include "proto.h"

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

        void message_handler(SKSE::MessagingInterface::Message* message) noexcept
        {
            if (!message)
                return;
            switch (message->type)
            {
                case SKSE::MessagingInterface::kDataLoaded:
                    Proto::instance().install();
                    break;
                // FR-06: a loading screen or a fresh game is no valid preview
                // context; drop the panel so it cannot carry stale studio
                // content across a session change.
                case SKSE::MessagingInterface::kPreLoadGame:
                    Proto::instance().close_panel("save loading");
                    break;
                case SKSE::MessagingInterface::kNewGame:
                    Proto::instance().close_panel("new game");
                    break;
                default:
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
        logger::info("M0 proto v4.6 panel composite installed: F7 toggles the panel (visible opaque rectangle "
                     "+ evidence on close), F8 grabs a mid-session frame");
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
            // Clear release/dump requests that have not reached the render
            // thread yet, then bump the generation so the bracket resets its
            // per-open counters. Store order matters for the bracket: the
            // generation must be visible by the time panel_frame_active()
            // returns true.
            m_release_pending.store(false, std::memory_order_relaxed);
            m_dump_on_close.store(false, std::memory_order_relaxed);
            m_panel_generation.fetch_add(1, std::memory_order_release);
            logger::info("Panel opened: every menu frame is now bracketed for studio redirection "
                         "(F7 closes, F8 dumps)");
            return;
        }
        m_panel_open.store(false, std::memory_order_release);
        m_dump_requested.store(false, std::memory_order_release);
        m_dump_on_close.store(true, std::memory_order_release);
        m_release_pending.store(true, std::memory_order_release);
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

    void Proto::close_panel(std::string_view reason)
    {
        bool expected = true;
        if (m_panel_open.compare_exchange_strong(expected, false, std::memory_order_acq_rel))
        {
            m_dump_requested.store(false, std::memory_order_release);
            m_release_pending.store(true, std::memory_order_release);
            logger::info("Panel force-closed ({})", reason);
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
