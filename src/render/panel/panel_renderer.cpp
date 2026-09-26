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

    // How many frames the world-copy cull waits for the preview to be rendered once before hiding it
    // anyway. The engine latches a skin instance's bone-matrix count at the skin's first render
    // submission, so the preview must be submitted at least once for the palette gate to pass; the
    // grace covers a preview whose 3D exists but that the player never faces. Kept short: the world
    // copy exists only for the engine to load and submit its 3D once, and every visible frame of it
    // is a duplicate of the player standing in the world (~0.2s at 60 fps).
    constexpr uint32_t Max_World_Copy_Grace_Frames = 12;

    // Rounds a fraction of an extent to whole render pixels and never exceeds the extent. Non-finite
    // input yields zero, so the conversion to pixels below can never be undefined even though the
    // configuration is validated at the INI boundary.
    uint32_t scale_to_pixels(double fraction, uint32_t extent)
    {
        if (!std::isfinite(fraction) || fraction <= 0.0)
            return 0u;
        double const scaled = static_cast<double>(extent) * fraction + 0.5;
        double const capped = (std::min)(scaled, static_cast<double>(extent));
        return static_cast<uint32_t>(capped);
    }

    // Rounds an already-pixel measure to whole render pixels and never exceeds the cap. The panel width
    // is the height times the aspect in pixels, not a fraction of some extent, so it does not go
    // through scale_to_pixels - whose extent is also its cap and would wrongly cap a landscape panel's
    // width at the panel height.
    uint32_t round_clamped_to_pixels(double pixels, uint32_t cap)
    {
        if (!std::isfinite(pixels) || pixels <= 0.0)
            return 0u;
        double const rounded = pixels + 0.5;
        double const capped = (std::min)(rounded, static_cast<double>(cap));
        return static_cast<uint32_t>(capped);
    }

    // Copies one GPU texture into a staging buffer and writes it as an uncompressed 32-bit TGA beside
    // the plugin log. A diagnostic only: the empty-window report needs the pixels themselves, because
    // every log-visible step around them already passed.
    void dump_texture_tga(REX::W32::ID3D11Device* device, REX::W32::ID3D11DeviceContext* context,
        REX::W32::ID3D11Texture2D* texture, std::string_view const& name)
    {
        REX::W32::D3D11_TEXTURE2D_DESC desc{};
        texture->GetDesc(&desc);
        if (desc.sampleDesc.count != 1)
            return;

        REX::W32::D3D11_TEXTURE2D_DESC staging_desc = desc;
        staging_desc.usage = REX::W32::D3D11_USAGE_STAGING;
        staging_desc.bindFlags = 0;
        staging_desc.cpuAccessFlags = REX::W32::D3D11_CPU_ACCESS_READ;
        staging_desc.miscFlags = 0;
        REX::W32::ID3D11Texture2D* staging = nullptr;
        REX::W32::HRESULT const create_hr = device->CreateTexture2D(&staging_desc, nullptr, &staging);
        if (!REX::W32::SUCCESS(create_hr) || !staging)
            return;

        context->CopyResource(staging, texture);
        REX::W32::D3D11_MAPPED_SUBRESOURCE mapped{};
        REX::W32::HRESULT const map_hr = context->Map(staging, 0, REX::W32::D3D11_MAP_READ, 0, &mapped);
        if (!REX::W32::SUCCESS(map_hr) || !mapped.data)
        {
            staging->Release();
            return;
        }

        std::optional<std::filesystem::path> directory = logger::log_directory();
        if (directory)
        {
            std::filesystem::path const path = *directory / (std::string(name) + ".tga");
            std::ofstream file(path, std::ios::binary);
            if (file)
            {
                std::uint8_t header[18]{};
                header[2] = 2;  // uncompressed truecolour
                header[12] = static_cast<std::uint8_t>(desc.width & 0xFF);
                header[13] = static_cast<std::uint8_t>((desc.width >> 8) & 0xFF);
                header[14] = static_cast<std::uint8_t>(desc.height & 0xFF);
                header[15] = static_cast<std::uint8_t>((desc.height >> 8) & 0xFF);
                header[16] = 32;   // bits per pixel
                header[17] = 0x28; // top-down, 8 alpha bits
                file.write(reinterpret_cast<char const*>(header), sizeof(header));

                // TGA stores BGRA; DXGI R8G8B8A8 stores RGBA in memory, B8G8R8A8 stores BGRA.
                bool const rgba_memory = desc.format == REX::W32::DXGI_FORMAT_R8G8B8A8_UNORM ||
                                         desc.format == REX::W32::DXGI_FORMAT_R8G8B8A8_TYPELESS;
                for (std::uint32_t row = 0; row < desc.height; ++row)
                {
                    std::uint8_t const* row_bytes = static_cast<std::uint8_t const*>(mapped.data) +
                        static_cast<std::size_t>(row) * mapped.rowPitch;
                    for (std::uint32_t column = 0; column < desc.width; ++column)
                    {
                        std::uint32_t texel = 0;
                        std::memcpy(&texel, row_bytes + static_cast<std::size_t>(column) * 4u, sizeof(texel));
                        std::uint8_t bgra[4];
                        if (rgba_memory)
                        {
                            bgra[0] = static_cast<std::uint8_t>((texel >> 16) & 0xFF);
                            bgra[1] = static_cast<std::uint8_t>((texel >> 8) & 0xFF);
                            bgra[2] = static_cast<std::uint8_t>(texel & 0xFF);
                            bgra[3] = static_cast<std::uint8_t>((texel >> 24) & 0xFF);
                        }
                        else
                        {
                            bgra[0] = static_cast<std::uint8_t>(texel & 0xFF);
                            bgra[1] = static_cast<std::uint8_t>((texel >> 8) & 0xFF);
                            bgra[2] = static_cast<std::uint8_t>((texel >> 16) & 0xFF);
                            bgra[3] = static_cast<std::uint8_t>((texel >> 24) & 0xFF);
                        }
                        file.write(reinterpret_cast<char const*>(bgra), sizeof(bgra));
                    }
                }
                logger::info("Panel diagnostics: dumped {} ({}x{}) to {}", name, desc.width, desc.height, path.string());
            }
        }

        context->Unmap(staging, 0);
        staging->Release();
    }
}

