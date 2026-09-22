//
// Created by AmazingBuff on 2026/09/21.
//

#include "panel_renderer.h"

#include "config/config.h"
#include "preview/preview_actor.h"
#include "render/dx11/d3d11_util.h"
#include "render/panel/panel_menu.h"
#include "render/shader_manager.h"

#include <algorithm>
#include <cmath>

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // The built-in chrome's fill: the same colour clears the private target and fills the rectangle on
    // the back buffer, so the window never lets world content through and the unfilled part of the
    // target still reads as panel background around the character. It is the built-in chrome only; a
    // loaded skin supplies its own backdrop.
    constexpr float Panel_Background_Color[4] = { 0.05f, 0.05f, 0.06f, 1.0f };

    // The private target's clear colour while a skin owns the chrome: the same fill colour, but fully
    // transparent. The character is then blended over the chrome the engine has already drawn in its UI
    // pass, and the parts of the panel the character does not cover keep a zero alpha, so the composite
    // writes nothing there and the skin's backdrop and frame stay visible. Clearing transparent is what
    // makes the skin useful: an opaque clear would hide it behind the character's rectangle.
    constexpr float Panel_Skin_Clear_Color[4] = { 0.05f, 0.05f, 0.06f, 0.0f };

    // The built-in chrome's single decoration for now: one hairline border in a neutral grey that
    // reads against both the dark fill and the world behind it. A skin's movie replaces it.
    constexpr float Panel_Border_Color[4] = { 0.38f, 0.38f, 0.42f, 1.0f };

    // The built-in chrome's border thickness as a fraction of the panel height, so one hairline stays
    // one hairline at every resolution and aspect ratio. The floor keeps a hairline existent when the
    // rounding would otherwise drop it. This is the built-in decoration only: the skin inset is its own
    // configuration value, because a hairline is not a frame a skin can be seen in.
    constexpr float Panel_Border_Thickness_Fraction = 0.002f;
    constexpr uint32_t Min_Border_Thickness = 1;

    // Absolute pixel floor for the character's inset while a skin owns the chrome. The configured
    // fraction resolves to a visibly thicker frame than the built-in hairline at any realistic panel
    // size; the floor covers a panel so short that the fraction alone would round back down to a
    // hairline, which is exactly the outcome the skin inset exists to avoid.
    constexpr uint32_t Min_Panel_Skin_Inset_Pixels = 6;

    // Ceiling for the skin inset as a divisor of the panel's tighter axis: the inset never takes more
    // than a quarter of it, so at least half of that axis is left for the character. It only binds on an
    // extreme panel aspect, where the configuration has already asked for a very narrow panel.
    constexpr uint32_t Panel_Skin_Inset_Axis_Divisor = 4;

    // Absolute floor for the panel size in render pixels. Below it the offscreen target is degenerate
    // and the character inside it unreadable, so the size is clamped up instead of being used.
    constexpr uint32_t Min_Panel_Pixels = 64;

    // How many frames the panel waits for the engine to create its menu before giving up on the skin.
    // The engine creates the menu when it processes the show message, which is later than the frame
    // that queued it, so waiting keeps the built-in chrome from flashing for a frame; the bound is what
    // keeps a menu that never appears from leaving the panel invisible.
    constexpr uint32_t Max_Chrome_Pending_Frames = 8;

    // Rounds a fraction of an extent to whole render pixels and never exceeds the extent. Non-finite
    // input yields zero, so the conversion to pixels below can never be undefined even though the
    // configuration is validated at the INI boundary.
    uint32_t scale_to_pixels(double a_fraction, uint32_t a_extent)
    {
        if (!std::isfinite(a_fraction) || a_fraction <= 0.0)
            return 0u;
        double const scaled = static_cast<double>(a_extent) * a_fraction + 0.5;
        double const capped = (std::min)(scaled, static_cast<double>(a_extent));
        return static_cast<uint32_t>(capped);
    }
}

