//
// Created by AmazingBuff on 2026/09/21.
//
// The D3D11 state descriptions mirror the pinned DirectXTK CommonStates source (vcpkg directxtk
// may2026, Src/CommonStates.cpp, MIT) as reused by the sibling Highlight-Lootable-Corpses commit
// 7a7c51e, re-typed to REX::W32. Unlike CommonStates the panel's own depth state uses a plain LESS
// comparison, because the panel camera is an ordinary forward-Z camera rather than the engine's
// reverse-Z one.
//

#include "panel_passes.h"

#include "render/shader_manager.h"

#include <DirectXMath.h>

#include <algorithm>
#include <cstring>

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // Neutral grey used when a draw carries no diffuse texture, so a textureless mesh still reads as
    // a solid surface under the panel lighting.
    constexpr float Flat_Albedo = 0.72f;

    struct PanelFrameCBData
    {
        DirectX::XMFLOAT4X4 view_proj;
        DirectX::XMFLOAT3 camera_position;
        float alpha_test;
    };

    // The per-draw transform, material and skinning palette share one constant buffer so the panel
    // touches only constant-buffer slots 0 and 1, which is exactly the set the engine state capture
    // restores.
    struct PanelDrawCBData
    {
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4 albedo;
        uint32_t has_texture;
        float pad[3];
        DirectX::XMFLOAT4X4 bones[Max_Palette_Bones];
    };

    // The composite parameters ride in the pass's existing pixel-stage slot 0, so the panel still
    // binds only the constant-buffer slots the engine state capture restores.
    struct PanelCompositeCBData
    {
        DirectX::XMFLOAT4 background;
        DirectX::XMFLOAT4 border;
        // The inset thickness in uv units along each axis, derived from the rectangle's pixel size so
        // it keeps its pixel thickness at every resolution. With the built-in chrome it is the hairline;
        // with a skin it is the configured skin inset.
        DirectX::XMFLOAT4 border_uv;
        // Whether the plugin paints its own chrome. A loaded skin owns the fill and the border band, so
        // the band is clipped away instead of being painted over the movie the engine drew there, and
        // the character keeps its own alpha so it blends over that movie instead of covering it.
        DirectX::XMFLOAT4 frame;
    };

    bool create_blend_state(REX::W32::ID3D11Device* a_device, REX::W32::ID3D11BlendState** a_result)
    {
        REX::W32::D3D11_BLEND_DESC desc{};
        desc.renderTarget[0].blendEnable = false;
        desc.renderTarget[0].srcBlend = desc.renderTarget[0].srcBlendAlpha = REX::W32::D3D11_BLEND_ONE;
        desc.renderTarget[0].destBlend = desc.renderTarget[0].destBlendAlpha = REX::W32::D3D11_BLEND_ZERO;
        desc.renderTarget[0].blendOp = desc.renderTarget[0].blendOpAlpha = REX::W32::D3D11_BLEND_OP_ADD;
        desc.renderTarget[0].renderTargetWriteMask = REX::W32::D3D11_COLOR_WRITE_ENABLE_ALL;

        REX::W32::HRESULT const hr = a_device->CreateBlendState(&desc, a_result);
        if (!REX::W32::SUCCESS(hr) || !*a_result)
        {
            logger::error("Panel pass: CreateBlendState failed ({:X})", static_cast<unsigned int>(hr));
            return false;
        }
        return true;
    }

    // Source alpha over inverse source alpha, so a fragment the character does not cover contributes
    // nothing and the destination - the chrome the engine already drew - is left exactly as it was.
    // The alpha channel keeps the destination's own value instead of being blended, so repeated frames
    // cannot accumulate coverage into the back buffer.
    bool create_alpha_blend_state(REX::W32::ID3D11Device* a_device, REX::W32::ID3D11BlendState** a_result)
    {
        REX::W32::D3D11_BLEND_DESC desc{};
        desc.renderTarget[0].blendEnable = true;
        desc.renderTarget[0].srcBlend = REX::W32::D3D11_BLEND_SRC_ALPHA;
        desc.renderTarget[0].destBlend = REX::W32::D3D11_BLEND_INV_SRC_ALPHA;
        desc.renderTarget[0].blendOp = REX::W32::D3D11_BLEND_OP_ADD;
        desc.renderTarget[0].srcBlendAlpha = REX::W32::D3D11_BLEND_ZERO;
        desc.renderTarget[0].destBlendAlpha = REX::W32::D3D11_BLEND_ONE;
        desc.renderTarget[0].blendOpAlpha = REX::W32::D3D11_BLEND_OP_ADD;
        desc.renderTarget[0].renderTargetWriteMask = REX::W32::D3D11_COLOR_WRITE_ENABLE_ALL;

        REX::W32::HRESULT const hr = a_device->CreateBlendState(&desc, a_result);
        if (!REX::W32::SUCCESS(hr) || !*a_result)
        {
            logger::error("Panel pass: CreateBlendState failed ({:X})", static_cast<unsigned int>(hr));
            return false;
        }
        return true;
    }

    bool create_depth_state(REX::W32::ID3D11Device* a_device, bool a_enable,
        REX::W32::D3D11_COMPARISON_FUNC a_function, REX::W32::ID3D11DepthStencilState** a_result)
    {
        REX::W32::D3D11_DEPTH_STENCIL_DESC desc{};
        desc.depthEnable = a_enable;
        desc.depthWriteMask = a_enable ? REX::W32::D3D11_DEPTH_WRITE_MASK_ALL : REX::W32::D3D11_DEPTH_WRITE_MASK_ZERO;
        desc.depthFunc = a_function;
        desc.stencilEnable = false;

        REX::W32::HRESULT const hr = a_device->CreateDepthStencilState(&desc, a_result);
        if (!REX::W32::SUCCESS(hr) || !*a_result)
        {
            logger::error("Panel pass: CreateDepthStencilState failed ({:X})", static_cast<unsigned int>(hr));
            return false;
        }
        return true;
    }

    bool create_rasterizer_state(REX::W32::ID3D11Device* a_device, REX::W32::ID3D11RasterizerState** a_result)
    {
        REX::W32::D3D11_RASTERIZER_DESC desc{};
        desc.cullMode = REX::W32::D3D11_CULL_NONE;
        desc.fillMode = REX::W32::D3D11_FILL_SOLID;
        desc.scissorEnable = false;
        desc.depthClipEnable = true;
        desc.multisampleEnable = true;

        REX::W32::HRESULT const hr = a_device->CreateRasterizerState(&desc, a_result);
        if (!REX::W32::SUCCESS(hr) || !*a_result)
        {
            logger::error("Panel pass: CreateRasterizerState failed ({:X})", static_cast<unsigned int>(hr));
            return false;
        }
        return true;
    }

    bool create_sampler_state(REX::W32::ID3D11Device* a_device, REX::W32::ID3D11SamplerState** a_result)
    {
        REX::W32::D3D11_SAMPLER_DESC desc{};
        desc.filter = REX::W32::D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.addressU = desc.addressV = desc.addressW = REX::W32::D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.maxAnisotropy = a_device->GetFeatureLevel() > REX::W32::D3D_FEATURE_LEVEL_9_1 ? REX::W32::D3D11_MAX_MAXANISOTROPY : 2u;
        desc.maxLOD = REX::W32::D3D11_FLOAT32_MAX;
        desc.comparisonFunc = REX::W32::D3D11_COMPARISON_NEVER;

        REX::W32::HRESULT const hr = a_device->CreateSamplerState(&desc, a_result);
        if (!REX::W32::SUCCESS(hr) || !*a_result)
        {
            logger::error("Panel pass: CreateSamplerState failed ({:X})", static_cast<unsigned int>(hr));
            return false;
        }
        return true;
    }

    bool create_constant_buffer(REX::W32::ID3D11Device* a_device, uint32_t a_byte_width, REX::W32::ID3D11Buffer** a_result)
    {
        REX::W32::D3D11_BUFFER_DESC desc{};
        desc.usage = REX::W32::D3D11_USAGE_DYNAMIC;
        desc.bindFlags = REX::W32::D3D11_BIND_CONSTANT_BUFFER;
        desc.cpuAccessFlags = REX::W32::D3D11_CPU_ACCESS_WRITE;
        desc.byteWidth = a_byte_width;

        REX::W32::HRESULT const hr = a_device->CreateBuffer(&desc, nullptr, a_result);
        if (!REX::W32::SUCCESS(hr) || !*a_result)
        {
            logger::error("Panel pass: CreateBuffer failed ({:X})", static_cast<unsigned int>(hr));
            return false;
        }
        return true;
    }

    // Column-vector 4x4 form of an engine transform: the rotation is scaled and the translation sits
    // in the fourth column, matching the row_major upload convention of the panel shaders.
    DirectX::XMFLOAT4X4 transform_to_matrix(RE::NiTransform const& a_transform)
    {
        DirectX::XMFLOAT4X4 matrix{};
        DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixIdentity());
        for (int row = 0; row < 3; ++row)
        {
            for (int col = 0; col < 3; ++col)
                matrix.m[row][col] = a_transform.rotate.entry[row][col] * a_transform.scale;
            matrix.m[row][3] = a_transform.translate[row];
        }
        return matrix;
    }

    // Palette in global bone index space: boneWorld * skinToBone with boneWorld first, so the
    // product maps model space straight to world space. Unused slots replicate the last valid entry
    // so no out-of-range read can pick up undefined content.
    bool build_palette(RE::NiSkinInstance& a_skin, DirectX::XMFLOAT4X4 (&a_palette)[Max_Palette_Bones])
    {
        if (!a_skin.skinData || !a_skin.boneWorldTransforms || a_skin.numMatrices == 0)
            return false;

        uint32_t const palette_count = std::min(a_skin.skinData->GetBoneCount(), a_skin.numMatrices);
        if (palette_count == 0 || palette_count > Max_Palette_Bones)
            return false;

        for (uint32_t index = 0; index < palette_count; ++index)
        {
            RE::NiTransform const* const bone_world = a_skin.boneWorldTransforms[index];
            if (!bone_world)
                return false;

            DirectX::XMFLOAT4X4 const bone = transform_to_matrix(*bone_world);
            DirectX::XMFLOAT4X4 const skin_to_bone = transform_to_matrix(a_skin.skinData->GetBoneDataSkinToBone(index));
            DirectX::XMStoreFloat4x4(&a_palette[index],
                DirectX::XMMatrixMultiply(DirectX::XMLoadFloat4x4(&bone), DirectX::XMLoadFloat4x4(&skin_to_bone)));
        }

        for (uint32_t index = palette_count; index < Max_Palette_Bones; ++index)
            a_palette[index] = a_palette[palette_count - 1];
        return true;
    }
}

