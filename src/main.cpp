#include "config/config.h"
#include "input/input.h"
#include "render/present_hook.h"
#include "spike/clone_actor.h"
#include "spike/pass_hook.h"
#include <spdlog/sinks/basic_file_sink.h>

PLUGIN_NAMESPACE_BEGIN

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

    // SPIKE CODE - game-thread frame tick for the clone actor. The present
    // hook marks each frame; one queued task per frame services the clone's
    // spawn/despawn requests and delayed 3D collection on the game thread.
    class FrameTick
    {
    public:
        static FrameTick& instance()
        {
            static FrameTick s_instance;
            return s_instance;
        }

        void on_present(REX::W32::IDXGISwapChain* /*swap_chain*/)
        {
            if (!m_frame.exchange(true, std::memory_order_acq_rel))
                SKSE::GetTaskInterface()->AddTask([this]() { tick(); });
        }

    private:
        void tick()
        {
            m_frame.store(false, std::memory_order_release);
            spike::CloneActor::instance().on_frame();
        }

        std::atomic<bool> m_frame{ false };
    };

    void message_handler(SKSE::MessagingInterface::Message* message) noexcept
    {
        if (!message)
            return;
        switch (message->type)
        {
            case SKSE::MessagingInterface::kDataLoaded:
                PLUGIN_NAMESPACE::InputManager::install();
                if (!PLUGIN_NAMESPACE::PresentHook::instance().install(
                        [](REX::W32::IDXGISwapChain* chain) { FrameTick::instance().on_present(chain); }))
                    logger::warn("Present hook not ready; retrying on next message");
                if (!spike::PassHook::instance().install())
                    logger::warn("Spike pass hook install failed");
                break;
            case SKSE::MessagingInterface::kNewGame:
            case SKSE::MessagingInterface::kPostLoadGame:
                PLUGIN_NAMESPACE::InputManager::install();
                PLUGIN_NAMESPACE::PresentHook::instance().install(
                    [](REX::W32::IDXGISwapChain* chain) { FrameTick::instance().on_present(chain); });
                spike::PassHook::instance().install();
                break;
            case SKSE::MessagingInterface::kSaveGame:
                PLUGIN_NAMESPACE::Setting::instance().save();
                break;
            case SKSE::MessagingInterface::kPreLoadGame:
                spike::CloneActor::instance().request_despawn();
                break;
            default:
                break;
        }
    }
}

PLUGIN_NAMESPACE_END

extern "C" DLLEXPORT bool SKSEPlugin_Load(SKSE::LoadInterface const* skse)
{
    REL::Module::reset();  // Clib-NG bug workaround

    PLUGIN_NAMESPACE::initialize_log();
    logger::info("{} v{}"sv, Plugin::Plugin_Name, Plugin::Plugin_Version.string());

    SKSE::Init(skse);
    PLUGIN_NAMESPACE::Setting::instance().load();
    const SKSE::MessagingInterface* messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(PLUGIN_NAMESPACE::message_handler))
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