bool resolve_panel_layout(Config const& a_config, uint32_t a_screen_width, uint32_t a_screen_height,
    PanelLayout& a_out)
{
    if (a_screen_width == 0 || a_screen_height == 0)
        return false;

    // Sizing is a function of the render height and the configuration only: the width follows from the
    // height and the configured aspect. The screen width is an upper clamp and nothing else, which is
    // exactly why a wider monitor shows more world rather than a bigger panel.
    uint32_t const height = (std::max)(scale_to_pixels(a_config.panel_height_fraction, a_screen_height),
        (std::min)(Min_Panel_Pixels, a_screen_height));
    uint32_t const width = (std::max)(scale_to_pixels(a_config.panel_aspect * static_cast<double>(height), a_screen_width),
        (std::min)(Min_Panel_Pixels, a_screen_width));
    uint32_t const margin = scale_to_pixels(a_config.panel_margin_fraction, a_screen_height);

    uint32_t const free_x = a_screen_width - width;
    uint32_t const free_y = a_screen_height - height;

    // A placed axis is a fraction of that axis' free space, so the same relative spot is reproduced at
    // any resolution. An unplaced axis keeps the default: right-anchored by the margin, vertically
    // centred.
    uint32_t const x = a_config.panel_position_x >= 0.0
        ? scale_to_pixels(free_x * (std::min)(a_config.panel_position_x, 1.0), free_x)
        : (a_screen_width > width + margin ? a_screen_width - width - margin : 0u);
    uint32_t const y = a_config.panel_position_y >= 0.0
        ? scale_to_pixels(free_y * (std::min)(a_config.panel_position_y, 1.0), free_y)
        : free_y / 2u;

    a_out.x = x;
    a_out.y = y;
    a_out.width = width;
    a_out.height = height;
    return true;
}

uint32_t resolve_border_thickness(uint32_t a_panel_height)
{
    return (std::max)(scale_to_pixels(static_cast<double>(Panel_Border_Thickness_Fraction), a_panel_height),
        Min_Border_Thickness);
}

uint32_t resolve_skin_inset_thickness(Config const& a_config, uint32_t a_panel_width, uint32_t a_panel_height)
{
    // The skin inset has its own configuration value; it is never the built-in hairline. The panel
    // height is the basis, so the frame keeps the same apparent thickness at every resolution, and the
    // absolute floor keeps it a frame even on a very short panel.
    uint32_t const preferred = (std::max)(scale_to_pixels(a_config.panel_skin_inset_fraction, a_panel_height),
        Min_Panel_Skin_Inset_Pixels);

    uint32_t const room = (std::min)(a_panel_width, a_panel_height) / Panel_Skin_Inset_Axis_Divisor;
    return room > 0 ? (std::min)(preferred, room) : preferred;
}

PanelRenderer& PanelRenderer::instance()
{
    static PanelRenderer s_instance;
    return s_instance;
}

PanelRenderer::PanelRenderer() :
    m_mutex(),
    m_prepare_draws(),
    m_draws(),
    m_render_draws(),
    m_camera{},
    m_alpha_test(0.0f),
    m_frame_ready(false),
    m_built_in_chrome(true),
    m_release_requested(false),
    m_grab_offset_x(0.0f),
    m_grab_offset_y(0.0f),
    m_left_button_down(false),
    m_press_pending(false),
    m_drag_active(false),
    m_panel_open(false),
    m_chrome_pending_frames(0),
    m_chrome_fallback_logged(false),
    m_ref_device(nullptr),
    m_ref_shader_failed_device(nullptr),
    m_target(),
    m_geometry_pass(),
    m_composite_pass(),
    m_ready(false),
    m_back_buffer_error_logged(false) {}

PanelRenderer::~PanelRenderer() = default;

void PanelRenderer::on_frame()
{
    try
    {
        instance().prepare();
    }
    catch (...)
    {
        PanelRenderer& self = instance();
        {
            std::lock_guard lock(self.m_mutex);
            self.m_frame_ready = false;
        }
        try { logger::error("Panel frame preparation failed; the panel frame was dropped"); } catch (...) {}
    }
}

void PanelRenderer::on_present(REX::W32::IDXGISwapChain* a_swap_chain)
{
    instance().draw(a_swap_chain);
}

void PanelRenderer::request_release()
{
    m_release_requested.store(true, std::memory_order_release);
}

void PanelRenderer::on_left_button(bool a_down)
{
    PanelRenderer& self = instance();

    // The press edge is only meaningful while the panel is on screen: a button already held down when
    // the panel opens or closes must not turn into a drag of its own.
    if (a_down && !self.m_left_button_down && self.m_panel_open)
        self.m_press_pending = true;

    self.m_left_button_down = a_down;
}

