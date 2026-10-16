// CS's native deferred material composite has already merged diffuse, specular and reflection.
// Match Lighting.hlsl's non-world forward output before drawing transparent menu materials.
cbuffer EncodingBuffer : register(b0)
{
    uint2 b_dimensions;
    uint b_linear_lighting;
    uint b_padding;
};

Texture2D<float4> t_material : register(t0);
RWTexture2D<float4> u_color : register(u0);

[numthreads(8, 8, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID)
{
    if (any(thread_id.xy >= b_dimensions))
        return;
    float3 material = max(t_material[thread_id.xy].rgb, 0.0);
    float3 color = b_linear_lighting != 0 ? pow(material, 1.0 / 2.2) : material;
    u_color[thread_id.xy] = float4(color, 1.0);
}