// ---------------------------------------------------------------------------
// PanelGeometryPass
// ---------------------------------------------------------------------------

PanelGeometryPass::PanelGeometryPass() :
    m_ref_static_vs(nullptr),
    m_ref_skinned_vs(nullptr),
    m_ref_pixel_shader(nullptr),
    m_frame_cb(nullptr),
    m_draw_cb(nullptr),
    m_depth(nullptr),
    m_rasterizer(nullptr),
    m_blend(nullptr),
    m_sampler(nullptr),
    m_layouts() {}

PanelGeometryPass::~PanelGeometryPass()
{
    release();
}

bool PanelGeometryPass::init(REX::W32::ID3D11Device* a_device)
{
    if (!a_device)
        return false;

    ShaderManager& shaders = ShaderManager::instance();
    m_ref_static_vs = shaders.panel_static_vs();
    m_ref_skinned_vs = shaders.panel_skinned_vs();
    m_ref_pixel_shader = shaders.panel_ps();

    bool const ready =
        m_ref_static_vs && m_ref_skinned_vs && m_ref_pixel_shader &&
        create_constant_buffer(a_device, sizeof(PanelFrameCBData), &m_frame_cb) &&
        create_constant_buffer(a_device, sizeof(PanelDrawCBData), &m_draw_cb) &&
        create_depth_state(a_device, true, REX::W32::D3D11_COMPARISON_LESS, &m_depth) &&
        create_rasterizer_state(a_device, &m_rasterizer) &&
        create_blend_state(a_device, &m_blend) &&
        create_sampler_state(a_device, &m_sampler);
    if (!ready)
    {
        release();
        logger::error("Panel geometry pass: pipeline creation failed, panel rendering disabled");
        return false;
    }

    logger::info("Panel geometry pass ready (palette {} bones/draw)", Max_Palette_Bones);
    return true;
}

