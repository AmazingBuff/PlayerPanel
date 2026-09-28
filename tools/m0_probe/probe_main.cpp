//
// Created by AmazingBuff on 2026/09/28.
//

#include "probe.h"

#include <RE/Skyrim.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>
#include <SKSE/Version.h>
#include <Windows.h>
#include <fmt/format.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "probe_config.h"

using namespace std::literals;
namespace logger = SKSE::log;

namespace CharacterPanelProbe
{
    namespace
    {
        constexpr std::uint32_t F7_Virtual_Key = VK_F7;
        constexpr std::uint32_t F8_Virtual_Key = VK_F8;

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
                    std::make_shared<spdlog::logger>("CharacterPanelProbe"s, std::move(sink));
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

                UINT const f7_scan = MapVirtualKeyA(F7_Virtual_Key, MAPVK_VK_TO_VSC);
                UINT const f8_scan = MapVirtualKeyA(F8_Virtual_Key, MAPVK_VK_TO_VSC);
                for (RE::InputEvent* event = *events; event; event = event->next)
                {
                    RE::ButtonEvent* button = event->AsButtonEvent();
                    if (!button || button->device.get() != RE::INPUT_DEVICE::kKeyboard || !button->IsDown())
                        continue;

                    if (f7_scan != 0 && button->GetIDCode() == f7_scan)
                        Probe::instance().capture(CaptureKind::kCodeAndMenu);
                    else if (f8_scan != 0 && button->GetIDCode() == f8_scan)
                        Probe::instance().capture(CaptureKind::kMenuOnly);
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
            if (message->type == SKSE::MessagingInterface::kDataLoaded)
                Probe::instance().install();
        }
    }

    Probe& Probe::instance()
    {
        static Probe s_instance;
        return s_instance;
    }

    void Probe::install()
    {
        if (m_installed)
            return;

        if (!supported_runtime())
        {
            logger::warn("Unsupported runtime; probe input remains disabled");
            return;
        }

        RE::BSInputDeviceManager* source = RE::BSInputDeviceManager::GetSingleton();
        if (!source)
        {
            logger::warn("Input device manager unavailable; F7/F8 probe capture is disabled");
            return;
        }

        source->AddEventSink(&InputHandler::instance());
        m_installed = true;
        logger::info("Read-only probe input installed: F7=code+menu, F8=menu metadata");
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
    if (!CharacterPanelProbe::initialize_log())
        return false;
    logger::info("{} v{} build={} loaded", CharacterPanelProbe::Name, CharacterPanelProbe::Version,
        CharacterPanelProbe::Build_Identity);

    SKSE::Init(skse);
    if (!CharacterPanelProbe::supported_runtime())
    {
        logger::warn("Unsupported runtime; expected Skyrim AE 1.6.1170, probe will not register input");
        return true;
    }
    SKSE::MessagingInterface const* messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(CharacterPanelProbe::message_handler))
        return false;
    return true;
}

extern "C" __declspec(dllexport) constinit auto SKSEPlugin_Version = [] {
    SKSE::PluginVersionData version;
    version.PluginVersion(REL::Version(1, 0, 0, 0));
    version.PluginName(CharacterPanelProbe::Name);
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