bool resolve_panel_layout(Config const& config, uint32_t screen_width, uint32_t screen_height,
    PanelLayout& out)
{
    if (screen_width == 0 || screen_height == 0)
        return false;

    // Sizing is a function of the render height and the configuration only: the height is a fraction of
    // the render height, and the width follows from the height and the configured aspect. The screen
    // width is an upper clamp and nothing else, which is exactly why a wider monitor shows more world
    // rather than a bigger panel.
    uint32_t const height = (std::max)(scale_to_pixels(config.panel_height_fraction, screen_height),
        (std::min)(Min_Panel_Pixels, screen_height));
    uint32_t const width = (std::max)(round_clamped_to_pixels(config.panel_aspect * static_cast<double>(height), screen_width),
        (std::min)(Min_Panel_Pixels, screen_width));
    uint32_t const margin = scale_to_pixels(config.panel_margin_fraction, screen_height);

    uint32_t const free_x = screen_width - width;
    uint32_t const free_y = screen_height - height;

    // A placed axis is a fraction of that axis' free space, so the same relative spot is reproduced at
    // any resolution; the fraction is taken against the free space directly, which is the exact inverse
    // of commit_panel_position's normalisation. An unplaced axis keeps the default: right-anchored by
    // the margin, vertically centred.
    uint32_t const x = config.panel_position_x >= 0.0
        ? scale_to_pixels((std::min)(config.panel_position_x, 1.0), free_x)
        : (screen_width > width + margin ? screen_width - width - margin : 0u);
    uint32_t const y = config.panel_position_y >= 0.0
        ? scale_to_pixels((std::min)(config.panel_position_y, 1.0), free_y)
        : free_y / 2u;

    out.x = x;
    out.y = y;
    out.width = width;
    out.height = height;
    return true;
}

uint32_t resolve_border_thickness(uint32_t panel_height)
{
    return (std::max)(scale_to_pixels(static_cast<double>(Panel_Border_Thickness_Fraction), panel_height),
        Min_Border_Thickness);
}

