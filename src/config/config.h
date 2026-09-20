#pragma once
#include <cstdint>
#include <mutex>

PLUGIN_NAMESPACE_BEGIN

struct Config
{
    bool enabled{ true };
    uint32_t hotkey{ 0x76 }; // F7 virtual-key code; zero disables.
};

class Setting
{
public:
    static Setting& instance();
    Setting(Setting const&) = delete;
    Setting& operator=(Setting const&) = delete;

    Config get_config() const;
    void toggle();
    void load();
    void save();
private:
    Setting() = default;
    ~Setting() = default;
    mutable std::mutex m_config_mutex;
    std::mutex m_file_mutex;
    Config m_config;
};

PLUGIN_NAMESPACE_END
