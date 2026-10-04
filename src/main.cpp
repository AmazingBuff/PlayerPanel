//
// Created by AmazingBuff on 2026/09/28.
//
// Entry TU: SKSE exports, session message handling, logging, and the AE
// 1.6.1170 runtime gate. (2026-10-05 src migration: split out of tools/m0_proto, behavior byte-identical)
//

#include "panel.h"
#include "pinstance/pinstance.h"

namespace
{
    constexpr spdlog::level::level_enum Log_Level = spdlog::level::info;
    void initialize_log() noexcept
    {
        std::optional<std::filesystem::path> path = SKSE::log::log_directory();
        if (!path)
            return;

        *path /= fmt::format("{}.log"sv, Plugin::Plugin_Name);
        std::shared_ptr<spdlog::sinks::basic_file_sink_mt> sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
        std::shared_ptr<spdlog::logger> log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

        log->set_level(Log_Level);
        log->flush_on(Log_Level);

        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);
    }

    void message_handler(SKSE::MessagingInterface::Message* message) noexcept
    {
        if (!message)
            return;
        switch (message->type)
        {
        case SKSE::MessagingInterface::kDataLoaded:
            PLUGIN_NAMESPACE::Proto::instance().install();
            // Run 49 (v6.15): from here on, idle frames auto-spawn P
            // on the first unpaused world frame so the engine
            // renderer-initializes its geometries (device buffers).
            PLUGIN_NAMESPACE::PInstance::instance().set_world_ready(true);
            break;
            // Run 49 (v6.15): each completed save load also re-arms the
            // auto-spawn (kDataLoaded fires once per app session only).
        case SKSE::MessagingInterface::kPostLoadGame:
            PLUGIN_NAMESPACE::PInstance::instance().set_world_ready(true);
            break;
            // FR-06: a loading screen or a fresh game is no valid preview
            // context; drop the panel so it cannot carry stale studio
            // content across a session change. Run-37 crash defense:
            // the loading screen also tears down HUD/menu state the
            // parked clone and the accumulator state depend on — a
            // force-close here is followed by a hard P kill so no
            // stale actor/graph survives the load either.
        case SKSE::MessagingInterface::kPreLoadGame:
            PLUGIN_NAMESPACE::PInstance::instance().set_world_ready(false);
            PLUGIN_NAMESPACE::Proto::instance().close_panel("save loading");
            PLUGIN_NAMESPACE::PInstance::instance().despawn();
            break;
        case SKSE::MessagingInterface::kNewGame:
            PLUGIN_NAMESPACE::PInstance::instance().set_world_ready(false);
            PLUGIN_NAMESPACE::Proto::instance().close_panel("new game");
            PLUGIN_NAMESPACE::PInstance::instance().despawn();
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
            PLUGIN_NAMESPACE::PInstance::instance().set_world_ready(false);
            PLUGIN_NAMESPACE::Proto::instance().close_panel("game exit");
            PLUGIN_NAMESPACE::PInstance::instance().despawn();
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

extern "C" DLLEXPORT bool SKSEPlugin_Load(SKSE::LoadInterface const* skse)
{
    REL::Module::reset();

    initialize_log();
    logger::info("{} v{}"sv, Plugin::Plugin_Name, Plugin::Plugin_Version.string());

    SKSE::Init(skse);

    SKSE::GetMessagingInterface()->RegisterListener(message_handler);

    logger::info("{} loaded"sv, Plugin::Plugin_Name);
    return true;
}

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []{
    SKSE::PluginVersionData version;
    version.PluginVersion(Plugin::Plugin_Version);
    version.PluginName(Plugin::Plugin_Name);
    version.AuthorName(Plugin::Plugin_Author);
    version.CompatibleVersions({ SKSE::RUNTIME_SSE_1_6_1170 });
    version.MinimumRequiredXSEVersion(REL::Version(2, 2, 6, 0));
    return version;
}();

extern "C" DLLEXPORT bool SKSEPlugin_Query(SKSE::QueryInterface const*, SKSE::PluginInfo* info)
{
    info->infoVersion = SKSE::PluginInfo::kVersion;
    info->name = SKSEPlugin_Version.pluginName;
    info->version = SKSEPlugin_Version.pluginVersion;
    return true;
}
