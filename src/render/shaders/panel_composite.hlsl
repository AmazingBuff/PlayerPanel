// Panel composite: draws the offscreen character over the panel rectangle. In the panel's built-in
// chrome it first fills the rectangle with an opaque background colour and then draws the offscreen
// colour over the same rectangle, framed by one hairline border, so no world content can show through
// the window and the window reads as a deliberate frame instead of a foreign rectangle. When a skin's
// movie owns the panel's chrome instead, the fill is not drawn at all and this shader clips the border
// band away, because the engine has already drawn the skin's frame underneath and it must stay visible.
//
// The two chrome modes therefore differ in what the character write leaves behind. Built-in chrome
// writes the character's colour with an alpha of one, because the window is meant to be solid. Skin
// chrome keeps the character's own straight alpha, because the pass is blended over the movie the
// engine already drew: a fragment with an alpha of zero must leave that movie untouched, and a
// frame's worth of the offscreen target carries no character at all.
//
// The rectangle itself comes from the viewport the caller sets, not from the vertices: the shared
// vertex shader always covers the whole target with one triangle.

cbuffer PanelCompositeCB : register(b0)
{
    float4 g_background;
    float4 g_border;
    // Inset thickness in uv units along each axis; the caller derives it from the rectangle's pixel
    // size, so it keeps the same pixel thickness at every resolution. It is the same value the
    // character is clipped to, which is what keeps a skin's border from being overdrawn. With the
    // built-in chrome it is the hairline; with a skin it is the skin inset the caller read from the
    // configuration, which is deliberately thicker than the hairline so a skin can be seen.
    float4 g_border_uv;
    // .x is one while the plugin paints its own chrome, and zero when a skin's movie owns the panel's
    // frame and background. It also selects whether the character write is opaque or keeps its alpha,
    // because only the skin path is blended over the movie the engine drew.
    float4 g_frame;
};

Texture2D g_panel : register(t0);
SamplerState g_sampler : register(s0);

struct PS_IN
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

PS_IN vs_main(uint id : SV_VertexID)
{
    const float2 uv = float2(float((id << 1u) & 2u), float(id & 2u));

    PS_IN o;
    o.pos = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, 0.0f, 1.0f);
    o.uv = uv;
    return o;
}

// Opaque fill: the alpha is pinned to one so the rectangle always occludes what is behind it.
float4 ps_background_main(PS_IN ps_in) : SV_Target
{
    return float4(g_background.rgb, 1.0f);
}

// The offscreen colour, plus the panel's single built-in decoration. With the built-in chrome the
// character is drawn by the geometry pass DIRECTLY onto the window before this shader runs, so this
// pass only paints the hairline band into the rectangle's outer edge and discards its interior -
// discarding instead of copying keeps the character pixels exactly where the geometry pass left
// them. (When a skin's movie owned the chrome, the interior kept the character's straight alpha and
// blended it over that movie; that path returns with the skin.)
float4 ps_panel_main(PS_IN ps_in) : SV_Target
{
    const float2 edge_distance = min(ps_in.uv, 1.0f - ps_in.uv);
    if (edge_distance.x < g_border_uv.x || edge_distance.y < g_border_uv.y)
    {
        if (g_frame.x < 0.5f)
            discard;

        return float4(g_border.rgb, 1.0f);
    }

    if (g_frame.x > 0.5f)
        discard;

    return g_panel.Sample(g_sampler, ps_in.uv);
}
