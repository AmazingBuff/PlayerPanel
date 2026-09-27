#pragma once
#include <cstdint>
#include <mutex>

PLUGIN_NAMESPACE_BEGIN

struct Config
{
    bool enabled;
    uint32_t hotkey;
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
    Setting();
    ~Setting() = default;
    mutable std::mutex m_config_mutex;
    std::mutex m_file_mutex;
    Config m_config;
};

PLUGIN_NAMESPACE_END
