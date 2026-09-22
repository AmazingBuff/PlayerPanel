#include "config.h"
#include <SimpleIni.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

PLUGIN_NAMESPACE_BEGIN

namespace
{
    constexpr bool Default_Enabled = true;
    constexpr std::uint32_t Default_Hotkey = 0x76; // F7 virtual-key code; zero disables.
    constexpr double Default_Preview_Distance = 150.0;
    constexpr long Max_Hotkey = 0xFE;

    // Panel presentation defaults and their accepted ranges. The two fractions and the aspect keep
    // the panel at roughly the size the previous absolute 480x720 pixel pair produced on a 1080p
    // screen, but they now scale with the render height instead of being a fixed box. Out-of-range
    // INI values fall back to the default rather than being clamped, so a typo is visible.
    constexpr double Default_Panel_Height_Fraction = 0.66;
    constexpr double Default_Panel_Aspect = 0.667;
    constexpr double Default_Panel_Margin_Fraction = 0.022;
    constexpr double Default_Panel_Position = -1.0; // negative means "not placed yet".
    constexpr double Default_Camera_Fov = 35.0;
    constexpr double Default_Camera_Distance = 0.0; // zero means automatic framing.
    constexpr double Default_Alpha_Test_Threshold = 0.5;

    // The character's inset while a skin owns the chrome. It is a fraction of the panel height, like
    // the size keys, so it stays the same apparent frame on every resolution. The default is chosen
    // against the shipped default skin's band: it reads as a frame around the character rather than a
    // hairline, while the character still occupies most of the panel. The renderer additionally floors
    // it in pixels, so a tiny panel still gets a visible frame.
    constexpr double Default_Panel_Skin_Inset_Fraction = 0.03;

    constexpr double Min_Panel_Height_Fraction = 0.05;
    constexpr double Max_Panel_Height_Fraction = 1.0;
    constexpr double Min_Panel_Aspect = 0.1;
    constexpr double Max_Panel_Aspect = 10.0;
    constexpr double Max_Panel_Margin_Fraction = 0.5;
    constexpr double Min_Panel_Skin_Inset_Fraction = 0.005;
    constexpr double Max_Panel_Skin_Inset_Fraction = 0.25;
    constexpr double Min_Camera_Fov = 1.0;
    constexpr double Max_Camera_Fov = 179.0;
    constexpr double Max_Camera_Distance = 100000.0;

    // The documented skin path is defined once, as Panel_Swf_Path; this is the only place it is copied
    // into the configuration's fixed buffer, for the default and for a value read from the INI.
    void set_panel_swf_path(Config& config, char const* path)
    {
        std::snprintf(config.panel_swf_path, Max_Panel_Swf_Path, "%s", path);
    }

    // The single authoritative set of defaults; both the constructor and load() start from it.
    Config default_config()
    {
        Config config{
            .enabled = Default_Enabled,
            .hotkey = Default_Hotkey,
            .preview_distance = Default_Preview_Distance,
            .panel_height_fraction = Default_Panel_Height_Fraction,
            .panel_aspect = Default_Panel_Aspect,
            .panel_margin_fraction = Default_Panel_Margin_Fraction,
            .panel_position_x = Default_Panel_Position,
            .panel_position_y = Default_Panel_Position,
            .panel_swf_path = {},
            .panel_skin_inset_fraction = Default_Panel_Skin_Inset_Fraction,
            .camera_fov = Default_Camera_Fov,
            .camera_distance = Default_Camera_Distance,
            .alpha_test_threshold = Default_Alpha_Test_Threshold,
        };
        set_panel_swf_path(config, Panel_Swf_Path);
        return config;
    }

    // The INI is an external boundary: one check normalizes every value read from it.
    bool is_usable_preview_distance(double value)
    {
        return std::isfinite(value) && value > 0.0;
    }

    bool is_usable_panel_height_fraction(double value)
    {
        return std::isfinite(value) && value >= Min_Panel_Height_Fraction && value <= Max_Panel_Height_Fraction;
    }

    bool is_usable_panel_aspect(double value)
    {
        return std::isfinite(value) && value >= Min_Panel_Aspect && value <= Max_Panel_Aspect;
    }

