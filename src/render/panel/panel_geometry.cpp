//
// Created by AmazingBuff on 2026/09/21.
//
// Ported from Highlight-Lootable-Corpses (https://github.com/AmazingBuff/Highlight-Lootable-Corpses.git),
// src/render/geometry/render_geometry.cpp, commit 7a7c51e, reduced to a single reference target and
// extended with the per-draw material. Both repositories are GPL-3.0 with the same author.
//

#include "panel_geometry.h"

#include <DirectXPackedVector.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <string>

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // ---- Static draw footprint checks: world bounding sphere / reference ownership ----
    constexpr float Max_Part_World_Radius = 1024.0f;  // upper bound on the world bounding-sphere radius (game units)
    constexpr float Ref_Proximity_Slack = 256.0f;     // minimum neighbourhood of the draw footprint around the reference position (game units)

    constexpr size_t Max_Draws_Per_Frame = 256;

    constexpr size_t Max_Position_Calibrations = 256;   // calibration cache cap (past it, no caching on the degraded path)
    constexpr uint32_t Calibration_Sample_Limit = 256;  // sampling cap for large meshes

    constexpr size_t Max_Skinned_Layout_Calibrations = 256;  // skinned calibration cache cap (past it, no caching on the degraded path)
    constexpr size_t Max_Skinned_Layout_Candidates = 4;      // at most 2 SKINNING layouts per stride x 2 strides

    // Positionless partitions (FaceGen/morph dynamic meshes - the head and the morphed body): the
    // partition's SKINNING block is contiguous by construction - weights 4xf16 at the skin offset,
    // indices 4xu8 right after - and model-space positions come from BSDynamicTriShape::dynamicData,
    // one float4 per ORIGINAL vertex, rebuilt per partition through its vertexMap. The stride of the
    // rebuilt stream is Dynamic_Position_Stride (panel_geometry.h).
    constexpr uint8_t Dynamic_Skin_Layout_Id = 1;
    constexpr uint32_t Dynamic_Skin_Block_Bytes = 12u;   // weights 4xf16 + indices 4xu8
    constexpr uint32_t Dynamic_Position_Components = 4u; // float components per rebuilt position

    // Weight validation thresholds (SSE vertex weights are expected to sum to 1, so the thresholds
    // are deliberately loose)
    constexpr float Skinned_Weight_Min = -0.001f;
    constexpr float Skinned_Weight_Max = 1.001f;
    constexpr float Skinned_Weight_Sum_Min = 0.98f;
    constexpr float Skinned_Weight_Sum_Max = 1.02f;

    // ---------------------------------------------------------------------------
    // Vertex layout: CLibNG's VertexDesc::GetSize() miscounts the half-precision position (16) and
    // UV (4), so the stride is derived as the max of offset+size over all attributes instead -
    // offsets come from the engine-written descriptor and sizes are the fixed SSE formats
    // (position/UV are FULLPREC-dependent, NORMAL/TANGENT/COLOR/EYEDATA/LANDDATA are 4 bytes,
    // SKINNING is weights 4xf16 + indices 4xu8 with weights first). The SKINNING byte count is
    // parameterisable: the skinned stride is unknown and both 8 and 12 are probed; the static path
    // always passes 12.
    // ---------------------------------------------------------------------------
    uint32_t vertex_size_of_with_skinning(RE::BSGraphics::VertexDesc const& desc, uint32_t skinning_bytes)
    {
        bool const full = desc.HasFlag(RE::BSGraphics::Vertex::VF_FULLPREC);
        uint32_t size = 0;

        if (desc.HasFlag(RE::BSGraphics::Vertex::VF_VERTEX))
            size = std::max(size, desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_POSITION) + (full ? 12u : 8u));
        if (desc.HasFlag(RE::BSGraphics::Vertex::VF_UV))
            size = std::max(size, desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_TEXCOORD0) + (full ? 8u : 4u));
        if (desc.HasFlag(RE::BSGraphics::Vertex::VF_UV_2))
            size = std::max(size, desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_TEXCOORD1) + (full ? 8u : 4u));
        if (desc.HasFlag(RE::BSGraphics::Vertex::VF_NORMAL))
            size = std::max(size, desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_NORMAL) + 4u);
        if (desc.HasFlag(RE::BSGraphics::Vertex::VF_TANGENT))
            size = std::max(size, desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_BINORMAL) + 4u);
        if (desc.HasFlag(RE::BSGraphics::Vertex::VF_COLORS))
            size = std::max(size, desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_COLOR) + 4u);
        if (desc.HasFlag(RE::BSGraphics::Vertex::VF_SKINNED))
            size = std::max(size, desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_SKINNING) + skinning_bytes);
        if (desc.HasFlag(RE::BSGraphics::Vertex::VF_LANDDATA))
            size = std::max(size, desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_LANDDATA) + 4u);
        if (desc.HasFlag(RE::BSGraphics::Vertex::VF_EYEDATA))
            size = std::max(size, desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_EYEDATA) + 4u);

        return size;
    }

    uint32_t vertex_size_of(RE::BSGraphics::VertexDesc const& desc)
    {
        return vertex_size_of_with_skinning(desc, 12u);
    }

    REX::W32::DXGI_FORMAT position_format_of(RE::BSGraphics::VertexDesc const& desc)
    {
        return desc.HasFlag(RE::BSGraphics::Vertex::VF_FULLPREC) ? REX::W32::DXGI_FORMAT_R32G32B32_FLOAT : REX::W32::DXGI_FORMAT_R16G16B16A16_FLOAT;
    }

    // ---------------------------------------------------------------------------
    // UV format: the descriptor carries a precision bit PER ATTRIBUTE, at bit (54 + attribute) - the
    // engine-side reconstruction of the descriptor (Community Shaders ShaderCache's AddAttribute) sets
    // exactly those bits, one per attribute present. So the UV's own bit is 54 + VA_TEXCOORD0 (bit
    // 55), while the bit CommonLibSSE exposes as VF_FULLPREC (0x400 -> flags bit 10 -> desc bit 54) is
    // the POSITION's bit, not the mesh's.
    //
    // Reading the position's bit as a mesh-wide precision flag is what put R32G32_FLOAT over
    // half-float UVs on every FaceGen mesh - head, body/CBBE, hair - whose position bit is set even
    // though their UV slot is 4 bytes wide. The shader then read two vertices' worth of data as one
    // coordinate pair: the reported digital camouflage, on the skin, head and hair only, while the
    // clothes, weapons and props (position bit clear) sampled their textures correctly.
    //
    // Measured on the head partition (desc 0x0044200010000044, stride 16, dumped beside the log): the
    // UV slot is 4 bytes at offset 0 and the skinning block starts at 4 - four f16 weights summing to
    // 1 on all 108 vertices, which is what fixes the block's place. Read as half-float the UVs land in
    // [0.03,0.49]x[0,1]; read as float32 they are denormals (1e-41). The same descriptor spacing
    // appears on all four FaceGen descriptors (the attribute after TEXCOORD0 starts 4 bytes later),
    // which is why it also serves as the ceiling below: a descriptor can never declare more UV bytes
    // than it has room for.
    // ---------------------------------------------------------------------------
    constexpr REX::W32::DXGI_FORMAT Panel_Uv_Format_Half = REX::W32::DXGI_FORMAT_R16G16_FLOAT;
    constexpr REX::W32::DXGI_FORMAT Panel_Uv_Format_Full = REX::W32::DXGI_FORMAT_R32G32_FLOAT;

    // Byte width of the UV slot as the descriptor itself defines it: the distance from the UV offset
    // to the closest attribute starting above it, in attribute order. The positionless FaceGen
    // descriptors carry a stale POSITION offset past their stride, and taking the SMALLEST offset
    // above the UV ignores it - the UV's real neighbour (normal, colour or the skinning block) starts
    // closer. Zero means the descriptor offers no neighbour above the UV, which leaves the bit
    // unclamped.
    uint32_t uv_slot_bytes(RE::BSGraphics::VertexDesc const& desc)
    {
        uint32_t const uv_offset = desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_TEXCOORD0);
        uint32_t nearest = 0;
        for (uint8_t attribute = 0; attribute < RE::BSGraphics::Vertex::VA_COUNT; ++attribute)
        {
            auto const flag = static_cast<RE::BSGraphics::Vertex::Flags>(1u << attribute);
            if (!desc.HasFlag(flag))
                continue;

            uint32_t const offset = desc.GetAttributeOffset(static_cast<RE::BSGraphics::Vertex::Attribute>(attribute));
            if (offset > uv_offset && (nearest == 0 || offset < nearest))
                nearest = offset;
        }
        return nearest > uv_offset ? nearest - uv_offset : 0u;
    }

    REX::W32::DXGI_FORMAT uv_format_of(RE::BSGraphics::VertexDesc const& desc)
    {
        static std::vector<std::pair<uint64_t, REX::W32::DXGI_FORMAT>> s_uv_formats;
        uint64_t desc_raw = 0;
        std::memcpy(&desc_raw, &desc, sizeof(desc_raw));
        for (auto const& [cached_raw, cached] : s_uv_formats)
        {
            if (cached_raw == desc_raw)
                return cached;
        }

        uint32_t const slot_bytes = uv_slot_bytes(desc);
        bool const uv_full_precision = ((desc_raw >> (54 + RE::BSGraphics::Vertex::VA_TEXCOORD0)) & 1ull) != 0;
        REX::W32::DXGI_FORMAT const format = (uv_full_precision && slot_bytes >= sizeof(float) * 2u)
            ? Panel_Uv_Format_Full
            : Panel_Uv_Format_Half;
        if (s_uv_formats.size() < Max_Position_Calibrations)
            s_uv_formats.emplace_back(desc_raw, format);
        logger::info("Panel: uv layout uv_prec={} slot={}B fmt={:#06x} off={} desc={:#018x}",
            uv_full_precision ? "full" : "half", slot_bytes,
            static_cast<unsigned>(format),
            desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_TEXCOORD0), desc_raw);
        return format;
    }

    // Column-vector point transform M.p (translation included): XMVector3Transform computes p.M, so
    // transpose first.
    RE::NiPoint3 transform_point(DirectX::XMFLOAT4X4 const& m, RE::NiPoint3 const& p)
    {
        DirectX::XMMATRIX const transposed = DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&m));
        DirectX::XMFLOAT4 const point{ p.x, p.y, p.z, 1.0f };
        DirectX::XMFLOAT4 transformed{};
        DirectX::XMStoreFloat4(&transformed, DirectX::XMVector3Transform(DirectX::XMLoadFloat4(&point), transposed));
        return RE::NiPoint3{ transformed.x, transformed.y, transformed.z };
    }

    float distance_to_point(RE::NiPoint3 const& a, RE::NiPoint3 const& b)
    {
        float const dx = a.x - b.x;
        float const dy = a.y - b.y;
        float const dz = a.z - b.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    // ---------------------------------------------------------------------------
    // Position format calibration: VF_FULLPREC is unreliable (0x1b/0x3b leave the flag clear on a
    // float32 position; misdeclaring it as half4 scrambles every position). Instead rawVertexData is
    // decoded and compared against the engine modelBound to select the matching (format, offset),
    // cached per descriptor.
    // ---------------------------------------------------------------------------

    struct PositionCandidate
    {
        REX::W32::DXGI_FORMAT format;
        uint32_t bytes;
        bool from_desc;  // true: offset taken from the descriptor's VA_POSITION; false: offset 0
    };

    // Candidate table (float32 first: on a tie prefer the conservative full-precision reading)
    constexpr PositionCandidate Position_Candidates[] = {
        { REX::W32::DXGI_FORMAT_R32G32B32_FLOAT, 12, true },
        { REX::W32::DXGI_FORMAT_R32G32B32_FLOAT, 12, false },
        { REX::W32::DXGI_FORMAT_R16G16B16A16_FLOAT, 8, true },
        { REX::W32::DXGI_FORMAT_R16G16B16A16_FLOAT, 8, false },
    };
    constexpr size_t Position_Candidate_Count = std::size(Position_Candidates);

    enum class PositionCalibrationState : uint8_t
    {
        e_measured,       // measured successfully, the calibration result is used
        e_desc_fallback,  // cannot calibrate (raw data missing, and so on) -> fall back to descriptor derivation, do not skip
        e_unresolved,     // no candidate matched -> skip this draw (better to draw too little than to smear garbage over the screen)
    };

    struct PositionCalibration
    {
        // Layout decision only (format/offset are descriptor attributes, so they can be cached per descriptor).
        REX::W32::DXGI_FORMAT format;
        uint32_t offset;
        PositionCalibrationState state;
    };

    // Decode a single position: the caller must already guarantee offset + bytes <= stride (bounds safety)
    bool decode_position(
        uint8_t const* base, uint32_t stride, uint32_t offset,
        uint32_t bytes, uint32_t index, RE::NiPoint3& out)
    {
        uint8_t const* src = base + static_cast<size_t>(index) * stride + offset;
        if (bytes == 12)
        {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            std::memcpy(&x, src + 0, sizeof(float));
            std::memcpy(&y, src + 4, sizeof(float));
            std::memcpy(&z, src + 8, sizeof(float));
            out = RE::NiPoint3{ x, y, z };
        }
        else
        {
            uint16_t half[4]{};
            std::memcpy(half, src, sizeof(half));
            out = RE::NiPoint3{
                DirectX::PackedVector::XMConvertHalfToFloat(half[0]),
                DirectX::PackedVector::XMConvertHalfToFloat(half[1]),
                DirectX::PackedVector::XMConvertHalfToFloat(half[2]),
            };
        }
        return std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z);
    }

    // Sample-decode the model-space AABB centre/radius and min/max (the sample cap flattens large
    // meshes); any non-finite value -> failure
    bool measure_position(
        uint8_t const* base, uint32_t stride, uint32_t offset,
        uint32_t bytes, uint32_t vertex_count,
        RE::NiPoint3& center, float& radius, RE::NiPoint3& min, RE::NiPoint3& max)
    {
        uint32_t const step = (vertex_count > Calibration_Sample_Limit) ?
            (vertex_count + Calibration_Sample_Limit - 1) / Calibration_Sample_Limit : 1;
        RE::NiPoint3 min_p{};
        RE::NiPoint3 max_p{};
        bool first = true;
        for (uint32_t v = 0; v < vertex_count; v += step)
        {
            RE::NiPoint3 p{};
            if (!decode_position(base, stride, offset, bytes, v, p))
                return false;
            if (first)
            {
                min_p = max_p = p;
                first = false;
            }
            else
            {
                min_p.x = std::min(min_p.x, p.x);
                max_p.x = std::max(max_p.x, p.x);
                min_p.y = std::min(min_p.y, p.y);
                max_p.y = std::max(max_p.y, p.y);
                min_p.z = std::min(min_p.z, p.z);
                max_p.z = std::max(max_p.z, p.z);
            }
        }
        if (first)
            return false;

        center = RE::NiPoint3{ (min_p.x + max_p.x) * 0.5f, (min_p.y + max_p.y) * 0.5f, (min_p.z + max_p.z) * 0.5f };
        float const dx = max_p.x - min_p.x;
        float const dy = max_p.y - min_p.y;
        float const dz = max_p.z - min_p.z;
        radius = 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz);
        min = min_p;
        max = max_p;
        return true;
    }

    // Decide and select the position layout; the first calibration of each descriptor emits one
    // evidence log line (INFO on success / WARN on no match); the degraded path logs INFO once. The
    // result is cached per descriptor (zero decoding in steady state).
    PositionCalibration calibrate_position_format(
        RE::BSGraphics::VertexDesc const& desc,
        RE::BSGraphics::TriShape const* renderer_data,
        uint32_t vertex_count,
        RE::NiBound const& model_bound,
        uint32_t stride)
    {
        uint64_t desc_raw = 0;
        std::memcpy(&desc_raw, &desc, sizeof(desc_raw));

        // Function-local cache with static storage: the calibration result is a pure function of the
        // descriptor, the vertex data and the model bound, so one process-wide cache is correct.
        static std::vector<std::pair<uint64_t, PositionCalibration>> s_calibrations;
        for (auto const& [cached_raw, cached] : s_calibrations)
        {
            if (cached_raw == desc_raw)
                return cached;
        }

        uint32_t const desc_offset = desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_POSITION);
        bool const decodable = renderer_data && renderer_data->rawVertexData &&
                               desc.HasFlag(RE::BSGraphics::Vertex::VF_VERTEX) && stride > 0;

        PositionCalibration result{ .format = position_format_of(desc), .offset = desc_offset, .state = PositionCalibrationState::e_desc_fallback };
        std::string const desc_fields = fmt::format(
            "desc={:#018x} flags={:#06x} stride={} pos={} uv={} nrm={} bin={} col={} vertices={} model_bound=({:.1f},{:.1f},{:.1f}) r={:.1f}",
            desc_raw,
            static_cast<unsigned>(desc.GetFlags()),
            stride,
            desc_offset,
            desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_TEXCOORD0),
            desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_NORMAL),
            desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_BINORMAL),
            desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_COLOR),
            vertex_count,
            model_bound.center.x, model_bound.center.y, model_bound.center.z, model_bound.radius);

        if (!decodable)
        {
            // Cannot calibrate -> fall back to descriptor derivation (the current behaviour), log INFO once, do not skip
            logger::info("Panel: position calibration unavailable (raw vertex data missing), using descriptor-derived layout {} format={:#06x} offset={}",
                    desc_fields, static_cast<unsigned>(result.format), result.offset);
            if (s_calibrations.size() < Max_Position_Calibrations)
                s_calibrations.emplace_back(desc_raw, result);
            return result;
        }

        struct CandidateResult
        {
            REX::W32::DXGI_FORMAT format;
            uint32_t offset;
            bool valid;          // survived candidate culling (stride/bounds)
            bool passed;         // matched modelBound
            RE::NiPoint3 center;
            float radius;
            float center_error;
        };
        CandidateResult results[Position_Candidate_Count]{};

        float const tolerance = 0.5f * std::max(model_bound.radius, 1.0f);
        for (size_t i = 0; i < Position_Candidate_Count; ++i)
        {
            PositionCandidate const& cand = Position_Candidates[i];
            CandidateResult& res = results[i];
            res.format = cand.format;
            res.offset = cand.from_desc ? desc_offset : 0;
            // Cull: the stride cannot hold the format, or offset+format runs past this vertex stride
            if (stride < cand.bytes || res.offset + cand.bytes > stride)
                continue;
            res.valid = true;

            RE::NiPoint3 center{};
            float radius = 0.0f;
            RE::NiPoint3 model_min{};
            RE::NiPoint3 model_max{};
            if (!measure_position(renderer_data->rawVertexData, stride, res.offset, cand.bytes, vertex_count, center, radius, model_min, model_max))
                continue;

            res.center = center;
            res.radius = radius;
            res.center_error = distance_to_point(center, model_bound.center);
            res.passed = res.center_error <= tolerance &&
                         radius >= 0.4f * model_bound.radius &&
                         radius <= 2.5f * model_bound.radius;
        }

        // Take the passing candidate with the smallest centre error; on a tie prefer float32
        // (conservative: read too many bytes rather than misread half precision)
        int best = -1;
        for (size_t i = 0; i < Position_Candidate_Count; ++i)
        {
            if (!results[i].passed)
                continue;
            if (best < 0)
            {
                best = static_cast<int>(i);
                continue;
            }
            float const best_error = results[best].center_error;
            float const error = results[i].center_error;
            bool const is_float32 = results[i].format == REX::W32::DXGI_FORMAT_R32G32B32_FLOAT;
            bool const best_is_float32 = results[best].format == REX::W32::DXGI_FORMAT_R32G32B32_FLOAT;
            if (error < best_error - 1e-4f || (std::fabs(error - best_error) <= 1e-4f && is_float32 && !best_is_float32))
                best = static_cast<int>(i);
        }

        if (best < 0)
        {
            result.state = PositionCalibrationState::e_unresolved;

            std::string table;
            for (size_t i = 0; i < Position_Candidate_Count; ++i)
            {
                CandidateResult const& r = results[i];
                table += fmt::format(" [{} fmt={:#06x} off={} {} center=({:.1f},{:.1f},{:.1f}) r={:.1f} err={:.1f}]",
                    i,
                    static_cast<unsigned>(r.format),
                    r.offset,
                    r.passed ? "PASS" : (r.valid ? "fail" : "rejected"),
                    r.center.x,
                    r.center.y,
                    r.center.z,
                    r.radius,
                    r.center_error);
            }
            logger::warn("Panel: position calibration found no matching candidate, draw skipped {} candidates:{}",
                    desc_fields, table);
        }
        else
        {
            result.format = results[best].format;
            result.offset = results[best].offset;
            result.state = PositionCalibrationState::e_measured;
            logger::info("Panel: position calibration selected fmt={:#06x} off={} (center err {:.2f}) desc={:#018x}",
                    static_cast<unsigned>(result.format), result.offset, results[best].center_error, desc_raw);
        }

        if (s_calibrations.size() < Max_Position_Calibrations)
            s_calibrations.emplace_back(desc_raw, result);
        return result;
    }

    // ---------------------------------------------------------------------------
    // Skinned partition layout calibration: neither the stride (SKINNING byte count) nor the
    // position layout has an authoritative definition, so the static path's method is reused -
    // decode the partition's own rawVertexData and compare against the geometry modelBound. The
    // position format comes from the attribute offset spacing (for flags 0x5b/0x9/0x1b/0x3b the
    // attribute after the position always has offset 16, that is a float32 16-byte slot). Results
    // are cached per raw partition descriptor; no candidate passes or rawVertexData missing ->
    // e_unresolved and the caller skips the draw.
    // ---------------------------------------------------------------------------

    enum class SkinnedCalibrationState : uint8_t
    {
        e_measured,    // measured successfully, the calibration result is used
        e_unresolved,  // no candidate passed / cannot calibrate -> skip this draw
    };

    // Layout decision: the cache stores only these fields and no per-mesh validation verdict
    struct SkinnedVertexLayout
    {
        REX::W32::DXGI_FORMAT position_format;
        uint32_t position_offset;
        uint32_t stride;
        uint8_t layout_id;     // SKINNING layout id (1..4)
        PanelSkinLayout skin;
        SkinnedCalibrationState state;
    };

    // SKINNING layout candidates (the order is the priority; *_delta is the byte increment relative
    // to the VA_SKINNING offset)
    struct SkinningLayoutSpec
    {
        uint8_t id;
        REX::W32::DXGI_FORMAT weight_format;
        uint32_t weight_bytes;
        uint32_t weight_delta;
        REX::W32::DXGI_FORMAT index_format;
        uint32_t index_delta;
    };

    // 1/2 are used when the available bytes A>=12; 3/4 are used when A==8
    constexpr SkinningLayoutSpec Skinning_Layouts[] = {
        { 1, REX::W32::DXGI_FORMAT_R16G16B16A16_FLOAT, 8, 0, REX::W32::DXGI_FORMAT_R8G8B8A8_UINT, 8 },
        { 2, REX::W32::DXGI_FORMAT_R16G16B16A16_FLOAT, 8, 4, REX::W32::DXGI_FORMAT_R8G8B8A8_UINT, 0 },
        { 3, REX::W32::DXGI_FORMAT_R8G8B8A8_UNORM, 4, 0, REX::W32::DXGI_FORMAT_R8G8B8A8_UINT, 4 },
        { 4, REX::W32::DXGI_FORMAT_R8G8B8A8_UNORM, 4, 4, REX::W32::DXGI_FORMAT_R8G8B8A8_UINT, 0 },
    };

    // Look up a candidate definition by layout id (the id comes from a cached layout decision so it
    // always hits; return the first entry defensively)
    SkinningLayoutSpec const* find_skinning_layout(uint8_t id)
    {
        for (const SkinningLayoutSpec& skinning_layout : Skinning_Layouts)
        {
            if (skinning_layout.id == id)
                return &skinning_layout;
        }
        return &Skinning_Layouts[0];
    }

    // Per-mesh validation statistics: produced by a full vertex traversal over each mesh's own data;
    // indices are interpreted as global bone indices bounded by the palette length
    struct SkinnedMeshStats
    {
        bool position_finite;                      // all vertices finite under the selected position format
        uint32_t index_min;
        uint32_t index_max;
        uint32_t out_of_range_index_count;         // number of vertices holding an index >= palette slot
        uint32_t out_of_range_weighted_count;      // of those, the number of vertices whose (that slot's) weight is non-zero -> reject
        uint32_t first_out_of_range_index;         // first out-of-range index
        float first_out_of_range_weight;           // its weight
        float weight_sum_min;
        float weight_sum_max;
        uint32_t bad_weight_vertices;              // number of vertices with an out-of-range weight component
    };

    // Per-mesh validation verdict: position failure -> skip; weight failure -> switch candidate;
    // out-of-range index: non-zero weight -> caller skips, zero weight -> drawn as usual
    enum class SkinnedMeshVerdict : uint8_t
    {
        e_ok,
        e_position_bad,  // positions non-finite -> skip this mesh
        e_weights_bad,   // weight sum/components invalid -> the cached layout does not fit this mesh, re-enumerate candidates
    };

    // Candidate measurement result (layout decision plus the validation statistics on this mesh,
    // used for the log table)
    struct SkinnedLayoutCandidateResult
    {
        uint32_t stride;
        uint8_t layout;
        bool bounds_ok;          // bounds culling (no read performed)
        SkinnedMeshStats stats;
        bool passed;             // positions finite and weights valid (an out-of-range index does not affect passing)
    };

    // Decode the weight quadruple of a single vertex (R16G16B16A16_FLOAT / R8G8B8A8_UNORM; the
    // caller already guarantees the read stays in bounds)
    void decode_weights(REX::W32::DXGI_FORMAT format, uint8_t const* src, float (&out)[4])
    {
        if (format == REX::W32::DXGI_FORMAT_R16G16B16A16_FLOAT)
        {
            uint16_t half[4]{};
            std::memcpy(half, src, sizeof(half));
            for (int i = 0; i < 4; ++i)
                out[i] = DirectX::PackedVector::XMConvertHalfToFloat(half[i]);
        }
        else
        {
            uint8_t byte[4]{};
            std::memcpy(byte, src, sizeof(byte));
            for (int i = 0; i < 4; ++i)
                out[i] = static_cast<float>(byte[i]) / 255.0f;
        }
    }

    // Decode the 4 bone indices of a single vertex (R8G8B8A8_UINT; the caller already guarantees the
    // read stays in bounds)
    void decode_indices(uint8_t const* src, uint8_t (&out)[4])
    {
        std::memcpy(out, src, sizeof(out));
    }

    // Traverse all vertices under the given position and SKINNING layout, fill in the statistics and
    // return the verdict. Indices are global bone indices with index_bound (the palette length) as
    // the out-of-range bound; the caller guarantees every read stays within the stride. A position
    // byte width of zero skips position decoding entirely (positionless partitions carry no
    // positions, so there is nothing to check).
    SkinnedMeshVerdict validate_skinned_mesh(
        uint8_t const* raw, uint32_t stride, uint32_t pos_offset, uint32_t pos_bytes,
        SkinningLayoutSpec const& spec, uint32_t weight_offset, uint32_t index_offset,
        uint32_t vertex_count, uint32_t index_bound, SkinnedMeshStats& stats)
    {
        stats = SkinnedMeshStats{ .position_finite = true };
        stats.index_min = 0xFFFFFFFFu;  // start from a sentinel and take the min over the indices
        bool first_sample = true;
        for (uint32_t v = 0; v < vertex_count; ++v)
        {
            if (pos_bytes > 0)
            {
                RE::NiPoint3 p{};
                if (!decode_position(raw, stride, pos_offset, pos_bytes, v, p))
                    stats.position_finite = false;
            }

            uint8_t const* const base = raw + static_cast<size_t>(v) * stride;
            float w[4]{};
            decode_weights(spec.weight_format, base + weight_offset, w);
            uint8_t idx[4]{};
            decode_indices(base + index_offset, idx);

            float sum = 0.0f;
            for (float const& i : w)
                sum += i;
            if (first_sample)
            {
                stats.weight_sum_min = stats.weight_sum_max = sum;
                first_sample = false;
            }
            else
            {
                stats.weight_sum_min = std::min(stats.weight_sum_min, sum);
                stats.weight_sum_max = std::max(stats.weight_sum_max, sum);
            }

            // Whether all weight components of a single vertex are within the allowed range
            if (!std::ranges::all_of(w, [](float const& i)
            {
                if (i < Skinned_Weight_Min || i > Skinned_Weight_Max)
                    return false;
                return true;
            }))
                ++stats.bad_weight_vertices;

            bool vertex_out_of_range = false;
            bool vertex_weighted_out_of_range = false;
            for (int i = 0; i < 4; ++i)
            {
                uint32_t const index = idx[i];
                stats.index_min = std::min(stats.index_min, index);
                stats.index_max = std::max(stats.index_max, index);
                if (index >= index_bound)
                {
                    vertex_out_of_range = true;
                    if (w[i] != 0.0f)  // this out-of-range slot carries a non-zero weight -> it cannot render correctly
                        vertex_weighted_out_of_range = true;
                }
            }
            if (vertex_out_of_range)
            {
                ++stats.out_of_range_index_count;
                if (stats.out_of_range_weighted_count == 0)
                {
                    // Record the first "out-of-range with non-zero weight" slot (evidence for the skip log)
                    for (int i = 0; i < 4; ++i)
                    {
                        if (static_cast<uint32_t>(idx[i]) >= index_bound && w[i] != 0.0f)
                        {
                            stats.first_out_of_range_index = static_cast<uint32_t>(idx[i]);
                            stats.first_out_of_range_weight = w[i];
                            break;
                        }
                    }
                }
                if (vertex_weighted_out_of_range)
                    ++stats.out_of_range_weighted_count;
            }
        }

        if (!stats.position_finite)
            return SkinnedMeshVerdict::e_position_bad;
        bool const weights_ok = stats.bad_weight_vertices == 0 &&
                                stats.weight_sum_min >= Skinned_Weight_Sum_Min &&
                                stats.weight_sum_max <= Skinned_Weight_Sum_Max;
        return weights_ok ? SkinnedMeshVerdict::e_ok : SkinnedMeshVerdict::e_weights_bad;
    }

    // Candidate table text (built on the one-shot path; each entry holds stride/layout/position
    // finiteness/weight sum/out-of-range counts/index)
    std::string format_skinned_candidate_table(
        SkinnedLayoutCandidateResult const (&candidates)[Max_Skinned_Layout_Candidates],
        PanelSkinLayout const (&skin)[Max_Skinned_Layout_Candidates],
        size_t count,
        uint32_t index_bound)
    {
        std::string table;
        for (size_t i = 0; i < count; ++i)
        {
            SkinnedLayoutCandidateResult const& c = candidates[i];
            SkinnedMeshStats const& s = c.stats;
            table += fmt::format(
                " [{} L{} stride={} w(fmt={:#06x},off={}) i(fmt={:#06x},off={}) {} pos_finite={} wsum=[{:.3f},{:.3f}] wbad={} imax={}/{} oob={}(w{})]",
                i, static_cast<unsigned>(c.layout), c.stride,
                static_cast<unsigned>(skin[i].weight_format), skin[i].weight_offset,
                static_cast<unsigned>(skin[i].index_format), skin[i].index_offset,
                c.passed ? "PASS" : (c.bounds_ok ? "fail" : "rejected"),
                s.position_finite, s.weight_sum_min, s.weight_sum_max, s.bad_weight_vertices,
                s.index_max, index_bound, s.out_of_range_index_count, s.out_of_range_weighted_count);
        }
        return table;
    }

    // Enumerate (stride, SKINNING layout) candidates and validate each on the mesh's own data; the
    // cache is neither read nor written.
    size_t enumerate_skinned_candidates(
        RE::BSGraphics::VertexDesc const& desc,
        RE::BSGraphics::TriShape const* renderer_data,
        uint32_t vertex_count,
        uint32_t index_bound,
        uint32_t pos_offset,
        uint32_t pos_bytes,
        uint32_t skin_offset,
        SkinnedLayoutCandidateResult (&candidates)[Max_Skinned_Layout_Candidates],
        PanelSkinLayout (&skin)[Max_Skinned_Layout_Candidates])
    {
        // Stride candidates (SKINNING 8/12 bytes, deduplicated)
        uint32_t const stride_options[2] = {
            vertex_size_of_with_skinning(desc, 8u),
            vertex_size_of_with_skinning(desc, 12u),
        };
        uint32_t strides[2]{};
        size_t stride_count = 0;
        for (uint32_t const stride : stride_options)
        {
            bool duplicate = false;
            for (size_t i = 0; i < stride_count; ++i)
                duplicate = duplicate || strides[i] == stride;
            if (!duplicate)
                strides[stride_count++] = stride;
        }

        bool const raw_available = renderer_data && renderer_data->rawVertexData && vertex_count > 0;
        uint8_t const* const raw = raw_available ? renderer_data->rawVertexData : nullptr;
        size_t candidate_count = 0;
        if (!raw_available)
            return 0;

        for (size_t si = 0; si < stride_count; ++si)
        {
            uint32_t const stride = strides[si];
            uint32_t const available = (stride > skin_offset) ? (stride - skin_offset) : 0u;
            for (const SkinningLayoutSpec& spec : Skinning_Layouts)
            {
                if (candidate_count >= Max_Skinned_Layout_Candidates)
                    break;  // defensive: stop once the candidate array is full
                bool const eligible = (spec.id <= 2) ? (available >= 12u) : (available == 8u);
                if (!eligible)
                    continue;

                uint32_t const weight_offset = skin_offset + spec.weight_delta;
                uint32_t const index_offset = skin_offset + spec.index_delta;
                // Bounds safety: the position/weight/index reads must all stay within the stride,
                // otherwise the candidate is unusable (nothing is read)
                bool const bounds_ok = (pos_offset + pos_bytes <= stride) &&
                                       (weight_offset + spec.weight_bytes <= stride) &&
                                       (index_offset + 4u <= stride);

                SkinnedLayoutCandidateResult& cr = candidates[candidate_count];
                cr.stride = stride;
                cr.layout = spec.id;
                cr.bounds_ok = bounds_ok;
                cr.stats.position_finite = true;
                cr.passed = false;
                skin[candidate_count] = PanelSkinLayout{ spec.weight_format, weight_offset, spec.index_format, index_offset };

                if (bounds_ok)
                {
                    SkinnedMeshVerdict const verdict = validate_skinned_mesh(
                        raw, stride, pos_offset, pos_bytes, spec, weight_offset, index_offset,
                        vertex_count, index_bound, cr.stats);
                    // An out-of-range index does not affect the candidate passing (the complete
                    // palette upload performed by the panel draw covers it)
                    cr.passed = verdict == SkinnedMeshVerdict::e_ok;
                }

                ++candidate_count;
            }
        }
        return candidate_count;
    }

    SkinnedVertexLayout calibrate_skinned_layout(
        RE::BSGraphics::VertexDesc const& desc,
        RE::BSGraphics::TriShape const* renderer_data,
        uint32_t vertex_count,
        uint32_t index_bound)
    {
        uint64_t desc_raw = 0;
        std::memcpy(&desc_raw, &desc, sizeof(desc_raw));

        // Function-local cache with static storage, as in the position calibration above.
        static std::vector<std::pair<uint64_t, SkinnedVertexLayout>> s_skinned_calibrations;
        for (auto const& [cached_raw, cached] : s_skinned_calibrations)
        {
            if (cached_raw == desc_raw)
                return cached;
        }

        uint32_t const pos_offset = desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_POSITION);
        uint32_t const skin_offset = desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_SKINNING);

        // ---- The position format is determined by the attribute offset spacing (gap = offset of the
        // next later attribute - position offset): for flags 0x5b/0x9/0x1b/0x3b the attribute
        // immediately after the position always has offset 16, that is a float32 16-byte slot. ----
        uint32_t next_offset = 0;
        bool has_next = false;
        auto const consider_next = [&](bool present, uint32_t offset)
        {
            if (!present || offset <= pos_offset)
                return;
            if (!has_next || offset < next_offset)
            {
                next_offset = offset;
                has_next = true;
            }
        };
        consider_next(desc.HasFlag(RE::BSGraphics::Vertex::VF_UV), desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_TEXCOORD0));
        consider_next(desc.HasFlag(RE::BSGraphics::Vertex::VF_UV_2), desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_TEXCOORD1));
        consider_next(desc.HasFlag(RE::BSGraphics::Vertex::VF_NORMAL), desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_NORMAL));
        consider_next(desc.HasFlag(RE::BSGraphics::Vertex::VF_TANGENT), desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_BINORMAL));
        consider_next(desc.HasFlag(RE::BSGraphics::Vertex::VF_COLORS), desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_COLOR));
        consider_next(desc.HasFlag(RE::BSGraphics::Vertex::VF_SKINNED), desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_SKINNING));
        consider_next(desc.HasFlag(RE::BSGraphics::Vertex::VF_LANDDATA), desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_LANDDATA));
        consider_next(desc.HasFlag(RE::BSGraphics::Vertex::VF_EYEDATA), desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_EYEDATA));

        int32_t const gap = has_next ? (static_cast<int32_t>(next_offset) - static_cast<int32_t>(pos_offset)) : -1;
        bool const gap_degenerate = gap < 8;

        SkinnedVertexLayout result{ .position_format = position_format_of(desc), .position_offset = pos_offset, .stride = 0, .layout_id = 0, .skin = {}, .state = SkinnedCalibrationState::e_unresolved };
        if (gap >= 12)
            result.position_format = REX::W32::DXGI_FORMAT_R32G32B32_FLOAT;
        else if (gap >= 8)
            result.position_format = REX::W32::DXGI_FORMAT_R16G16B16A16_FLOAT;
        uint32_t const pos_bytes = (result.position_format == REX::W32::DXGI_FORMAT_R32G32B32_FLOAT) ? 12u : 8u;

        std::string const desc_fields = fmt::format(
            "desc={:#018x} flags={:#06x} pos={} uv={} nrm={} bin={} col={} skin={} gap={}{} pos_fmt={:#06x} pos_off={} vertices={} index_bound={}",
            desc_raw,
            static_cast<unsigned>(desc.GetFlags()),
            pos_offset,
            desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_TEXCOORD0),
            desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_NORMAL),
            desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_BINORMAL),
            desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_COLOR),
            skin_offset,
            gap,
            gap_degenerate ? " (degenerate: no later attribute, fell back to fullprec flag)" : "",
            static_cast<unsigned>(result.position_format),
            pos_offset,
            vertex_count,
            index_bound);

        // ---- Candidate enumeration plus per-candidate validation on this mesh's own data: a full
        // vertex traversal, no sampling, and no validation verdict is read from or written to the
        // cache. ----
        SkinnedLayoutCandidateResult candidates[Max_Skinned_Layout_Candidates]{};
        PanelSkinLayout candidate_skin[Max_Skinned_Layout_Candidates]{};
        size_t const candidate_count = enumerate_skinned_candidates(
            desc, renderer_data, vertex_count, index_bound,
            pos_offset, pos_bytes, skin_offset,
            candidates, candidate_skin);

        if (candidate_count == 0)
        {
            result.state = SkinnedCalibrationState::e_unresolved;
            logger::warn("Panel: skinned layout calibration unavailable ({}), draw skipped {}",
                    (renderer_data && renderer_data->rawVertexData) ? "geometry has no usable candidate" : "raw vertex data missing", desc_fields);
            if (s_skinned_calibrations.size() < Max_Skinned_Layout_Calibrations)
                s_skinned_calibrations.emplace_back(desc_raw, result);
            return result;
        }

        // Take the first candidate that passes everything (the priority is the enumeration order:
        // stride order x layouts 1..4)
        int64_t best = -1;
        for (size_t i = 0; i < candidate_count; ++i)
        {
            if (candidates[i].passed)
            {
                best = static_cast<int64_t>(i);
                break;
            }
        }

        if (best < 0)
        {
            result.state = SkinnedCalibrationState::e_unresolved;
            std::string const table = format_skinned_candidate_table(candidates, candidate_skin, candidate_count, index_bound);
            logger::warn("Panel: skinned layout calibration found no matching candidate, draw skipped {} candidates:{}",
                    desc_fields, table);
        }
        else
        {
            result.stride = candidates[best].stride;
            result.layout_id = candidates[best].layout;
            result.skin = candidate_skin[best];
            result.state = SkinnedCalibrationState::e_measured;
            logger::info("Panel: skinned layout calibration selected pos(fmt={:#06x},off={}) stride={} layout={} weight(fmt={:#06x},off={}) index(fmt={:#06x},off={}) desc={:#018x}",
                    static_cast<unsigned>(result.position_format),
                    result.position_offset,
                    result.stride,
                    static_cast<unsigned>(result.layout_id),
                    static_cast<unsigned>(result.skin.weight_format),
                    result.skin.weight_offset,
                    static_cast<unsigned>(result.skin.index_format),
                    result.skin.index_offset,
                    desc_raw);
        }

        // Only the layout decision is written to the cache (the result struct holds no per-mesh
        // statistics)
        if (s_skinned_calibrations.size() < Max_Skinned_Layout_Calibrations)
            s_skinned_calibrations.emplace_back(desc_raw, result);
        return result;
    }

    // ---------------------------------------------------------------------------
    // Geometry collection
    // ---------------------------------------------------------------------------

    // Context shared by every mesh of one collection pass: the reference position and form id serve
    // the ownership check and the logs; the draw list is the caller's reused buffer. The census
    // counters feed the one-shot funnel log that pinpoints where skinned meshes are rejected.
    struct PanelWalkContext
    {
        RE::NiPoint3 position;
        RE::FormID form_id;
        std::vector<PanelDraw>* draws;
        Config const& config;
        // The preview's NPC base, resolved once per collection: the FaceGen material family shades
        // its grayscale detail textures with the NPC's own skin tone, which lives on the base.
        RE::TESNPC const* npc{ nullptr };
        std::uint32_t visited{ 0 };
        std::uint32_t skinned_seen{ 0 };
        std::uint32_t skinned_ok{ 0 };
        std::uint32_t static_seen{ 0 };
        std::uint32_t static_ok{ 0 };
        std::uint32_t sk_incomplete{ 0 };
        std::uint32_t p0{ 0 };
        std::uint32_t no_root{ 0 };
        std::uint32_t wbound{ 0 };
        std::uint32_t part_gate{ 0 };
        std::uint32_t strips{ 0 };
        std::uint32_t palette{ 0 };
        std::uint32_t calib{ 0 };
        std::uint32_t vpos{ 0 };
        std::uint32_t vweights{ 0 };
        std::uint32_t oob{ 0 };
        std::uint32_t st_nogpu{ 0 };
        std::uint32_t st_excl{ 0 };
        std::uint32_t st_nottri{ 0 };
        std::uint32_t st_empty{ 0 };
        std::uint32_t st_bound{ 0 };
        std::uint32_t st_worldr{ 0 };
        std::uint32_t st_calib{ 0 };
        std::uint32_t st_notref{ 0 };
        // The first palette-gate rejection's raw values: the census counters above say *how many*
        // partitions the gate turned away, but without the actual bone and matrix counts a
        // body-less panel cannot be told apart from the log alone - a zero count and an over-budget
        // count have different causes and different fixes.
        bool first_palette_captured{ false };
        std::uint32_t first_palette_bone_count{ 0 };
        std::uint32_t first_palette_num_matrices{ 0 };
        std::string first_palette_node;
    };

    // The material the panel needs for shading, resolved once per mesh through the engine's own
    // accessors. has_uv is clear when the mesh carries no UV attribute or no diffuse texture, and
    // diffuse_view stays null in both cases: that null is the explicit "no diffuse texture" marker
    // the pixel shader reads as "shade with the flat albedo". hair_tint carries the engine's own
    // hair-dye colour when the mesh's material is a hair-tint material, so the panel's pixel shader
    // can reproduce the player's chosen hair colour instead of the texture's raw grayscale.
    // skin_tint carries the character's skin tone for the FaceGen material family (kFaceGen and
    // kFaceGenRGBTint): the shader remaps the texture's own RGB toward it with the engine's
    // quadratic (Community Shaders GetFacegenRGBTintBaseColor).
    struct PanelMaterial
    {
        RE::BSShaderProperty* shader_property;
        REX::W32::ID3D11ShaderResourceView* diffuse_view;
        float material_alpha;
        bool has_uv;
        bool hair_tint;
        RE::NiColor hair_tint_color;
        bool skin_tint;
        RE::NiColor skin_tint_color;
        // The material's own UV remap, applied by the engine's vertex shader before sampling.
        float uv_offset_u;
        float uv_offset_v;
        float uv_scale_u;
        float uv_scale_v;

        // Diagnosis of what the engine's own material said, so a mesh shaded with the flat albedo
        // can be told apart from one bound to the wrong texture: the texture set's own diffuse path,
        // the renderer texture's dimensions, and the material's family (Feature) and shader property.
        char const* diffuse_path;
        uint32_t diffuse_width;
        uint32_t diffuse_height;
        uint32_t diffuse_mips;
        uint32_t diffuse_format;
        char const* property_rtti;
        uint32_t material_feature;
    };

    PanelMaterial resolve_material(RE::BSShaderProperty* property, RE::BSGraphics::VertexDesc const& desc,
        RE::TESNPC const* npc)
    {
        // Take a NiPointer reference to the texture before reading its renderer data: the caller
        // holds the geometry (and through it the property), but the property's texture member can
        // be cleared concurrently, and reading rendererTexture of a dying NiSourceTexture returns
        // freed memory (the 0x8 garbage SRV the AddRef crash faulted on).
        RE::NiPointer<RE::NiSourceTexture> const texture(property ? property->GetBaseTexture() : nullptr);
        RE::BSGraphics::Texture* const renderer_texture = texture ? texture->rendererTexture : nullptr;
        REX::W32::ID3D11ShaderResourceView* const view =
            (renderer_texture && renderer_texture->resourceView) ? renderer_texture->resourceView : nullptr;

        // Hair tint: the engine dyes hair in the lighting shader by multiplying the grayscale
        // texture against the material's tint colour. The colour is resolved from the placed
        // actor's own material, so it already reflects the player's chosen hair colour.
        bool hair_tint = false;
        RE::NiColor tint_color{};
        bool skin_tint = false;
        RE::NiColor skin_tint_color{};
        float uv_offset_u = 0.0f;
        float uv_offset_v = 0.0f;
        float uv_scale_u = 1.0f;
        float uv_scale_v = 1.0f;
        char const* diffuse_path = nullptr;
        char const* property_rtti = nullptr;
        uint32_t material_feature = 0;
        if (property)
        {
            property_rtti = property->GetRTTI() ? property->GetRTTI()->GetName() : nullptr;
            RE::BSShaderMaterial* const material = property->GetBaseMaterial();
            if (material)
            {
                // The material itself derives from BSIntrusiveRefCounted and carries no RTTI, so its
                // family is its Feature (kFaceGen 4, kFaceGenRGBTint 5, kHairTint 6, kEye 16, ...) and
                // the property's RTTI names the shader it belongs to.
                material_feature = static_cast<uint32_t>(material->GetFeature());
                if (material->GetFeature() == RE::BSShaderMaterial::Feature::kHairTint)
                {
                    if (RE::BSLightingShaderMaterialHairTint* const hair =
                            skyrim_cast<RE::BSLightingShaderMaterialHairTint*>(material))
                    {
                        hair_tint = true;
                        tint_color = hair->tintColor;
                    }
                }
                // The FaceGen RGB-tint family (kFaceGenRGBTint - the CBBE body, feet and hands)
                // keeps its colour texture and remaps it toward the character's skin tone with the
                // engine's lighting-shader quadratic (Community Shaders GetFacegenRGBTintBaseColor,
                // PS constant 23 = the NPC's tint). The tone is the NPC's tint layer of type
                // kSkinTone when present, else the QNAM body tint. The plain kFaceGen family (the
                // head) takes the same tone; its engine path adds the runtime-generated tint and
                // detail textures, which this panel approximates with the same quadratic.
                if ((material->GetFeature() == RE::BSShaderMaterial::Feature::kFaceGen ||
                     material->GetFeature() == RE::BSShaderMaterial::Feature::kFaceGenRGBTint) && npc)
                {
                    RE::Color tone = npc->bodyTintColor;
                    if (npc->tintLayers)
                    {
                        for (RE::TESNPC::Layer* layer : *npc->tintLayers)
                        {
                            if (layer && layer->tintIndex == static_cast<std::uint16_t>(RE::TintMask::Type::kSkinTone))
                            {
                                tone = layer->tintColor;
                                break;
                            }
                        }
                    }
                    skin_tint = true;
                    skin_tint_color = RE::NiColor{
                        static_cast<float>(tone.red) / 255.0f,
                        static_cast<float>(tone.green) / 255.0f,
                        static_cast<float>(tone.blue) / 255.0f };
                }
                // The engine's own UV remap for this material (the VS maps uv*scale+offset before
                // sampling; atlassed CBBE slots are non-trivial here).
                if (material->texCoordScale[0].x != 0.0f && material->texCoordScale[0].y != 0.0f)
                {
                    uv_offset_u = material->texCoordOffset[0].x;
                    uv_offset_v = material->texCoordOffset[0].y;
                    uv_scale_u = material->texCoordScale[0].x;
                    uv_scale_v = material->texCoordScale[0].y;
                }
            }
        }

        return PanelMaterial{
            .shader_property = property,
            .diffuse_view = view,
            .material_alpha = property ? property->QMaterialAlpha() : 1.0f,
            .has_uv = view != nullptr && desc.HasFlag(RE::BSGraphics::Vertex::VF_UV),
            .hair_tint = hair_tint,
            .hair_tint_color = tint_color,
            .skin_tint = skin_tint,
            .skin_tint_color = skin_tint_color,
            .uv_offset_u = uv_offset_u,
            .uv_offset_v = uv_offset_v,
            .uv_scale_u = uv_scale_u,
            .uv_scale_v = uv_scale_v,
            .diffuse_path = diffuse_path,
            .diffuse_width = renderer_texture ? static_cast<uint32_t>(renderer_texture->width) : 0u,
            .diffuse_height = renderer_texture ? static_cast<uint32_t>(renderer_texture->height) : 0u,
            .diffuse_mips = renderer_texture ? static_cast<uint32_t>(renderer_texture->mips) : 0u,
            .diffuse_format = renderer_texture ? static_cast<uint32_t>(renderer_texture->format) : 0u,
            .property_rtti = property_rtti,
            .material_feature = material_feature,
        };
    }

    void apply_material(PanelDraw& draw, PanelMaterial const& material)
    {
        draw.shader_property = material.shader_property;
        draw.diffuse_view = material.diffuse_view;
        // Hold the SRV from collection to draw: the caller guarantees the texture chain is alive
        // (NiPointer on the geometry and on the texture), so this reference pins the D3D object.
        if (draw.diffuse_view)
            draw.diffuse_view->AddRef();
        draw.material_alpha = material.material_alpha;
        draw.has_uv = material.has_uv;
        draw.hair_tint = material.hair_tint;
        draw.hair_tint_color = material.hair_tint_color;
        draw.skin_tint = material.skin_tint;
        draw.skin_tint_color = material.skin_tint_color;
        draw.uv_offset_u = material.uv_offset_u;
        draw.uv_offset_v = material.uv_offset_v;
        draw.uv_scale_u = material.uv_scale_u;
        draw.uv_scale_v = material.uv_scale_v;
    }

    // ---------------------------------------------------------------------------
    // Per-mesh census, bounded to the first frames of a session. The collection decides each mesh's
    // index space, UV slot and material, and none of it is visible above the debug level - which
    // leaves what the panel renders without a reason: positions, UVs and weights only agree when the
    // vertices they name are the same ones, and a mesh with no diffuse view is silently shaded with
    // the flat albedo instead of its texture.
    // ---------------------------------------------------------------------------
    void log_mesh_census(char const* node, char const* kind, int32_t partition, uint32_t verts, uint32_t tris,
        uint32_t tri_max, char const* index_space, bool has_vertex_map, uint32_t dynamic_capacity,
        uint32_t palette_count, RE::BSGraphics::VertexDesc const& desc, PanelDraw const& draw,
        PanelMaterial const& material, RE::NiAlphaProperty const* alpha_property)
    {
        static uint32_t s_budget = 64;
        if (s_budget == 0)
            return;
        --s_budget;

        uint64_t desc_raw = 0;
        std::memcpy(&desc_raw, &desc, sizeof(desc_raw));
        logger::info("Panel mesh: {} node=\"{}\" p={} verts={} tris={} tri_max={} space={} map={} dyn={} palette={} desc={:#018x} stride={} uv(flag={},off={},fmt={:#06x},slot={}B) skin(w={:#06x}@{},i={:#06x}@{}) pos(fmt={:#06x},off={},stream={}) alpha={:.2f} cutoff={:.2f} opaque={}",
            kind, node ? node : "?", partition, verts, tris, tri_max, index_space,
            has_vertex_map ? "yes" : "no", dynamic_capacity, palette_count, desc_raw, draw.vertex_stride,
            desc.HasFlag(RE::BSGraphics::Vertex::VF_UV) ? "yes" : "no",
            desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_TEXCOORD0),
            static_cast<unsigned>(draw.uv_format), uv_slot_bytes(desc),
            static_cast<unsigned>(draw.skin_layout.weight_format), draw.skin_layout.weight_offset,
            static_cast<unsigned>(draw.skin_layout.index_format), draw.skin_layout.index_offset,
            static_cast<unsigned>(draw.position_format), draw.position_offset,
            draw.position_stream ? "yes" : "no", draw.material_alpha, draw.alpha_cutoff,
            draw.opaque_alpha ? "yes" : "no");

        logger::info("Panel material: node=\"{}\" p={} mat=\"{}\" feature={} diffuse={:X} path=\"{}\" tex={}x{} mips={} fmt={} hair_tint={} skin_tint={} tint=({:.2f},{:.2f},{:.2f}) alpha(test={},blend={})",
            node ? node : "?", partition, material.property_rtti ? material.property_rtti : "?",
            material.material_feature, reinterpret_cast<std::uintptr_t>(material.diffuse_view),
            material.diffuse_path ? material.diffuse_path : "?",
            material.diffuse_width, material.diffuse_height, material.diffuse_mips, material.diffuse_format,
            material.hair_tint ? "yes" : "no", material.skin_tint ? "yes" : "no",
            material.hair_tint ? material.hair_tint_color.red : material.skin_tint_color.red,
            material.hair_tint ? material.hair_tint_color.green : material.skin_tint_color.green,
            material.hair_tint ? material.hair_tint_color.blue : material.skin_tint_color.blue,
            alpha_property && alpha_property->GetAlphaTesting() ? "yes" : "no",
            alpha_property && alpha_property->GetAlphaBlending() ? "yes" : "no");
    }

    // The engine's own per-mesh alpha handling, from the geometry's NiAlphaProperty. SSE's diffuse
    // alpha channel usually stores a specular mask whose near-zero values are not transparency, so a
    // global alpha cutout would discard every opaque mesh wholesale - the whole panel content once
    // vanished exactly this way. The cutout applies only where the property enables testing, and the
    // written alpha follows the texture only where the property actually blends.
    void resolve_alpha_handling(RE::BSGeometry::GEOMETRY_RUNTIME_DATA const& geom_rt, Config const& config,
        float& alpha_cutoff, bool& opaque_alpha)
    {
        RE::NiAlphaProperty const* const property = geom_rt.alphaProperty.get();
        bool const testing = property && property->GetAlphaTesting();
        bool const blending = property && property->GetAlphaBlending();
        alpha_cutoff = testing ? static_cast<float>(config.alpha_test_threshold) : 0.0f;
        opaque_alpha = !blending;
    }

    void collect_static(RE::BSGeometry* geom, RE::BSGeometry::GEOMETRY_RUNTIME_DATA const& geom_rt, PanelWalkContext& context)
    {
        // Same hold as the skinned path: the geometry (and through it the property and textures)
        // must outlive every read in this function.
        RE::NiPointer<RE::BSGeometry> const keep_alive(geom);
        ++context.static_seen;
        // ---- Geometry-level GPU buffer check specific to the static path. The skinned path does not
        // pass this gate. ----
        if (!geom_rt.rendererData || !geom_rt.rendererData->vertexBuffer || !geom_rt.rendererData->indexBuffer)
        {
            ++context.st_nogpu;
            logger::debug("Panel: skip geometry without GPU buffers");
            return;
        }

        // ---- Classification guard: these BSTriShape subclasses and data flags cannot be drawn as
        // ordinary static geometry - forcing a draw produces garbage triangles over the screen. ----
        char const* const rtti_name = geom->GetRTTI() ? geom->GetRTTI()->GetName() : "";
        char const* const node_name = geom->name.c_str();
        char const* exclusion_reason = nullptr;

        if (strcmp(rtti_name, "BSDynamicTriShape") == 0)
            exclusion_reason = "BSDynamicTriShape (dynamic vertex layout)";
        else if (strcmp(rtti_name, "BSInstanceTriShape") == 0)
            exclusion_reason = "BSInstanceTriShape (instanced, per-instance stream not bound)";
        else if (strcmp(rtti_name, "BSMultiStreamInstanceTriShape") == 0)
            exclusion_reason = "BSMultiStreamInstanceTriShape (multi-stream instanced, extra streams not bound)";
        else if (strcmp(rtti_name, "BSSubIndexTriShape") == 0)
            exclusion_reason = "BSSubIndexTriShape (decal sub-range index structure)";
        else if (geom_rt.vertexDesc.HasFlag(RE::BSGraphics::Vertex::VF_INSTANCEDATA))
            exclusion_reason = "vertexDesc VF_INSTANCEDATA";
        else if (geom_rt.vertexDesc.HasFlag(RE::BSGraphics::Vertex::VF_EYEDATA))
            exclusion_reason = "vertexDesc VF_EYEDATA";
        else if (geom_rt.vertexDesc.HasFlag(RE::BSGraphics::Vertex::VF_LANDDATA))
            exclusion_reason = "vertexDesc VF_LANDDATA";

        if (exclusion_reason)
        {
            ++context.st_excl;
            logger::debug("Panel: skip static geometry ({}) rtti={} node={}", exclusion_reason, rtti_name, node_name ? node_name : "?");
            return;
        }

        RE::BSTriShape* tri = geom->AsTriShape();
        if (!tri)
        {
            ++context.st_nottri;
            // Regular geometry in an SSE scene is always of the BSTriShape family; other types are not drawn
            logger::debug("Panel: skip non-BSTriShape geometry");
            return;
        }

        RE::BSTriShape::TRISHAPE_RUNTIME_DATA const& tri_rt = tri->GetTrishapeRuntimeData();
        if (tri_rt.vertexCount == 0 || tri_rt.triangleCount == 0)
        {
            ++context.st_empty;
            logger::debug("Panel: skip empty geometry (vertices={} tris={}) rtti={} node={}",
                    tri_rt.vertexCount, tri_rt.triangleCount, rtti_name, node_name ? node_name : "?");
            return;
        }

        RE::NiBound const& model_bound = geom->GetModelData().modelBound;
        if (model_bound.radius <= 0.0f)
        {
            ++context.st_bound;
            logger::debug("Panel: skip geometry with invalid model bound rtti={} node={}", rtti_name, node_name ? node_name : "?");
            return;
        }

        uint32_t const vertex_stride = vertex_size_of(geom_rt.vertexDesc);

        // ---- World bounding-sphere validation: modelBound extrapolated through the world transform,
        // the largest column norm of the 3x3 block as the anisotropic scale upper bound; a
        // non-finite/degenerate/over-budget sphere means effect-type or anomalous data. ----
        RE::NiTransform const& world_transform = geom->world;
        DirectX::XMFLOAT4X4 world{};
        DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixIdentity());
        for (int row = 0; row < 3; ++row)
        {
            for (int col = 0; col < 3; ++col)
                world.m[row][col] = world_transform.rotate.entry[row][col] * world_transform.scale;
            world.m[row][3] = world_transform.translate[row];
        }
        float scale_max = 0.0f;
        for (int col = 0; col < 3; ++col)
        {
            float const dx = world.m[0][col];
            float const dy = world.m[1][col];
            float const dz = world.m[2][col];
            scale_max = std::max(scale_max, std::sqrt(dx * dx + dy * dy + dz * dz));
        }
        RE::NiPoint3 const world_center = transform_point(world, model_bound.center);
        float const world_radius = model_bound.radius * scale_max;
        if (world_radius <= 0.0f || world_radius > Max_Part_World_Radius)
        {
            ++context.st_worldr;
            logger::debug("Panel: skip static draw [world radius out of range] target={:08X} node={} model_bound r={:.1f} world_radius={:.1f} cap={:.1f}",
                    context.form_id, node_name, model_bound.radius, world_radius, Max_Part_World_Radius);
            return;
        }

        // ---- Position format calibration: VF_FULLPREC is not assumed; rawVertexData is measured and
        // compared against modelBound to select the format+offset, cached per descriptor (zero
        // decoding in steady state); no candidate matches -> skip this draw (better to draw too
        // little than to smear garbage over the screen); cannot calibrate -> fall back to descriptor
        // derivation. ----
        PositionCalibration const calibration = calibrate_position_format(
            geom_rt.vertexDesc, geom_rt.rendererData, tri_rt.vertexCount, model_bound, vertex_stride);
        if (calibration.state == PositionCalibrationState::e_unresolved)
        {
            ++context.st_calib;
            return;
        }

        // ---- Ownership check: only geometry "at the reference" is drawn (engine world bounding
        // sphere, looser than a box test). The per-mesh model AABB only feeds calibration scoring and
        // never enters the cache or this test (to prevent cross-mesh pollution). ----
        float const ref_distance = distance_to_point(context.position, world_center);
        float const proximity_limit = world_radius + Ref_Proximity_Slack;
        if (ref_distance > proximity_limit)
        {
            logger::debug("Panel: skip static draw [geometry not at the reference] target={:08X} node={} distance={:.1f} limit={:.1f} ref=({:.1f},{:.1f},{:.1f}) world_center=({:.1f},{:.1f},{:.1f}) world_radius={:.1f}",
                    context.form_id,
                    node_name,
                    ref_distance,
                    proximity_limit,
                    context.position.x,
                    context.position.y,
                    context.position.z,
                    world_center.x,
                    world_center.y,
                    world_center.z,
                    world_radius);
            ++context.st_notref;
            return;
        }

        PanelDraw draw{};
        // Hold the geometry BEFORE anything reads its property/textures: another thread may be
        // destroying this transient mesh (the blood decals churn constantly), and every read
        // below - GPU buffers, shader property, diffuse texture - needs the engine-side reference
        // taken first. The D3D objects themselves are resolved at draw time from renderer_data.
        draw.node.reset(geom);
        draw.vertex_buffer = geom_rt.rendererData->vertexBuffer;
        draw.index_buffer = geom_rt.rendererData->indexBuffer;
        if (draw.vertex_buffer)
            draw.vertex_buffer->AddRef();
        if (draw.index_buffer)
            draw.index_buffer->AddRef();
        draw.vertex_desc = geom_rt.vertexDesc;
        draw.vertex_stride = vertex_stride;
        draw.vertex_count = tri_rt.vertexCount;
        draw.triangle_count = tri_rt.triangleCount;
        draw.index_count = static_cast<uint32_t>(tri_rt.triangleCount) * 3u;
        draw.position_format = calibration.format;
        draw.position_offset = calibration.offset;
        draw.uv_format = uv_format_of(geom_rt.vertexDesc);
        PanelMaterial const material = resolve_material(geom_rt.shaderProperty.get(), geom_rt.vertexDesc, context.npc);
        apply_material(draw, material);
        resolve_alpha_handling(geom_rt, context.config, draw.alpha_cutoff, draw.opaque_alpha);
        // The static meshes are the control group: they are the ones whose texture and index space
        // the panel gets right, so their census line is what the skinned ones are compared against.
        log_mesh_census(node_name, "static", -1, draw.vertex_count, draw.triangle_count, 0u, "n/a",
            false, 0u, 0u, geom_rt.vertexDesc, draw, material, geom_rt.alphaProperty.get());
        ++context.static_ok;
        context.draws->push_back(std::move(draw));
    }

    // One-shot binary dump of a positionless partition's raw sources, written beside the log the
    // first time one is collected. Answers from the bytes what inference cannot: whether the
    // partition's GPU buffer and the dynamic-morph data actually agree with the descriptor's
    // offsets, what the vertexMap holds, and how the triangle list indexes the vertex space. The
    // dump is small (first 256 vertices) and bounded to this one diagnostic.
    void dump_positionless_partition(
        RE::BSGraphics::TriShape const& buff, RE::NiSkinPartition::Partition const& part,
        void const* dynamic_positions, uint32_t dynamic_vertex_capacity,
        uint32_t partition_stride, uint32_t palette_count, char const* node_name, uint32_t p)
    {
        // Several partitions, not one: the head and the body are the same family of mesh on paper and
        // they do not render alike, so the bytes have to be comparable across them. The vertices the
        // triangle list actually reaches are dumped, not the first part.vertices of the buffer, which
        // is what makes the index space answerable.
        static uint32_t s_dumps = 0;
        if (s_dumps >= 6)
            return;
        uint32_t const ordinal = s_dumps++;

        std::optional<std::filesystem::path> directory = logger::log_directory();
        if (!directory)
            return;

        std::string label = node_name ? node_name : "unknown";
        for (char& c : label)
        {
            if (!std::isalnum(static_cast<unsigned char>(c)))
                c = '_';
        }
        std::filesystem::path const path = *directory /
            ("PlayerPanel_partition_" + std::to_string(ordinal) + "_" + label + "_p" + std::to_string(p) + ".bin");
        std::ofstream file(path, std::ios::binary);
        if (!file)
            return;

        auto const write_raw = [&file](void const* data, std::size_t bytes) {
            file.write(static_cast<char const*>(data), static_cast<std::streamsize>(bytes));
        };

        uint32_t max_index = 0;
        for (uint32_t t = 0; t < static_cast<uint32_t>(part.triangles) * 3u && part.triList; ++t)
            max_index = (std::max)(max_index, static_cast<uint32_t>(part.triList[t]));

        uint64_t desc_raw = 0;
        std::memcpy(&desc_raw, &buff.vertexDesc, sizeof(desc_raw));
        uint32_t const desc_u32[2] = { partition_stride, part.vertices };
        uint32_t const vertex_dump_count =
            (std::min<uint32_t>)((std::max<uint32_t>)(part.vertices, max_index + 1u), 4096u);
        uint32_t const meta[8] = { part.triangles, part.numBones, palette_count,
            dynamic_vertex_capacity, static_cast<uint32_t>(buff.rawVertexData != nullptr),
            static_cast<uint32_t>(part.vertexMap != nullptr), max_index, vertex_dump_count };
        write_raw(&desc_raw, sizeof(desc_raw));
        write_raw(desc_u32, sizeof(desc_u32));
        write_raw(meta, sizeof(meta));

        // The partition's GPU buffer, verbatim, for every vertex the triangle list can reach.
        if (buff.rawVertexData)
            write_raw(buff.rawVertexData, static_cast<std::size_t>(partition_stride) * vertex_dump_count);

        // The partition's triangle list, verbatim (up to 4096 indices).
        uint32_t const tri_dump_count = (std::min<uint32_t>)(static_cast<uint32_t>(part.triangles) * 3u, 4096u);
        if (part.triList)
            write_raw(part.triList, static_cast<std::size_t>(tri_dump_count) * sizeof(uint16_t));

        // The vertex map, verbatim.
        if (part.vertexMap)
            write_raw(part.vertexMap, static_cast<std::size_t>(part.vertices) * sizeof(uint16_t));

        // The partition's bone list, verbatim (diagnostic only, it takes part in no decision).
        if (part.bones)
            write_raw(part.bones, static_cast<std::size_t>(part.numBones) * sizeof(uint16_t));

        // The dynamic morph data for the first 1024 ORIGINAL vertices.
        uint32_t const dynamic_dump_count = (std::min<uint32_t>)(dynamic_vertex_capacity, 1024u);
        write_raw(dynamic_positions, static_cast<std::size_t>(dynamic_dump_count) * Dynamic_Position_Stride);

        logger::info("Panel: dumped positionless partition {} p={} (verts={} tris={} stride={} map={} raw={} tri_max={} dumped={} dyn={}) to {}",
            node_name ? node_name : "?", p, part.vertices, part.triangles, partition_stride,
            part.vertexMap != nullptr, buff.rawVertexData != nullptr, max_index, vertex_dump_count,
            dynamic_vertex_capacity, path.string());
    }

    void collect_skinned(RE::BSGeometry* geom, RE::BSGeometry::GEOMETRY_RUNTIME_DATA const& geom_rt, PanelWalkContext& context)
    {
        // Hold the geometry for the whole collection walk of this mesh: another thread may be
        // destroying it (transient decals, physics-skinned meshes) and every read below - skin
        // instance, partitions, GPU buffers, shader property, diffuse texture - needs it alive.
        RE::NiPointer<RE::BSGeometry> const keep_alive(geom);
        ++context.skinned_seen;
        char const* const node_name = geom->name.c_str();
        char const* const rtti_name = geom->GetRTTI() ? geom->GetRTTI()->GetName() : "?";

        RE::NiSkinInstance* skin = geom_rt.skinInstance.get();
        RE::NiSkinPartition* skin_partition = skin ? skin->skinPartition.get() : nullptr;
        if (!skin || !skin_partition || !skin->skinData || !skin->skinData->GetBoneData() || !skin->boneWorldTransforms || !skin->bones)
        {
            std::string missing;
            auto const note = [&missing](bool null, char const* field)
            {
                if (null)
                {
                    if (!missing.empty())
                        missing += ", ";
                    missing += field;
                }
            };
            note(!skin, "skinInstance");
            note(skin && !skin_partition, "skinPartition");
            note(skin && !skin->skinData, "skinData");
            note(skin && skin->skinData && !skin->skinData->GetBoneData(), "skinData->GetBoneData()");
            note(skin && !skin->boneWorldTransforms, "boneWorldTransforms");
            note(skin && !skin->bones, "bones");
            ++context.sk_incomplete;
            logger::warn("Panel: skip skinned draw [skin instance incomplete] node={} missing=[{}] rtti={}",
                node_name, missing, rtti_name ? rtti_name : "?");
            return;
        }

        uint32_t const partition_count = std::min(skin_partition->numPartitions, static_cast<uint32_t>(skin_partition->partitions.size()));
        if (partition_count < skin_partition->numPartitions)
        {
            logger::debug("Panel: skip skinned draw [partition count exceeds array size] node={} numPartitions={} partitions.size()={} extra partitions ignored",
                node_name, skin_partition->numPartitions, skin_partition->partitions.size());
        }
        if (partition_count == 0)
        {
            ++context.p0;
            logger::debug("Panel: skip skinned draw [no skin partitions] node={} numPartitions={} partitions.size()={}",
                node_name, skin_partition->numPartitions, skin_partition->partitions.size());
            return;
        }

        RE::NiAVObject* const root_parent = skin->rootParent;
        if (!root_parent)
        {
            ++context.no_root;
            logger::warn("Panel: skip skinned draw [skin instance has no rootParent] node={} rtti={} partitions={}",
                node_name, rtti_name ? rtti_name : "?", partition_count);
            return;
        }

        // ---- modelBound is unusable for engine-managed skinned meshes (measured: every
        // body/equipment/hair mesh has r=0.0), so it must not gate the draw; the worldBound serves
        // as a sanity gate only when the engine provides one (radius > 0). ----
        RE::NiBound const& world_bound = geom->worldBound;
        if (world_bound.radius > Max_Part_World_Radius)
        {
            ++context.wbound;
            logger::warn("Panel: skip skinned draw [world bound out of range] node={} world_bound=({:.1f},{:.1f},{:.1f}) r={:.1f} cap={:.1f}",
                node_name, world_bound.center.x, world_bound.center.y, world_bound.center.z, world_bound.radius, Max_Part_World_Radius);
            return;
        }

        // ---- Position source for positionless partitions (FaceGen/morph dynamic meshes - the head
        // and the morphed body): the partition buffers carry no positions; model-space positions live
        // in BSDynamicTriShape::dynamicData, one float4 per ORIGINAL vertex. Resolved once per
        // geometry, rebuilt per partition through its vertexMap at collection time. ----
        void const* dynamic_positions = nullptr;
        uint32_t dynamic_vertex_capacity = 0;
        if (RE::BSDynamicTriShape* dyn = geom->AsDynamicTriShape())
        {
            RE::BSDynamicTriShape::DYNAMIC_TRISHAPE_RUNTIME_DATA const& dyn_rt = dyn->GetDynamicTrishapeRuntimeData();
            if (dyn_rt.dynamicData && dyn_rt.dataSize >= Dynamic_Position_Stride)
            {
                dynamic_positions = dyn_rt.dynamicData;
                dynamic_vertex_capacity = dyn_rt.dataSize / Dynamic_Position_Stride;
            }
        }

        for (uint32_t p = 0; p < partition_count; ++p)
        {
            RE::NiSkinPartition::Partition const& part = skin_partition->partitions[p];
            RE::BSGraphics::TriShape* const buff = part.buffData;

            // The partition is rejected - name the exact condition plus the partition ordinal and vertices/triangles
            char const* reject = nullptr;
            if (!buff)
                reject = "partition buffData missing";
            else if (!buff->vertexBuffer)
                reject = "partition vertex buffer missing";
            else if (!buff->indexBuffer)
                reject = "partition index buffer missing";
            else if (part.triangles == 0)
                reject = "partition has no triangles";
            else if (!part.triList)
                reject = "partition triList missing";
            if (reject)
            {
                ++context.part_gate;
                logger::debug("Panel: skip skinned draw [{}] node={} partition={} vertices={} triangles={}",
                    reject, node_name, p, part.vertices, part.triangles);
                continue;
            }

            if (part.strips != 0)
            {
                // A strip partition (stripLengths index layout) cannot be drawn as a triangle list - skip to avoid errors
                ++context.strips;
                logger::debug("Panel: skip skinned draw [partition is a triangle strip] node={} partition={} strips={} vertices={} triangles={}",
                    node_name, p, part.strips, part.vertices, part.triangles);
                continue;
            }

            // ---- The palette is built in global bone index space (a vertex index is a subscript into
            // the skin bone array). Its valid length is palette = min(GetBoneCount(), numMatrices).
            // part.bones/numBones feed diagnostic logs only and take part in no decision. ----
            uint32_t const palette_count = std::min(skin->skinData->GetBoneCount(), skin->numMatrices);
            if (palette_count == 0 || palette_count > Max_Palette_Bones)
            {
                ++context.palette;
                if (!context.first_palette_captured)
                {
                    context.first_palette_captured = true;
                    context.first_palette_bone_count = skin->skinData->GetBoneCount();
                    context.first_palette_num_matrices = skin->numMatrices;
                    context.first_palette_node = node_name ? node_name : "?";
                }
                logger::debug("Panel: skip skinned draw [palette slot count out of range] node={} partition={} palette={} budget={}",
                    node_name, p, palette_count, Max_Palette_Bones);
                continue;
            }

            // ---- Positionless partition (partition desc without VF_VERTEX, FaceGen/morph dynamic
            // meshes: head, body): the partition buffer holds only UV/normal/tangent and the
            // contiguous SKINNING block (weights f16x4 at the skin offset, indices u8x4 right
            // after). Positions are rebuilt from dynamicData through the partition's vertexMap
            // (partition-local -> original index; null = identity) into a CPU float4 stream, which
            // the render thread uploads as vertex stream 1 - keeping the index space of the position
            // stream, the partition vertex buffer and the partition index buffer consistent. ----
            if (!buff->vertexDesc.HasFlag(RE::BSGraphics::Vertex::VF_VERTEX))
            {
                if (!dynamic_positions)
                {
                    logger::warn("Panel: skip skinned draw [positionless partition but no dynamic position data] node={} partition={} rtti={}",
                        node_name, p, rtti_name ? rtti_name : "?");
                    continue;
                }

                uint32_t const partition_stride = vertex_size_of(buff->vertexDesc);
                uint32_t const skin_offset = buff->vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_SKINNING);
                if (partition_stride == 0 || skin_offset + Dynamic_Skin_Block_Bytes > partition_stride)
                {
                    logger::warn("Panel: skip skinned draw [skin block does not fit the positionless partition layout] node={} partition={} skin_offset={} stride={}",
                        node_name, p, skin_offset, partition_stride);
                    continue;
                }

                uint8_t const* const raw = buff->rawVertexData;
                if (!raw)
                {
                    logger::warn("Panel: skip skinned draw [raw vertex data missing for positionless partition validation] node={} partition={}",
                        node_name, p);
                    continue;
                }

                // Weights/indices validation on the partition's own data (positions are not in this
                // buffer); the layout inside the SKINNING block is fixed by construction.
                SkinningLayoutSpec const& spec = *find_skinning_layout(Dynamic_Skin_Layout_Id);
                SkinnedMeshStats stats{ .position_finite = true };
                SkinnedMeshVerdict const verdict = validate_skinned_mesh(
                    raw, partition_stride, 0u, 0u, spec,
                    skin_offset + spec.weight_delta, skin_offset + spec.index_delta,
                    part.vertices, palette_count, stats);
                if (verdict == SkinnedMeshVerdict::e_weights_bad)
                {
                    logger::warn("Panel: skip skinned draw [positionless partition skin validation failed] node={} partition={} vertices={} stride={} skin_offset={} wsum=[{:.3f},{:.3f}] wbad={} imax={}/{}",
                        node_name, p, part.vertices, partition_stride, skin_offset,
                        stats.weight_sum_min, stats.weight_sum_max, stats.bad_weight_vertices, stats.index_max, palette_count);
                    continue;
                }
                if (stats.out_of_range_weighted_count > 0)
                {
                    logger::warn("Panel: skip skinned draw [bone index exceeds palette bounds with non-zero weight] node={} partition={} palette={} oob_weighted={} first_oob_index={} first_oob_weight={:.4f}",
                        node_name, p, palette_count, stats.out_of_range_weighted_count,
                        stats.first_out_of_range_index, stats.first_out_of_range_weight);
                    continue;
                }

                // ---- Index space: reverted to the measured heuristic while the binary dump
                // decides the real rule. The two prior rules are each refuted by one symptom: a
                // non-null vertexMap followed unconditionally blanked the FACE (face partitions
                // carry a map but keep whole-mesh indexing, so remapping scrambled them), and the
                // max_index test ignoring the map left the ARMS/HAIR as camouflage. The dump (one
                // per session, beside the log) records the partition's raw GPU bytes, triangle
                // list, vertex map and morph data, so the layout is decided from evidence. ----
                float const* const positions = static_cast<float const*>(dynamic_positions);
                uint32_t max_index = 0;
                for (uint32_t t = 0; t < static_cast<uint32_t>(part.triangles) * 3u; ++t)
                    max_index = std::max(max_index, static_cast<uint32_t>(part.triList[t]));
                bool const original_indexing = max_index >= part.vertices;

                uint32_t const reachable_count = original_indexing ? max_index + 1u :
                    (part.vertexMap ? part.vertices : std::min<uint32_t>(part.vertices, dynamic_vertex_capacity));
                uint16_t const* const vertex_map = original_indexing ? nullptr : part.vertexMap;
                if (reachable_count > dynamic_vertex_capacity)
                {
                    logger::warn("Panel: skip skinned draw [dynamic position index out of bounds] node={} partition={} reachable={} capacity={} max_index={}",
                        node_name, p, reachable_count, dynamic_vertex_capacity, max_index);
                    continue;
                }

                // Every position this draw can reach must be finite (sampled, not walked: the body
                // alone is five-digit vertex counts and the check runs per collection).
                constexpr uint32_t Dynamic_Sample_Limit = 256;
                uint32_t const sample_step = (reachable_count > Dynamic_Sample_Limit) ?
                    (reachable_count + Dynamic_Sample_Limit - 1) / Dynamic_Sample_Limit : 1;
                bool positions_usable = true;
                for (uint32_t v = 0; v < reachable_count; v += sample_step)
                {
                    float const* const src_pos = positions + static_cast<size_t>(v) * Dynamic_Position_Components;
                    if (!std::isfinite(src_pos[0]) || !std::isfinite(src_pos[1]) || !std::isfinite(src_pos[2]))
                    {
                        logger::warn("Panel: skip skinned draw [dynamic positions non-finite] node={} partition={} vertex={}",
                            node_name, p, v);
                        positions_usable = false;
                        break;
                    }
                }
                if (positions_usable && vertex_map)
                {
                    for (uint32_t v = 0; v < part.vertices; ++v)
                    {
                        if (static_cast<uint32_t>(vertex_map[v]) >= dynamic_vertex_capacity)
                        {
                            logger::warn("Panel: skip skinned draw [dynamic position index out of bounds] node={} partition={} vertex={} original={} capacity={}",
                                node_name, p, v, vertex_map[v], dynamic_vertex_capacity);
                            positions_usable = false;
                            break;
                        }
                    }
                }
                if (!positions_usable)
                    continue;

                dump_positionless_partition(*buff, part, dynamic_positions, dynamic_vertex_capacity,
                    partition_stride, palette_count, node_name, p);

                // ---- Bake the float4 stream: identity copy over the reachable range, then the
                // vertexMap overwrite for packed partitions. The fourth component stays at one (the
                // float3 POSITION fetch never reads it). ----
                auto stream = std::make_shared<std::vector<float>>(static_cast<size_t>(reachable_count) * Dynamic_Position_Components);
                for (uint32_t v = 0; v < reachable_count; ++v)
                {
                    float const* const src = positions + static_cast<size_t>(v) * Dynamic_Position_Components;
                    float* const dst = stream->data() + static_cast<size_t>(v) * Dynamic_Position_Components;
                    dst[0] = src[0];
                    dst[1] = src[1];
                    dst[2] = src[2];
                    dst[3] = 1.0f;
                }
                if (vertex_map)
                {
                    for (uint32_t v = 0; v < part.vertices; ++v)
                    {
                        float const* const src = positions + static_cast<size_t>(vertex_map[v]) * Dynamic_Position_Components;
                        float* const dst = stream->data() + static_cast<size_t>(v) * Dynamic_Position_Components;
                        dst[0] = src[0];
                        dst[1] = src[1];
                        dst[2] = src[2];
                    }
                }

                PanelDraw draw{};
                draw.skin = geom_rt.skinInstance;
                draw.partition = p;
                draw.node.reset(geom);  // keeps the geometry (and its dynamicData) alive
                draw.vertex_buffer = buff->vertexBuffer;
                draw.index_buffer = buff->indexBuffer;
                if (draw.vertex_buffer)
                    draw.vertex_buffer->AddRef();
                if (draw.index_buffer)
                    draw.index_buffer->AddRef();
                draw.vertex_desc = buff->vertexDesc;
                draw.vertex_stride = partition_stride;
                draw.vertex_count = part.vertices;
                draw.triangle_count = part.triangles;
                draw.index_count = static_cast<uint32_t>(part.triangles) * 3u;
                draw.position_format = REX::W32::DXGI_FORMAT_R32G32B32_FLOAT;
                draw.position_offset = 0u;
                draw.uv_format = uv_format_of(buff->vertexDesc);
                draw.skin_layout = PanelSkinLayout{ spec.weight_format, skin_offset + spec.weight_delta, spec.index_format, skin_offset + spec.index_delta };
                draw.partition_bones = part.bones;
                draw.partition_bone_count = part.numBones;
                draw.position_stream = std::move(stream);
                PanelMaterial const material = resolve_material(geom_rt.shaderProperty.get(), buff->vertexDesc, context.npc);
                apply_material(draw, material);
                resolve_alpha_handling(geom_rt, context.config, draw.alpha_cutoff, draw.opaque_alpha);
                log_mesh_census(node_name, "skinned", static_cast<int32_t>(p), part.vertices, part.triangles,
                    max_index, original_indexing ? "whole-mesh" : "packed", part.vertexMap != nullptr,
                    dynamic_vertex_capacity, palette_count, buff->vertexDesc, draw, material, geom_rt.alphaProperty.get());
                ++context.skinned_ok;
                context.draws->push_back(std::move(draw));
                continue;
            }

            // ---- The position format is determined by the attribute offset spacing; the stride and
            // the SKINNING (weight/index) layout are self-calibrated from the mesh's own vertex data;
            // with no solution the draw is skipped (it must not fall back to an UNKNOWN position
            // format or a hard-coded skinning order). ----
            SkinnedVertexLayout calibration = calibrate_skinned_layout(buff->vertexDesc, buff, part.vertices, palette_count);
            if (calibration.state != SkinnedCalibrationState::e_measured)
            {
                ++context.calib;
                continue;
            }

            // ---- Per-mesh validation runs on this mesh (no verdict reused from another mesh, full
            // vertex traversal); indices are global with palette as the bound. Position failure ->
            // skip; weights invalid -> re-enumerate candidates (without writing the cache);
            // out-of-range index: non-zero weight -> skip, zero weight -> drawn (the replica fill
            // makes it a no-op). ----
            uint8_t const* const raw = buff->rawVertexData;
            uint32_t const pos_bytes = (calibration.position_format == REX::W32::DXGI_FORMAT_R32G32B32_FLOAT) ? 12u : 8u;
            SkinningLayoutSpec const* spec = find_skinning_layout(calibration.layout_id);

            SkinnedMeshStats stats{ .position_finite = true };
            SkinnedMeshVerdict verdict = validate_skinned_mesh(
                raw, calibration.stride, calibration.position_offset, pos_bytes, *spec,
                calibration.skin.weight_offset, calibration.skin.index_offset,
                part.vertices, palette_count, stats);

            if (verdict == SkinnedMeshVerdict::e_position_bad)
            {
                ++context.vpos;
                logger::warn("Panel: skip skinned draw [mesh positions non-finite] node={} partition={} vertices={} stride={} pos(fmt={:#06x},off={}",
                    node_name, p, part.vertices, calibration.stride, static_cast<unsigned>(calibration.position_format), calibration.position_offset);
                continue;
            }
            if (verdict == SkinnedMeshVerdict::e_weights_bad)
            {
                // The cached layout does not fit this mesh: re-enumerate candidates on this mesh's data (without writing the cache)
                SkinnedLayoutCandidateResult candidates[Max_Skinned_Layout_Candidates]{};
                PanelSkinLayout candidate_skin[Max_Skinned_Layout_Candidates]{};
                uint32_t const skin_offset = buff->vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_SKINNING);
                size_t const candidate_count = enumerate_skinned_candidates(
                    buff->vertexDesc, buff, part.vertices, palette_count, calibration.position_offset, pos_bytes,
                    skin_offset, candidates, candidate_skin);
                int64_t switch_to = -1;
                for (size_t i = 0; i < candidate_count; ++i)
                {
                    if (candidates[i].passed)
                    {
                        switch_to = static_cast<int64_t>(i);
                        break;
                    }
                }
                if (switch_to < 0)
                {
                    ++context.vweights;
                    logger::warn("Panel: skip skinned draw [no skinned layout fits this mesh] node={} partition={} vertices={} palette={} cached_layout={} candidates:{}",
                        node_name, p, part.vertices, palette_count, static_cast<unsigned>(calibration.layout_id),
                        format_skinned_candidate_table(candidates, candidate_skin, candidate_count, palette_count));
                    continue;
                }
                calibration.stride = candidates[switch_to].stride;
                calibration.layout_id = candidates[switch_to].layout;
                calibration.skin = candidate_skin[switch_to];
                spec = find_skinning_layout(calibration.layout_id);
                verdict = validate_skinned_mesh(
                    raw, calibration.stride, calibration.position_offset, pos_bytes, *spec,
                    calibration.skin.weight_offset, calibration.skin.index_offset,
                    part.vertices, palette_count, stats);
                if (verdict != SkinnedMeshVerdict::e_ok)
                {
                    logger::warn("Panel: skip skinned draw [switched layout still fails mesh validation] node={} partition={} vertices={} palette={} stride={} layout={}",
                        node_name, p, part.vertices, palette_count, calibration.stride, static_cast<unsigned>(calibration.layout_id));
                    continue;
                }
            }

            // index >= palette with a non-zero weight -> it cannot render correctly, skip + WARN (the
            // replica fill is a no-op for zero-weight slots, which are only counted in the stats).
            if (stats.out_of_range_weighted_count > 0)
            {
                ++context.oob;
                logger::warn("Panel: skip skinned draw [bone index exceeds palette bounds with non-zero weight] node={} partition={} palette={} oob_weighted={} index_max={} first_oob_index={} first_oob_weight={:.4f}",
                    node_name, p, palette_count, stats.out_of_range_weighted_count, stats.index_max,
                    stats.first_out_of_range_index, stats.first_out_of_range_weight);
                continue;
            }

            PanelDraw draw{};
            draw.skin = geom_rt.skinInstance;
            draw.partition = p;
            draw.node.reset(geom);  // keep the geometry alive
            draw.vertex_buffer = buff->vertexBuffer;
            draw.index_buffer = buff->indexBuffer;
            if (draw.vertex_buffer)
                draw.vertex_buffer->AddRef();
            if (draw.index_buffer)
                draw.index_buffer->AddRef();
            draw.vertex_desc = buff->vertexDesc;
            draw.vertex_stride = calibration.stride;
            draw.vertex_count = part.vertices;
            draw.triangle_count = part.triangles;
            draw.index_count = static_cast<uint32_t>(part.triangles) * 3u;
            draw.position_format = calibration.position_format;
            draw.position_offset = calibration.position_offset;
            draw.uv_format = uv_format_of(buff->vertexDesc);
            draw.skin_layout = calibration.skin;
            draw.partition_bones = part.bones;
            draw.partition_bone_count = part.numBones;
            PanelMaterial const material = resolve_material(geom_rt.shaderProperty.get(), buff->vertexDesc, context.npc);
            apply_material(draw, material);
            resolve_alpha_handling(geom_rt, context.config, draw.alpha_cutoff, draw.opaque_alpha);
            log_mesh_census(node_name, "skinned", static_cast<int32_t>(p), part.vertices, part.triangles,
                0u, "n/a", part.vertexMap != nullptr, 0u, palette_count, buff->vertexDesc, draw, material, geom_rt.alphaProperty.get());
            ++context.skinned_ok;
            context.draws->push_back(std::move(draw));
        }
    }

    void collect_geometry(RE::BSGeometry* geom, PanelWalkContext& context)
    {
        ++context.visited;
        switch (geom->GetType().get())
        {
        case RE::BSGeometry::Type::kParticles:
        case RE::BSGeometry::Type::kStripParticles:
        case RE::BSGeometry::Type::kParticleShaderDynamicTriShape:
        case RE::BSGeometry::Type::kLines:
        case RE::BSGeometry::Type::kDynamicLines:
        case RE::BSGeometry::Type::kInstanceGroup:
            logger::debug("Panel: skip particle/line geometry (type {})", static_cast<int>(geom->GetType().get()));
            return;
        default:
            break;
        }

        // Transient weapon decals ("BloodLighting" and friends) are created and destroyed on the
        // engine's threads without notice; reading their shader property or texture raced with
        // that destruction three sessions in a row (the AddRef-on-garbage-SRV crashes). They are
        // additive blood splats on weapons with no place in a static portrait, so they are skipped
        // wholesale. The name check runs before any property read.
        {
            char const* const geom_name = geom->name.c_str();
            if (geom_name && std::strstr(geom_name, "Blood") != nullptr)
            {
                logger::debug("Panel: skip transient blood decal node=\"{}\"", geom_name);
                return;
            }
        }

        RE::BSGeometry::GEOMETRY_RUNTIME_DATA const& geom_rt = geom->GetGeometryRuntimeData();

        if (geom_rt.shaderProperty && geom_rt.shaderProperty->GetRTTI() &&
            strcmp(geom_rt.shaderProperty->GetRTTI()->GetName(), "BSEffectShaderProperty") == 0)
        {
            logger::debug("Panel: skip effect-shader geometry (BSEffectShaderProperty, the fx attachment is not part of the character) node=\"{}\"",
                    geom->name.c_str() ? geom->name.c_str() : "?");
            return;
        }

        if (geom_rt.skinInstance)
            collect_skinned(geom, geom_rt, context);
        else
            collect_static(geom, geom_rt, context);
    }
}

