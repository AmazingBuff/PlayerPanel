#include "config/config.h"
#include "input/input.h"
#include "preview/preview_actor.h"
#include "render/frame_hook.h"
#include "render/panel/panel_menu.h"
#include "render/panel/panel_renderer.h"
#include "render/shader_manager.h"
#include <spdlog/sinks/basic_file_sink.h>

namespace
{
    constexpr spdlog::level::level_enum Log_Level = spdlog::level::info;
    void initialize_log()
    {
        std::optional<std::filesystem::path> path = logger::log_directory();
        if (!path)
            SKSE::stl::report_and_fail("Failed to find standard logging directory"sv);

        *path /= fmt::format("{}.log"sv, Plugin::Plugin_Name);
        std::shared_ptr<spdlog::sinks::basic_file_sink_mt> sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
        std::shared_ptr<spdlog::logger> log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

        log->set_level(Log_Level);
        log->flush_on(Log_Level);

        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);
    }

    // The swap chain may not exist yet when the data is loaded, so every later session message
    // retries the same idempotent install instead of installing a second hook.
    void install_frame_hook()
    {
        (void)PLUGIN_NAMESPACE::FrameHook::instance().install(&PLUGIN_NAMESPACE::PreviewActor::on_frame);
        (void)PLUGIN_NAMESPACE::FrameHook::instance().install(&PLUGIN_NAMESPACE::PanelRenderer::on_frame);
        (void)PLUGIN_NAMESPACE::FrameHook::instance().install_present(&PLUGIN_NAMESPACE::PanelRenderer::on_present);
    }

    // Gives back everything the panel took from the player-visible state. The panel's menu is hidden
    // first, which is what returns the engine's cursor and input context, and only then are the panel's
    // device objects and the preview released.
    void release_panel_state()
    {
        PLUGIN_NAMESPACE::PanelRenderer::instance().set_panel_open(false);
        PLUGIN_NAMESPACE::PanelRenderer::instance().request_release();
        PLUGIN_NAMESPACE::PanelMenu::set_open(false);
    }

    void message_handler(SKSE::MessagingInterface::Message* message) noexcept
    {
        if (!message)
            return;
        try
        {
            switch (message->type)
            {
            case SKSE::MessagingInterface::kDataLoaded:
                PLUGIN_NAMESPACE::PanelMenu::install();
                PLUGIN_NAMESPACE::InputManager::install();
                install_frame_hook();
                // The D3D11 device may not exist yet, in which case this attempt only reports that it
                // was skipped; the renderer makes the attempt that counts as soon as it has a device.
                // Compiling here as well keeps the first panel frame from paying for the compilation,
                // and a skipped or failed attempt is expected at this point, so it is not fatal.
                (void)PLUGIN_NAMESPACE::ShaderManager::instance().compile();
                break;
            case SKSE::MessagingInterface::kPreLoadGame:
                release_panel_state();
                PLUGIN_NAMESPACE::PreviewActor::instance().destroy();
                break;
            case SKSE::MessagingInterface::kNewGame:
            case SKSE::MessagingInterface::kPostLoadGame:
                release_panel_state();
                PLUGIN_NAMESPACE::PreviewActor::instance().destroy();
                PLUGIN_NAMESPACE::PanelMenu::install();
                PLUGIN_NAMESPACE::InputManager::install();
                install_frame_hook();
                break;
            case SKSE::MessagingInterface::kSaveGame:
                PLUGIN_NAMESPACE::Setting::instance().save();
                break;
            default:
                break;
            }
        }
        catch (...)
        {
            try { logger::error("Feature message handler failed"); } catch (...) {}
        }
    }
}

extern "C" DLLEXPORT bool SKSEPlugin_Load(SKSE::LoadInterface const* skse)
{
    REL::Module::reset();  // Clib-NG bug workaround

    initialize_log();
    logger::info("{} v{}"sv, Plugin::Plugin_Name, Plugin::Plugin_Version.string());

    SKSE::Init(skse);
    try
    {
        PLUGIN_NAMESPACE::Setting::instance().load();
    }
    catch (...)
    {
        return false;
    }
    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(message_handler))
        return false;

    logger::info("{} loaded"sv, Plugin::Plugin_Name);
    return true;
}

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = [] {
    SKSE::PluginVersionData v;
    v.PluginVersion(Plugin::Plugin_Version);
    v.PluginName(Plugin::Plugin_Name);
    v.AuthorName(Plugin::Plugin_Author);
    v.UsesAddressLibrary();
    v.UsesNoStructs();
    return v;
}();

extern "C" DLLEXPORT bool SKSEPlugin_Query(SKSE::QueryInterface const*, SKSE::PluginInfo* info)
{
    info->infoVersion = SKSE::PluginInfo::kVersion;
    info->name = SKSEPlugin_Version.pluginName;
    info->version = SKSEPlugin_Version.pluginVersion;
    return true;
}