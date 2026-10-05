//
// Panel composite implementation. (2026-10-05 src migration: split out of tools/m0_proto, behavior byte-identical)
//

#include "render/composite.h"
#include "render/shader_sources.h"
#include "render/offscreen_target.h"
#include "render/render_internal.h"

#include <Windows.h>
#include <d3dcompiler.h>

#include <cstring>

PLUGIN_NAMESPACE_BEGIN
namespace
{
    // --- composite (v4, work package 3) --------------------------------

    // M0 calibration constants: the panel occupies a fixed screen
    // fraction — v6.30 (user red-box, run 63): a tall strip on the
    // right (73%..99% horizontally, 5%..96% vertically from the top) —
    // and shows the WHOLE studio target squeezed into it (uv 0..1).
    // The anisotropic squeeze of the 16:9 target into this narrow rect
    // is inherent to the mapping; framing/zoom are studio concerns.
    constexpr float Panel_Screen_MinX = 0.73f;
    constexpr float Panel_Screen_MaxX = 0.99f;
    constexpr float Panel_Screen_MinY = 0.05f;
    constexpr float Panel_Screen_MaxY = 0.96f;

    // The quad's HLSL lives in render/shaders/composite_vs.hlsl and
    // composite_ps.hlsl, embedded into render/shader_sources.h at build
    // time (render_shaders::CompositeVS / CompositePS) — the HLC
    // reference-project pattern. The panel-rect constants below remain
    // the single framing authority.
}

