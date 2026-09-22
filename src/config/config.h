#pragma once
#include <cstddef>
#include <cstdint>
#include <mutex>

PLUGIN_NAMESPACE_BEGIN

// The panel's chrome comes from this file. It is the panel's public interface: the documented path as
// a mod author sees it, inside the game's Data folder, so shipping a file here restyles the panel the
// way a vanilla menu reskin replaces a menu's SWF. The engine's movie loader resolves its own argument
// against the interface folder and appends the extension itself, so this documented path is turned
// into the loader's own form in exactly one place (src/render/panel/panel_menu.cpp).
inline constexpr char Panel_Swf_Path[] = "Interface\\PlayerPanel\\panel.swf";

// Longest accepted skin path. Keeping the path in the configuration as a fixed array keeps Config a
// plain value type that a frame can copy without allocating.
inline constexpr std::size_t Max_Panel_Swf_Path = 128;

// Values are filled by Setting's constructor and by Setting::load before first use; the struct
// itself carries no field initializers.
struct Config
{
    bool enabled;
    std::uint32_t hotkey;
    double preview_distance;

    // Panel presentation. The size and the margin are fractions of the render height, so the panel
    // reads the same on 16:9, 21:9 and 32:9 screens, and the position is the fraction of the free
    // space on each axis so a dragged spot is reproduced at any resolution. A negative position
    // means "not placed yet" and yields the right-anchored, vertically centred default.
    double panel_height_fraction;
    double panel_aspect;
    double panel_margin_fraction;
    double panel_position_x;
    double panel_position_y;

    // The path of the SWF the panel's chrome is loaded from, in the documented form. Replacing the
    // file at the default value is the supported way to restyle the panel; setting this key to another
    // path only exercises the same plumbing with a different file.
    char panel_swf_path[Max_Panel_Swf_Path];

    // The character's inset from the panel rectangle while a skin owns the chrome, as a fraction of
    // the panel height. It is deliberately its own value rather than the built-in hairline: a skin
    // authors its frame to be at least this thick, and the character is composited inside that frame.
    // The renderer adds an absolute pixel floor, so this fraction can never resolve to a hairline.
    double panel_skin_inset_fraction;

    double camera_fov;
    double camera_distance;
    double alpha_test_threshold;
};

class Setting
{
public:
    static Setting& instance();
    Setting(Setting const&) = delete;
    Setting& operator=(Setting const&) = delete;

    Config get_config() const;
    void load();
    void save();

    // Stores the panel's normalised position, clamping both axes to `0`..`1` so the value that can
    // reach the INI is always a fraction of the free space. Called by the panel drag on the game
    // thread; the next save() persists it.
    void set_panel_position(double a_x, double a_y);
private:
    Setting();
    ~Setting() = default;
    mutable std::mutex m_config_mutex;
    std::mutex m_file_mutex;
    Config m_config;
};

PLUGIN_NAMESPACE_END
