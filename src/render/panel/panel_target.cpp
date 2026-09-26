//
// Created by AmazingBuff on 2026/09/21.
//
// Ported from Highlight-Lootable-Corpses (https://github.com/AmazingBuff/Highlight-Lootable-Corpses.git),
// src/render/mask/mask_passes.cpp (RenderTarget), commit 7a7c51e, with the panel's fixed colour and
// depth formats. Both repositories are GPL-3.0 with the same author.
//

#include "panel_target.h"

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // The panel's fixed target formats: an 8-bit colour target the swap chain can show unchanged,
    // and a 32-bit float depth buffer for the panel camera's forward-Z range.
    constexpr REX::W32::DXGI_FORMAT Panel_Color_Format = REX::W32::DXGI_FORMAT_R8G8B8A8_UNORM;
    constexpr REX::W32::DXGI_FORMAT Panel_Depth_Format = REX::W32::DXGI_FORMAT_D32_FLOAT;
}

PanelTarget::PanelTarget() :
    m_ref_device(nullptr),
    m_texture(nullptr),
    m_depth_texture(nullptr),
    m_rtv(nullptr),
    m_dsv(nullptr),
    m_srv(nullptr),
    m_colour_width(0),
    m_colour_height(0),
    m_depth_width(0),
    m_depth_height(0) {}

PanelTarget::~PanelTarget()
{
    release();
}

bool PanelTarget::matches(REX::W32::ID3D11Device* device, uint32_t colour_width, uint32_t colour_height,
    uint32_t depth_width, uint32_t depth_height) const
{
    return m_ref_device == device && m_colour_width == colour_width && m_colour_height == colour_height &&
           m_depth_width == depth_width && m_depth_height == depth_height && m_rtv && m_dsv && m_srv;
}

void PanelTarget::release()
{
    if (m_dsv)
    {
        m_dsv->Release();
        m_dsv = nullptr;
    }
    if (m_depth_texture)
    {
        m_depth_texture->Release();
        m_depth_texture = nullptr;
    }
    if (m_srv)
    {
        m_srv->Release();
        m_srv = nullptr;
    }
    if (m_rtv)
    {
        m_rtv->Release();
        m_rtv = nullptr;
    }
    if (m_texture)
    {
        m_texture->Release();
        m_texture = nullptr;
    }
    m_colour_width = 0;
    m_colour_height = 0;
    m_depth_width = 0;
    m_depth_height = 0;
    m_ref_device = nullptr;
}

bool PanelTarget::init(REX::W32::ID3D11Device* device, uint32_t colour_width, uint32_t colour_height,
    uint32_t depth_width, uint32_t depth_height)
{
    if (!device || colour_width == 0 || colour_height == 0 || depth_width == 0 || depth_height == 0)
    {
        logger::error("Panel target: invalid device or sizes (colour {}x{}, depth {}x{})",
            colour_width, colour_height, depth_width, depth_height);
        return false;
    }

    REX::W32::D3D11_TEXTURE2D_DESC desc{};
    desc.width = colour_width;
    desc.height = colour_height;
    desc.mipLevels = 1;
    desc.arraySize = 1;
    desc.format = Panel_Color_Format;
    desc.sampleDesc.count = 1;
    desc.usage = REX::W32::D3D11_USAGE_DEFAULT;
    desc.bindFlags = REX::W32::D3D11_BIND_RENDER_TARGET | REX::W32::D3D11_BIND_SHADER_RESOURCE;

    REX::W32::HRESULT const texture_hr = device->CreateTexture2D(&desc, nullptr, &m_texture);
    if (!REX::W32::SUCCESS(texture_hr) || !m_texture)
    {
        logger::error("Panel target: failed to create the colour texture ({:X})", static_cast<unsigned int>(texture_hr));
        release();
        return false;
    }

    REX::W32::HRESULT const rtv_hr = device->CreateRenderTargetView(m_texture, nullptr, &m_rtv);
    REX::W32::HRESULT const srv_hr = device->CreateShaderResourceView(m_texture, nullptr, &m_srv);
    if (!REX::W32::SUCCESS(rtv_hr) || !m_rtv || !REX::W32::SUCCESS(srv_hr) || !m_srv)
    {
        logger::error("Panel target: failed to create the colour views (rtv={:X}, srv={:X})",
            static_cast<unsigned int>(rtv_hr), static_cast<unsigned int>(srv_hr));
        release();
        return false;
    }

    // The depth texture is bound together with the engine's own back buffer in the built-in chrome's
    // geometry pass, and D3D11 silently drops an OMSetRenderTargets whose depth-stencil resource size
    // differs from the render target's - it is therefore sized to the back buffer, not to the panel.
    desc.width = depth_width;
    desc.height = depth_height;
    desc.format = Panel_Depth_Format;
    desc.bindFlags = REX::W32::D3D11_BIND_DEPTH_STENCIL;
    REX::W32::HRESULT const depth_hr = device->CreateTexture2D(&desc, nullptr, &m_depth_texture);
    if (!REX::W32::SUCCESS(depth_hr) || !m_depth_texture)
    {
        logger::error("Panel target: failed to create the depth texture ({:X})", static_cast<unsigned int>(depth_hr));
        release();
        return false;
    }

    REX::W32::HRESULT const dsv_hr = device->CreateDepthStencilView(m_depth_texture, nullptr, &m_dsv);
    if (!REX::W32::SUCCESS(dsv_hr) || !m_dsv)
    {
        logger::error("Panel target: failed to create the depth view ({:X})", static_cast<unsigned int>(dsv_hr));
        release();
        return false;
    }

    m_ref_device = device;
    m_colour_width = colour_width;
    m_colour_height = colour_height;
    m_depth_width = depth_width;
    m_depth_height = depth_height;
    return true;
}

PLUGIN_NAMESPACE_END
