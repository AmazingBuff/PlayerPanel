//
// composite_vs.hlsl — the panel composite quad's vertex shader.
// Generated-header embedding: cmake/embed_shaders.cmake (render_shaders::
// CompositeVS); consumed by render/composite.cpp. Extracted verbatim from
// the former inline string literal (2026-10-05 file-organization pass).
//

// A fullscreen-triangle-pair quad addressed purely by SV_VertexID
// (no vertex buffers, no input layout): the vertex shader places the
// corners from a one-float4 constant buffer holding the panel rect
// in NDC (x0, yBottom, x1, yTop), the pixel shader samples the
// studio target opaquely. Drawn at DrawInterfaceStart entry into
// whatever target is bound, BEFORE the original menu draw — the
// PRD §2 order (panel under other UI). Only the state the quad
// actually sets is saved and restored; the engine rebinds the rest
// for its own draws.

cbuffer PanelCB : register(b0)
{
    float4 g_ndcRect;
    float4 g_flags;
}

struct PS_IN
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

PS_IN vs_main(uint id : SV_VertexID)
{
    PS_IN o;
    // v4.4: clockwise in window space (the viewport flips NDC y). The old
    // order was counter-clockwise on screen — back-facing — and the
    // engine's CULL_BACK rasterizer state silently culled every quad
    // since v4.1 (Draw succeeded, zero pixels rasterized).
    float2 corners[6] = { {-1,-1}, {-1,1}, {1,-1}, {-1,1}, {1,1}, {1,-1} };
    float2 c = corners[id];
    float x = lerp(g_ndcRect.x, g_ndcRect.z, c.x * 0.5 + 0.5);
    float y = lerp(g_ndcRect.y, g_ndcRect.w, c.y * 0.5 + 0.5);
    o.pos = float4(x, y, 0.0, 1.0);
    o.uv = float2(c.x * 0.5 + 0.5, 0.5 - c.y * 0.5);
    return o;
}


Texture2D g_tex : register(t0);
SamplerState g_samp : register(s0);

float4 ps_main(PS_IN ps_in) : SV_Target
{
    // v6.32: sample an aspect-correct horizontal slice of the target —
    // squeezing the full 16:9 target into the narrow panel stretched the
    // figure (run 65). The window spans (panel aspect / target aspect) of
    // the target width, centered — the figure keeps its world proportions.
    ps_in.uv.x = 0.5 + (ps_in.uv.x - 0.5) * g_flags.y;
    float3 c = g_tex.Sample(g_samp, ps_in.uv).rgb;
    if (g_flags.x > 0.5)
        c = c / (1.0 + c);
    return float4(c, 1.0);
}