void collect_panel_geometry(RE::TESObjectREFR& ref, Config const& config, std::vector<PanelDraw>& draws)
{
    draws.clear();

    RE::NiAVObject* const root = ref.GetCurrent3D();
    if (!root)
        return;

    // The walk state lives on the stack and is captured by reference, so the traversal's callable
    // stays small enough not to allocate.
    RE::TESNPC* const walk_npc = ref.GetBaseObject() ? ref.GetBaseObject()->As<RE::TESNPC>() : nullptr;
    PanelWalkContext context{ .position = ref.GetPosition(), .form_id = ref.GetFormID(), .draws = &draws,
        .config = config, .npc = walk_npc };

    RE::BSVisit::TraverseScenegraphGeometries(root, [&context](RE::BSGeometry* geometry) {
        if (context.draws->size() >= Max_Draws_Per_Frame)
        {
            logger::warn("Panel: draw cap {} reached, extra geometry dropped", Max_Draws_Per_Frame);
            return RE::BSVisit::BSVisitControl::kStop;
        }
        collect_geometry(geometry, context);
        return RE::BSVisit::BSVisitControl::kContinue;
    });

    // One-shot funnel census: the skinned and static paths reject meshes at several debug-logged
    // gates that are invisible at the info level, so a missing character is otherwise undiagnosable.
    // Logged once per process, the first time skinned meshes were seen but none survived.
    static bool s_census_logged = false;
    if (!s_census_logged && context.skinned_seen > 0 && context.skinned_ok == 0)
    {
        s_census_logged = true;
        logger::warn("Panel skinned census: visited={} skinned={} static={} ok(skinned={} static={}) | skinned reject: incomplete={} p0={} noroot={} wbound={} part={} strips={} palette={} calib={} vpos={} vweights={} oob={} | static reject: nogpu={} excl={} nottri={} empty={} bound={} worldr={} calib={} notref={} | first palette reject: node=\"{}\" skinBoneCount={} skinNumMatrices={} budget={}",
            context.visited, context.skinned_seen, context.static_seen, context.skinned_ok, context.static_ok,
            context.sk_incomplete, context.p0, context.no_root, context.wbound, context.part_gate,
            context.strips, context.palette, context.calib, context.vpos, context.vweights, context.oob,
            context.st_nogpu, context.st_excl, context.st_nottri, context.st_empty, context.st_bound,
            context.st_worldr, context.st_calib, context.st_notref,
            context.first_palette_node, context.first_palette_bone_count, context.first_palette_num_matrices,
            Max_Palette_Bones);
    }
}

PLUGIN_NAMESPACE_END
