//
// Created by AmazingBuff on 2026/09/21.
//
// Ported from Highlight-Lootable-Corpses (https://github.com/AmazingBuff/Highlight-Lootable-Corpses.git),
// src/render/geometry/render_geometry.h and render_geometry.cpp, commit 7a7c51e, reduced to a single
// reference target and extended with the per-draw material. Both repositories are GPL-3.0 with the
// same author.
//

#pragma once

#include <REX/W32/D3D11.h>
#include <cstdint>
#include <vector>

PLUGIN_NAMESPACE_BEGIN

// ---------------------------------------------------------------------------
// Skinned palette constant-buffer budget (matrices per draw); 128 x 64B = 8KB.
// ---------------------------------------------------------------------------
inline constexpr size_t Max_Palette_Bones = 128;

// The UV attribute is four bytes wide, but the pinned headers prove only that width, not its layout
// (two half-floats versus two normalized shorts). R16G16_FLOAT is the default chosen by the
// contract and is a recorded runtime checklist item in docs/features/preview-panel.md.
inline constexpr REX::W32::DXGI_FORMAT Panel_Uv_Format = REX::W32::DXGI_FORMAT_R16G16_FLOAT;

// Weight/index layout inside the skinned vertex buffer: a calibration result, never a guess.
struct PanelSkinLayout
{
    REX::W32::DXGI_FORMAT weight_format;
    uint32_t weight_offset;
    REX::W32::DXGI_FORMAT index_format;
    uint32_t index_offset;
};

// One draw (a temporary render-thread list; vertex and index buffers are borrowed from engine
// objects). Lifetime: node_ref keeps the static-path geometry (and its GPU buffers) alive; the
// skinned path keeps its buffData/VB/IB alive through the skin (NiSkinInstance -> skinPartition ->
// buffData) chain. No raw engine pointer is cached across frames.
struct PanelDraw
{
    REX::W32::ID3D11Buffer* vertex_buffer;
    REX::W32::ID3D11Buffer* index_buffer;
    uint32_t vertex_stride;
    uint32_t vertex_count;
    uint32_t triangle_count;
    uint32_t index_count;

    RE::NiPointer<RE::NiSkinInstance> skin;  // keeps the skin instance alive (bone world matrices)
    RE::NiPointer<RE::BSGeometry> node;      // keeps the static-path geometry (and its GPU buffers) alive

    // Position attribute layout: the static path stores the calibration result (UNKNOWN means
    // "derive from the descriptor"); the skinned path stores the offset-spacing result.
    RE::BSGraphics::VertexDesc vertex_desc;
    REX::W32::DXGI_FORMAT position_format;
    uint32_t position_offset;
    uint32_t partition;
    // Skinning weight/index layout: only skinned draws store a calibration result; static draws keep
    // the default values.
    PanelSkinLayout skin_layout;

    // Material needed for shading: the node's shader property (a borrow, kept alive by node), the
    // resolved diffuse shader-resource view (null is the explicit "no diffuse texture" marker) and
    // the engine's material alpha.
    RE::BSShaderProperty* shader_property;
    REX::W32::ID3D11ShaderResourceView* diffuse_view;
    float material_alpha;
    bool has_uv;
};

// Collect this frame's draws from the single reference's 3D along two paths, static (the BSTriShape
// family) and skinned (NiSkinPartition partitions), including position-format and skin-layout
// self-calibration and per-mesh validation - meshes with no solution are skipped rather than drawn
// with a guessed layout. a_draws is cleared first and its capacity is reused, so a steady-state
// frame allocates nothing.
void collect_panel_geometry(RE::TESObjectREFR& a_ref, std::vector<PanelDraw>& a_draws);

PLUGIN_NAMESPACE_END
