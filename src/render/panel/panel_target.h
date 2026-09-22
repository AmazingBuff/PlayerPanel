//
// Created by AmazingBuff on 2026/09/21.
//

#pragma once

#include <REX/W32/D3D11.h>
#include <cstdint>

PLUGIN_NAMESPACE_BEGIN

// The panel's private offscreen target: an R8G8B8A8_UNORM colour texture plus a D32_FLOAT depth
// texture, together with the render-target, depth-stencil and shader-resource views the panel passes
// need. Owned exclusively by the render thread and rebuilt when the device or the size changes.
class PanelTarget
{
public:
    PanelTarget();
    ~PanelTarget();

    PanelTarget(PanelTarget const&) = delete;
    PanelTarget& operator=(PanelTarget const&) = delete;

    [[nodiscard]] bool init(REX::W32::ID3D11Device* a_device, uint32_t a_width, uint32_t a_height);
    void release();

    // True when the current views already match this device and size, so no rebuild is needed.
    [[nodiscard]] bool matches(REX::W32::ID3D11Device* a_device, uint32_t a_width, uint32_t a_height) const;
    [[nodiscard]] REX::W32::ID3D11RenderTargetView* rtv() const noexcept { return m_rtv; }
    [[nodiscard]] REX::W32::ID3D11DepthStencilView* dsv() const noexcept { return m_dsv; }
    [[nodiscard]] REX::W32::ID3D11ShaderResourceView* srv() const noexcept { return m_srv; }
    [[nodiscard]] uint32_t width() const noexcept { return m_width; }
    [[nodiscard]] uint32_t height() const noexcept { return m_height; }

private:
    REX::W32::ID3D11Device* m_ref_device;
    REX::W32::ID3D11Texture2D* m_texture;
    REX::W32::ID3D11Texture2D* m_depth_texture;
    REX::W32::ID3D11RenderTargetView* m_rtv;
    REX::W32::ID3D11DepthStencilView* m_dsv;
    REX::W32::ID3D11ShaderResourceView* m_srv;
    uint32_t m_width;
    uint32_t m_height;
};

PLUGIN_NAMESPACE_END
