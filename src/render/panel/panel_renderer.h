//
// Created by AmazingBuff on 2026/09/21.
//

#pragma once

#include "config/config.h"
#include "render/panel/panel_camera.h"
#include "render/panel/panel_geometry.h"
#include "render/panel/panel_passes.h"
#include "render/panel/panel_target.h"

#include <REX/W32/D3D11.h>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

PLUGIN_NAMESPACE_BEGIN

// The panel rectangle in render pixels. It is resolved by a pure function of the render size and the
// configuration, so the game thread (hit test and drag) and the render thread (composite viewport)
// agree without sharing any state.
struct PanelLayout
{
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
};

// Resolves the panel rectangle. The height is the configured fraction of the render height, the width
// follows from the configured aspect and the margin is a configured fraction of the render height, so
// the panel reads the same on 16:9, 21:9 and 32:9 screens and only the world around it grows. Both
// dimensions are clamped to an absolute minimum and to the screen. A set position is the configured
// fraction of the free space on that axis, which is what makes a dragged position resolution
// independent; an unset axis keeps the right-anchored, vertically centred default. Returns false for a
// degenerate render size or configuration, in which case the caller must skip the frame instead of
// building a zero-sized target.
[[nodiscard]] bool resolve_panel_layout(Config const& config, uint32_t screen_width,
    uint32_t screen_height, PanelLayout& out);

// The built-in chrome's border thickness in render pixels, derived from the panel height so one
// hairline stays one hairline at every resolution. Never zero: a thickness that rounds away is not a
// border. This is the *built-in* chrome's decoration only; the inset the character is composited to
// while a skin owns the chrome is `resolve_skin_inset_thickness`, which is a separate configuration
// value and deliberately never a hairline.
[[nodiscard]] uint32_t resolve_border_thickness(uint32_t panel_height);

// The character's inset in render pixels while a skin owns the panel's chrome. It is a fraction of the
// panel height, from its own configuration value and never from the built-in hairline, so one frame
// stays the same apparent frame at every resolution, and it is floored in pixels so even a tiny panel
// gets a visible frame rather than a hairline. It is also the band the composite discards, so a skin's
// frame is never overdrawn: a skin authors its frame to be at least this thick. The result never takes
// more than a quarter of either panel axis, so the character always keeps room to be drawn.
[[nodiscard]] uint32_t resolve_skin_inset_thickness(Config const& config, uint32_t panel_width,
    uint32_t panel_height);

// The panel renderer: the single consumer of PreviewActor that turns the preview into an on-screen
// window. It owns the private offscreen target, its own camera and the two passes, and it never
// creates, dresses or destroys the preview. Work is split across two threads: the game-thread
// listener collects this frame's meshes and camera, the render-thread present listener draws them
// inside the Present callback. When no frame is prepared neither side submits a draw call.
//
// The panel's chrome is not the plugin's own unless no skin is available: the panel is a registered
// Scaleform menu (src/render/panel/panel_menu.cpp) whose movie is loaded from an overridable path, and
// while that movie is loaded this renderer composites only the character, inset by the configured skin
// inset and alpha-blended over the chrome the engine has already drawn, so a skin supplies the panel's
// backdrop as well as its frame and both stay visible. The plugin's built-in chrome is the fallback,
// and it is what draws when the movie is missing or unloadable.
class PanelRenderer
{
public:
    static PanelRenderer& instance();
    PanelRenderer(PanelRenderer const&) = delete;
    PanelRenderer& operator=(PanelRenderer const&) = delete;

    static void on_frame();
    static void on_present(REX::W32::IDXGISwapChain* swap_chain);

    // Asks the render thread to release every D3D object the panel owns; the release happens on the
    // next present so all device calls stay on the render thread.
    void request_release();

    // Game-thread input entry point, called by the input sink. It only records the button edge;
    // prepare() turns it into a drag, so no engine state is touched inside the input sink. The cursor
    // position is not recorded here at all: it is read from the engine's own menu cursor each frame.
    static void on_left_button(bool down);

    // Game-thread: opens or closes the panel's claim on player input by showing or hiding the panel's
    // menu. Showing it is what makes the engine drive the cursor and push the menu's input context,
    // which is what keeps the camera from rotating while the panel is open; hiding it gives both back.
    // Idempotent, so prepare() may call it every frame and the session messages call it with `false` so
    // nothing the panel changed survives a load, even when no frame runs in between.
    void set_panel_open(bool open);

private:
    PanelRenderer();
    ~PanelRenderer();