void PanelRenderer::set_panel_open(bool a_open)
{
    if (a_open == m_panel_open)
        return;

    m_panel_open = a_open;

    // Everything the input sink recorded belongs to the side of the transition it happened on, and the
    // physical button state is the one thing that must survive it: a press while the panel was hidden
    // must not start a drag, and no drag may stay latched while the panel is hidden.
    m_press_pending = false;
    m_drag_active = false;
    m_chrome_pending_frames = 0;
    m_chrome_fallback_logged = false;

    // The menu is the panel's whole claim on player input. Showing it is what makes the engine drive
    // its own cursor and push the menu's input context, and hiding it is what takes both back; the
    // plugin therefore freezes no control group and marshals no cursor of its own.
    PanelMenu::set_open(a_open);
    if (a_open)
        logger::info("Panel open: the game's own menu cursor and input context are in use");
    else
        logger::info("Panel closed: the menu is hidden and the cursor and input context are given back");
}

void PanelRenderer::prepare()
{
    RE::TESObjectREFR* const preview = PreviewActor::instance().current_reference();
    Config const config = Setting::instance().get_config();
    RE::BSGraphics::ScreenSize const screen = RE::BSGraphics::Renderer::GetScreenSize();

    PanelLayout layout{};
    bool const open = preview != nullptr && resolve_panel_layout(config, screen.width, screen.height, layout);

    // The panel claims player input exactly while it is on screen, so its menu follows this one
    // transition and every early return below leaves that state consistent.
    set_panel_open(open);
    if (!open)
    {
        std::lock_guard lock(m_mutex);
        m_frame_ready = false;
        return;
    }

    // The engine creates the menu when it processes the show message, which happens after this frame
    // has been prepared. Until it reports a verdict the panel draws nothing rather than painting the
    // built-in chrome over a skin that is about to appear.
    PanelChrome const chrome = PanelMenu::chrome_mode();
    if (chrome == PanelChrome::e_pending && m_chrome_pending_frames < Max_Chrome_Pending_Frames)
    {
        ++m_chrome_pending_frames;
        std::lock_guard lock(m_mutex);
        m_frame_ready = false;
        return;
    }

    bool const built_in_chrome = chrome != PanelChrome::e_swf;
    if (built_in_chrome && chrome == PanelChrome::e_pending && !m_chrome_fallback_logged)
    {
        m_chrome_fallback_logged = true;
        logger::warn("Panel: the menu was not created, so the panel uses its built-in chrome");
    }

    // The movie is scaled into exactly the panel rectangle, so a skin's stage-filling frame covers it
    // and the skin needs no script.
    if (!built_in_chrome)
        PanelMenu::set_viewport(screen.width, screen.height, layout.x, layout.y, layout.width, layout.height);

    // The single world-copy visibility change of this change set: the preview is shown in the panel
    // and nowhere else, so the engine's own cull switch hides its in-world 3D root. Reapplied every
    // frame because the engine may rebuild the 3D root.
    if (RE::NiAVObject* const root = preview->GetCurrent3D())
        root->SetAppCulled(true);

    // The drag is applied against this frame's rectangle, with the cursor the engine is driving for
    // the panel's own menu, and the position it commits is the one the next frame's rectangle is
    // resolved from. An unavailable cursor leaves the panel where it is instead of moving it.
    float cursor_x = 0.0f;
    float cursor_y = 0.0f;
    if (PanelMenu::read_menu_cursor(screen.width, screen.height, cursor_x, cursor_y))
        apply_panel_input(layout, screen.width, screen.height, cursor_x, cursor_y);

    PanelCameraFrame camera{};
    if (!PanelCamera::build(*preview, config, camera))
    {
        std::lock_guard lock(m_mutex);
        m_frame_ready = false;
        return;
    }

    // Collection runs outside the lock: m_prepare_draws is touched only by this thread.
    collect_panel_geometry(*preview, m_prepare_draws);

    std::lock_guard lock(m_mutex);
    m_draws.swap(m_prepare_draws);
    if (m_draws.empty())
    {
        m_frame_ready = false;
        return;
    }

    m_camera = camera;
    m_alpha_test = static_cast<float>(config.alpha_test_threshold);
    m_built_in_chrome = built_in_chrome;
    m_frame_ready = true;
}

