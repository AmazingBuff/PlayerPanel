//
// Created by AmazingBuff on 2026/09/21.
//

#pragma once

#include <REX/W32/D3D11.h>
#include <cstdint>

PLUGIN_NAMESPACE_BEGIN

// The panel's render targets: the colour texture (R8G8B8A8_UNORM) keeps the skin path's offscreen
// character, while the depth texture (D32_FLOAT) is sized to the ENGINE'S OWN back buffer, because
// the built-in chrome's geometry pass draws the character directly onto that back buffer and D3D11
// silently drops an OMSetRenderTargets whose depth-stencil resource size differs from the render
// target's. Both are owned exclusively by the render thread and rebuilt when the device or either
// size changes.
class PanelTarget
{
public:
    PanelTarget();
    ~PanelTarget();

    PanelTarget(PanelTarget const&) = delete;
    PanelTarget& operator=(PanelTarget const&) = delete;

    [[nodiscard]] bool init(REX::W32::ID3D11Device* device, uint32_t colour_width, uint32_t colour_height,
        uint32_t depth_width, uint32_t depth_height);
    void release();

    // True when the current views already match this device and both sizes, so no rebuild is needed.
    [[nodiscard]] bool matches(REX::W32::ID3D11Device* device, uint32_t colour_width, uint32_t colour_height,
        uint32_t depth_width, uint32_t depth_height) const;
    [[nodiscard]] REX::W32::ID3D11RenderTargetView* rtv() const noexcept { return m_rtv; }
    [[nodiscard]] REX::W32::ID3D11DepthStencilView* dsv() const noexcept { return m_dsv; }
    [[nodiscard]] REX::W32::ID3D11ShaderResourceView* srv() const noexcept { return m_srv; }
    [[nodiscard]] REX::W32::ID3D11Texture2D* texture() const noexcept { return m_texture; }
    [[nodiscard]] uint32_t colour_width() const noexcept { return m_colour_width; }
    [[nodiscard]] uint32_t colour_height() const noexcept { return m_colour_height; }
    [[nodiscard]] uint32_t depth_width() const noexcept { return m_depth_width; }
    [[nodiscard]] uint32_t depth_height() const noexcept { return m_depth_height; }

private:
    REX::W32::ID3D11Device* m_ref_device;
    REX::W32::ID3D11Texture2D* m_texture;
    REX::W32::ID3D11Texture2D* m_depth_texture;
    REX::W32::ID3D11RenderTargetView* m_rtv;
    REX::W32::ID3D11DepthStencilView* m_dsv;
    REX::W32::ID3D11ShaderResourceView* m_srv;
    uint32_t m_colour_width;
    uint32_t m_colour_height;
    uint32_t m_depth_width;
    uint32_t m_depth_height;
};

PLUGIN_NAMESPACE_END
