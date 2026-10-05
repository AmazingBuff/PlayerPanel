//
// composite_ps.hlsl — the panel composite quad's pixel shader.
// Generated-header embedding: cmake/embed_shaders.cmake (render_shaders::
// CompositePS); consumed by render/composite.cpp. Extracted verbatim from
// the former inline string literal (2026-10-05 file-organization pass).
//

// Run 31: the studio target follows the call site's format. In the
// menu stream that is the LDR UI composite (R8G8B8A8, format 28);
// in the world stream (route 3 P replays) it is the HDR main
// target (R11G11B10_FLOAT, format 10) — sampling and writing it
// verbatim produced the washed noise in the first P image. The
// composite now Reinhard-maps when the studio target is HDR; LDR
// targets pass through unchanged.

Texture2D g_tex : register(t0);
SamplerState g_samp : register(s0);
cbuffer PanelCB : register(b0) { float4 g_ndcRect; float4 g_flags; }  // g_flags.x = 1 when HDR; g_flags.y = sampled x-span (aspect-correct window)
float4 ps_main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target {
    // v6.32: sample an aspect-correct horizontal slice of the target —
    // squeezing the full 16:9 target into the narrow panel stretched the
    // figure (run 65). The window spans (panel aspect / target aspect) of
    // the target width, centered — the figure keeps its world proportions.
    uv.x = 0.5 + (uv.x - 0.5) * g_flags.y;
    float3 c = g_tex.Sample(g_samp, uv).rgb;
    if (g_flags.x > 0.5)
        c = c / (1.0 + c);
    return float4(c, 1.0);
}