void PanelRenderer::apply_panel_input(PanelLayout const& a_layout, uint32_t a_screen_width,
    uint32_t a_screen_height, float a_cursor_x, float a_cursor_y)
{
    if (m_press_pending)
    {
        m_press_pending = false;
        // The whole panel surface is the drag handle: nothing else on the panel consumes mouse input.
        if (a_cursor_x >= static_cast<float>(a_layout.x) &&
            a_cursor_x < static_cast<float>(a_layout.x) + static_cast<float>(a_layout.width) &&
            a_cursor_y >= static_cast<float>(a_layout.y) &&
            a_cursor_y < static_cast<float>(a_layout.y) + static_cast<float>(a_layout.height))
        {
            m_drag_active = true;
            // The offset inside the panel is kept, so the panel does not jump under the cursor.
            m_grab_offset_x = a_cursor_x - static_cast<float>(a_layout.x);
            m_grab_offset_y = a_cursor_y - static_cast<float>(a_layout.y);
        }
    }

    if (!m_drag_active)
        return;

    if (!m_left_button_down)
    {
        m_drag_active = false;
        return;
    }

    commit_panel_position(a_layout, a_screen_width, a_screen_height, a_cursor_x, a_cursor_y);
}

void PanelRenderer::commit_panel_position(PanelLayout const& a_layout, uint32_t a_screen_width,
    uint32_t a_screen_height, float a_cursor_x, float a_cursor_y)
{
    // The requested top-left is stored as the fraction of the free space on each axis, so the panel
    // lands at the same relative spot at any resolution. Clamping the fraction is what keeps the panel
    // from leaving the screen, and it happens at the single writer of the stored position.
    uint32_t const free_x = a_screen_width - a_layout.width;
    uint32_t const free_y = a_screen_height - a_layout.height;
    float const requested_x = a_cursor_x - m_grab_offset_x;
    float const requested_y = a_cursor_y - m_grab_offset_y;
    double const normalized_x = free_x > 0
        ? static_cast<double>(requested_x) / static_cast<double>(free_x)
        : 0.0;
    double const normalized_y = free_y > 0
        ? static_cast<double>(requested_y) / static_cast<double>(free_y)
        : 0.0;
    Setting::instance().set_panel_position(normalized_x, normalized_y);
}