void PanelGeometryPass::release()
{
    if (m_sampler)
    {
        m_sampler->Release();
        m_sampler = nullptr;
    }
    if (m_blend)
    {
        m_blend->Release();
        m_blend = nullptr;
    }
    if (m_rasterizer)
    {
        m_rasterizer->Release();
        m_rasterizer = nullptr;
    }
    if (m_depth)
    {
        m_depth->Release();
        m_depth = nullptr;
    }
    if (m_draw_cb)
    {
        m_draw_cb->Release();
        m_draw_cb = nullptr;
    }
    if (m_frame_cb)
    {
        m_frame_cb->Release();
        m_frame_cb = nullptr;
    }
    m_ref_pixel_shader = nullptr;
    m_ref_skinned_vs = nullptr;
    m_ref_static_vs = nullptr;
    release_layouts();
}

bool PanelGeometryPass::same_layout(LayoutKey const& a_lhs, LayoutKey const& a_rhs) noexcept
{
    return a_lhs.skinned == a_rhs.skinned && a_lhs.has_uv == a_rhs.has_uv &&
        a_lhs.position_format == a_rhs.position_format && a_lhs.position_offset == a_rhs.position_offset &&
        a_lhs.skinning_offset == a_rhs.skinning_offset && a_lhs.uv_offset == a_rhs.uv_offset &&
        a_lhs.stride == a_rhs.stride && a_lhs.weight_format == a_rhs.weight_format &&
        a_lhs.weight_offset == a_rhs.weight_offset && a_lhs.index_format == a_rhs.index_format &&
        a_lhs.index_offset == a_rhs.index_offset;
}

