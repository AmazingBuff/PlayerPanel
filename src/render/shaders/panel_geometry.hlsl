// Panel geometry: draws one preview character into the panel's private offscreen target.
//
// The two vertex shaders never read a packed engine normal. The lighting normal is derived in the
// pixel shader from screen-space derivatives of the interpolated world position, so the shading
// cannot depend on an unverified packed-vertex-normal layout.
//
// Matrix convention: every matrix in a constant buffer is row_major and is multiplied on the left of
// a column vector, so `mul(matrix, float4(value, 1.0f))` transforms the point. The engine's
// row-vector transforms are transposed into this form before upload.
//
// Constant-buffer budget: the panel binds at most two constant-buffer slots per stage so the engine
// state capture, which restores slots 0..1 of both stages, covers every slot the panel touches. b0
// holds the per-frame camera and cutoff, b1 holds the per-draw transform, material and skinning
// palette together.

// Per-frame constant buffer (b0): the panel camera and the configured alpha cutoff. bound to both
// the vertex and the pixel stage.
cbuffer PanelFrameCB : register(b0)
{
    row_major float4x4 g_view_proj;
    float3 g_camera_position;
    float g_alpha_test;
};

// Per-draw constant buffer (b1): the world transform, the flat albedo with the material alpha in .w,
// the diffuse-texture flag, and the skinning palette. g_world is the node's world transform for a
// static draw and the identity for a skinned draw, because the palette already produced world-space
// vertices.
cbuffer PanelDrawCB : register(b1)
{
    row_major float4x4 g_world;
    float4 g_albedo;
    uint g_has_texture;
    float3 g_pad;
    row_major float4x4 g_bones[128];
};

Texture2D g_diffuse : register(t0);
SamplerState g_sampler : register(s0);

struct VS_OUT
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
    float3 world : TEXCOORD1;
    nointerpolation float4 albedo : TEXCOORD2;
    nointerpolation uint has_texture : TEXCOORD3;
};

// Static BSTriShape path. The UV input is bound only when the mesh's vertex descriptor carries the
// UV attribute; when it is not bound Direct3D fills the component with zero and the diffuse-texture
// flag is clear, so the pixel shader never samples with an unbound UV.
VS_OUT vs_static_main(float3 pos : POSITION, float2 uv : TEXCOORD0)
{
    float4 const world = mul(g_world, float4(pos, 1.0f));

    VS_OUT o;
    o.pos = mul(g_view_proj, world);
    o.uv = uv;
    o.world = world.xyz;
    o.albedo = g_albedo;
    o.has_texture = g_has_texture;
    return o;
}

// Skinned NiSkinPartition path: POSITION, optional TEXCOORD0 and the skinning attributes. Indices
// are global bone indices, and the palette is built with boneWorld first, matching the engine's
// skin-to-bone-then-bone-world composition.
struct VS_SKIN_IN
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD0;
    float4 weights : BLENDWEIGHT;
    uint4 indices : BLENDINDICES;
};

VS_OUT vs_skinned_main(VS_SKIN_IN skin_in)
{
    float4 p = 0.0f;
    [unroll]
    for (uint i = 0; i < 4; ++i)
        p += skin_in.weights[i] * mul(g_bones[skin_in.indices[i]], float4(skin_in.pos, 1.0f));

    float4 const world = mul(g_world, p);

    VS_OUT o;
    o.pos = mul(g_view_proj, world);
    o.uv = skin_in.uv;
    o.world = world.xyz;
    o.albedo = g_albedo;
    o.has_texture = g_has_texture;
    return o;
}

float4 ps_panel_main(VS_OUT ps_in) : SV_Target
{
    // Geometric normal from the screen-space derivatives of the world position; a degenerate
    // triangle (zero-area derivatives) falls back to a fixed direction instead of returning NaN.
    float3 const geometric = cross(ddx(ps_in.world), ddy(ps_in.world));
    float3 normal = length(geometric) > 1e-8f ? normalize(geometric) : float3(0.0f, 0.0f, 1.0f);

    // Face the normal towards the camera so a two-sided mesh (hair, cloth) is lit on both sides.
    float3 const to_camera = g_camera_position - ps_in.world;
    if (dot(normal, to_camera) < 0.0f)
        normal = -normal;

    // Albedo: the sampled diffuse texture, or the per-draw flat albedo when the draw carries no
    // diffuse texture.
    float3 albedo = ps_in.albedo.rgb;
    float alpha = ps_in.albedo.a;
    if (ps_in.has_texture != 0)
    {
        float4 const texel = g_diffuse.Sample(g_sampler, ps_in.uv);
        albedo = texel.rgb;
        alpha *= texel.a;
    }

    // Alpha cutout: the material alpha multiplied by the sampled texture alpha against the
    // configured threshold.
    if (alpha < g_alpha_test)
        discard;

    // One fixed directional light plus an ambient term: the panel lighting never depends on the
    // game's time of day, weather or cell.
    float3 const to_light = normalize(float3(-0.35f, -0.45f, 0.82f));
    float3 const ambient = float3(0.32f, 0.32f, 0.34f);
    float const diffuse = saturate(dot(normal, to_light));

    return float4(saturate(albedo * (ambient + diffuse * 0.78f)), alpha);
}