void PanelRenderer::draw(REX::W32::IDXGISwapChain* a_swap_chain)
{
    // A requested release is serviced here so every D3D call stays on the render thread.
    if (m_release_requested.exchange(false, std::memory_order_acq_rel))
    {
        release();
        return;
    }

    PanelCameraFrame camera{};
    float alpha_test = 0.0f;
    bool built_in_chrome = true;
    {
        std::lock_guard lock(m_mutex);
        if (!m_frame_ready)
            return;

        // Steal the prepared frame; the buffer stays with the panel and comes back on the next swap.
        m_draws.swap(m_render_draws);
        m_frame_ready = false;
        camera = m_camera;
        alpha_test = m_alpha_test;
        built_in_chrome = m_built_in_chrome;
    }

    RE::BSGraphics::Renderer* const renderer = RE::BSGraphics::Renderer::GetSingleton();
    if (!renderer)
        return;

    REX::W32::ID3D11Device* const device = renderer->GetRuntimeData().forwarder;
    REX::W32::ID3D11DeviceContext* const context = renderer->GetRuntimeData().context;
    if (!device || !context)
        return;

    if (!ensure_device_objects(device))
        return;

    Config const config = Setting::instance().get_config();
    RE::BSGraphics::ScreenSize const screen = RE::BSGraphics::Renderer::GetScreenSize();

    // The rectangle is the same pure function of the render size and the configuration that the
    // game-thread drag hit-tests against; a degenerate render size skips the frame here too, so no
    // zero-sized target is ever built.
    PanelLayout layout{};
    if (!resolve_panel_layout(config, screen.width, screen.height, layout))
        return;

    if (!m_target.matches(device, layout.width, layout.height))
    {
        m_target.release();
        if (!m_target.init(device, layout.width, layout.height))
            return;
    }

    // Report the first failure of a streak and stay quiet afterwards: a persistent failure must not
    // log a line every frame, and nothing here retries or spins.
    auto const report_failure = [this](REX::W32::HRESULT a_result)
    {
        if (m_back_buffer_error_logged)
            return;
        logger::error("Panel: the swap-chain back buffer or its view is unavailable ({:X}); the panel is skipped this frame", static_cast<unsigned int>(a_result));
        m_back_buffer_error_logged = true;
    };

    // The back buffer and its view are acquired for this frame only and released again before draw()
    // returns. Caching either across frames would hold a swap-chain reference and make the engine's
    // own ResizeBuffers fail with DXGI_ERROR_INVALID_CALL on a resolution change, so the panel never
    // keeps them (see the header comment on the render-thread members).
    REX::W32::ID3D11Texture2D* back_buffer = nullptr;
    REX::W32::HRESULT const buffer_hr = a_swap_chain->GetBuffer(0, REX::W32::IID_ID3D11Texture2D, reinterpret_cast<void**>(&back_buffer));
    if (!REX::W32::SUCCESS(buffer_hr) || !back_buffer)
    {
        report_failure(buffer_hr);
        return;
    }

    REX::W32::ID3D11RenderTargetView* back_buffer_rtv = nullptr;
    REX::W32::HRESULT const rtv_hr = device->CreateRenderTargetView(back_buffer, nullptr, &back_buffer_rtv);
    if (!REX::W32::SUCCESS(rtv_hr) || !back_buffer_rtv)
    {
        report_failure(rtv_hr);
        back_buffer->Release();
        return;
    }
    m_back_buffer_error_logged = false;

    // Everything the panel overwrites is put back afterwards, so the engine's own draw of the same
    // frame is unaffected.
    D3D11StateCapture capture(context);
    capture.capture();

    // The chrome mode decides both the offscreen clear and the inset the character is clipped to. The
    // built-in chrome clears opaque and insets by its own hairline, exactly as before. A skin clears
    // fully transparent so the composite only writes where the character actually is, and insets by its
    // own configured value, so the skin's frame and backdrop are both left visible.
    float const* const clear_color = built_in_chrome ? Panel_Background_Color : Panel_Skin_Clear_Color;
    uint32_t const inset = built_in_chrome
        ? resolve_border_thickness(layout.height)
        : resolve_skin_inset_thickness(config, layout.width, layout.height);

    m_geometry_pass.draw(device, context, m_target, camera, alpha_test, clear_color, m_render_draws);

    REX::W32::D3D11_VIEWPORT const rectangle{
        .topLeftX = static_cast<float>(layout.x),
        .topLeftY = static_cast<float>(layout.y),
        .width = static_cast<float>(layout.width),
        .height = static_cast<float>(layout.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    // The composite owns the panel's chrome decision: with a loaded skin it draws only the character,
    // alpha-blended and inset by the skin inset, because the engine has already drawn the skin's
    // chrome there and this pass runs on top of it. Without one it draws the built-in fill and hairline
    // as before.
    m_composite_pass.draw(context, back_buffer_rtv, m_target.srv(), rectangle, Panel_Background_Color,
        Panel_Border_Color, inset, built_in_chrome);

    capture.restore();

    // Released only after the state capture has restored the engine's own targets, so the engine's
    // draw of this frame never dangles and the swap chain carries no reference between frames.
    back_buffer_rtv->Release();
    back_buffer->Release();
}

bool PanelRenderer::ensure_device_objects(REX::W32::ID3D11Device* a_device)
{
    if (m_ready && m_ref_device == a_device)
        return true;

    // The panel's shaders are compiled here, on the first frame that has a device, because a pass built
    // from the manager's null accessors would fail and be rebuilt every frame with nothing to show for
    // it. The compilation is idempotent, so this only does work until it has succeeded. A failure is
    // remembered for that device: it has already been logged once, and the frame is skipped rather than
    // retried, which is why the latch is checked before the per-frame release() below and not cleared
    // by it. A different device, or an explicit panel release, is a fresh attempt.
    if (m_ref_shader_failed_device == a_device)
        return false;

    release();

    if (!ShaderManager::instance().compile())
    {
        m_ref_shader_failed_device = a_device;
        return false;
    }

    if (!m_geometry_pass.init(a_device) || !m_composite_pass.init(a_device))
        return false;

    m_ref_device = a_device;
    m_ready = true;
    return true;
}

void PanelRenderer::release()
{
    {
        std::lock_guard lock(m_mutex);
        m_frame_ready = false;
        m_draws.clear();
    }

    // Nothing back-buffer related to release here: the panel holds no swap-chain reference between
    // frames, so only its own objects need tearing down. This runs on the render thread and therefore
    // touches no engine or Scaleform state; the game-thread prepare() and the session messages own the
    // menu and therefore the cursor and the input context.
    m_target.release();
    m_geometry_pass.release();
    m_composite_pass.release();
    m_ref_device = nullptr;
    // Forgetting the failed compilation is what lets a later device, or the next panel session, try
    // again; the latch exists only to stop the retry from repeating once per frame.
    m_ref_shader_failed_device = nullptr;
    m_ready = false;
    m_back_buffer_error_logged = false;
}

PLUGIN_NAMESPACE_END
