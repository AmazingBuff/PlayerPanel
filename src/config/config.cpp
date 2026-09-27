#include "config.h"
#include <SimpleIni.h>

PLUGIN_NAMESPACE_BEGIN

namespace
{
    constexpr Config Default_Config{ .enabled = true, .hotkey = 0x76 };

    std::filesystem::path get_config_path()
    {
        std::wstring_view executable = REL::Module::get().filePath();
        return std::filesystem::path(executable).parent_path() / "Data" / "SKSE" / "Plugins" /
            (std::string(Plugin::Plugin_Name) + ".ini");
    }
}

Setting::Setting() : m_config(Default_Config) {}

Setting& Setting::instance()
{
    static Setting s_instance;
    return s_instance;
}

Config Setting::get_config() const
{
    std::lock_guard lock(m_config_mutex);
    return m_config;
}

void Setting::toggle()
{
    std::lock_guard lock(m_config_mutex);
    m_config.enabled = !m_config.enabled;
}

void Setting::load()
{
    std::lock_guard file_lock(m_file_mutex);
    CSimpleIniA ini;
    ini.SetUnicode();
    std::filesystem::path const path = get_config_path();
    if (ini.LoadFile(path.c_str()) < 0)
        logger::info("INI unavailable; using defaults");
    Config config = Default_Config;
    config.enabled = ini.GetBoolValue("General", "Enabled", config.enabled);
    long const key = ini.GetLongValue("General", "Hotkey", static_cast<long>(config.hotkey));
    config.hotkey = key >= 0 && key <= 0xFE ? static_cast<uint32_t>(key) : 0u;
    std::lock_guard config_lock(m_config_mutex);
    m_config = config;
}

void Setting::save()
{
    std::lock_guard file_lock(m_file_mutex);
    std::filesystem::path const path = get_config_path();
    std::error_code ec;
    CSimpleIniA ini;
    ini.SetUnicode();
    if (std::filesystem::exists(path, ec) && ini.LoadFile(path.c_str()) < 0)
    {
        logger::warn("Cannot read existing INI; refusing to overwrite it");
        return;
    }
    if (ec)
    {
        logger::warn("Cannot inspect existing INI path ({}); refusing to overwrite it", ec.message());
        return;
    }
    Config const config = get_config();
    ini.SetBoolValue("General", "Enabled", config.enabled);
    ini.SetLongValue("General", "Hotkey", static_cast<long>(config.hotkey));
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec)
    {
        logger::warn("Failed to create INI directory ({})", ec.message());
        return;
    }
    if (ini.SaveFile(path.c_str()) < 0)
        logger::warn("Failed to save INI");
}

PLUGIN_NAMESPACE_END