// One-time setup: compile the shaders (native d3dcompiler, no
// blob-type coupling into REX) and create the fixed states.
bool CompositeRenderer::ensure(REX::W32::ID3D11Device* device)
{
    if (m_vs || m_failed)
        return !m_failed;
    if (!device)
        return false;

    REX::W32::ID3D11VertexShader* vs = nullptr;
    REX::W32::ID3D11PixelShader* ps = nullptr;
    ID3DBlob* code = nullptr;
    ID3DBlob* errors = nullptr;
    if (FAILED(::D3DCompile(render_shaders::CompositeVS, std::strlen(render_shaders::CompositeVS), nullptr, nullptr, nullptr,
            "vs_main", "vs_5_0", 0, 0, &code, &errors)))
    {
        logger::warn("Proto v4 composite VS compile failed: {}",
            errors ? static_cast<const char*>(errors->GetBufferPointer()) : "(no message)");
        if (errors)
            errors->Release();
        m_failed = true;
        return false;
    }
    const HRESULT vs_hr = device->CreateVertexShader(
        code->GetBufferPointer(), code->GetBufferSize(), nullptr, &vs);
    code->Release();
    if (vs_hr != 0 || !vs)
    {
        logger::warn("Proto v4 composite VS creation failed (hr=0x{:X})",
            static_cast<std::uint32_t>(vs_hr));
        m_failed = true;
        return false;
    }

    if (FAILED(::D3DCompile(render_shaders::CompositePS, std::strlen(render_shaders::CompositePS), nullptr, nullptr, nullptr,
            "ps_main", "ps_5_0", 0, 0, &code, &errors)))
    {
        logger::warn("Proto v4 composite PS compile failed: {}",
            errors ? static_cast<const char*>(errors->GetBufferPointer()) : "(no message)");
        if (errors)
            errors->Release();
        vs->Release();
        m_failed = true;
        return false;
    }
    const HRESULT ps_hr = device->CreatePixelShader(
        code->GetBufferPointer(), code->GetBufferSize(), nullptr, &ps);
    code->Release();
    if (ps_hr != 0 || !ps)
    {
        logger::warn("Proto v4 composite PS creation failed (hr=0x{:X})",
            static_cast<std::uint32_t>(ps_hr));
        vs->Release();
        m_failed = true;
        return false;
    }

    REX::W32::D3D11_SAMPLER_DESC sampler_desc{};
    sampler_desc.filter = REX::W32::D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler_desc.addressU = REX::W32::D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.addressV = REX::W32::D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.addressW = REX::W32::D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.comparisonFunc = REX::W32::D3D11_COMPARISON_NEVER;
    sampler_desc.maxLOD = REX::W32::D3D11_FLOAT32_MAX;
    REX::W32::ID3D11SamplerState* sampler = nullptr;
    REX::W32::ID3D11BlendState* blend = nullptr;
    REX::W32::ID3D11BlendState* opaque = nullptr;
    REX::W32::ID3D11DepthStencilState* depth_off = nullptr;
    REX::W32::ID3D11RasterizerState* rs = nullptr;
    REX::W32::ID3D11Buffer* cb = nullptr;

    // v4.6 (user RenderDoc finding): this composite target is
    // consumed through the UI alpha-composition pipeline, so the
    // quad must be BLENDED over the existing layer content, not
    // written opaquely — the disabled-blend overwrite broke the
    // layer's composition semantics. Standard non-premultiplied
    // over: src alpha scales the studio image, inv-src alpha
    // keeps the layer underneath.
    REX::W32::D3D11_BLEND_DESC blend_desc{};
    {
        auto& rt0 = blend_desc.renderTarget[0];
        rt0.blendEnable = 1;
        rt0.srcBlend = REX::W32::D3D11_BLEND_SRC_ALPHA;
        rt0.destBlend = REX::W32::D3D11_BLEND_INV_SRC_ALPHA;
        rt0.blendOp = REX::W32::D3D11_BLEND_OP_ADD;
        rt0.srcBlendAlpha = REX::W32::D3D11_BLEND_ONE;
        rt0.destBlendAlpha = REX::W32::D3D11_BLEND_INV_SRC_ALPHA;
        rt0.blendOpAlpha = REX::W32::D3D11_BLEND_OP_ADD;
        rt0.renderTargetWriteMask = REX::W32::D3D11_COLOR_WRITE_ENABLE_ALL;
    }
    // Run 41: OPAQUE overwrite for the proactive P draw — the
    // engine's blend state at that point is the item pass's
    // leftover; with unknown factors the studio pixels could be
    // multiplied to black. Opaque (blend disabled) writes the
    // raw shader output with alpha forced to 1 by the guard's
    // semantics (write-all).
    REX::W32::D3D11_BLEND_DESC opaque_desc{};
    {
        auto& rt0 = opaque_desc.renderTarget[0];
        rt0.blendEnable = 0;
        rt0.renderTargetWriteMask = REX::W32::D3D11_COLOR_WRITE_ENABLE_ALL;
    }
    const REX::W32::D3D11_DEPTH_STENCILOP_DESC keep_op{
        REX::W32::D3D11_STENCIL_OP_KEEP, REX::W32::D3D11_STENCIL_OP_KEEP,
        REX::W32::D3D11_STENCIL_OP_KEEP, REX::W32::D3D11_COMPARISON_ALWAYS
    };
    REX::W32::D3D11_DEPTH_STENCIL_DESC depth_desc{};
    depth_desc.depthEnable = 0;
    depth_desc.stencilEnable = 0;
    depth_desc.stencilReadMask = 0xFF;
    depth_desc.stencilWriteMask = 0xFF;
    depth_desc.frontFace = keep_op;
    depth_desc.backFace = keep_op;

    // v4.4: the quad carries its own rasterizer state — culling
    // off and scissor off — so whatever the engine leaves bound
    // (CULL_BACK, an active scissor rect) cannot silently drop
    // it. The corrected winding above is the primary fix; this
    // is the belt to its braces.
    REX::W32::D3D11_RASTERIZER_DESC rs_desc{};
    rs_desc.fillMode = REX::W32::D3D11_FILL_SOLID;
    rs_desc.cullMode = REX::W32::D3D11_CULL_NONE;
    rs_desc.frontCounterClockwise = 0;
    rs_desc.depthClipEnable = 1;
    rs_desc.scissorEnable = 0;

    // NDC rect derived once from the screen-fraction constants:
    // x = 2u-1; y_top = 1-2*top, y_bottom = 1-2*bottom. v6.31:
    // the CB carries a SECOND float4 — the per-draw HDR flag
    // lives in g_flags.x, NOT in g_ndcRect.y. Run 64: the flag
    // used to overwrite the rect's yBottom (both lived in
    // g_ndcRect.y), so LDR targets drew the quad from NDC 0 up
    // — exactly the upper half of the screen. That is why the
    // panel had always been shorter than its constants and the
    // new taller rect exposed it as "half a panel".
    const float cb_data[8] = {
        2.0f * Panel_Screen_MinX - 1.0f, 1.0f - 2.0f * Panel_Screen_MaxY,
        2.0f * Panel_Screen_MaxX - 1.0f, 1.0f - 2.0f * Panel_Screen_MinY,
        0.0f,
        (Panel_Screen_MaxX - Panel_Screen_MinX) / (Panel_Screen_MaxY - Panel_Screen_MinY),
        0.0f, 0.0f
    };
    REX::W32::D3D11_BUFFER_DESC cb_desc{};
    cb_desc.byteWidth = sizeof(cb_data);
    // Run 31: DYNAMIC — the draw path rewrites the HDR flag per
    // draw (y), the VS still reads the NDC rect (x, z, w).
    cb_desc.usage = REX::W32::D3D11_USAGE_DYNAMIC;
    cb_desc.cpuAccessFlags = REX::W32::D3D11_CPU_ACCESS_WRITE;
    cb_desc.bindFlags = REX::W32::D3D11_BIND_CONSTANT_BUFFER;

    bool ok = device->CreateSamplerState(&sampler_desc, &sampler) == 0 && sampler &&
        device->CreateBlendState(&blend_desc, &blend) == 0 && blend &&
        device->CreateBlendState(&opaque_desc, &opaque) == 0 && opaque &&
        device->CreateDepthStencilState(&depth_desc, &depth_off) == 0 && depth_off &&
        device->CreateRasterizerState(&rs_desc, &rs) == 0 && rs;
    REX::W32::D3D11_SUBRESOURCE_DATA init{ cb_data, 0, 0 };
    ok = ok && device->CreateBuffer(&cb_desc, &init, &cb) == 0 && cb;
    if (!ok)
    {
        logger::warn("Proto v4 composite state creation failed");
        if (sampler)
            sampler->Release();
        if (blend)
            blend->Release();
        if (opaque)
            opaque->Release();
        if (depth_off)
            depth_off->Release();
        if (rs)
            rs->Release();
        if (cb)
            cb->Release();
        vs->Release();
        ps->Release();
        m_failed = true;
        return false;
    }

    m_vs = vs;
    m_ps = ps;
    m_sampler = sampler;
    m_blend = blend;
    m_opaque = opaque;
    m_depth_off = depth_off;
    m_rs = rs;
    m_cb = cb;
    logger::info("Proto v4.6 composite ready: panel rect {}%..{}% x {}%..{}% of screen",
        static_cast<int>(Panel_Screen_MinX * 100), static_cast<int>(Panel_Screen_MaxX * 100),
        static_cast<int>(Panel_Screen_MinY * 100), static_cast<int>(Panel_Screen_MaxY * 100));
    return true;
}