    void prepare();
    void draw(REX::W32::IDXGISwapChain* swap_chain);
    void release();
    [[nodiscard]] bool ensure_device_objects(REX::W32::ID3D11Device* device);
    void apply_panel_input(PanelLayout const& layout, uint32_t screen_width, uint32_t screen_height,
        float cursor_x, float cursor_y);
    void commit_panel_position(PanelLayout const& layout, uint32_t screen_width,
        uint32_t screen_height, float cursor_x, float cursor_y);

private:
    // Handoff between the game and render threads. The game thread collects into m_prepare_draws and
    // swaps it with m_draws; the render thread swaps m_draws with m_render_draws. Each of the three
    // buffers keeps its capacity, so a steady-state frame allocates nothing.
    std::mutex m_mutex;
    std::vector<PanelDraw> m_prepare_draws;
    std::vector<PanelDraw> m_draws;
    std::vector<PanelDraw> m_render_draws;
    PanelCameraFrame m_camera;
    bool m_frame_ready;
    // Which chrome this prepared frame belongs to. It travels with the frame because draw() chooses
    // the offscreen clear colour, the inset the character is clipped to and the composite's blend
    // state and chrome flag from it.
    bool m_built_in_chrome;
    std::atomic<bool> m_release_requested;

    // Game-thread only: the input sink records here and prepare() applies. No lock is needed, because
    // both run on the game thread and the render thread never reads any of it.
    //
    // The drag is expressed in render pixels, the same unit as the panel rectangle and as the engine
    // menu cursor the drag follows, so the hit test compares like with like.
    float m_grab_offset_x;
    float m_grab_offset_y;
    bool m_left_button_down;
    bool m_press_pending;
    bool m_drag_active;
    bool m_panel_open;
    // Frames spent waiting for the engine to create the menu, and whether the give-up fallback has
    // been reported, so a menu that never appears costs one log line and not one per frame.
    uint32_t m_chrome_pending_frames;
    bool m_chrome_fallback_logged;
    // One-shot diagnostics per panel-open session: chrome mode, collected draw count and the first
    // composite, plus one line for each silent-failure branch, so an invisible panel is diagnosable
    // from the log alone.
    bool m_session_diagnostics_done;
    // The draw-side half of the one-shot diagnostics, kept separate from m_session_diagnostics_done:
    // prepare() sets that flag first, so a shared flag would suppress the composite's own line forever
    // and leave a silent composite failure unprovable from the log.
    bool m_first_composite_logged;
    // One-shot per-session content dump of the offscreen target and of the composed back buffer, as
    // TGA files beside the log. The panel showed an empty window while every upstream check passed,
    // so the images answer from the pixels what the log cannot: whether the geometry pass reaches the
    // target at all, and whether the composite's write survives onto the back buffer. A second dump
    // at composited frame 90 revisits the target after the palette latch (the head and body only
    // reach the collection after it).
    bool m_target_dumped;
    bool m_second_dumped;
    // Render-thread frame counter for the second dump.
    uint32_t m_composited_frames;
    // Whether the panel is on screen, mirrored for the render thread: the redraw-last fallback (an
    // empty collection redraws the previous frame's draws) applies only while the panel is open -
    // after close it would keep painting a window the plugin has already given back.
    std::atomic<bool> m_chrome_active;
    // Latches the first per-session shortfall (no 3D yet, or no drawable geometry) so the warning
    // costs one log line instead of one per frame while the preview's 3D is still loading.
    bool m_shortfall_logged;
    // Game-thread only. The world-copy cull waits for the engine to render the preview once: a skin
    // instance's bone-matrix count is latched at its first render submission, and a world copy culled
    // from its first frame is never submitted, which leaves the count at zero and the palette gate
    // rejects every skinned mesh. The latch reports whether that first submission has happened (one
    // collection with skinned draws), and the grace bounds how long an unrendered preview stays
    // visible before it is hidden anyway.
    bool m_world_copy_cull_active;
    uint32_t m_world_copy_cull_grace_frames;

    // Render-thread owned: only draw() and release() touch these.
    //
    // The panel deliberately holds no swap-chain reference between frames. The back buffer and its
    // render-target view are acquired and released inside each draw() call, because any outstanding
    // direct or indirect reference to a swap-chain buffer makes the engine's own ResizeBuffers fail
    // with DXGI_ERROR_INVALID_CALL when the player changes resolution or toggles windowed/fullscreen,
    // which would break the engine's rendering. Only the panel's own objects live here.
    REX::W32::ID3D11Device* m_ref_device;
    // The device whose shader compilation has already failed. The compilation reports its failure once
    // and is retried once per device and not once per frame: without this latch the retry, and its log
    // line, would repeat on every frame the panel draws.
    REX::W32::ID3D11Device* m_ref_shader_failed_device;
    PanelTarget m_target;
    PanelGeometryPass m_geometry_pass;
    PanelCompositePass m_composite_pass;
    bool m_ready;
    // Latches a back-buffer acquisition failure so a persistent failure logs once, not every frame.
    bool m_back_buffer_error_logged;
};

PLUGIN_NAMESPACE_END
