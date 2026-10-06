//
// Panel composite implementation. (2026-10-05 src migration: split out of tools/m0_proto, behavior byte-identical)
//

#include "composite.h"
#include "render/shader_sources.h"
#include "render/shader_manager.h"

PLUGIN_NAMESPACE_BEGIN
namespace
{
    constexpr float Panel_Screen_MinX = 0.73f;
    constexpr float Panel_Screen_MaxX = 0.99f;
    constexpr float Panel_Screen_MinY = 0.05f;
    constexpr float Panel_Screen_MaxY = 0.96f;
}

CompositePass::CompositePass() : m_sampler(nullptr), m_cb(nullptr)
{
    m_ref_vertex_shader = ShaderManager::instance().composite_vs();
    m_ref_pixel_shader = ShaderManager::instance().composite_ps();
}

CompositePass::~CompositePass()
{

}

bool CompositePass::init(REX::W32::ID3D11Device* device)
{
    if (m_sampler && m_cb)
        return true;

    REX::W32::D3D11_SAMPLER_DESC sampler_desc{};
    sampler_desc.filter = REX::W32::D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler_desc.addressU = REX::W32::D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.addressV = REX::W32::D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.addressW = REX::W32::D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.comparisonFunc = REX::W32::D3D11_COMPARISON_NEVER;
    sampler_desc.maxLOD = REX::W32::D3D11_FLOAT32_MAX;

    device->CreateSamplerState(&sampler_desc, &m_sampler);

    constexpr float cb_data[8] = {
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

    const REX::W32::D3D11_SUBRESOURCE_DATA subresource_data{
        .sysMem = cb_data,
        .sysMemPitch = 0,
        .sysMemSlicePitch = 0
    };
    device->CreateBuffer(&cb_desc, &subresource_data, &m_cb);

    if (!m_sampler || !m_cb)
    {
        logger::warn("Proto v4 composite state creation failed");
        if (m_sampler)
            m_sampler->Release();
        if (m_cb)
            m_cb->Release();
        return false;
    }

    return true;
}

void CompositePass::draw(REX::W32::ID3D11DeviceContext* context, REX::W32::ID3D11RenderTargetView* rtv, const CommonStates& states, RenderTarget& render_target) const
{
    REX::W32::D3D11_MAPPED_SUBRESOURCE mapped{};
    if (context->Map(m_cb, 0, REX::W32::D3D11_MAP_WRITE_DISCARD, 0, &mapped) == 0)
    {
        auto* out = static_cast<float*>(mapped.data);
        out[0] = 2.0f * Panel_Screen_MinX - 1.0f;
        out[1] = 1.0f - 2.0f * Panel_Screen_MaxY;
        out[2] = 2.0f * Panel_Screen_MaxX - 1.0f;
        out[3] = 1.0f - 2.0f * Panel_Screen_MinY;
        out[4] = 0.f;
        out[5] = (Panel_Screen_MaxX - Panel_Screen_MinX) /
                 (Panel_Screen_MaxY - Panel_Screen_MinY);
        context->Unmap(m_cb, 0);
    }

    context->VSSetShader(m_ref_vertex_shader, nullptr, 0);
    context->VSSetConstantBuffers(0, 1, &m_cb);
    context->IASetPrimitiveTopology(REX::W32::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->RSSetState(states.cull_none());

    REX::W32::ID3D11ShaderResourceView* srv = render_target.srv();
    context->PSSetShader(m_ref_pixel_shader, nullptr, 0);
    context->PSSetShaderResources(0, 1, &srv);
    context->PSSetSamplers(0, 1, &m_sampler);
    context->PSSetConstantBuffers(0, 1, &m_cb);

    context->OMSetRenderTargets(1, &rtv, nullptr);
    context->OMSetBlendState(states.opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(states.depth_none(), 0);

    context->Draw(6, 0);
}
PLUGIN_NAMESPACE_END
