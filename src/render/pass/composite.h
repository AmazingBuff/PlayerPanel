#pragma once

#include "render/dx11/common_states.h"
#include "render/dx11/d3d11_util.h"

PLUGIN_NAMESPACE_BEGIN

class CompositePass
{
public:
    CompositePass();
    ~CompositePass();

    bool init(REX::W32::ID3D11Device* device);
    void draw(REX::W32::ID3D11DeviceContext* context, REX::W32::ID3D11RenderTargetView* rtv, const CommonStates& states, RenderTarget& render_target) const;
private:
    REX::W32::ID3D11VertexShader* m_ref_vertex_shader;
    REX::W32::ID3D11PixelShader* m_ref_pixel_shader;
    REX::W32::ID3D11SamplerState* m_sampler;
    REX::W32::ID3D11Buffer* m_cb;
};
PLUGIN_NAMESPACE_END
