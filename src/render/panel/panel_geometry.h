//
// Created by AmazingBuff on 2026/09/21.
//
// Ported from Highlight-Lootable-Corpses (https://github.com/AmazingBuff/Highlight-Lootable-Corpses.git),
// src/render/geometry/render_geometry.h and render_geometry.cpp, commit 7a7c51e, reduced to a single
// reference target and extended with the per-draw material. Both repositories are GPL-3.0 with the
// same author.
//

#pragma once

#include "config/config.h"

#include <REX/W32/D3D11.h>
#include <cstdint>
#include <memory>
#include <vector>

PLUGIN_NAMESPACE_BEGIN

// ---------------------------------------------------------------------------
// Skinned palette constant-buffer budget (matrices per draw); 128 x 64B = 8KB.
// ---------------------------------------------------------------------------
inline constexpr size_t Max_Palette_Bones = 128;

// The UV format is a per-attribute property of the descriptor, not a mesh-wide flag: the precision
// bit of TEXCOORD0 is bit 54 + VA_TEXCOORD0, the one CommonLibSSE names VF_FULLPREC (0x400 -> flags
// bit 10 -> desc bit 54) is the POSITION's. The rule and its measurements live in uv_format_of
// (panel_geometry.cpp); the format travels in PanelDraw::uv_format.

// Weight/index layout inside the skinned vertex buffer: a calibration result, never a guess.
struct PanelSkinLayout
{
    REX::W32::DXGI_FORMAT weight_format;
    uint32_t weight_offset;
    REX::W32::DXGI_FORMAT index_format;
    uint32_t index_offset;
};

// Bytes per rebuilt position vertex (4 x float32) in PanelDraw::position_stream.
inline constexpr uint32_t Dynamic_Position_Stride = 16u;

// One draw (a temporary render-thread list). The D3D objects are resolved and AddRef'd during
// the game-thread collection - the one moment the engine guarantees its renderer data is alive
// (it is rendering the same mesh this frame) - and Released after the present-time draw. They
// CANNOT be resolved at draw time: the engine deletes the whole BSGraphics::TriShape mid-frame
// (mesh unload/reload), and the NiPointers here keep the NiAVObject shell, not the renderer data.
struct PanelDraw
{
    REX::W32::ID3D11Buffer* vertex_buffer = nullptr;
    REX::W32::ID3D11Buffer* index_buffer = nullptr;
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
    // UV attribute format, decided from the mesh's own descriptor: half-float (two uint16) unless
    // the TEXCOORD0 precision bit says float32 AND the descriptor's own attribute spacing leaves room
    // for a float32 pair. See uv_format_of.
    REX::W32::DXGI_FORMAT uv_format;
    // Skinning weight/index layout: only skinned draws store a calibration result; static draws keep
    // the default values.
    PanelSkinLayout skin_layout;
    // The partition's bone list, diagnostics only. A vertex's bone indices are GLOBAL subscripts
    // into the skin bone array (the engine's own vertex shader addresses Bones[g], a flat 80-bone
    // array, with no per-partition indirection), so neither this list nor numBones may bound them or
    // reorder the palette - doing either rejects real meshes and binds the survivors to the wrong
    // bones. Kept as a borrow; the skin instance (held alive by this draw) owns the partition and
    // its arrays.
    std::uint16_t const* partition_bones = nullptr;
    std::uint32_t partition_bone_count = 0;

    // Material needed for shading: the node's shader property (a borrow, kept alive by node), the
    // diffuse SRV resolved and AddRef'd at collection (null = "no texture", flat albedo) and the
    // engine's material alpha.
    RE::BSShaderProperty* shader_property = nullptr;
    REX::W32::ID3D11ShaderResourceView* diffuse_view = nullptr;
    float material_alpha;
    bool has_uv;
    // Hair-dye reproduction: set when the mesh's material is the engine's hair-tint material, with
    // the tint colour resolved from that material (already the player's chosen hair colour). The
    // pixel shader multiplies the sampled hair texture's grayscale against it.
    bool hair_tint;
    RE::NiColor hair_tint_color;
    // Skin-tone reproduction for the FaceGen material family (kFaceGen / kFaceGenRGBTint): the
    // diffuse is a grayscale detail map the engine tints with the NPC's skin tone at render time.
    // Enabled per draw with the resolved tone; the pixel shader maps the sampled luminance onto it.
    bool skin_tint;
    RE::NiColor skin_tint_color;
    // The material's own UV remap (BSShaderMaterial::texCoordOffset[0] / texCoordScale[0]): the
    // engine's vertex shader maps raw UVs through uv * scale + offset before sampling, and atlassed
    // materials (the CBBE body slots) carry non-trivial values, so sampling raw UVs reads the wrong
    // atlas regions (the residual skin camouflage). Identity for plain materials.
    float uv_offset_u = 0.0f;
    float uv_offset_v = 0.0f;
    float uv_scale_u = 1.0f;
    float uv_scale_v = 1.0f;

    // The engine's own per-mesh alpha handling, resolved at collection time. SSE's diffuse alpha
    // channel usually stores a specular mask whose near-zero values are not transparency, so the
    // alpha cutout may only run on a mesh whose NiAlphaProperty enables testing (at the configured
    // threshold), and the written alpha may only follow the texture when that property actually
    // blends - an opaque mesh must write an alpha of one, or the composite would ghost it over the
    // panel's backdrop.
    float alpha_cutoff;
    bool opaque_alpha;

    // Position stream for positionless partitions (FaceGen/morph dynamic meshes: head, body). Their
    // partition buffers carry no positions; model-space positions are rebuilt at collection time from
    // BSDynamicTriShape::dynamicData through the partition's vertexMap into float4s (x, y, z, 1). The
    // render thread uploads the stream into a scratch vertex buffer and binds it as stream 1, with the
    // POSITION semantic fetched there while every other attribute stays in the partition buffer on
    // stream 0. Null for draws whose positions live in the partition buffer itself.
    std::shared_ptr<std::vector<float>> position_stream;
};

// Collect this frame's draws from the single reference's 3D along two paths, static (the BSTriShape
// family) and skinned (NiSkinPartition partitions), including position-format and skin-layout
// self-calibration, the per-mesh alpha handling and per-mesh validation - meshes with no solution are
// skipped rather than drawn with a guessed layout. draws is cleared first and its capacity is reused,
// so a steady-state frame allocates nothing.
void collect_panel_geometry(RE::TESObjectREFR& ref, Config const& config, std::vector<PanelDraw>& draws);

PLUGIN_NAMESPACE_END
