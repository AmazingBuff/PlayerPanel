//
// Created by AmazingBuff on 2026/09/21.
//

#pragma once

#include "render/panel/panel_camera.h"
#include "render/panel/panel_geometry.h"
#include "render/panel/panel_target.h"

#include <DirectXMath.h>
#include <REX/W32/D3D11.h>
#include <cstdint>
#include <span>
#include <vector>

PLUGIN_NAMESPACE_BEGIN

// The panel geometry pass: draws the collected preview meshes into the private target with the panel
// camera and a depth state of its own (forward Z, LESS). Static and skinned meshes share one shader
// pair; the skinned palette is built per draw. The input layouts are created on demand and cached,
// because the per-mesh vertex descriptors come in very few varieties but creating a device object
// per mesh per frame would not be acceptable.
class PanelGeometryPass
{
public:
    PanelGeometryPass();
    ~PanelGeometryPass();

    PanelGeometryPass(PanelGeometryPass const&) = delete;
    PanelGeometryPass& operator=(PanelGeometryPass const&) = delete;

    [[nodiscard]] bool init(REX::W32::ID3D11Device* device);
    void release();

    // Sets the target and its views, clears them, binds the panel states and draws every mesh. The
    // caller wraps the call in a D3D11StateCapture. clear_color is the caller's decision: the opaque
    // background for the built-in chrome, and a fully transparent clear for a skin, so a fragment the
    // character does not cover keeps a zero alpha and the composite blends nothing into it.
    void draw(REX::W32::ID3D11Device* device, REX::W32::ID3D11DeviceContext* context,
        PanelTarget const& target, PanelCameraFrame const& camera, float alpha_test,
        float const clear_color[4], std::span<PanelDraw const> draws);

private:
    // Input-layout cache key: the skinning path, whether a UV input is bound, the position format and
    // offset (a calibration result), the skinning block offset, the stride and the calibrated
    // weight/index layout.
    struct LayoutKey
    {
        bool skinned;
        bool has_uv;
        uint32_t position_format;
        uint32_t position_offset;
        uint32_t skinning_offset;
        uint32_t uv_offset;
        uint32_t stride;
        uint32_t weight_format;
        uint32_t weight_offset;
        uint32_t index_format;
        uint32_t index_offset;
    };

    struct LayoutEntry
    {
        LayoutKey key;
        REX::W32::ID3D11InputLayout* layout;
    };

    [[nodiscard]] static bool same_layout(LayoutKey const& lhs, LayoutKey const& rhs) noexcept;
    [[nodiscard]] REX::W32::ID3D11InputLayout* acquire_layout(REX::W32::ID3D11Device* device, PanelDraw const& draw);
    void release_layouts();

private:
    REX::W32::ID3D11VertexShader* m_ref_static_vs;
    REX::W32::ID3D11VertexShader* m_ref_skinned_vs;
    REX::W32::ID3D11PixelShader* m_ref_pixel_shader;
    REX::W32::ID3D11Buffer* m_frame_cb;
    REX::W32::ID3D11Buffer* m_draw_cb;
    REX::W32::ID3D11DepthStencilState* m_depth;
    REX::W32::ID3D11RasterizerState* m_rasterizer;
    REX::W32::ID3D11BlendState* m_blend;
    REX::W32::ID3D11SamplerState* m_sampler;
    std::vector<LayoutEntry> m_layouts;
};

// The panel composite pass: draws the offscreen character over the panel rectangle on the swap-chain
// back buffer. In the panel's built-in chrome it first fills the rectangle with an opaque background
// colour and then paints the one hairline border into its outer band, with blending disabled, so the
// window is solid. When a skin's movie owns the panel's chrome instead, this pass draws the character
// alone with source-alpha blending over whatever is already on the target, which is the chrome the
// engine drew in its own UI pass, and leaves that same outer band untouched, because the band is where
// the skin's own frame sits. Blending is what makes a fragment with no character coverage leave the
// skin's backdrop visible instead of covering it with a transparent black. The rectangle is the
// caller's viewport, so the shared fullscreen triangle always covers exactly it.
class PanelCompositePass
{
public:
    PanelCompositePass();
    ~PanelCompositePass();

    PanelCompositePass(PanelCompositePass const&) = delete;
    PanelCompositePass& operator=(PanelCompositePass const&) = delete;

    [[nodiscard]] bool init(REX::W32::ID3D11Device* device);
    void release();

    // border_thickness is in render pixels and is expressed in the shader's uv space with the
    // rectangle's own size, so the inset keeps its pixel thickness at every resolution. It is also the
    // inset the character is clipped to: with the built-in chrome it is the hairline border's own
    // thickness, and with a skin it is the configured skin inset, which the caller derives separately.
    // built_in_chrome selects between the plugin's own background and border and a skin's movie,
    // which is already on the target, and between the opaque and the blended character write.
    void draw(REX::W32::ID3D11DeviceContext* context, REX::W32::ID3D11RenderTargetView* target,
        REX::W32::ID3D11ShaderResourceView* panel, REX::W32::D3D11_VIEWPORT const& rectangle,
        float const background[4], float const border[4], uint32_t border_thickness,
        bool built_in_chrome) const;

private:
    REX::W32::ID3D11VertexShader* m_ref_fullscreen_vs;
    REX::W32::ID3D11PixelShader* m_ref_background_ps;
    REX::W32::ID3D11PixelShader* m_ref_copy_ps;
    REX::W32::ID3D11Buffer* m_background_cb;
    REX::W32::ID3D11BlendState* m_blend;
    // The skin path's own blend state: source alpha over inverse source alpha with the destination
    // alpha preserved, so no coverage writes nothing and repeated frames do not accumulate.
    REX::W32::ID3D11BlendState* m_blend_alpha;
    REX::W32::ID3D11DepthStencilState* m_depth_none;
    REX::W32::ID3D11RasterizerState* m_rasterizer;
    REX::W32::ID3D11SamplerState* m_sampler;
};

PLUGIN_NAMESPACE_END