void PanelGeometryPass::release_layouts()
{
    for (LayoutEntry const& entry : m_layouts)
    {
        if (entry.layout)
            entry.layout->Release();
    }
    m_layouts.clear();
}

REX::W32::ID3D11InputLayout* PanelGeometryPass::acquire_layout(REX::W32::ID3D11Device* a_device, PanelDraw const& a_draw)
{
    bool const skinned = a_draw.skin != nullptr;
    bool const from_desc = a_draw.position_format == REX::W32::DXGI_FORMAT_UNKNOWN;
    REX::W32::DXGI_FORMAT const position_format = from_desc
        ? (a_draw.vertex_desc.HasFlag(RE::BSGraphics::Vertex::VF_FULLPREC) ? REX::W32::DXGI_FORMAT_R32G32B32_FLOAT : REX::W32::DXGI_FORMAT_R16G16B16A16_FLOAT)
        : a_draw.position_format;
    uint32_t const position_offset = from_desc
        ? a_draw.vertex_desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_POSITION)
        : a_draw.position_offset;

    LayoutKey const key{
        skinned,
        a_draw.has_uv,
        static_cast<uint32_t>(position_format),
        position_offset,
        a_draw.vertex_desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_SKINNING),
        a_draw.vertex_desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_TEXCOORD0),
        a_draw.vertex_stride,
        static_cast<uint32_t>(a_draw.skin_layout.weight_format),
        a_draw.skin_layout.weight_offset,
        static_cast<uint32_t>(a_draw.skin_layout.index_format),
        a_draw.skin_layout.index_offset,
    };

    for (LayoutEntry const& entry : m_layouts)
    {
        if (same_layout(entry.key, key))
            return entry.layout;
    }

    REX::W32::ID3DBlob* const blob = skinned
        ? ShaderManager::instance().panel_skinned_vs_blob()
        : ShaderManager::instance().panel_static_vs_blob();
    if (!blob)
        return nullptr;

    REX::W32::D3D11_INPUT_ELEMENT_DESC elements[4]{};
    uint32_t count = 0;
    elements[count++] = {
        .semanticName = "POSITION",
        .semanticIndex = 0,
        .format = position_format,
        .inputSlot = 0,
        .alignedByteOffset = position_offset,
        .inputSlotClass = REX::W32::D3D11_INPUT_PER_VERTEX_DATA,
        .instanceDataStepRate = 0
    };
    if (key.has_uv)
    {
        elements[count++] = {
            .semanticName = "TEXCOORD",
            .semanticIndex = 0,
            .format = Panel_Uv_Format,
            .inputSlot = 0,
            .alignedByteOffset = key.uv_offset,
            .inputSlotClass = REX::W32::D3D11_INPUT_PER_VERTEX_DATA,
            .instanceDataStepRate = 0
        };
    }
    if (skinned)
    {
        elements[count++] = {
            .semanticName = "BLENDWEIGHT",
            .semanticIndex = 0,
            .format = a_draw.skin_layout.weight_format,
            .inputSlot = 0,
            .alignedByteOffset = a_draw.skin_layout.weight_offset,
            .inputSlotClass = REX::W32::D3D11_INPUT_PER_VERTEX_DATA,
            .instanceDataStepRate = 0
        };
        elements[count++] = {
            .semanticName = "BLENDINDICES",
            .semanticIndex = 0,
            .format = a_draw.skin_layout.index_format,
            .inputSlot = 0,
            .alignedByteOffset = a_draw.skin_layout.index_offset,
            .inputSlotClass = REX::W32::D3D11_INPUT_PER_VERTEX_DATA,
            .instanceDataStepRate = 0
        };
    }

    REX::W32::ID3D11InputLayout* layout = nullptr;
    REX::W32::HRESULT const hr = a_device->CreateInputLayout(elements, count, blob->GetBufferPointer(), blob->GetBufferSize(), &layout);
    if (!REX::W32::SUCCESS(hr) || !layout)
    {
        logger::error("Panel geometry pass: CreateInputLayout failed ({:X}), affected meshes skipped", static_cast<unsigned int>(hr));
        return nullptr;
    }

    m_layouts.push_back(LayoutEntry{ key, layout });
    return layout;
}