    bool is_usable_panel_margin_fraction(double value)
    {
        return std::isfinite(value) && value >= 0.0 && value <= Max_Panel_Margin_Fraction;
    }

    // Either the explicit "not placed yet" marker or a fraction of the free space on that axis.
    bool is_usable_panel_position(double value)
    {
        return std::isfinite(value) &&
            (value == Default_Panel_Position || (value >= 0.0 && value <= 1.0));
    }

    // A skin path that names something and still fits the configuration buffer. A longer value would
    // have to be truncated, which would silently load a different file.
    bool is_usable_panel_swf_path(char const* value)
    {
        return value && value[0] != '\0' && std::strlen(value) < Max_Panel_Swf_Path;
    }

    bool is_usable_panel_skin_inset_fraction(double value)
    {
        return std::isfinite(value) && value >= Min_Panel_Skin_Inset_Fraction && value <= Max_Panel_Skin_Inset_Fraction;
    }

    bool is_usable_camera_fov(double value)
    {
        return std::isfinite(value) && value >= Min_Camera_Fov && value <= Max_Camera_Fov;
    }

    bool is_usable_camera_distance(double value)
    {
        return std::isfinite(value) && value >= 0.0 && value <= Max_Camera_Distance;
    }

    bool is_usable_alpha_test_threshold(double value)
    {
        return std::isfinite(value) && value >= 0.0 && value <= 1.0;
    }

    std::filesystem::path get_config_path()
    {
        std::wstring_view executable = REL::Module::get().filePath();
        return std::filesystem::path(executable).parent_path() / "Data" / "SKSE" / "Plugins" /
            (std::string(Plugin::Plugin_Name) + ".ini");
    }
}

Setting::Setting() : m_config(default_config())
{
}

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

void Setting::set_panel_position(double x, double y)
{
    std::lock_guard config_lock(m_config_mutex);
    m_config.panel_position_x = (std::clamp)(x, 0.0, 1.0);
    m_config.panel_position_y = (std::clamp)(y, 0.0, 1.0);
}

