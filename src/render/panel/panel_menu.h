//
// Created by AmazingBuff on 2026/09/21.
//

#pragma once

#include <cstdint>

PLUGIN_NAMESPACE_BEGIN

// The one name the panel's menu is registered and shown under. A menu is identified by its map key,
// because IMenu carries no name field on SE/AE.
inline constexpr char Panel_Menu_Name[] = "PlayerPanelMenu";

// Which chrome the panel draws this frame.
enum class PanelChrome : uint8_t
{
    e_pending,   // The engine has not created the menu yet, so nothing is drawn for this frame.
    e_swf,       // The menu's movie is loaded: the SWF owns the panel's frame and background.
    e_built_in   // No usable movie: the plugin paints its own chrome.
};

// The panel as a real, engine-registered Scaleform menu. Registering it is what gives the panel the
// engine's own cursor and input context, and loading its movie from a caller-chosen path is what makes
// the panel's chrome a file a reskin mod can replace, exactly the way a vanilla menu reskin replaces a
// menu's SWF. Both the registration and every call that touches Scaleform run on the game thread.
class PanelMenu final : public RE::IMenu
{
public:
    ~PanelMenu() override = default;

    // Registers the creator once. Idempotent, so the session messages may call it on every load.
    static void install();

    // Queues the engine's own show or hide for the panel's menu. Showing the menu is what puts the
    // engine's cursor and the menu's input context in place, and hiding it is what gives both back, so
    // the plugin never touches the cursor or the control state itself.
    static void set_open(bool open);

    // What the panel may draw this frame. The engine creates the menu when it processes the show
    // message, so this reports `e_pending` until that has happened.
    [[nodiscard]] static PanelChrome chrome_mode();

    // Scales the menu's movie into the panel rectangle, in render pixels. The caller sets its own
    // boundaries and the movie's stage is fitted to them, which is what lets a skin supply art for a
    // frame that fills its own stage and needs no script at all.
    static void set_viewport(uint32_t buffer_width, uint32_t buffer_height, uint32_t left,
        uint32_t top, uint32_t width, uint32_t height);

    // The engine's own menu cursor converted into render pixels, which is the unit the panel rectangle
    // is expressed in. Returns false while the engine cursor or its screen extents are unavailable, in
    // which case the caller must not move the panel.
    [[nodiscard]] static bool read_menu_cursor(uint32_t render_width, uint32_t render_height,
        float& out_x, float& out_y);

private:
    // The engine's creator ABI hands out a raw pointer that carries the reference the engine stores in
    // its menu map, so the instance lives for the session and every later show reuses the same movie.
    static RE::IMenu* create();

    // The menu the engine created, or nullptr while it has not created one.
    [[nodiscard]] static PanelMenu* current();

    PanelMenu();

    bool m_movie_loaded;
};

PLUGIN_NAMESPACE_END
