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

    bool create_blend_state(REX::W32::ID3D11Device* device, REX::W32::ID3D11BlendState** result)
    {
        REX::W32::D3D11_BLEND_DESC desc{};
        desc.renderTarget[0].blendEnable = false;
        desc.renderTarget[0].srcBlend = desc.renderTarget[0].srcBlendAlpha = REX::W32::D3D11_BLEND_ONE;
        desc.renderTarget[0].destBlend = desc.renderTarget[0].destBlendAlpha = REX::W32::D3D11_BLEND_ZERO;
        desc.renderTarget[0].blendOp = desc.renderTarget[0].blendOpAlpha = REX::W32::D3D11_BLEND_OP_ADD;
        desc.renderTarget[0].renderTargetWriteMask = REX::W32::D3D11_COLOR_WRITE_ENABLE_ALL;

        REX::W32::HRESULT const hr = device->CreateBlendState(&desc, result);
        if (!REX::W32::SUCCESS(hr) || !*result)
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
    bool create_alpha_blend_state(REX::W32::ID3D11Device* device, REX::W32::ID3D11BlendState** result)
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

        REX::W32::HRESULT const hr = device->CreateBlendState(&desc, result);
        if (!REX::W32::SUCCESS(hr) || !*result)
        {
            logger::error("Panel pass: CreateBlendState failed ({:X})", static_cast<unsigned int>(hr));
            return false;
        }
        return true;
    }

    bool create_depth_state(REX::W32::ID3D11Device* device, bool enable,
        REX::W32::D3D11_COMPARISON_FUNC function, REX::W32::ID3D11DepthStencilState** result)
    {
        REX::W32::D3D11_DEPTH_STENCIL_DESC desc{};
        desc.depthEnable = enable;
        desc.depthWriteMask = enable ? REX::W32::D3D11_DEPTH_WRITE_MASK_ALL : REX::W32::D3D11_DEPTH_WRITE_MASK_ZERO;
        desc.depthFunc = function;
        desc.stencilEnable = false;

        REX::W32::HRESULT const hr = device->CreateDepthStencilState(&desc, result);
        if (!REX::W32::SUCCESS(hr) || !*result)
        {
            logger::error("Panel pass: CreateDepthStencilState failed ({:X})", static_cast<unsigned int>(hr));
            return false;
        }
        return true;
    }

    bool create_rasterizer_state(REX::W32::ID3D11Device* device, REX::W32::ID3D11RasterizerState** result)
    {
        REX::W32::D3D11_RASTERIZER_DESC desc{};
        desc.cullMode = REX::W32::D3D11_CULL_NONE;
        desc.fillMode = REX::W32::D3D11_FILL_SOLID;
        desc.scissorEnable = false;
        desc.depthClipEnable = true;
        desc.multisampleEnable = true;

        REX::W32::HRESULT const hr = device->CreateRasterizerState(&desc, result);
        if (!REX::W32::SUCCESS(hr) || !*result)
        {
            logger::error("Panel pass: CreateRasterizerState failed ({:X})", static_cast<unsigned int>(hr));
            return false;
        }
        return true;
    }

    bool create_sampler_state(REX::W32::ID3D11Device* device, REX::W32::ID3D11SamplerState** result)
    {
        REX::W32::D3D11_SAMPLER_DESC desc{};
        desc.filter = REX::W32::D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.addressU = desc.addressV = desc.addressW = REX::W32::D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.maxAnisotropy = device->GetFeatureLevel() > REX::W32::D3D_FEATURE_LEVEL_9_1 ? REX::W32::D3D11_MAX_MAXANISOTROPY : 2u;
        desc.maxLOD = REX::W32::D3D11_FLOAT32_MAX;
        desc.comparisonFunc = REX::W32::D3D11_COMPARISON_NEVER;

        REX::W32::HRESULT const hr = device->CreateSamplerState(&desc, result);
        if (!REX::W32::SUCCESS(hr) || !*result)
        {
            logger::error("Panel pass: CreateSamplerState failed ({:X})", static_cast<unsigned int>(hr));
            return false;
        }
        return true;
    }

    bool create_constant_buffer(REX::W32::ID3D11Device* device, uint32_t byte_width, REX::W32::ID3D11Buffer** result)
    {
        REX::W32::D3D11_BUFFER_DESC desc{};
        desc.usage = REX::W32::D3D11_USAGE_DYNAMIC;
        desc.bindFlags = REX::W32::D3D11_BIND_CONSTANT_BUFFER;
        desc.cpuAccessFlags = REX::W32::D3D11_CPU_ACCESS_WRITE;
        desc.byteWidth = byte_width;

        REX::W32::HRESULT const hr = device->CreateBuffer(&desc, nullptr, result);
        if (!REX::W32::SUCCESS(hr) || !*result)
        {
            logger::error("Panel pass: CreateBuffer failed ({:X})", static_cast<unsigned int>(hr));
            return false;
        }
        return true;
    }

    // Column-vector 4x4 form of an engine transform: the rotation is scaled and the translation sits
    // in the fourth column, matching the row_major upload convention of the panel shaders.
    DirectX::XMFLOAT4X4 transform_to_matrix(RE::NiTransform const& transform)
    {
        DirectX::XMFLOAT4X4 matrix{};
        DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixIdentity());
        for (int row = 0; row < 3; ++row)
        {
            for (int col = 0; col < 3; ++col)
                matrix.m[row][col] = transform.rotate.entry[row][col] * transform.scale;
            matrix.m[row][3] = transform.translate[row];
        }
        return matrix;
    }

    // Palette in global bone index space: boneWorld * skinToBone with boneWorld first, so the
    // product maps model space straight to world space. Unused slots replicate the last valid entry
    // so no out-of-range read can pick up undefined content.
    bool build_palette(RE::NiSkinInstance& skin, DirectX::XMFLOAT4X4 (&palette)[Max_Palette_Bones])
    {
        if (!skin.skinData || !skin.boneWorldTransforms || skin.numMatrices == 0)
            return false;

        uint32_t const palette_count = std::min(skin.skinData->GetBoneCount(), skin.numMatrices);
        if (palette_count == 0 || palette_count > Max_Palette_Bones)
            return false;

        for (uint32_t index = 0; index < palette_count; ++index)
        {
            RE::NiTransform const* const bone_world = skin.boneWorldTransforms[index];
            if (!bone_world)
                return false;

            DirectX::XMFLOAT4X4 const bone = transform_to_matrix(*bone_world);
            DirectX::XMFLOAT4X4 const skin_to_bone = transform_to_matrix(skin.skinData->GetBoneDataSkinToBone(index));
            DirectX::XMStoreFloat4x4(&palette[index],
                DirectX::XMMatrixMultiply(DirectX::XMLoadFloat4x4(&bone), DirectX::XMLoadFloat4x4(&skin_to_bone)));
        }

        for (uint32_t index = palette_count; index < Max_Palette_Bones; ++index)
            palette[index] = palette[palette_count - 1];
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

bool PanelGeometryPass::init(REX::W32::ID3D11Device* device)
{
    if (!device)
        return false;

    ShaderManager& shaders = ShaderManager::instance();
    m_ref_static_vs = shaders.panel_static_vs();
    m_ref_skinned_vs = shaders.panel_skinned_vs();
    m_ref_pixel_shader = shaders.panel_ps();

    bool const ready =
        m_ref_static_vs && m_ref_skinned_vs && m_ref_pixel_shader &&
        create_constant_buffer(device, sizeof(PanelFrameCBData), &m_frame_cb) &&
        create_constant_buffer(device, sizeof(PanelDrawCBData), &m_draw_cb) &&
        create_depth_state(device, true, REX::W32::D3D11_COMPARISON_LESS, &m_depth) &&
        create_rasterizer_state(device, &m_rasterizer) &&
        create_blend_state(device, &m_blend) &&
        create_sampler_state(device, &m_sampler);
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

bool PanelGeometryPass::same_layout(LayoutKey const& lhs, LayoutKey const& rhs) noexcept
{
    return lhs.skinned == rhs.skinned && lhs.has_uv == rhs.has_uv &&
        lhs.position_format == rhs.position_format && lhs.position_offset == rhs.position_offset &&
        lhs.skinning_offset == rhs.skinning_offset && lhs.uv_offset == rhs.uv_offset &&
        lhs.stride == rhs.stride && lhs.weight_format == rhs.weight_format &&
        lhs.weight_offset == rhs.weight_offset && lhs.index_format == rhs.index_format &&
        lhs.index_offset == rhs.index_offset;
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

REX::W32::ID3D11InputLayout* PanelGeometryPass::acquire_layout(REX::W32::ID3D11Device* device, PanelDraw const& draw)
{
    bool const skinned = draw.skin != nullptr;
    bool const from_desc = draw.position_format == REX::W32::DXGI_FORMAT_UNKNOWN;
    REX::W32::DXGI_FORMAT const position_format = from_desc
        ? (draw.vertex_desc.HasFlag(RE::BSGraphics::Vertex::VF_FULLPREC) ? REX::W32::DXGI_FORMAT_R32G32B32_FLOAT : REX::W32::DXGI_FORMAT_R16G16B16A16_FLOAT)
        : draw.position_format;
    uint32_t const position_offset = from_desc
        ? draw.vertex_desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_POSITION)
        : draw.position_offset;

    LayoutKey const key{
        skinned,
        draw.has_uv,
        static_cast<uint32_t>(position_format),
        position_offset,
        draw.vertex_desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_SKINNING),
        draw.vertex_desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_TEXCOORD0),
        draw.vertex_stride,
        static_cast<uint32_t>(draw.skin_layout.weight_format),
        draw.skin_layout.weight_offset,
        static_cast<uint32_t>(draw.skin_layout.index_format),
        draw.skin_layout.index_offset,
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
            .format = draw.skin_layout.weight_format,
            .inputSlot = 0,
            .alignedByteOffset = draw.skin_layout.weight_offset,
            .inputSlotClass = REX::W32::D3D11_INPUT_PER_VERTEX_DATA,
            .instanceDataStepRate = 0
        };
        elements[count++] = {
            .semanticName = "BLENDINDICES",
            .semanticIndex = 0,
            .format = draw.skin_layout.index_format,
            .inputSlot = 0,
            .alignedByteOffset = draw.skin_layout.index_offset,
            .inputSlotClass = REX::W32::D3D11_INPUT_PER_VERTEX_DATA,
            .instanceDataStepRate = 0
        };
    }

    REX::W32::ID3D11InputLayout* layout = nullptr;
    REX::W32::HRESULT const hr = device->CreateInputLayout(elements, count, blob->GetBufferPointer(), blob->GetBufferSize(), &layout);
    if (!REX::W32::SUCCESS(hr) || !layout)
    {
        logger::error("Panel geometry pass: CreateInputLayout failed ({:X}), affected meshes skipped", static_cast<unsigned int>(hr));
        return nullptr;
    }

    m_layouts.push_back(LayoutEntry{ key, layout });
    return layout;
}