void PanelGeometryPass::draw(REX::W32::ID3D11Device* a_device, REX::W32::ID3D11DeviceContext* a_context,
    PanelTarget const& a_target, PanelCameraFrame const& a_camera, float a_alpha_test,
    float const a_clear_color[4], std::span<PanelDraw const> a_draws)
{
    if (!a_device || !a_context || !m_ref_static_vs || !m_ref_skinned_vs || !m_ref_pixel_shader ||
        !m_frame_cb || !m_draw_cb || !m_depth || !m_rasterizer || !m_blend || !m_sampler)
        return;
    if (!a_target.rtv() || !a_target.dsv())
        return;

    REX::W32::ID3D11RenderTargetView* const rtv = a_target.rtv();
    a_context->OMSetRenderTargets(1, &rtv, a_target.dsv());
    a_context->ClearRenderTargetView(rtv, a_clear_color);
    a_context->ClearDepthStencilView(a_target.dsv(), REX::W32::D3D11_CLEAR_DEPTH, 1.0f, 0);

    REX::W32::D3D11_VIEWPORT const viewport{
        .topLeftX = 0.0f,
        .topLeftY = 0.0f,
        .width = static_cast<float>(a_target.width()),
        .height = static_cast<float>(a_target.height()),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    a_context->RSSetViewports(1, &viewport);
    a_context->OMSetBlendState(m_blend, nullptr, 0xFFFFFFFF);
    a_context->OMSetDepthStencilState(m_depth, 0);
    a_context->RSSetState(m_rasterizer);
    a_context->PSSetSamplers(0, 1, &m_sampler);

    // The camera and cutoff are uploaded once per frame, into the constant buffer both stages read.
    PanelFrameCBData frame{};
    frame.view_proj = a_camera.view_proj;
    frame.camera_position = a_camera.eye;
    frame.alpha_test = a_alpha_test;
    REX::W32::D3D11_MAPPED_SUBRESOURCE mapped{};
    if (!REX::W32::SUCCESS(a_context->Map(m_frame_cb, 0, REX::W32::D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;
    std::memcpy(mapped.data, &frame, sizeof(frame));
    a_context->Unmap(m_frame_cb, 0);
    a_context->VSSetConstantBuffers(0, 1, &m_frame_cb);
    a_context->PSSetConstantBuffers(0, 1, &m_frame_cb);

    for (PanelDraw const& draw : a_draws)
    {
        if (!draw.vertex_buffer || !draw.index_buffer || draw.index_count == 0 || draw.vertex_stride == 0)
            continue;

        bool const skinned = draw.skin != nullptr;
        REX::W32::ID3D11InputLayout* const layout = acquire_layout(a_device, draw);
        if (!layout)
            continue;

        PanelDrawCBData data{};
        if (!skinned && draw.node)
            data.world = transform_to_matrix(draw.node->world);
        else
            DirectX::XMStoreFloat4x4(&data.world, DirectX::XMMatrixIdentity());

        data.albedo = DirectX::XMFLOAT4{ Flat_Albedo, Flat_Albedo, Flat_Albedo, draw.material_alpha };
        data.has_texture = draw.has_uv ? 1u : 0u;

        if (skinned && !build_palette(*draw.skin, data.bones))
            continue;

        REX::W32::D3D11_MAPPED_SUBRESOURCE draw_mapped{};
        if (!REX::W32::SUCCESS(a_context->Map(m_draw_cb, 0, REX::W32::D3D11_MAP_WRITE_DISCARD, 0, &draw_mapped)))
            return;
        std::memcpy(draw_mapped.data, &data, sizeof(data));
        a_context->Unmap(m_draw_cb, 0);
        a_context->VSSetConstantBuffers(1, 1, &m_draw_cb);

        a_context->IASetInputLayout(layout);
        a_context->IASetPrimitiveTopology(REX::W32::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        uint32_t const stride = draw.vertex_stride;
        uint32_t const offset = 0;
        REX::W32::ID3D11Buffer* const vertex_buffer = draw.vertex_buffer;
        a_context->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
        // BSTriShape::vertexCount and a partition's vertices are both 16-bit, so the indices always are.
        a_context->IASetIndexBuffer(draw.index_buffer, REX::W32::DXGI_FORMAT_R16_UINT, 0);

        a_context->VSSetShader(skinned ? m_ref_skinned_vs : m_ref_static_vs, nullptr, 0);
        a_context->PSSetShader(m_ref_pixel_shader, nullptr, 0);

        // A draw without the UV flag binds no texture; the pixel shader reads the flat albedo instead.
        REX::W32::ID3D11ShaderResourceView* const diffuse = draw.has_uv ? draw.diffuse_view : nullptr;
        a_context->PSSetShaderResources(0, 1, &diffuse);

        a_context->DrawIndexed(draw.index_count, 0, 0);
    }
}

// ---------------------------------------------------------------------------
// PanelCompositePass
// ---------------------------------------------------------------------------

PanelCompositePass::PanelCompositePass() :
    m_ref_fullscreen_vs(nullptr),
    m_ref_background_ps(nullptr),
    m_ref_copy_ps(nullptr),
    m_background_cb(nullptr),
    m_blend(nullptr),
    m_blend_alpha(nullptr),
    m_depth_none(nullptr),
    m_rasterizer(nullptr),
    m_sampler(nullptr) {}

PanelCompositePass::~PanelCompositePass()
{
    release();
}

bool PanelCompositePass::init(REX::W32::ID3D11Device* a_device)
{
    if (!a_device)
        return false;

    ShaderManager& shaders = ShaderManager::instance();
    m_ref_fullscreen_vs = shaders.panel_fullscreen_vs();
    m_ref_background_ps = shaders.panel_background_ps();
    m_ref_copy_ps = shaders.panel_composite_ps();

    bool const ready =
        m_ref_fullscreen_vs && m_ref_background_ps && m_ref_copy_ps &&
        create_constant_buffer(a_device, sizeof(PanelCompositeCBData), &m_background_cb) &&
        create_blend_state(a_device, &m_blend) &&
        create_alpha_blend_state(a_device, &m_blend_alpha) &&
        create_depth_state(a_device, false, REX::W32::D3D11_COMPARISON_ALWAYS, &m_depth_none) &&
        create_rasterizer_state(a_device, &m_rasterizer) &&
        create_sampler_state(a_device, &m_sampler);
    if (!ready)
    {
        release();
        logger::error("Panel composite pass: pipeline creation failed, panel rendering disabled");
        return false;
    }

    logger::info("Panel composite pass ready");
    return true;
}

void PanelCompositePass::release()
{
    if (m_sampler)
    {
        m_sampler->Release();
        m_sampler = nullptr;
    }
    if (m_rasterizer)
    {
        m_rasterizer->Release();
        m_rasterizer = nullptr;
    }
    if (m_depth_none)
    {
        m_depth_none->Release();
        m_depth_none = nullptr;
    }
    if (m_blend_alpha)
    {
        m_blend_alpha->Release();
        m_blend_alpha = nullptr;
    }
    if (m_blend)
    {
        m_blend->Release();
        m_blend = nullptr;
    }
    if (m_background_cb)
    {
        m_background_cb->Release();
        m_background_cb = nullptr;
    }
    m_ref_copy_ps = nullptr;
    m_ref_background_ps = nullptr;
    m_ref_fullscreen_vs = nullptr;
}

void PanelCompositePass::draw(REX::W32::ID3D11DeviceContext* a_context, REX::W32::ID3D11RenderTargetView* a_target,
    REX::W32::ID3D11ShaderResourceView* a_panel, REX::W32::D3D11_VIEWPORT const& a_rectangle,
    float const a_background[4], float const a_border[4], uint32_t a_border_thickness,
    bool a_built_in_chrome) const
{
    if (!a_context || !a_target || !m_ref_fullscreen_vs || !m_ref_background_ps || !m_ref_copy_ps ||
        !m_background_cb || !m_depth_none || !m_blend || !m_blend_alpha || !m_rasterizer || !m_sampler)
        return;

    // The panel never tests against the world depth: the window must be a solid overlay.
    a_context->OMSetRenderTargets(1, &a_target, nullptr);
    // The chrome decides how the character reaches the target: opaque over the plugin's own fill, or
    // source-alpha blended over the movie the engine has already drawn underneath. Both states belong
    // to this pass and are bound here, inside the caller's single state-capture pair.
    a_context->OMSetBlendState(a_built_in_chrome ? m_blend : m_blend_alpha, nullptr, 0xFFFFFFFF);
    a_context->OMSetDepthStencilState(m_depth_none, 0);
    a_context->RSSetState(m_rasterizer);
    a_context->RSSetViewports(1, &a_rectangle);
    a_context->IASetInputLayout(nullptr);
    a_context->IASetPrimitiveTopology(REX::W32::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    a_context->VSSetShader(m_ref_fullscreen_vs, nullptr, 0);
    a_context->PSSetSamplers(0, 1, &m_sampler);

    float const border_uv_x = a_rectangle.width > 0.0f
        ? static_cast<float>(a_border_thickness) / a_rectangle.width
        : 0.0f;
    float const border_uv_y = a_rectangle.height > 0.0f
        ? static_cast<float>(a_border_thickness) / a_rectangle.height
        : 0.0f;

    PanelCompositeCBData data{};
    data.background = DirectX::XMFLOAT4{ a_background[0], a_background[1], a_background[2], a_background[3] };
    data.border = DirectX::XMFLOAT4{ a_border[0], a_border[1], a_border[2], a_border[3] };
    data.border_uv = DirectX::XMFLOAT4{ border_uv_x, border_uv_y, 0.0f, 0.0f };
    data.frame = DirectX::XMFLOAT4{ a_built_in_chrome ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f };
    REX::W32::D3D11_MAPPED_SUBRESOURCE mapped{};
    if (!REX::W32::SUCCESS(a_context->Map(m_background_cb, 0, REX::W32::D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;
    std::memcpy(mapped.data, &data, sizeof(data));
    a_context->Unmap(m_background_cb, 0);
    a_context->PSSetConstantBuffers(0, 1, &m_background_cb);

    // 1) The built-in chrome's opaque fill, so no world content shows through the window. A skin owns
    // that fill, and painting it here would cover the movie the engine drew underneath.
    if (a_built_in_chrome)
    {
        REX::W32::ID3D11ShaderResourceView* const no_texture = nullptr;
        a_context->PSSetShaderResources(0, 1, &no_texture);
        a_context->PSSetShader(m_ref_background_ps, nullptr, 0);
        a_context->Draw(3, 0);
    }

    // 2) The offscreen character over the same rectangle. With the built-in chrome it is opaque and
    // framed by the built-in hairline. With a skin the shader clips the border band away and keeps the
    // character's alpha, so the blend writes the character over the movie the engine drew and leaves
    // every fragment the character does not cover exactly as the engine left it.
    a_context->PSSetShader(m_ref_copy_ps, nullptr, 0);
    a_context->PSSetShaderResources(0, 1, &a_panel);
    a_context->Draw(3, 0);
}

PLUGIN_NAMESPACE_END
