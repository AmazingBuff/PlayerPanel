//
// Created by AmazingBuff on 2026/09/21.
//
// Ported from Highlight-Lootable-Corpses (https://github.com/AmazingBuff/Highlight-Lootable-Corpses.git),
// src/render/dx11/d3d11_util.h, commit 7a7c51e. Both repositories are GPL-3.0 with the same author.
//

#pragma once

#include <REX/W32/D3D11.h>
#include <cstdint>

PLUGIN_NAMESPACE_BEGIN

// Compiles one entry point from an embedded HLSL source string. Returns the bytecode blob, or
// nullptr after logging the compiler diagnostics; D3DCompile always produces a blob or an error
// blob, never both.
[[nodiscard]] REX::W32::ID3DBlob* compile_shader(char const* source, char const* entry, char const* target, char const* name, char const* log_prefix);

// Saves every D3D11 state a private pass overwrites and puts it back on destruction. The captured
// set is deliberately wider than any single pass needs: render targets, depth-stencil view, blend
// state, depth-stencil state, rasterizer state, viewport, scissor rectangle, input layout,
// primitive topology, vertex and index buffers, vertex and pixel shaders with their class
// instances, vertex and pixel constant buffers, pixel shader resources and the pixel sampler.
class D3D11StateCapture
{
public:
    explicit D3D11StateCapture(REX::W32::ID3D11DeviceContext* context);
    ~D3D11StateCapture();

    D3D11StateCapture(D3D11StateCapture const&) = delete;
    D3D11StateCapture(D3D11StateCapture&&) = delete;
    D3D11StateCapture& operator=(D3D11StateCapture const&) = delete;
    D3D11StateCapture& operator=(D3D11StateCapture&&) = delete;

    void capture();
    void restore() const;
private:
    REX::W32::ID3D11DeviceContext* m_ref_context;

    REX::W32::ID3D11RenderTargetView* m_render_target;
    REX::W32::ID3D11DepthStencilView* m_depth_stencil;
    REX::W32::ID3D11BlendState* m_blend;
    float m_blend_factor[4];
    uint32_t m_sample_mask;
    REX::W32::ID3D11DepthStencilState* m_depth;
    uint32_t m_stencil_ref;
    REX::W32::ID3D11RasterizerState* m_rasterizer;
    uint32_t m_viewport_count;
    REX::W32::D3D11_VIEWPORT m_viewports[REX::W32::D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    uint32_t m_scissor_count;
    REX::W32::D3D11_RECT m_scissor_rects[REX::W32::D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    REX::W32::ID3D11InputLayout* m_input_layout;
    REX::W32::D3D11_PRIMITIVE_TOPOLOGY m_topology;
    REX::W32::ID3D11Buffer* m_vertex_buffer;
    uint32_t m_vertex_stride;
    uint32_t m_vertex_offset;
    REX::W32::ID3D11Buffer* m_index_buffer;
    REX::W32::DXGI_FORMAT m_index_format;
    uint32_t m_index_offset;
    REX::W32::ID3D11VertexShader* m_vertex_shader;
    REX::W32::ID3D11ClassInstance* m_vertex_instances[8];
    uint32_t m_vertex_instance_count;
    REX::W32::ID3D11PixelShader* m_pixel_shader;
    REX::W32::ID3D11ClassInstance* m_pixel_instances[8];
    uint32_t m_pixel_instance_count;
    REX::W32::ID3D11Buffer* m_vertex_cbs[2];
    REX::W32::ID3D11Buffer* m_pixel_cbs[2];
    REX::W32::ID3D11ShaderResourceView* m_pixel_srvs[3];
    REX::W32::ID3D11SamplerState* m_pixel_sampler;
};

PLUGIN_NAMESPACE_END