uint32_t resolve_skin_inset_thickness(Config const& config, uint32_t panel_width, uint32_t panel_height)
{
    // The skin inset has its own configuration value; it is never the built-in hairline. The panel
    // height is the basis, so the frame keeps the same apparent thickness at every resolution, and the
    // absolute floor keeps it a frame even on a very short panel.
    uint32_t const preferred = (std::max)(scale_to_pixels(config.panel_skin_inset_fraction, panel_height),
        Min_Panel_Skin_Inset_Pixels);

    uint32_t const room = (std::min)(panel_width, panel_height) / Panel_Skin_Inset_Axis_Divisor;
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
    m_session_diagnostics_done(false),
    m_first_composite_logged(false),
    m_target_dumped(false),
    m_second_dumped(false),
    m_composited_frames(0),
    m_chrome_active(false),
    m_shortfall_logged(false),
    m_world_copy_cull_active(false),
    m_world_copy_cull_grace_frames(0),
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

void PanelRenderer::on_present(REX::W32::IDXGISwapChain* swap_chain)
{
    instance().draw(swap_chain);
}

void PanelRenderer::request_release()
{
    m_release_requested.store(true, std::memory_order_release);
}

void PanelRenderer::on_left_button(bool down)
{
    PanelRenderer& self = instance();

    // The press edge is only meaningful while the panel is on screen: a button already held down when
    // the panel opens or closes must not turn into a drag of its own.
    if (down && !self.m_left_button_down && self.m_panel_open)
        self.m_press_pending = true;

    self.m_left_button_down = down;
}

void PanelRenderer::set_panel_open(bool open)
{
    if (open == m_panel_open)
        return;

    m_panel_open = open;

    // Everything the input sink recorded belongs to the side of the transition it happened on, and the
    // physical button state is the one thing that must survive it: a press while the panel was hidden
    // must not start a drag, and no drag may stay latched while the panel is hidden.
    m_press_pending = false;
    m_drag_active = false;
    m_chrome_pending_frames = 0;
    m_chrome_fallback_logged = false;
    m_session_diagnostics_done = false;
    m_first_composite_logged = false;
    m_target_dumped = false;
    m_second_dumped = false;
    m_composited_frames = 0;
    m_chrome_active.store(open, std::memory_order_release);
    m_shortfall_logged = false;
    // Every open creates a fresh preview whose skins have not been submitted yet, so the
    // palette-readiness latch and its grace restart with the transition.
    m_world_copy_cull_active = false;
    m_world_copy_cull_grace_frames = 0;

    // The menu is the panel's whole claim on player input. Showing it is what makes the engine drive
    // its own cursor and push the menu's input context, and hiding it is what takes both back; the
    // plugin therefore freezes no control group and marshals no cursor of its own.
    PanelMenu::set_open(open);
    if (open)
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
    //
    // The cull waits for one rendered frame: the engine latches a skin instance's bone-matrix count at
    // the skin's first render submission, and a world copy culled from its first frame is never
    // submitted - measured as skinBoneCount=46 with skinNumMatrices=0, which the palette gate then
    // rejects in full and the panel loses the whole body. So the cull applies only once this session's
    // collection has carried skinned draws (the engine has therefore submitted the preview once), or
    // once the bounded grace expires, so a preview the player never faces is hidden anyway.
    if (RE::NiAVObject* const root = preview->GetCurrent3D())
    {
        if (m_world_copy_cull_active || m_world_copy_cull_grace_frames >= Max_World_Copy_Grace_Frames)
            root->SetAppCulled(true);
        else
            ++m_world_copy_cull_grace_frames;
    }
    else if (!m_shortfall_logged)
    {
        m_shortfall_logged = true;
        logger::warn("Panel diagnostics: the preview 3D is not available (3D loaded: {}, disabled: {}, at ({:.0f},{:.0f},{:.0f}))",
            preview->Is3DLoaded(), preview->IsDisabled(), preview->GetPosition().x,
            preview->GetPosition().y, preview->GetPosition().z);
    }

    // The drag is applied against this frame's rectangle, with the cursor the engine is driving for
    // the panel's own menu, and the position it commits is the one the next frame's rectangle is
    // resolved from. An unavailable cursor leaves the panel where it is instead of moving it.
    float cursor_x = 0.0f;
    float cursor_y = 0.0f;
    if (PanelMenu::read_menu_cursor(screen.width, screen.height, cursor_x, cursor_y))
        apply_panel_input(layout, screen.width, screen.height, cursor_x, cursor_y);

    // Collection runs outside the lock: m_prepare_draws is touched only by this thread - and the
    // camera is built FROM it, aiming at the geometry's real place (the reference's cached position
    // drifts from its 3D, measured 262 units after placement).
    collect_panel_geometry(*preview, config, m_prepare_draws);

    PanelAim aim{};
    if (!PanelCamera::resolve_body_aim(m_prepare_draws, aim))
    {
        if (!m_shortfall_logged)
        {
            m_shortfall_logged = true;
            logger::warn("Panel diagnostics: the collection offers no geometry to aim at");
        }
        std::lock_guard lock(m_mutex);
        m_frame_ready = false;
        return;
    }

    PanelCameraFrame camera{};
    if (!PanelCamera::build(*preview, aim, config, camera))
    {
        if (!m_session_diagnostics_done)
            logger::warn("Panel diagnostics: the camera could not be built");
        std::lock_guard lock(m_mutex);
        m_frame_ready = false;
        return;
    }

    // The palette-readiness latch: a collection that carries skinned draws means the engine has
    // submitted the preview once and the skin matrix counts are latched, so the world copy can be
    // culled from the next frame on (see the cull comment above).
    if (std::any_of(m_prepare_draws.begin(), m_prepare_draws.end(),
            [](PanelDraw const& draw) { return draw.skin != nullptr; }))
    {
        if (!m_world_copy_cull_active)
        {
            // One-shot aim diagnosis: where the camera stands versus where the first meshes'
            // transforms actually place them, so a content-out-of-frame report is checkable.
            m_world_copy_cull_active = true;
            std::string places;
            for (std::size_t i = 0; i < m_prepare_draws.size() && i < 4; ++i)
            {
                PanelDraw const& draw = m_prepare_draws[i];
                if (draw.node)
                {
                    RE::NiPoint3 const& t = draw.node->world.translate;
                    places += fmt::format(" [{} \"{}\" at ({:.0f},{:.0f},{:.0f})]", i,
                        draw.node->name.c_str() ? draw.node->name.c_str() : "?", t.x, t.y, t.z);
                }
            }
            logger::info("Panel latch: skinned draws ready; camera eye=({:.0f},{:.0f},{:.0f}) draws={} places={}",
                camera.eye.x, camera.eye.y, camera.eye.z, m_prepare_draws.size(), places);
        }
        m_world_copy_cull_active = true;
    }

    std::lock_guard lock(m_mutex);
    m_draws.swap(m_prepare_draws);
    if (m_draws.empty())
    {
        if (!m_shortfall_logged)
        {
            m_shortfall_logged = true;
            logger::warn("Panel diagnostics: no drawable geometry collected from the preview");
        }
        m_frame_ready = false;
        return;
    }

    // The collection's first frames carry the weapons before the preview's 3D is ready, so the draw
    // list is only worth logging once the skinned draws are in it: that is the frame whose list
    // actually names every mesh the panel renders.
    bool const has_skinned = std::any_of(m_draws.begin(), m_draws.end(),
        [](PanelDraw const& draw) { return draw.skin != nullptr; });

    if (!m_session_diagnostics_done && has_skinned)
    {
        m_session_diagnostics_done = true;
        float bound_radius = 0.0f;
        RE::NiPoint3 bound_center{};
        if (RE::NiAVObject const* const root = preview->GetCurrent3D())
        {
            bound_center = root->worldBound.center;
            bound_radius = root->worldBound.radius;
        }
        logger::info("Panel diagnostics: chrome={} draws={} camera=ok screen={}x{} rect=({},{}),{}x{} bound=({:.0f},{:.0f},{:.0f}) r={:.1f} cull={}",
            built_in_chrome ? "built-in" : "swf", m_draws.size(), screen.width, screen.height,
            layout.x, layout.y, layout.width, layout.height,
            bound_center.x, bound_center.y, bound_center.z, bound_radius,
            m_world_copy_cull_active ? "active" : "grace");
        // Name every draw once per session: the panel renders whatever collection passed,
        // and a weapon-looking object that the player does not recognise is only
        // identifiable from the node names.
        for (PanelDraw const& draw : m_draws)
        {
            char const* const name = draw.node ? draw.node->name.c_str() : nullptr;
            logger::info("Panel draw: {} node=\"{}\" vertices={} triangles={} alpha={:.2f} uv={}",
                draw.skin ? "skinned" : "static", name && name[0] != '\0' ? name : "?",
                draw.vertex_count, draw.triangle_count, draw.material_alpha,
                draw.has_uv ? "yes" : "no");
        }
        PanelMenu::log_movie_state();
    }

    m_camera = camera;
    m_built_in_chrome = built_in_chrome;
    m_frame_ready = true;
}

void PanelRenderer::apply_panel_input(PanelLayout const& layout, uint32_t screen_width,
    uint32_t screen_height, float cursor_x, float cursor_y)
{
    if (m_press_pending)
    {
        m_press_pending = false;
        // The whole panel surface is the drag handle: nothing else on the panel consumes mouse input.
        if (cursor_x >= static_cast<float>(layout.x) &&
            cursor_x < static_cast<float>(layout.x) + static_cast<float>(layout.width) &&
            cursor_y >= static_cast<float>(layout.y) &&
            cursor_y < static_cast<float>(layout.y) + static_cast<float>(layout.height))
        {
            m_drag_active = true;
            // The offset inside the panel is kept, so the panel does not jump under the cursor.
            m_grab_offset_x = cursor_x - static_cast<float>(layout.x);
            m_grab_offset_y = cursor_y - static_cast<float>(layout.y);
        }
    }

    if (!m_drag_active)
        return;

    if (!m_left_button_down)
    {
        m_drag_active = false;
        return;
    }

    commit_panel_position(layout, screen_width, screen_height, cursor_x, cursor_y);
}

void PanelRenderer::commit_panel_position(PanelLayout const& layout, uint32_t screen_width,
    uint32_t screen_height, float cursor_x, float cursor_y)
{
    // The requested top-left is stored as the fraction of the free space on each axis, so the panel
    // lands at the same relative spot at any resolution. Clamping the fraction is what keeps the panel
    // from leaving the screen, and it happens at the single writer of the stored position.
    uint32_t const free_x = screen_width - layout.width;
    uint32_t const free_y = screen_height - layout.height;
    float const requested_x = cursor_x - m_grab_offset_x;
    float const requested_y = cursor_y - m_grab_offset_y;
    double const normalized_x = free_x > 0
        ? static_cast<double>(requested_x) / static_cast<double>(free_x)
        : 0.0;
    double const normalized_y = free_y > 0
        ? static_cast<double>(requested_y) / static_cast<double>(free_y)
        : 0.0;
    Setting::instance().set_panel_position(normalized_x, normalized_y);
}

void PanelRenderer::draw(REX::W32::IDXGISwapChain* swap_chain)
{
    // A requested release is serviced here so every D3D call stays on the render thread.
    if (m_release_requested.exchange(false, std::memory_order_acq_rel))
    {
        release();
        return;
    }

    // A frame to draw: either a freshly prepared one, or - when the collection came back empty for
    // a frame (the engine rebuilding the preview's 3D root does this transiently, and the empty
    // frame would flicker the whole panel) - the previous frame's draws, which the render thread
    // still owns and whose borrowed buffers the previous draw's NiPointers keep alive.
    PanelCameraFrame camera{};
    bool built_in_chrome = true;
    {
        std::lock_guard lock(m_mutex);
        if (m_frame_ready)
        {
            // Steal the prepared frame; the buffer stays with the panel and comes back on the next swap.
            m_draws.swap(m_render_draws);
            m_frame_ready = false;
            camera = m_camera;
            built_in_chrome = m_built_in_chrome;
        }
        else if (!m_chrome_active.load(std::memory_order_acquire) || m_render_draws.empty())
        {
            return;
        }
        else
        {
            camera = m_camera;
            built_in_chrome = m_built_in_chrome;
        }
    }

    RE::BSGraphics::Renderer* const renderer = RE::BSGraphics::Renderer::GetSingleton();
    if (!renderer)
        return;

    auto const& renderer_runtime = renderer->GetRuntimeData();
    REX::W32::ID3D11Device* const device = renderer_runtime.forwarder;
    REX::W32::ID3D11DeviceContext* const context = renderer_runtime.context;
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

    // Report the first failure of a streak and stay quiet afterwards: a persistent failure must not
    // log a line every frame, and nothing here retries or spins.
    auto const report_failure = [this](REX::W32::HRESULT result)
    {
        if (m_back_buffer_error_logged)
            return;
        logger::error("Panel: the swap-chain back buffer or its view is unavailable ({:X}); the panel is skipped this frame", static_cast<unsigned int>(result));
        m_back_buffer_error_logged = true;
    };

    // The composite target is the ENGINE'S OWN back-buffer view - renderWindows[0].renderView, the
    // target the engine's UI pass paints the panel menu into and the pixels that actually reach the
    // screen. Reaching it through the engine's renderer sidesteps the present hook's swap chain
    // entirely: that swap chain is another mod's proxy (the crash log names a DXGISwapChainProxy),
    // and its buffer slots do not necessarily match what gets presented - the content dump caught the
    // composed panel absent from slot 0 while the movie's chrome was on screen. The engine's view is
    // borrowed, never owned; when it is absent (mid-resize) the plugin falls back to slot 0 of the
    // proxied swap chain for that frame.
    REX::W32::ID3D11RenderTargetView* back_buffer_rtv = renderer_runtime.renderWindows[0].renderView;
    REX::W32::ID3D11Texture2D* back_buffer = nullptr;
    if (!back_buffer_rtv)
    {
        REX::W32::HRESULT const buffer_hr = swap_chain->GetBuffer(0, REX::W32::IID_ID3D11Texture2D, reinterpret_cast<void**>(&back_buffer));
        if (!REX::W32::SUCCESS(buffer_hr) || !back_buffer)
        {
            report_failure(buffer_hr);
            return;
        }

        REX::W32::HRESULT const rtv_hr = device->CreateRenderTargetView(back_buffer, nullptr, &back_buffer_rtv);
        if (!REX::W32::SUCCESS(rtv_hr) || !back_buffer_rtv)
        {
            report_failure(rtv_hr);
            back_buffer->Release();
            return;
        }
    }
    m_back_buffer_error_logged = false;

    // The colour target stays panel-sized; the depth buffer is sized to the actual resource behind
    // the back-buffer view, because the geometry pass binds the two views together and D3D11
    // silently drops the whole OMSetRenderTargets when their resource sizes differ - the empty
    // window this once produced. Querying the bound resource (instead of trusting the reported
    // render size) also rebuilds the depth buffer whenever the engine swaps in a differently sized
    // back buffer. The queried texture is a temporary reference used for the desc alone; the
    // function-end cleanup only ever releases the fallback path's own slot-0 view and texture.
    uint32_t target_width = screen.width;
    uint32_t target_height = screen.height;
    if (!back_buffer)
    {
        REX::W32::ID3D11Texture2D* bound_texture = nullptr;
        back_buffer_rtv->GetResource(reinterpret_cast<REX::W32::ID3D11Resource**>(&bound_texture));
        if (bound_texture)
        {
            REX::W32::D3D11_TEXTURE2D_DESC back_desc{};
            bound_texture->GetDesc(&back_desc);
            target_width = back_desc.width;
            target_height = back_desc.height;
            bound_texture->Release();
        }
    }
    if (!m_target.matches(device, layout.width, layout.height, target_width, target_height))
    {
        m_target.release();
        if (!m_target.init(device, layout.width, layout.height, target_width, target_height))
            return;
    }

    // Everything the panel overwrites is put back afterwards, so the engine's own draw of the same
    // frame is unaffected.
    D3D11StateCapture capture(context);
    capture.capture();

    // The chrome mode decides both the offscreen clear and the inset the character is clipped to. The
    // built-in chrome clears opaque and insets by its own hairline, exactly as before. A skin clears
    // fully transparent so the composite only writes where the character actually is, and insets by its
    // own configured value, so the skin's frame and backdrop are both left visible.
    uint32_t const inset = built_in_chrome
        ? resolve_border_thickness(layout.height)
        : resolve_skin_inset_thickness(config, layout.width, layout.height);

    REX::W32::D3D11_VIEWPORT const rectangle{
        .topLeftX = static_cast<float>(layout.x),
        .topLeftY = static_cast<float>(layout.y),
        .width = static_cast<float>(layout.width),
        .height = static_cast<float>(layout.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    // The chrome draws in two halves with the character between them, ALL onto the engine's own
    // back-buffer view: the fill (so no world content shows through), the character meshes directly
    // over the fill with the panel's own depth, and the hairline band last. There is no offscreen
    // pass - the offscreen target's empty reads are what once turned the window black.
    m_composite_pass.draw(context, back_buffer_rtv, rectangle, PanelCompositePhase::kFill,
        Panel_Background_Color, Panel_Border_Color, inset, built_in_chrome);

    m_geometry_pass.draw(device, context, back_buffer_rtv, m_target.dsv(), rectangle,
        camera, m_render_draws);

    m_composite_pass.draw(context, back_buffer_rtv, rectangle, PanelCompositePhase::kBand,
        Panel_Background_Color, Panel_Border_Color, inset, built_in_chrome);

    // One-shot per-session content dump: the pixels answer what the log cannot - whether the geometry
    // pass reaches the target at all, and whether the composite's write survives onto the back buffer.
    ++m_composited_frames;
    bool const dump_now = !m_target_dumped || (m_composited_frames == 90 && !m_second_dumped);
    if (dump_now)
    {
        if (m_composited_frames == 90)
            m_second_dumped = true;
        m_target_dumped = true;
        std::string const suffix = m_second_dumped && m_composited_frames == 90 ? "2" : "";
        REX::W32::ID3D11Resource* presented_resource = nullptr;
        back_buffer_rtv->GetResource(&presented_resource);
        if (presented_resource)
        {
            dump_texture_tga(device, context, static_cast<REX::W32::ID3D11Texture2D*>(presented_resource), "PlayerPanel_backbuffer" + suffix);
            presented_resource->Release();
        }
    }

    capture.restore();

    if (!m_first_composite_logged)
    {
        m_first_composite_logged = true;
        logger::info("Panel composite: first draw (chrome={} rect=({},{}),{}x{})",
            built_in_chrome ? "built-in" : "swf", layout.x, layout.y, layout.width, layout.height);
    }

    // Released only after the state capture has restored the engine's own targets. The engine's own
    // view is borrowed and never released here; only the fallback path's slot-0 view and texture are
    // owned by this frame.
    if (back_buffer)
    {
        back_buffer_rtv->Release();
        back_buffer->Release();
    }
}

bool PanelRenderer::ensure_device_objects(REX::W32::ID3D11Device* device)
{
    if (m_ready && m_ref_device == device)
        return true;

    // The panel's shaders are compiled here, on the first frame that has a device, because a pass built
    // from the manager's null accessors would fail and be rebuilt every frame with nothing to show for
    // it. The compilation is idempotent, so this only does work until it has succeeded. A failure is
    // remembered for that device: it has already been logged once, and the frame is skipped rather than
    // retried, which is why the latch is checked before the per-frame release() below and not cleared
    // by it. A different device, or an explicit panel release, is a fresh attempt.
    if (m_ref_shader_failed_device == device)
        return false;

    release();

    if (!ShaderManager::instance().compile())
    {
        m_ref_shader_failed_device = device;
        return false;
    }

    if (!m_geometry_pass.init(device) || !m_composite_pass.init(device))
        return false;

    m_ref_device = device;
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
