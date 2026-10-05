#pragma once
// The panel composite: a fullscreen SV_VertexID quad plus the fixed
// pipeline states that draw the studio image into the visible composite
// target. (2026-10-05 src migration: split out of tools/m0_proto, behavior byte-identical)

PLUGIN_NAMESPACE_BEGIN
class CompositeRenderer
{
public:
    static CompositeRenderer& instance()
    {
        static CompositeRenderer s_instance;
        return s_instance;
    }

    bool ensure(REX::W32::ID3D11Device* device);
    void draw(REX::W32::ID3D11DeviceContext* ctx, REX::W32::ID3D11RenderTargetView* rtv);

    // Run 41: the OPAQUE blend used by the proactive P draw guard —
    // blend disabled, all channels written (the studio target is
    // ours alone; unknown engine blend factors were the second
    // black-out suspect).
    [[nodiscard]] REX::W32::ID3D11BlendState* opaque_blend() const { return m_opaque; }

    // Run 42: the CLEAN rasterizer for the proactive P draw — no
    // depth bias (the item call site leaves depthBiasClamp=-100
    // bound, which shoves far-plane-adjacent skinned depth out of
    // range), no scissor, cull off.
    [[nodiscard]] REX::W32::ID3D11RasterizerState* clean_rasterizer() const { return m_rs; }

private:
    CompositeRenderer() = default;

    REX::W32::ID3D11VertexShader* m_vs = nullptr;
    REX::W32::ID3D11PixelShader* m_ps = nullptr;
    REX::W32::ID3D11SamplerState* m_sampler = nullptr;
    REX::W32::ID3D11BlendState* m_blend = nullptr;
    REX::W32::ID3D11BlendState* m_opaque = nullptr;
    REX::W32::ID3D11DepthStencilState* m_depth_off = nullptr;
    REX::W32::ID3D11RasterizerState* m_rs = nullptr;
    REX::W32::ID3D11Buffer* m_cb = nullptr;
    std::uint32_t m_draws = 0;
    bool m_failed = false;
    bool m_rs_logged = false;
    bool m_blend_logged = false;
};
PLUGIN_NAMESPACE_END