void Setting::load()
{
    std::lock_guard file_lock(m_file_mutex);
    CSimpleIniA ini;
    ini.SetUnicode();
    auto const path = get_config_path();
    if (ini.LoadFile(path.c_str()) < 0)
        logger::info("INI unavailable; using defaults");
    Config config = default_config();
    config.enabled = ini.GetBoolValue("General", "Enabled", Default_Enabled);
    auto const key = ini.GetLongValue("General", "Hotkey", static_cast<long>(Default_Hotkey));
    config.hotkey = key >= 0 && key <= Max_Hotkey ? static_cast<std::uint32_t>(key) : 0u;
    double const distance = ini.GetDoubleValue("General", "PreviewDistance", Default_Preview_Distance);
    config.preview_distance = is_usable_preview_distance(distance) ? distance : Default_Preview_Distance;

    double const height_fraction = ini.GetDoubleValue("General", "PanelHeightFraction", Default_Panel_Height_Fraction);
    config.panel_height_fraction = is_usable_panel_height_fraction(height_fraction) ? height_fraction : Default_Panel_Height_Fraction;
    double const aspect = ini.GetDoubleValue("General", "PanelAspect", Default_Panel_Aspect);
    config.panel_aspect = is_usable_panel_aspect(aspect) ? aspect : Default_Panel_Aspect;
    double const margin_fraction = ini.GetDoubleValue("General", "PanelMarginFraction", Default_Panel_Margin_Fraction);
    config.panel_margin_fraction = is_usable_panel_margin_fraction(margin_fraction) ? margin_fraction : Default_Panel_Margin_Fraction;
    double const position_x = ini.GetDoubleValue("General", "PanelPositionX", Default_Panel_Position);
    config.panel_position_x = is_usable_panel_position(position_x) ? position_x : Default_Panel_Position;
    double const position_y = ini.GetDoubleValue("General", "PanelPositionY", Default_Panel_Position);
    config.panel_position_y = is_usable_panel_position(position_y) ? position_y : Default_Panel_Position;
    char const* const swf_path = ini.GetValue("General", "PanelSwfPath", Panel_Swf_Path);
    set_panel_swf_path(config, is_usable_panel_swf_path(swf_path) ? swf_path : Panel_Swf_Path);
    double const skin_inset_fraction = ini.GetDoubleValue("General", "PanelSkinInsetFraction", Default_Panel_Skin_Inset_Fraction);
    config.panel_skin_inset_fraction = is_usable_panel_skin_inset_fraction(skin_inset_fraction) ? skin_inset_fraction : Default_Panel_Skin_Inset_Fraction;
    double const camera_fov = ini.GetDoubleValue("General", "CameraFov", Default_Camera_Fov);
    config.camera_fov = is_usable_camera_fov(camera_fov) ? camera_fov : Default_Camera_Fov;
    double const camera_distance = ini.GetDoubleValue("General", "CameraDistance", Default_Camera_Distance);
    config.camera_distance = is_usable_camera_distance(camera_distance) ? camera_distance : Default_Camera_Distance;
    double const alpha_test_threshold = ini.GetDoubleValue("General", "AlphaTestThreshold", Default_Alpha_Test_Threshold);
    config.alpha_test_threshold = is_usable_alpha_test_threshold(alpha_test_threshold) ? alpha_test_threshold : Default_Alpha_Test_Threshold;

    // The three absolute pixel keys this module used before are no longer read. Their presence is
    // reported once so an existing INI's stale values are visible instead of being silently obeyed;
    // the keys themselves are left in the file, because deleting a user's configuration is not this
    // module's decision.
    if (ini.KeyExists("General", "PanelWidth") || ini.KeyExists("General", "PanelHeight") ||
        ini.KeyExists("General", "PanelRightMargin"))
        logger::warn("PlayerPanel.ini still sets PanelWidth/PanelHeight/PanelRightMargin; those absolute pixel keys are ignored now, and the panel is sized from PanelHeightFraction, PanelAspect and PanelMarginFraction");

    // The same treatment for the cursor-sensitivity key: the panel follows the game's own menu cursor
    // now, so there is no plugin-side cursor left for a tuning constant to scale.
    if (ini.KeyExists("General", "PanelCursorSensitivity"))
        logger::warn("PlayerPanel.ini still sets PanelCursorSensitivity; that key is ignored now, because the panel uses the game's own menu cursor instead of an integrated one");

    std::lock_guard config_lock(m_config_mutex);
    m_config = config;
}

void Setting::save()
{
    std::lock_guard file_lock(m_file_mutex);
    auto const path = get_config_path();
    CSimpleIniA ini;
    ini.SetUnicode();
    if (std::filesystem::exists(path) && ini.LoadFile(path.c_str()) < 0)
    {
        logger::warn("Cannot read existing INI; refusing to overwrite it");
        return;
    }
    auto const config = get_config();
    ini.SetBoolValue("General", "Enabled", config.enabled);
    ini.SetLongValue("General", "Hotkey", static_cast<long>(config.hotkey));
    ini.SetDoubleValue("General", "PreviewDistance", config.preview_distance);
    ini.SetDoubleValue("General", "PanelHeightFraction", config.panel_height_fraction);
    ini.SetDoubleValue("General", "PanelAspect", config.panel_aspect);
    ini.SetDoubleValue("General", "PanelMarginFraction", config.panel_margin_fraction);
    ini.SetDoubleValue("General", "PanelPositionX", config.panel_position_x);
    ini.SetDoubleValue("General", "PanelPositionY", config.panel_position_y);
    ini.SetValue("General", "PanelSwfPath", config.panel_swf_path);
    ini.SetDoubleValue("General", "PanelSkinInsetFraction", config.panel_skin_inset_fraction);
    ini.SetDoubleValue("General", "CameraFov", config.camera_fov);
    ini.SetDoubleValue("General", "CameraDistance", config.camera_distance);
    ini.SetDoubleValue("General", "AlphaTestThreshold", config.alpha_test_threshold);
    std::filesystem::create_directories(path.parent_path());
    if (ini.SaveFile(path.c_str()) < 0)
        logger::warn("Failed to save INI");
}

PLUGIN_NAMESPACE_END
