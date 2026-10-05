//
// Offscreen studio target implementation. (2026-10-05 src migration: split out of tools/m0_proto, behavior byte-identical)
//

#include "render/offscreen_target.h"

PLUGIN_NAMESPACE_BEGIN
void OffscreenTarget::destroy()
{
    if (srv)
        srv->Release();
    if (rtv)
        rtv->Release();
    if (color)
        color->Release();
    if (dsv)
        dsv->Release();
    if (depth_texture)
        depth_texture->Release();
    if (ds_state)
        ds_state->Release();
    srv = nullptr;
    rtv = nullptr;
    color = nullptr;
    dsv = nullptr;
    depth_texture = nullptr;
    ds_state = nullptr;
    width = 0;
    height = 0;
    format = REX::W32::DXGI_FORMAT_UNKNOWN;
    depth_format = REX::W32::DXGI_FORMAT_UNKNOWN;
}

// Sized from the call site's own render target (run 12: the menu
// pass draws into an R8G8B8A8_UNORM UI composite, format 28 —
// not kMAIN's R11G11B10), so the replayed pass writes exactly the
// representation it was built for. Run 14's readback exposed the
// missing intra-item occlusion (the handle drawn over the boss):
// without a DSV the pass's depth test is bypassed entirely, so a
// private depth buffer (engine-matched format) plus an explicit
// depth-on state accompany the color target.
bool OffscreenTarget::create(REX::W32::ID3D11Device* device, const REX::W32::D3D11_TEXTURE2D_DESC& template_desc,
    REX::W32::DXGI_FORMAT a_depth_format)
{
    destroy();

    REX::W32::D3D11_TEXTURE2D_DESC color_desc{};
    color_desc.width = template_desc.width;
    color_desc.height = template_desc.height;
    color_desc.mipLevels = 1;
    color_desc.arraySize = 1;
    color_desc.format = template_desc.format;
    color_desc.sampleDesc.count = 1;
    color_desc.usage = REX::W32::D3D11_USAGE_DEFAULT;
    color_desc.bindFlags = REX::W32::D3D11_BIND_RENDER_TARGET | REX::W32::D3D11_BIND_SHADER_RESOURCE;
    if (device->CreateTexture2D(&color_desc, nullptr, &color) != 0)
        return false;
    if (device->CreateRenderTargetView(color, nullptr, &rtv) != 0)
    {
        destroy();
        return false;
    }
    // v4: the composite quad samples the studio image, so the
    // color texture needs an SRV alongside the RTV.
    if (device->CreateShaderResourceView(color, nullptr, &srv) != 0)
    {
        destroy();
        return false;
    }

    REX::W32::D3D11_TEXTURE2D_DESC depth_desc{};
    depth_desc.width = template_desc.width;
    depth_desc.height = template_desc.height;
    depth_desc.mipLevels = 1;
    depth_desc.arraySize = 1;
    depth_desc.format = a_depth_format;
    depth_desc.sampleDesc.count = 1;
    depth_desc.usage = REX::W32::D3D11_USAGE_DEFAULT;
    depth_desc.bindFlags = REX::W32::D3D11_BIND_DEPTH_STENCIL;
    const HRESULT depth_hr = device->CreateTexture2D(&depth_desc, nullptr, &depth_texture);
    if (depth_hr != 0)
    {
        logger::warn("Proto v3 depth texture creation failed (format={} hr=0x{:X})",
            static_cast<int>(a_depth_format), static_cast<std::uint32_t>(depth_hr));
        destroy();
        return false;
    }
    if (device->CreateDepthStencilView(depth_texture, nullptr, &dsv) != 0)
    {
        logger::warn("Proto v3 depth view creation failed");
        destroy();
        return false;
    }

    // The state at the call site has depth testing disabled (run
    // 10), and whatever the original draw leaves behind is not
    // guaranteed to have it on either, so the replay binds its
    // own standard depth-on state.
    REX::W32::D3D11_DEPTH_STENCIL_DESC ds_desc{};
    ds_desc.depthEnable = 1;
    ds_desc.depthWriteMask = REX::W32::D3D11_DEPTH_WRITE_MASK_ALL;
    ds_desc.depthFunc = REX::W32::D3D11_COMPARISON_LESS_EQUAL;
    ds_desc.stencilEnable = 0;
    ds_desc.stencilReadMask = 0xFF;
    ds_desc.stencilWriteMask = 0xFF;
    const REX::W32::D3D11_DEPTH_STENCILOP_DESC keep_op{
        REX::W32::D3D11_STENCIL_OP_KEEP, REX::W32::D3D11_STENCIL_OP_KEEP,
        REX::W32::D3D11_STENCIL_OP_KEEP, REX::W32::D3D11_COMPARISON_ALWAYS
    };
    ds_desc.frontFace = keep_op;
    ds_desc.backFace = keep_op;
    if (device->CreateDepthStencilState(&ds_desc, &ds_state) != 0 || !ds_state)
    {
        destroy();
        return false;
    }

    width = template_desc.width;
    height = template_desc.height;
    format = template_desc.format;
    depth_format = a_depth_format;
    return true;
}

TargetSig target_sig(const OffscreenTarget& target)
{
    return { target.width, target.height, static_cast<std::uint32_t>(target.format),
        static_cast<std::uint32_t>(target.depth_format) };
}

OffscreenTarget& offscreen_target()
{
    static OffscreenTarget s_target;
    return s_target;
}
PLUGIN_NAMESPACE_END