void PanelGeometryPass::draw(REX::W32::ID3D11Device* device, REX::W32::ID3D11DeviceContext* context,
    PanelTarget const& target, PanelCameraFrame const& camera, float alpha_test,
    float const clear_color[4], std::span<PanelDraw const> draws)
{
    if (!device || !context || !m_ref_static_vs || !m_ref_skinned_vs || !m_ref_pixel_shader ||
        !m_frame_cb || !m_draw_cb || !m_depth || !m_rasterizer || !m_blend || !m_sampler)
        return;
    if (!target.rtv() || !target.dsv())
        return;

    REX::W32::ID3D11RenderTargetView* const rtv = target.rtv();
    context->OMSetRenderTargets(1, &rtv, target.dsv());
    context->ClearRenderTargetView(rtv, clear_color);
    context->ClearDepthStencilView(target.dsv(), REX::W32::D3D11_CLEAR_DEPTH, 1.0f, 0);

    REX::W32::D3D11_VIEWPORT const viewport{
        .topLeftX = 0.0f,
        .topLeftY = 0.0f,
        .width = static_cast<float>(target.width()),
        .height = static_cast<float>(target.height()),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    context->RSSetViewports(1, &viewport);
    context->OMSetBlendState(m_blend, nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_depth, 0);
    context->RSSetState(m_rasterizer);
    context->PSSetSamplers(0, 1, &m_sampler);

    // The camera and cutoff are uploaded once per frame, into the constant buffer both stages read.
    PanelFrameCBData frame{};
    frame.view_proj = camera.view_proj;
    frame.camera_position = camera.eye;
    frame.alpha_test = alpha_test;
    REX::W32::D3D11_MAPPED_SUBRESOURCE mapped{};
    if (!REX::W32::SUCCESS(context->Map(m_frame_cb, 0, REX::W32::D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;
    std::memcpy(mapped.data, &frame, sizeof(frame));
    context->Unmap(m_frame_cb, 0);
    context->VSSetConstantBuffers(0, 1, &m_frame_cb);
    context->PSSetConstantBuffers(0, 1, &m_frame_cb);

    for (PanelDraw const& draw : draws)
    {
        if (!draw.vertex_buffer || !draw.index_buffer || draw.index_count == 0 || draw.vertex_stride == 0)
            continue;

        bool const skinned = draw.skin != nullptr;
        REX::W32::ID3D11InputLayout* const layout = acquire_layout(device, draw);
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
        if (!REX::W32::SUCCESS(context->Map(m_draw_cb, 0, REX::W32::D3D11_MAP_WRITE_DISCARD, 0, &draw_mapped)))
            return;
        std::memcpy(draw_mapped.data, &data, sizeof(data));
        context->Unmap(m_draw_cb, 0);
        context->VSSetConstantBuffers(1, 1, &m_draw_cb);

        context->IASetInputLayout(layout);
        context->IASetPrimitiveTopology(REX::W32::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        uint32_t const stride = draw.vertex_stride;
        uint32_t const offset = 0;
        REX::W32::ID3D11Buffer* const vertex_buffer = draw.vertex_buffer;
        context->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);
        // BSTriShape::vertexCount and a partition's vertices are both 16-bit, so the indices always are.
        context->IASetIndexBuffer(draw.index_buffer, REX::W32::DXGI_FORMAT_R16_UINT, 0);

        context->VSSetShader(skinned ? m_ref_skinned_vs : m_ref_static_vs, nullptr, 0);
        context->PSSetShader(m_ref_pixel_shader, nullptr, 0);

        // A draw without the UV flag binds no texture; the pixel shader reads the flat albedo instead.
        REX::W32::ID3D11ShaderResourceView* const diffuse = draw.has_uv ? draw.diffuse_view : nullptr;
        context->PSSetShaderResources(0, 1, &diffuse);

        context->DrawIndexed(draw.index_count, 0, 0);
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

bool PanelCompositePass::init(REX::W32::ID3D11Device* device)
{
    if (!device)
        return false;

    ShaderManager& shaders = ShaderManager::instance();
    m_ref_fullscreen_vs = shaders.panel_fullscreen_vs();
    m_ref_background_ps = shaders.panel_background_ps();
    m_ref_copy_ps = shaders.panel_composite_ps();

    bool const ready =
        m_ref_fullscreen_vs && m_ref_background_ps && m_ref_copy_ps &&
        create_constant_buffer(device, sizeof(PanelCompositeCBData), &m_background_cb) &&
        create_blend_state(device, &m_blend) &&
        create_alpha_blend_state(device, &m_blend_alpha) &&
        create_depth_state(device, false, REX::W32::D3D11_COMPARISON_ALWAYS, &m_depth_none) &&
        create_rasterizer_state(device, &m_rasterizer) &&
        create_sampler_state(device, &m_sampler);
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

void PanelCompositePass::draw(REX::W32::ID3D11DeviceContext* context, REX::W32::ID3D11RenderTargetView* target,
    REX::W32::ID3D11ShaderResourceView* panel, REX::W32::D3D11_VIEWPORT const& rectangle,
    float const background[4], float const border[4], uint32_t border_thickness,
    bool built_in_chrome) const
{
    if (!context || !target || !m_ref_fullscreen_vs || !m_ref_background_ps || !m_ref_copy_ps ||
        !m_background_cb || !m_depth_none || !m_blend || !m_blend_alpha || !m_rasterizer || !m_sampler)
        return;

    // The panel never tests against the world depth: the window must be a solid overlay.
    context->OMSetRenderTargets(1, &target, nullptr);
    // The chrome decides how the character reaches the target: opaque over the plugin's own fill, or
    // source-alpha blended over the movie the engine has already drawn underneath. Both states belong
    // to this pass and are bound here, inside the caller's single state-capture pair.
    context->OMSetBlendState(built_in_chrome ? m_blend : m_blend_alpha, nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_depth_none, 0);
    context->RSSetState(m_rasterizer);
    context->RSSetViewports(1, &rectangle);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(REX::W32::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(m_ref_fullscreen_vs, nullptr, 0);
    context->PSSetSamplers(0, 1, &m_sampler);

    float const border_uv_x = rectangle.width > 0.0f
        ? static_cast<float>(border_thickness) / rectangle.width
        : 0.0f;
    float const border_uv_y = rectangle.height > 0.0f
        ? static_cast<float>(border_thickness) / rectangle.height
        : 0.0f;

    PanelCompositeCBData data{};
    data.background = DirectX::XMFLOAT4{ background[0], background[1], background[2], background[3] };
    data.border = DirectX::XMFLOAT4{ border[0], border[1], border[2], border[3] };
    data.border_uv = DirectX::XMFLOAT4{ border_uv_x, border_uv_y, 0.0f, 0.0f };
    data.frame = DirectX::XMFLOAT4{ built_in_chrome ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f };
    REX::W32::D3D11_MAPPED_SUBRESOURCE mapped{};
    if (!REX::W32::SUCCESS(context->Map(m_background_cb, 0, REX::W32::D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;
    std::memcpy(mapped.data, &data, sizeof(data));
    context->Unmap(m_background_cb, 0);
    context->PSSetConstantBuffers(0, 1, &m_background_cb);

    // 1) The built-in chrome's opaque fill, so no world content shows through the window. A skin owns
    // that fill, and painting it here would cover the movie the engine drew underneath.
    if (built_in_chrome)
    {
        REX::W32::ID3D11ShaderResourceView* const no_texture = nullptr;
        context->PSSetShaderResources(0, 1, &no_texture);
        context->PSSetShader(m_ref_background_ps, nullptr, 0);
        context->Draw(3, 0);
    }

    // 2) The offscreen character over the same rectangle. With the built-in chrome it is opaque and
    // framed by the built-in hairline. With a skin the shader clips the border band away and keeps the
    // character's alpha, so the blend writes the character over the movie the engine drew and leaves
    // every fragment the character does not cover exactly as the engine left it.
    context->PSSetShader(m_ref_copy_ps, nullptr, 0);
    context->PSSetShaderResources(0, 1, &panel);
    context->Draw(3, 0);
}

PLUGIN_NAMESPACE_END
