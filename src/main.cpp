//
// Created by AmazingBuff on 2026/09/28.
//

#include "character/character_manager.h"
#include "panel/panel.h"
#include "render/renderer.h"
#include "render/shader_manager.h"
#include "render/studio/light.h"
#include "character/puppet_driver.h"
#include "character/scene_graph_copy.h"
#include "input/input.h"
#include "oar/hooks.h"

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
#if CHARACTER_PANEL_SCENE_COPY_EXPERIMENT
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [thread %t] %g(%#): [%^%l%$] %v"s);
#else
        spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);
#endif
    }

    void message_handler(SKSE::MessagingInterface::Message* message) noexcept
    {
        switch (message->type)
        {
        case SKSE::MessagingInterface::kPostPostLoad:
            (void)PLUGIN_NAMESPACE::oar::install_hooks();
            break;
        case SKSE::MessagingInterface::kDataLoaded:
            (void)PLUGIN_NAMESPACE::ShaderManager::instance().compile();
            PLUGIN_NAMESPACE::Renderer::install();
            PLUGIN_NAMESPACE::PanelMonitor::instance().install();
#if CHARACTER_PANEL_SCENE_COPY_EXPERIMENT
            PLUGIN_NAMESPACE::InputManager::install();

            logger::info("SCOPY BUILD {} runtime={} mode=scene-copy-primary display=detached-copy(puppet-spawn-off) animation=puppet-clip/captured-clip/procedural-idle cbpc=not-registered smp=not-registered hotkeys=F7-recapture F8-draw F2-mark F4-drive F6-r01-diagnostic", Plugin::Plugin_Build_Identity, REL::Module::get().version().string());

#endif
            break;
        case SKSE::MessagingInterface::kPreLoadGame:
        case SKSE::MessagingInterface::kNewGame:
            PLUGIN_NAMESPACE::CharacterManager::instance().clear_clones();
#if CHARACTER_PANEL_SCENE_COPY_EXPERIMENT
            PLUGIN_NAMESPACE::SceneGraphCopy::instance().reset_for_load();
            PLUGIN_NAMESPACE::PuppetDriver::instance().request_release();
            // The copy-route decision (2026-10-15): no display puppet is spawned any more. The
            // panel draws the detached copy of the player's real 3D, which carries every
            // body-morph and outfit-transform state the mods gave the player without any
            // per-plugin compatibility work; the OAR decision for the figure reads the captured
            // clip instead of a live puppet graph. request_spawn() stays available for the
            // diagnostics that still want an actor, but nothing queues it on its own.
#endif
            break;
        default:
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