// v4.1: draw the panel quad into the CAPTURED call-site target
// (the format-28 UI composite the visible menu preview uses) —
// run 20 proved the target bound at DrawInterfaceStart entry is
// a different, non-visible intermediate. Binding another target
// means the OM pair and the viewport join the save/restore set.
void CompositeRenderer::draw(REX::W32::ID3D11DeviceContext* ctx, REX::W32::ID3D11RenderTargetView* rtv)
{
    OffscreenTarget& target = offscreen_target();
    if (!ctx || !m_vs || !m_ps || !m_rs || !target.srv || !rtv || rtv == target.rtv)
        return;

    // Size the viewport from the target itself: the panel rect
    // is NDC, so it follows whatever resolution this target has.
    REX::W32::ID3D11Resource* resource = nullptr;
    rtv->GetResource(&resource);
    if (!resource)
        return;
    REX::W32::D3D11_TEXTURE2D_DESC desc{};
    static_cast<REX::W32::ID3D11Texture2D*>(resource)->GetDesc(&desc);
    resource->Release();

    REX::W32::ID3D11RenderTargetView* prev_rtv = nullptr;
    REX::W32::ID3D11DepthStencilView* prev_dsv = nullptr;
    ctx->OMGetRenderTargets(1, &prev_rtv, &prev_dsv);
    REX::W32::D3D11_VIEWPORT prev_viewports[16] = {};
    UINT prev_viewport_count = 16;
    ctx->RSGetViewports(&prev_viewport_count, prev_viewports);

    REX::W32::ID3D11BlendState* prev_blend = nullptr;
    FLOAT prev_blend_factor[4] = {};
    UINT prev_sample_mask = 0;
    ctx->OMGetBlendState(&prev_blend, prev_blend_factor, &prev_sample_mask);
    if (prev_blend && !m_blend_logged)
    {
        m_blend_logged = true;
        REX::W32::D3D11_BLEND_DESC prev_blend_desc{};
        prev_blend->GetDesc(&prev_blend_desc);
        const auto& rt0 = prev_blend_desc.renderTarget[0];
        logger::info("Proto v4.6 engine blend at composite time: enable={} src={} dest={} "
                     "(v4.5 wrote opaquely into an alpha-composited layer)",
            rt0.blendEnable, static_cast<int>(rt0.srcBlend), static_cast<int>(rt0.destBlend));
    }
    REX::W32::ID3D11DepthStencilState* prev_ds = nullptr;
    UINT prev_stencil_ref = 0;
    ctx->OMGetDepthStencilState(&prev_ds, &prev_stencil_ref);
    REX::W32::ID3D11RasterizerState* prev_rs = nullptr;
    ctx->RSGetState(&prev_rs);
    if (prev_rs && !m_rs_logged)
    {
        m_rs_logged = true;
        REX::W32::D3D11_RASTERIZER_DESC prev_rs_desc{};
        prev_rs->GetDesc(&prev_rs_desc);
        logger::info("Proto v4.6 rasterizer bound at composite time: cull={} scissor={} "
                     "(the v4.1-4.3 quads were back-facing under this state and culled)",
            static_cast<int>(prev_rs_desc.cullMode),
            static_cast<int>(prev_rs_desc.scissorEnable));
    }
    REX::W32::ID3D11PixelShader* prev_ps = nullptr;
    ctx->PSGetShader(&prev_ps, nullptr, nullptr);
    REX::W32::ID3D11ShaderResourceView* prev_srv = nullptr;
    ctx->PSGetShaderResources(0, 1, &prev_srv);
    REX::W32::ID3D11SamplerState* prev_sampler = nullptr;
    ctx->PSGetSamplers(0, 1, &prev_sampler);
    REX::W32::ID3D11Buffer* prev_ps_cb = nullptr;
    ctx->PSGetConstantBuffers(0, 1, &prev_ps_cb);
    REX::W32::ID3D11VertexShader* prev_vs = nullptr;
    ctx->VSGetShader(&prev_vs, nullptr, nullptr);
    REX::W32::ID3D11Buffer* prev_vs_cb = nullptr;
    ctx->VSGetConstantBuffers(0, 1, &prev_vs_cb);
    REX::W32::D3D11_PRIMITIVE_TOPOLOGY prev_topology{};
    ctx->IAGetPrimitiveTopology(&prev_topology);

    REX::W32::D3D11_VIEWPORT viewport{ 0.0f, 0.0f,
        static_cast<float>(desc.width), static_cast<float>(desc.height), 0.0f, 1.0f };
    // Run 31: the studio target's format decides the Reinhard
    // map (HDR world main target, format 10) vs pass-through
    // (LDR UI composite, format 28). v6.31: the flag lives in
    // g_flags.x — the FULL panel rect is written every draw
    // (run 64: the flag used to overwrite g_ndcRect.y = the
    // quad's yBottom, halving the panel).
    const float hdr = desc.format == REX::W32::DXGI_FORMAT_R11G11B10_FLOAT ? 1.0f : 0.0f;
    {
        REX::W32::D3D11_MAPPED_SUBRESOURCE mapped{};
        if (ctx->Map(m_cb, 0, REX::W32::D3D11_MAP_WRITE_DISCARD, 0, &mapped) == 0)
        {
            auto* out = static_cast<float*>(mapped.data);
            out[0] = 2.0f * Panel_Screen_MinX - 1.0f;
            out[1] = 1.0f - 2.0f * Panel_Screen_MaxY;
            out[2] = 2.0f * Panel_Screen_MaxX - 1.0f;
            out[3] = 1.0f - 2.0f * Panel_Screen_MinY;
            out[4] = hdr;
            out[5] = (Panel_Screen_MaxX - Panel_Screen_MinX) /
                     (Panel_Screen_MaxY - Panel_Screen_MinY);
            ctx->Unmap(m_cb, 0);
        }
    }
    ctx->OMSetRenderTargets(1, &rtv, nullptr);
    if (prev_viewport_count > 0)
        ctx->RSSetViewports(1, &viewport);
    ctx->OMSetBlendState(m_blend, prev_blend_factor, prev_sample_mask);
    ctx->OMSetDepthStencilState(m_depth_off, prev_stencil_ref);
    ctx->RSSetState(m_rs);
    ctx->PSSetShader(m_ps, nullptr, 0);
    ctx->PSSetShaderResources(0, 1, &target.srv);
    ctx->PSSetSamplers(0, 1, &m_sampler);
    ctx->PSSetConstantBuffers(0, 1, &m_cb);
    ctx->VSSetShader(m_vs, nullptr, 0);
    ctx->VSSetConstantBuffers(0, 1, &m_cb);
    ctx->IASetPrimitiveTopology(REX::W32::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->Draw(6, 0);
    ++m_draws;

    ctx->OMSetRenderTargets(1, &prev_rtv, prev_dsv);
    if (prev_viewport_count > 0)
        ctx->RSSetViewports(prev_viewport_count, prev_viewports);
    ctx->OMSetBlendState(prev_blend, prev_blend_factor, prev_sample_mask);
    ctx->OMSetDepthStencilState(prev_ds, prev_stencil_ref);
    ctx->RSSetState(prev_rs);
    ctx->PSSetShader(prev_ps, nullptr, 0);
    ctx->PSSetShaderResources(0, 1, &prev_srv);
    ctx->PSSetSamplers(0, 1, &prev_sampler);
    ctx->PSSetConstantBuffers(0, 1, &prev_ps_cb);
    ctx->VSSetShader(prev_vs, nullptr, 0);
    ctx->VSSetConstantBuffers(0, 1, &prev_vs_cb);
    ctx->IASetPrimitiveTopology(prev_topology);

    if (prev_rtv)
        prev_rtv->Release();
    if (prev_dsv)
        prev_dsv->Release();
    if (prev_blend)
        prev_blend->Release();
    if (prev_ds)
        prev_ds->Release();
    if (prev_rs)
        prev_rs->Release();
    if (prev_ps)
        prev_ps->Release();
    if (prev_srv)
        prev_srv->Release();
    if (prev_sampler)
        prev_sampler->Release();
    if (prev_ps_cb)
        prev_ps_cb->Release();
    if (prev_vs)
        prev_vs->Release();
    if (prev_vs_cb)
        prev_vs_cb->Release();

    if (m_draws == 1 || m_draws % Heartbeat_Frames == 0)
        logger::info("Proto v4.6 composite draw #{} ({}x{} panel rect on screen)", m_draws,
            desc.width, desc.height);
}
PLUGIN_NAMESPACE_END
