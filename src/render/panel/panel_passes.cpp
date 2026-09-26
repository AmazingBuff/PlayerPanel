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
        float pad;
    };

    // The per-draw transform, material, alpha handling and skinning palette share one constant buffer
    // so the panel touches only constant-buffer slots 0 and 1, which is exactly the set the engine
    // state capture restores. tint_color packs the engine-resolved colour with the tint mode in its
    // .w (0 = none, 1 = hair dye over grayscale, 2 = FaceGen skin tone over grayscale), so the
    // shader needs no extra flag slots.
    struct PanelDrawCBData
    {
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4 albedo;
        uint32_t has_texture;
        float alpha_cutoff;
        float write_alpha_1;
        float tint_mode;
        DirectX::XMFLOAT4 tint_color;
        // The material's own UV remap (offset.xy, scale.zw), applied by the vertex stage before
        // sampling - atlassed CBBE slots carry non-trivial values, plain materials are identity.
        DirectX::XMFLOAT4 uv_remap;
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
        // Mirrors the engine's own character rasterizer state (measured in RenderDoc, the O draw of
        // the same mesh): back-face culling with a CCW front, multisampling off, and the engine's
        // depth-bias clamp. The panel draws the same geometry with the same vertex data, so the
        // same rasterizer settings keep the two paths comparable.
        REX::W32::D3D11_RASTERIZER_DESC desc{};
        desc.cullMode = REX::W32::D3D11_CULL_BACK;
        desc.frontCounterClockwise = true;
        desc.fillMode = REX::W32::D3D11_FILL_SOLID;
        desc.scissorEnable = false;
        desc.depthClipEnable = true;
        desc.multisampleEnable = false;
        desc.depthBiasClamp = -100.0f;

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
        // Mirrors the engine's diffuse sampler (RenderDoc, the O draw of the same mesh): wrap
        // addressing with 8x anisotropic filtering. Community Shaders also caps engine anisotropy
        // at exactly 8 (Hooks.cpp ID3D11Device_CreateSamplerState).
        REX::W32::D3D11_SAMPLER_DESC desc{};
        desc.filter = REX::W32::D3D11_FILTER_ANISOTROPIC;
        desc.addressU = desc.addressV = desc.addressW = REX::W32::D3D11_TEXTURE_ADDRESS_WRAP;
        desc.maxAnisotropy = 8u;
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

    // Palette in global bone index space: slot i is boneWorldTransforms[i] * skinToBone(i), so the
    // product maps model space straight to world space and a vertex's bone index - a global subscript
    // into the skin bone array - addresses the palette directly.
    //
    // That subscript is global, not partition-local. The engine's own skinning says so: its vertex
    // shader (reconstructed in Community Shaders' Common/Skinned.hlsli) reads BLENDINDICES and
    // addresses Bones[g] - a flat 80-bone matrix array - with no per-partition indirection at all.
    // So does measurement: a mesh whose partition carries numBones=13 nonetheless carries indices up
    // to 30 against a 31-bone skin (clothes), and one with numBones=1 carries index 1 against a
    // 2-bone skin (head, hair). Partition bones arrays are legacy/partial in SSE, so
    // partition_bones/numBones take part in no decision here - they are diagnostics only. Assembling
    // the palette through them permutes the bones and scrambles the body; bounding the vertex
    // indices by numBones rejects real meshes every frame (5 partitions per frame, measured).
    //
    // Unused slots replicate the last valid entry so no out-of-range read can pick up undefined
    // content.
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

    // Why a collected draw never reached the GPU, bounded to the first frames of a session. A draw
    // dropped here leaves no other trace: the collection has it, and the panel simply shows less.
    void log_draw_skip(PanelDraw const& draw, char const* reason)
    {
        static uint32_t s_budget = 32;
        if (s_budget == 0)
            return;
        --s_budget;

        char const* name = draw.node ? draw.node->name.c_str() : nullptr;
        logger::warn("Panel draw skipped [{}]: node=\"{}\" p={} skinned={} verts={} indices={} stride={} uv={} palette_bones={}",
            reason, name && name[0] != '\0' ? name : "?", draw.partition,
            draw.skin ? "yes" : "no", draw.vertex_count, draw.index_count, draw.vertex_stride,
            draw.has_uv ? "yes" : "no",
            draw.skin ? draw.skin->numMatrices : 0u);
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
    m_position_scratch(nullptr),
    m_position_scratch_vertices(0),
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
    if (m_position_scratch)
    {
        m_position_scratch->Release();
        m_position_scratch = nullptr;
    }
    m_position_scratch_vertices = 0;
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
        lhs.skinning_offset == rhs.skinning_offset && lhs.uv_format == rhs.uv_format &&
        lhs.uv_offset == rhs.uv_offset && lhs.stride == rhs.stride &&
        lhs.weight_format == rhs.weight_format && lhs.weight_offset == rhs.weight_offset &&
        lhs.index_format == rhs.index_format && lhs.index_offset == rhs.index_offset;
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
{    bool const skinned = draw.skin != nullptr;
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
        draw.position_stream != nullptr,
        static_cast<uint32_t>(position_format),
        position_offset,
        draw.vertex_desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_SKINNING),
        draw.has_uv ? static_cast<uint32_t>(draw.uv_format) : 0u,
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
        .inputSlot = key.position_stream ? 1u : 0u,
        .alignedByteOffset = key.position_stream ? 0u : position_offset,
        .inputSlotClass = REX::W32::D3D11_INPUT_PER_VERTEX_DATA,
        .instanceDataStepRate = 0
    };
    if (key.has_uv)
    {
        elements[count++] = {
            .semanticName = "TEXCOORD",
            .semanticIndex = 0,
            .format = draw.uv_format,
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

// The scratch vertex stream for positionless (dynamic) draws is created on demand and grown to the
// largest streamed draw of this frame: it exists only when such a draw is actually queued, so the
// common skinned/static path never pays for it. A DISCARD map replaces the contents every upload,
// so the buffer never needs to be filled ahead of binding.
bool PanelGeometryPass::ensure_position_scratch(REX::W32::ID3D11Device* device, uint32_t vertices)
{
    if (m_position_scratch && m_position_scratch_vertices >= vertices)
        return true;

    if (m_position_scratch)
    {
        m_position_scratch->Release();
        m_position_scratch = nullptr;
        m_position_scratch_vertices = 0;
    }

    // Headroom of a quarter over the request absorbs per-frame growth of the morphed body stream
    // without re-creating the buffer every session; the whole request is honoured even when the
    // headroom is rejected by the driver (a zero-sized request is guarded by the callers).
    REX::W32::D3D11_BUFFER_DESC desc{};
    desc.byteWidth = vertices * Dynamic_Position_Stride + vertices / 4u * Dynamic_Position_Stride;
    desc.usage = REX::W32::D3D11_USAGE_DYNAMIC;
    desc.bindFlags = REX::W32::D3D11_BIND_VERTEX_BUFFER;
    desc.cpuAccessFlags = REX::W32::D3D11_CPU_ACCESS_WRITE;

    REX::W32::HRESULT const hr = device->CreateBuffer(&desc, nullptr, &m_position_scratch);
    if (!REX::W32::SUCCESS(hr) || !m_position_scratch)
    {
        logger::error("Panel geometry pass: failed to create the position scratch buffer ({:X})", static_cast<unsigned int>(hr));
        return false;
    }

    m_position_scratch_vertices = vertices;
    return true;
}

    void PanelGeometryPass::draw(REX::W32::ID3D11Device* device, REX::W32::ID3D11DeviceContext* context,
        REX::W32::ID3D11RenderTargetView* target, REX::W32::ID3D11DepthStencilView* depth,
        REX::W32::D3D11_VIEWPORT const& rectangle, PanelCameraFrame const& camera,
        std::span<PanelDraw const> draws)
{
    if (!device || !context || !m_ref_static_vs || !m_ref_skinned_vs || !m_ref_pixel_shader ||
        !m_frame_cb || !m_draw_cb || !m_depth || !m_rasterizer ||
        !m_blend || !m_sampler)
        return;
    if (!target || !depth)
        return;

    // The character draws straight onto the window over the caller's fill; only the depth is cleared,
    // so the body's own partitions occlude each other correctly.
    context->OMSetRenderTargets(1, &target, depth);
    context->ClearDepthStencilView(depth, REX::W32::D3D11_CLEAR_DEPTH, 1.0f, 0);

    // The panel's place on the screen: the geometry pass draws through a viewport the size of the
    // panel rectangle at its own offset, so the character lands inside the window. The depth view is
    // screen-sized (it must match the back buffer's resource size or D3D11 drops the binding), but a
    // full-screen viewport would let geometry spill outside the panel over the game world - the
    // viewport is the panel rectangle, which is what confines the character to the window.
    context->RSSetViewports(1, &rectangle);
    context->OMSetBlendState(m_blend, nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_depth, 0);
    context->RSSetState(m_rasterizer);
    context->PSSetSamplers(0, 1, &m_sampler);

    // The camera is uploaded once per frame, into the constant buffer both stages read.
    PanelFrameCBData frame{};
    frame.view_proj = camera.view_proj;
    frame.camera_position = camera.eye;
    REX::W32::D3D11_MAPPED_SUBRESOURCE mapped{};
    if (!REX::W32::SUCCESS(context->Map(m_frame_cb, 0, REX::W32::D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;
    std::memcpy(mapped.data, &frame, sizeof(frame));
    context->Unmap(m_frame_cb, 0);
    context->VSSetConstantBuffers(0, 1, &m_frame_cb);
    context->PSSetConstantBuffers(0, 1, &m_frame_cb);

    uint32_t drawn = 0;
    for (PanelDraw const& draw : draws)
    {
        // The D3D objects were resolved and AddRef'd during the game-thread collection - the one
        // moment the engine guarantees its renderer data is alive - and these references pin them
        // through the present-time draw. Released at the end of this iteration.
        REX::W32::ID3D11Buffer* const vertex_buffer = draw.vertex_buffer;
        REX::W32::ID3D11Buffer* const index_buffer = draw.index_buffer;
        if (!vertex_buffer || !index_buffer || draw.index_count == 0 || draw.vertex_stride == 0)
        {
            if (vertex_buffer)
                vertex_buffer->Release();
            if (index_buffer)
                index_buffer->Release();
            log_draw_skip(draw, "geometry incomplete");
            continue;
        }

        bool const skinned = draw.skin != nullptr;
        REX::W32::ID3D11InputLayout* const layout = acquire_layout(device, draw);
        if (!layout)
        {
            log_draw_skip(draw, "no input layout");
            continue;
        }

        PanelDrawCBData data{};
        if (!skinned && draw.node)
            data.world = transform_to_matrix(draw.node->world);
        else
            DirectX::XMStoreFloat4x4(&data.world, DirectX::XMMatrixIdentity());

        data.albedo = DirectX::XMFLOAT4{ Flat_Albedo, Flat_Albedo, Flat_Albedo, draw.material_alpha };
        data.has_texture = draw.has_uv ? 1u : 0u;
        data.alpha_cutoff = draw.alpha_cutoff;
        data.write_alpha_1 = draw.opaque_alpha ? 1.0f : 0.0f;
        // Tint mode: 1 = hair dye (the material's own tint colour over the grayscale texture),
        // 2 = FaceGen skin tone (the NPC's skin tone over the grayscale detail texture). The skin
        // tone wins when a material somehow claims both, because the hair dye path never appears on
        // a FaceGen-family material in practice.
        float const tint_mode = draw.skin_tint ? 2.0f : (draw.hair_tint ? 1.0f : 0.0f);
        RE::NiColor const& tint = draw.skin_tint ? draw.skin_tint_color : draw.hair_tint_color;
        data.tint_mode = tint_mode;
        data.tint_color = DirectX::XMFLOAT4{ tint.red, tint.green, tint.blue, tint_mode };
        data.uv_remap = DirectX::XMFLOAT4{
            draw.uv_offset_u, draw.uv_offset_v, draw.uv_scale_u, draw.uv_scale_v };

        if (skinned && !build_palette(*draw.skin, data.bones))
        {
            log_draw_skip(draw, "palette unavailable");
            if (index_buffer)
                index_buffer->Release();
            if (vertex_buffer)
                vertex_buffer->Release();
            continue;
        }

        REX::W32::D3D11_MAPPED_SUBRESOURCE draw_mapped{};
        if (!REX::W32::SUCCESS(context->Map(m_draw_cb, 0, REX::W32::D3D11_MAP_WRITE_DISCARD, 0, &draw_mapped)))
            return;
        std::memcpy(draw_mapped.data, &data, sizeof(data));
        context->Unmap(m_draw_cb, 0);
        context->VSSetConstantBuffers(1, 1, &m_draw_cb);

        context->IASetInputLayout(layout);
        context->IASetPrimitiveTopology(REX::W32::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Streamed (positionless dynamic) draws bind the rebuilt positions as stream 1: one
        // DISCARD-mapped upload of the collection-baked float4 stream, with the POSITION semantic
        // fetched from this slot while every other attribute stays in the partition buffer on
        // stream 0. The scratch buffer is created (or regrown) for the largest streamed draw here -
        // the collection's shape decides at draw time, not at init time.
        if (draw.position_stream)
        {
            // The baked stream is float4s, so its vertex count is the byte size over the float4 stride.
            std::size_t const stream_vertices = draw.position_stream->size() * sizeof(float) / Dynamic_Position_Stride;
            if (stream_vertices == 0 || !ensure_position_scratch(device, static_cast<uint32_t>(stream_vertices)))
            {
                if (index_buffer)
                    index_buffer->Release();
                if (vertex_buffer)
                    vertex_buffer->Release();
                continue;
            }
            std::size_t const stream_bytes = draw.position_stream->size() * sizeof(float);
            REX::W32::D3D11_MAPPED_SUBRESOURCE stream_mapped{};
            if (!REX::W32::SUCCESS(context->Map(m_position_scratch, 0, REX::W32::D3D11_MAP_WRITE_DISCARD, 0, &stream_mapped)) || !stream_mapped.data)
            {
                if (index_buffer)
                    index_buffer->Release();
                if (vertex_buffer)
                    vertex_buffer->Release();
                continue;
            }
            std::memcpy(stream_mapped.data, draw.position_stream->data(), stream_bytes);
            context->Unmap(m_position_scratch, 0);

            REX::W32::ID3D11Buffer* const stream_buffers[2] = { vertex_buffer, m_position_scratch };
            std::uint32_t const stream_strides[2] = { draw.vertex_stride, Dynamic_Position_Stride };
            std::uint32_t const stream_offsets[2] = { 0, 0 };
            context->IASetVertexBuffers(0, 2, stream_buffers, stream_strides, stream_offsets);
        }
        else
        {
            uint32_t const stride = draw.vertex_stride;
            uint32_t const offset = 0;
            REX::W32::ID3D11Buffer* const bind_buffers[1] = { vertex_buffer };
            context->IASetVertexBuffers(0, 1, bind_buffers, &stride, &offset);
        }
        // BSTriShape::vertexCount and a partition's vertices are both 16-bit, so the indices always are.
        context->IASetIndexBuffer(index_buffer, REX::W32::DXGI_FORMAT_R16_UINT, 0);

        context->VSSetShader(skinned ? m_ref_skinned_vs : m_ref_static_vs, nullptr, 0);
        context->PSSetShader(m_ref_pixel_shader, nullptr, 0);

        // A draw without a live diffuse texture binds none; the pixel shader reads the flat albedo
        // instead. The SRV was resolved and AddRef'd at collection like the buffers.
        REX::W32::ID3D11ShaderResourceView* const diffuse = draw.has_uv ? draw.diffuse_view : nullptr;
        context->PSSetShaderResources(0, 1, &diffuse);

        context->DrawIndexed(draw.index_count, 0, 0);

        if (diffuse)
            diffuse->Release();
        if (index_buffer)
            index_buffer->Release();
        if (vertex_buffer)
            vertex_buffer->Release();
        ++drawn;
    }

    // A few frames of the drawn/collected tally: a draw dropped in this pass (not in the collection)
    // is invisible everywhere else, and the tally is the only check that the panel draws what it
    // collected.
    static uint32_t s_tally_frames = 8;
    if (s_tally_frames > 0)
    {
        --s_tally_frames;
        logger::info("Panel frame: drew {}/{} collected draws", drawn, draws.size());
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
    REX::W32::D3D11_VIEWPORT const& rectangle, PanelCompositePhase phase,
    float const background[4], float const border[4], uint32_t border_thickness,
    bool built_in_chrome) const
{
    if (!context || !target || !m_ref_fullscreen_vs || !m_ref_background_ps || !m_ref_copy_ps ||
        !m_background_cb || !m_depth_none || !m_blend || !m_blend_alpha || !m_rasterizer || !m_sampler)
        return;
    (void)m_blend_alpha;

    // The panel never tests against the world depth: the window must be a solid overlay.
    context->OMSetRenderTargets(1, &target, nullptr);
    // The chrome decides how the character reaches the target: opaque over the plugin's own fill, or
    // source-alpha blended over the movie the engine has already drawn underneath. Both states belong
    // to this pass and are bound here, inside the caller's single state-capture pair.
    context->OMSetBlendState(m_blend, nullptr, 0xFFFFFFFF);
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

    // The fill half: the built-in chrome's opaque fill, so no world content shows through the
    // window. The character draws over this fill in the geometry pass, straight onto the same target.
    REX::W32::ID3D11ShaderResourceView* const no_texture = nullptr;
    context->PSSetShaderResources(0, 1, &no_texture);
    if (phase == PanelCompositePhase::kFill)
    {
        context->PSSetShader(m_ref_background_ps, nullptr, 0);
        context->Draw(3, 0);
        return;
    }

    // The band half: the hairline into the rectangle's outer band; the interior discards, leaving
    // the character pixels the geometry pass wrote. (A skin's movie would be sampled here once the
    // skin path returns.)
    context->PSSetShader(m_ref_copy_ps, nullptr, 0);
    context->Draw(3, 0);
}

PLUGIN_NAMESPACE_END
