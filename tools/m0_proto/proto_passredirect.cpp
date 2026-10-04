//
// Created by AmazingBuff on 2026/09/29.
//

#include "proto.h"
#include "proto_pinstance.h"

#include <RE/Skyrim.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>
#include <RE/B/BSLight.h>
#include <RE/B/BSShaderManager.h>
#include <RE/N/NiDirectionalLight.h>
#include <RE/N/NiPointLight.h>
#include <RE/R/RendererShadowState.h>
#include <RE/S/ShadowSceneNode.h>
#include <REX/W32/D3D11.h>

#include <Windows.h>
#include <d3dcompiler.h>
#include <detours/detours.h>
#include <fmt/format.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace std::literals;
namespace logger = SKSE::log;

// M0 panel prototype v3: persistent studio redirection. The pass-level
// evidence chain is closed (runs 9-16: menu passes are identifiable with zero
// false positives, hook-shared with Community Shaders, and re-drawn 1:1 into
// a private target with correct shading and intra-item occlusion), so v3
// turns the F6 one-shot into the M0 panel skeleton (handoff work packages
// 1+2): F6 toggles the panel, and while it is open EVERY DrawInterfaceStart
// frame is bracketed — menu-scene BSLightingShader passes are replayed into
// the persistent studio target after the original call, keeping the target
// current every menu frame. The target is created from the call site's own
// render-target description (self-recreating on resolution/format change),
// held for the panel's lifetime, and released on the render thread when the
// panel closes or the session invalidates (FR-06). F7 stays as a one-shot
// synchronous TGA readback for evidence; the steady-state path does no GPU
// readback (PRD 5.3). Logging is throttled for the persistent frame: menu
// passes are logged once per geometry per open, frame summaries only on
// state changes, plus a slow heartbeat.

namespace CharacterPanelProto
{
    namespace
    {
        using DrawInterfaceStart_t = void (*)(std::int64_t);
        using RenderPassImmediately_t = void (*)(RE::BSRenderPass*, std::uint32_t, bool, std::uint32_t);

        // The three RenderPassImmediately call sites Community Shaders and the
        // stage-0 spike hook: RelocationID + the AE call-instruction offsets
        // inside those functions. SetupAndDrawPass is RELOCATION_ID(100854,
        // 107644); the runtime is gated to AE 1.6.1170, so the AE column is
        // the live one.
        struct CallSite
        {
            std::uint64_t id_se;
            std::uint64_t id_ae;
            std::ptrdiff_t offset_se;
            std::ptrdiff_t offset_ae;
        };
        constexpr CallSite k_call_sites[3] = {
            { 100877, 107673, 0x1E5, 0x1EE },
            { 100852, 107642, 0x29E, 0x28F },
            { 100871, 107667, 0xEE, 0xED },
        };

        // Pre-patch call targets, parsed from the E8 rel32 before our
        // write_call. The three call sites are hook-sharing real estate:
        // Community Shaders' LightLimitFix patches the same instructions, and
        // run 11 (garbage shield textures while geometry stayed correct) is
        // consistent with our patch having bypassed its interposer — CS's
        // replaced shaders then draw with stale constant buffers. Both the
        // passthrough and the replay must run the true pre-patch target.
        std::uintptr_t s_original_targets[3]{};

        // v4.1: the render target the visible menu preview draws into (the
        // format-28 UI composite, runs 11-19) — captured AddRef'd during
        // replays, because the target bound at DrawInterfaceStart ENTRY
        // proved to be a different, non-visible intermediate (format 24,
        // run 20: the composite quad drew there and never showed). The
        // engine reallocates this pool target on resolution change, which
        // shows up as a pointer change and triggers a recapture.
        REX::W32::ID3D11RenderTargetView* s_panel_rtv = nullptr;

        void call_site_original(std::size_t site_index, RE::BSRenderPass* pass, std::uint32_t technique,
            bool alpha_test, std::uint32_t render_flags)
        {
            const std::uintptr_t target = s_original_targets[site_index];
            if (target)
                reinterpret_cast<RenderPassImmediately_t>(target)(pass, technique, alpha_test, render_flags);
        }

        // --- private offscreen target -------------------------------------

        // The engine's depth resources are usually created with a TYPELESS
        // format; a new texture with the same format cannot back a default-
        // desc depth view. Map typeless families to their typed depth
        // counterparts, keep typed depth formats, and fall back to D24S8 for
        // anything unexpected.
        REX::W32::DXGI_FORMAT normalize_depth_format(REX::W32::DXGI_FORMAT format)
        {
            switch (format)
            {
                case REX::W32::DXGI_FORMAT_R24G8_TYPELESS:
                    return REX::W32::DXGI_FORMAT_D24_UNORM_S8_UINT;
                case REX::W32::DXGI_FORMAT_R32G8X24_TYPELESS:
                    return REX::W32::DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
                case REX::W32::DXGI_FORMAT_R32_TYPELESS:
                    return REX::W32::DXGI_FORMAT_D32_FLOAT;
                case REX::W32::DXGI_FORMAT_R16_TYPELESS:
                    return REX::W32::DXGI_FORMAT_D16_UNORM;
                case REX::W32::DXGI_FORMAT_D24_UNORM_S8_UINT:
                case REX::W32::DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
                case REX::W32::DXGI_FORMAT_D32_FLOAT:
                case REX::W32::DXGI_FORMAT_D16_UNORM:
                    return format;
                default:
                    return REX::W32::DXGI_FORMAT_D24_UNORM_S8_UINT;
            }
        }

        struct OffscreenTarget
        {
            REX::W32::ID3D11Texture2D* color = nullptr;
            REX::W32::ID3D11RenderTargetView* rtv = nullptr;
            REX::W32::ID3D11ShaderResourceView* srv = nullptr;
            REX::W32::ID3D11Texture2D* depth_texture = nullptr;
            REX::W32::ID3D11DepthStencilView* dsv = nullptr;
            REX::W32::ID3D11DepthStencilState* ds_state = nullptr;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            REX::W32::DXGI_FORMAT format = REX::W32::DXGI_FORMAT_UNKNOWN;
            REX::W32::DXGI_FORMAT depth_format = REX::W32::DXGI_FORMAT_UNKNOWN;

            void destroy()
            {
                if (srv)
                    srv->Release();
                if (rtv)
                    rtv->Release();
                if (color)
                    color->Release();
                if (dsv)
                    dsv->Release();
                if (depth_texture)
                    depth_texture->Release();
                if (ds_state)
                    ds_state->Release();
                srv = nullptr;
                rtv = nullptr;
                color = nullptr;
                dsv = nullptr;
                depth_texture = nullptr;
                ds_state = nullptr;
                width = 0;
                height = 0;
                format = REX::W32::DXGI_FORMAT_UNKNOWN;
                depth_format = REX::W32::DXGI_FORMAT_UNKNOWN;
            }

            // Sized from the call site's own render target (run 12: the menu
            // pass draws into an R8G8B8A8_UNORM UI composite, format 28 —
            // not kMAIN's R11G11B10), so the replayed pass writes exactly the
            // representation it was built for. Run 14's readback exposed the
            // missing intra-item occlusion (the handle drawn over the boss):
            // without a DSV the pass's depth test is bypassed entirely, so a
            // private depth buffer (engine-matched format) plus an explicit
            // depth-on state accompany the color target.
            bool create(REX::W32::ID3D11Device* device, const REX::W32::D3D11_TEXTURE2D_DESC& template_desc,
                REX::W32::DXGI_FORMAT a_depth_format)
            {
                destroy();

                REX::W32::D3D11_TEXTURE2D_DESC color_desc{};
                color_desc.width = template_desc.width;
                color_desc.height = template_desc.height;
                color_desc.mipLevels = 1;
                color_desc.arraySize = 1;
                color_desc.format = template_desc.format;
                color_desc.sampleDesc.count = 1;
                color_desc.usage = REX::W32::D3D11_USAGE_DEFAULT;
                color_desc.bindFlags = REX::W32::D3D11_BIND_RENDER_TARGET | REX::W32::D3D11_BIND_SHADER_RESOURCE;
                if (device->CreateTexture2D(&color_desc, nullptr, &color) != 0)
                    return false;
                if (device->CreateRenderTargetView(color, nullptr, &rtv) != 0)
                {
                    destroy();
                    return false;
                }
                // v4: the composite quad samples the studio image, so the
                // color texture needs an SRV alongside the RTV.
                if (device->CreateShaderResourceView(color, nullptr, &srv) != 0)
                {
                    destroy();
                    return false;
                }

                REX::W32::D3D11_TEXTURE2D_DESC depth_desc{};
                depth_desc.width = template_desc.width;
                depth_desc.height = template_desc.height;
                depth_desc.mipLevels = 1;
                depth_desc.arraySize = 1;
                depth_desc.format = a_depth_format;
                depth_desc.sampleDesc.count = 1;
                depth_desc.usage = REX::W32::D3D11_USAGE_DEFAULT;
                depth_desc.bindFlags = REX::W32::D3D11_BIND_DEPTH_STENCIL;
                const HRESULT depth_hr = device->CreateTexture2D(&depth_desc, nullptr, &depth_texture);
                if (depth_hr != 0)
                {
                    logger::warn("Proto v3 depth texture creation failed (format={} hr=0x{:X})",
                        static_cast<int>(a_depth_format), static_cast<std::uint32_t>(depth_hr));
                    destroy();
                    return false;
                }
                if (device->CreateDepthStencilView(depth_texture, nullptr, &dsv) != 0)
                {
                    logger::warn("Proto v3 depth view creation failed");
                    destroy();
                    return false;
                }

                // The state at the call site has depth testing disabled (run
                // 10), and whatever the original draw leaves behind is not
                // guaranteed to have it on either, so the replay binds its
                // own standard depth-on state.
                REX::W32::D3D11_DEPTH_STENCIL_DESC ds_desc{};
                ds_desc.depthEnable = 1;
                ds_desc.depthWriteMask = REX::W32::D3D11_DEPTH_WRITE_MASK_ALL;
                ds_desc.depthFunc = REX::W32::D3D11_COMPARISON_LESS_EQUAL;
                ds_desc.stencilEnable = 0;
                ds_desc.stencilReadMask = 0xFF;
                ds_desc.stencilWriteMask = 0xFF;
                const REX::W32::D3D11_DEPTH_STENCILOP_DESC keep_op{
                    REX::W32::D3D11_STENCIL_OP_KEEP, REX::W32::D3D11_STENCIL_OP_KEEP,
                    REX::W32::D3D11_STENCIL_OP_KEEP, REX::W32::D3D11_COMPARISON_ALWAYS
                };
                ds_desc.frontFace = keep_op;
                ds_desc.backFace = keep_op;
                if (device->CreateDepthStencilState(&ds_desc, &ds_state) != 0 || !ds_state)
                {
                    destroy();
                    return false;
                }

                width = template_desc.width;
                height = template_desc.height;
                format = template_desc.format;
                depth_format = a_depth_format;
                return true;
            }
        };

        // Configuration identity of a studio target: detects description
        // changes (resolution change) and makes creation failures sticky per
        // configuration within one panel open.
        struct TargetSig
        {
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            std::uint32_t format = 0;
            std::uint32_t depth_format = 0;
            bool operator==(const TargetSig&) const = default;
        };

        TargetSig target_sig(const OffscreenTarget& target)
        {
            return { target.width, target.height, static_cast<std::uint32_t>(target.format),
                static_cast<std::uint32_t>(target.depth_format) };
        }

        OffscreenTarget& offscreen_target()
        {
            static OffscreenTarget s_target;
            return s_target;
        }

        void write_tga(const std::filesystem::path& file, std::uint32_t width, std::uint32_t height,
            const std::vector<std::uint8_t>& bgra)
        {
            std::FILE* out = _wfopen(file.c_str(), L"wb");
            if (!out)
                return;
            std::uint8_t header[18] = {};
            header[2] = 2;  // uncompressed true-color
            header[12] = static_cast<std::uint8_t>(width & 0xFF);
            header[13] = static_cast<std::uint8_t>((width >> 8) & 0xFF);
            header[14] = static_cast<std::uint8_t>(height & 0xFF);
            header[15] = static_cast<std::uint8_t>((height >> 8) & 0xFF);
            header[16] = 32;
            header[17] = 0x20;  // top-down origin
            std::fwrite(header, 1, sizeof(header), out);
            std::fwrite(bgra.data(), 1, bgra.size(), out);
            std::fclose(out);
        }

        // One-shot GPU -> CPU copy of the studio color target (F7 debug
        // path), tone-mapped to an 8-bit BGRA TGA under the SKSE log
        // directory when the target is HDR, copied verbatim otherwise (the
        // call-site composite is R8G8B8A8_UNORM). Synchronous by design —
        // never called on the steady-state path.
        void dump_offscreen_to_log_dir()
        {
            auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
            OffscreenTarget& target = offscreen_target();
            if (!renderer || !target.color)
                return;
            auto& runtime = renderer->GetRuntimeData();
            if (!runtime.context || !runtime.forwarder)
                return;

            REX::W32::D3D11_TEXTURE2D_DESC desc{};
            target.color->GetDesc(&desc);
            REX::W32::D3D11_TEXTURE2D_DESC staging_desc = desc;
            staging_desc.usage = REX::W32::D3D11_USAGE_STAGING;
            staging_desc.bindFlags = 0;
            staging_desc.cpuAccessFlags = REX::W32::D3D11_CPU_ACCESS_READ;
            staging_desc.miscFlags = 0;
            staging_desc.mipLevels = 1;
            staging_desc.arraySize = 1;
            REX::W32::ID3D11Texture2D* staging = nullptr;
            if (runtime.forwarder->CreateTexture2D(&staging_desc, nullptr, &staging) != 0 || !staging)
                return;
            runtime.context->CopyResource(staging, target.color);

            REX::W32::D3D11_MAPPED_SUBRESOURCE mapped{};
            if (FAILED(runtime.context->Map(staging, 0, REX::W32::D3D11_MAP_READ, 0, &mapped)))
            {
                staging->Release();
                return;
            }

            auto decode_r11f = [](std::uint32_t bits) -> float {
                if (bits == 0)
                    return 0.0f;
                std::uint32_t const exponent = (bits >> 6) & 0x1F;
                std::uint32_t const mantissa = bits & 0x3F;
                if (exponent == 0x1F)
                    return 1e30f;
                if (exponent == 0)
                    return std::ldexp(static_cast<float>(mantissa), -14 - 6);
                return std::ldexp(static_cast<float>(mantissa | 0x40), static_cast<int>(exponent) - 15 - 6);
            };
            auto tone = [](float v) {
                v = v / (1.0f + v);
                return static_cast<std::uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
            };

            const bool hdr = desc.format == REX::W32::DXGI_FORMAT_R11G11B10_FLOAT;
            std::vector<std::uint8_t> bgra;
            bgra.reserve(static_cast<std::size_t>(desc.width) * desc.height * 4);
            for (std::uint32_t y = 0; y < desc.height; ++y)
            {
                auto const* row = reinterpret_cast<std::uint8_t const*>(mapped.data) + y * mapped.rowPitch;
                for (std::uint32_t x = 0; x < desc.width; ++x)
                {
                    std::uint8_t r8 = 0;
                    std::uint8_t g8 = 0;
                    std::uint8_t b8 = 0;
                    if (hdr)
                    {
                        std::uint32_t packed = 0;
                        std::memcpy(&packed, row + static_cast<std::size_t>(x) * 4, sizeof(packed));
                        r8 = tone(decode_r11f(packed & 0x7FF));
                        g8 = tone(decode_r11f((packed >> 11) & 0x7FF));
                        b8 = tone(decode_r11f((packed >> 22) & 0x3FF));
                    }
                    else
                    {
                        r8 = row[static_cast<std::size_t>(x) * 4 + 0];
                        g8 = row[static_cast<std::size_t>(x) * 4 + 1];
                        b8 = row[static_cast<std::size_t>(x) * 4 + 2];
                    }
                    bgra.push_back(b8);
                    bgra.push_back(g8);
                    bgra.push_back(r8);
                    bgra.push_back(255);
                }
            }
            runtime.context->Unmap(staging, 0);
            staging->Release();

            std::filesystem::path dir =
                SKSE::log::log_directory().value_or(std::filesystem::path(".")) / "CharacterPanelProto";
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            for (std::uint32_t index = 0; index < 1000; ++index)
            {
                std::filesystem::path file = dir / fmt::format("proto-pass-{:03}.tga", index);
                if (std::filesystem::exists(file))
                    continue;
                write_tga(file, desc.width, desc.height, bgra);
                logger::info("Proto v3 studio dump written: {}", file.string());
                return;
            }
            logger::warn("Proto v3 dump directory is full; TGA not written");
        }

        // Identify an RTV's resource against the engine's render-target pool
        // and log its dimensions/format — used to learn which engine target
        // a re-applied binding points at.
        void log_rtv_identity(const char* tag, REX::W32::ID3D11RenderTargetView* rtv)
        {
            if (!rtv)
            {
                logger::info("Proto v3 {}: <null rtv>", tag);
                return;
            }
            REX::W32::ID3D11Resource* resource = nullptr;
            rtv->GetResource(&resource);
            auto* texture = static_cast<REX::W32::ID3D11Texture2D*>(resource);
            if (!texture)
            {
                logger::info("Proto v3 {}: rtv={} <no resource>", tag, static_cast<void*>(rtv));
                return;
            }
            REX::W32::D3D11_TEXTURE2D_DESC desc{};
            texture->GetDesc(&desc);
            int pool_index = -1;
            if (auto* renderer = RE::BSGraphics::Renderer::GetSingleton())
            {
                auto& targets = renderer->GetRuntimeData().renderTargets;
                const std::size_t total = std::to_underlying(RE::RENDER_TARGET::kTOTAL);
                for (std::size_t i = 0; i < total; ++i)
                {
                    if (targets[i].texture == texture)
                    {
                        pool_index = static_cast<int>(i);
                        break;
                    }
                }
            }
            logger::info("Proto v3 {}: rtv={} texture={} pool={} {}x{} format={}", tag, static_cast<void*>(rtv),
                static_cast<void*>(texture), pool_index, desc.width, desc.height,
                static_cast<int>(desc.format));
        }

        // Log a captured PS texture-binding set (slot -> pointer, dimension,
        // format).
        void log_srv_set(const char* tag, REX::W32::ID3D11ShaderResourceView* const (&views)[16])
        {
            std::string line;
            for (std::size_t i = 0; i < 16; ++i)
            {
                if (!views[i])
                    continue;
                REX::W32::D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
                views[i]->GetDesc(&desc);
                line += fmt::format(" [{}]=0x{:X} dim={} fmt={}", i,
                    reinterpret_cast<std::uintptr_t>(views[i]), static_cast<int>(desc.viewDimension),
                    static_cast<int>(desc.format));
            }
            logger::info("Proto v3 {}:{}", tag, line.empty() ? " (all unbound)" : line);
        }

        void release_srvs(REX::W32::ID3D11ShaderResourceView* (&views)[16])
        {
            for (auto*& view : views)
            {
                if (view)
                    view->Release();
                view = nullptr;
            }
        }

        // --- shared frame-rate/log constants --------------------------------

        // Per-open discovery cap for menu-geometry log lines: each menu
        // geometry is logged once per panel open; afterwards only counters
        // advance (a persistent frame would otherwise spam the log at frame
        // rate).
        constexpr std::size_t Menu_Geom_Log_Cap = 256;
        // Slow log cadence for both summary flavors — one line per N frames
        // (~30 s at 60 fps). Content summaries additionally fire whenever
        // the replay count changes (item switches).
        constexpr std::uint32_t Heartbeat_Frames = 1800;

        // v6.47: self-built studio lights — the panel's lighting no longer
        // depends on menu-item lights at all. v6.56: slot 0 = ambient base
        // (engine convention: point lights start at sceneLights[1]), slots
        // 1/2 = key/fill point lights.
        constexpr std::size_t Studio_Light_Count = 3;

        // --- composite (v4, work package 3) --------------------------------

        // M0 calibration constants: the panel occupies a fixed screen
        // fraction — v6.30 (user red-box, run 63): a tall strip on the
        // right (73%..99% horizontally, 5%..96% vertically from the top) —
        // and shows the WHOLE studio target squeezed into it (uv 0..1).
        // The anisotropic squeeze of the 16:9 target into this narrow rect
        // is inherent to the mapping; framing/zoom are studio concerns.
        constexpr float Panel_Screen_MinX = 0.73f;
        constexpr float Panel_Screen_MaxX = 0.99f;
        constexpr float Panel_Screen_MinY = 0.05f;
        constexpr float Panel_Screen_MaxY = 0.96f;

        // A fullscreen-triangle-pair quad addressed purely by SV_VertexID
        // (no vertex buffers, no input layout): the vertex shader places the
        // corners from a one-float4 constant buffer holding the panel rect
        // in NDC (x0, yBottom, x1, yTop), the pixel shader samples the
        // studio target opaquely. Drawn at DrawInterfaceStart entry into
        // whatever target is bound, BEFORE the original menu draw — the
        // PRD §2 order (panel under other UI). Only the state the quad
        // actually sets is saved and restored; the engine rebinds the rest
        // for its own draws.
        constexpr std::string_view Composite_VS = R"(
cbuffer PanelCB : register(b0) { float4 g_ndcRect; float4 g_flags; }
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
VSOut vs_main(uint id : SV_VertexID) {
    VSOut o;
    // v4.4: clockwise in window space (the viewport flips NDC y). The old
    // order was counter-clockwise on screen — back-facing — and the
    // engine's CULL_BACK rasterizer state silently culled every quad
    // since v4.1 (Draw succeeded, zero pixels rasterized).
    float2 corners[6] = { {-1,-1}, {-1,1}, {1,-1}, {-1,1}, {1,1}, {1,-1} };
    float2 c = corners[id];
    float x = lerp(g_ndcRect.x, g_ndcRect.z, c.x * 0.5 + 0.5);
    float y = lerp(g_ndcRect.y, g_ndcRect.w, c.y * 0.5 + 0.5);
    o.pos = float4(x, y, 0.0, 1.0);
    o.uv = float2(c.x * 0.5 + 0.5, 0.5 - c.y * 0.5);
    return o;
}
)";
        // Run 31: the studio target follows the call site's format. In the
        // menu stream that is the LDR UI composite (R8G8B8A8, format 28);
        // in the world stream (route 3 P replays) it is the HDR main
        // target (R11G11B10_FLOAT, format 10) — sampling and writing it
        // verbatim produced the washed noise in the first P image. The
        // composite now Reinhard-maps when the studio target is HDR; LDR
        // targets pass through unchanged.
        constexpr std::string_view Composite_PS = R"(
Texture2D g_tex : register(t0);
SamplerState g_samp : register(s0);
cbuffer PanelCB : register(b0) { float4 g_ndcRect; float4 g_flags; }  // g_flags.x = 1 when HDR; g_flags.y = sampled x-span (aspect-correct window)
float4 ps_main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target {
    // v6.32: sample an aspect-correct horizontal slice of the target —
    // squeezing the full 16:9 target into the narrow panel stretched the
    // figure (run 65). The window spans (panel aspect / target aspect) of
    // the target width, centered — the figure keeps its world proportions.
    uv.x = 0.5 + (uv.x - 0.5) * g_flags.y;
    float3 c = g_tex.Sample(g_samp, uv).rgb;
    if (g_flags.x > 0.5)
        c = c / (1.0 + c);
    return float4(c, 1.0);
}
)";

        class CompositeRenderer
        {
        public:
            static CompositeRenderer& instance()
            {
                static CompositeRenderer s_instance;
                return s_instance;
            }

            // One-time setup: compile the shaders (native d3dcompiler, no
            // blob-type coupling into REX) and create the fixed states.
            bool ensure(REX::W32::ID3D11Device* device)
            {
                if (m_vs || m_failed)
                    return !m_failed;
                if (!device)
                    return false;

                REX::W32::ID3D11VertexShader* vs = nullptr;
                REX::W32::ID3D11PixelShader* ps = nullptr;
                ID3DBlob* code = nullptr;
                ID3DBlob* errors = nullptr;
                if (FAILED(::D3DCompile(Composite_VS.data(), Composite_VS.size(), nullptr, nullptr, nullptr,
                        "vs_main", "vs_5_0", 0, 0, &code, &errors)))
                {
                    logger::warn("Proto v4 composite VS compile failed: {}",
                        errors ? static_cast<const char*>(errors->GetBufferPointer()) : "(no message)");
                    if (errors)
                        errors->Release();
                    m_failed = true;
                    return false;
                }
                const HRESULT vs_hr = device->CreateVertexShader(
                    code->GetBufferPointer(), code->GetBufferSize(), nullptr, &vs);
                code->Release();
                if (vs_hr != 0 || !vs)
                {
                    logger::warn("Proto v4 composite VS creation failed (hr=0x{:X})",
                        static_cast<std::uint32_t>(vs_hr));
                    m_failed = true;
                    return false;
                }

                if (FAILED(::D3DCompile(Composite_PS.data(), Composite_PS.size(), nullptr, nullptr, nullptr,
                        "ps_main", "ps_5_0", 0, 0, &code, &errors)))
                {
                    logger::warn("Proto v4 composite PS compile failed: {}",
                        errors ? static_cast<const char*>(errors->GetBufferPointer()) : "(no message)");
                    if (errors)
                        errors->Release();
                    vs->Release();
                    m_failed = true;
                    return false;
                }
                const HRESULT ps_hr = device->CreatePixelShader(
                    code->GetBufferPointer(), code->GetBufferSize(), nullptr, &ps);
                code->Release();
                if (ps_hr != 0 || !ps)
                {
                    logger::warn("Proto v4 composite PS creation failed (hr=0x{:X})",
                        static_cast<std::uint32_t>(ps_hr));
                    vs->Release();
                    m_failed = true;
                    return false;
                }

                REX::W32::D3D11_SAMPLER_DESC sampler_desc{};
                sampler_desc.filter = REX::W32::D3D11_FILTER_MIN_MAG_MIP_LINEAR;
                sampler_desc.addressU = REX::W32::D3D11_TEXTURE_ADDRESS_CLAMP;
                sampler_desc.addressV = REX::W32::D3D11_TEXTURE_ADDRESS_CLAMP;
                sampler_desc.addressW = REX::W32::D3D11_TEXTURE_ADDRESS_CLAMP;
                sampler_desc.comparisonFunc = REX::W32::D3D11_COMPARISON_NEVER;
                sampler_desc.maxLOD = REX::W32::D3D11_FLOAT32_MAX;
                REX::W32::ID3D11SamplerState* sampler = nullptr;
                REX::W32::ID3D11BlendState* blend = nullptr;
                REX::W32::ID3D11BlendState* opaque = nullptr;
                REX::W32::ID3D11DepthStencilState* depth_off = nullptr;
                REX::W32::ID3D11RasterizerState* rs = nullptr;
                REX::W32::ID3D11Buffer* cb = nullptr;

                // v4.6 (user RenderDoc finding): this composite target is
                // consumed through the UI alpha-composition pipeline, so the
                // quad must be BLENDED over the existing layer content, not
                // written opaquely — the disabled-blend overwrite broke the
                // layer's composition semantics. Standard non-premultiplied
                // over: src alpha scales the studio image, inv-src alpha
                // keeps the layer underneath.
                REX::W32::D3D11_BLEND_DESC blend_desc{};
                {
                    auto& rt0 = blend_desc.renderTarget[0];
                    rt0.blendEnable = 1;
                    rt0.srcBlend = REX::W32::D3D11_BLEND_SRC_ALPHA;
                    rt0.destBlend = REX::W32::D3D11_BLEND_INV_SRC_ALPHA;
                    rt0.blendOp = REX::W32::D3D11_BLEND_OP_ADD;
                    rt0.srcBlendAlpha = REX::W32::D3D11_BLEND_ONE;
                    rt0.destBlendAlpha = REX::W32::D3D11_BLEND_INV_SRC_ALPHA;
                    rt0.blendOpAlpha = REX::W32::D3D11_BLEND_OP_ADD;
                    rt0.renderTargetWriteMask = REX::W32::D3D11_COLOR_WRITE_ENABLE_ALL;
                }
                // Run 41: OPAQUE overwrite for the proactive P draw — the
                // engine's blend state at that point is the item pass's
                // leftover; with unknown factors the studio pixels could be
                // multiplied to black. Opaque (blend disabled) writes the
                // raw shader output with alpha forced to 1 by the guard's
                // semantics (write-all).
                REX::W32::D3D11_BLEND_DESC opaque_desc{};
                {
                    auto& rt0 = opaque_desc.renderTarget[0];
                    rt0.blendEnable = 0;
                    rt0.renderTargetWriteMask = REX::W32::D3D11_COLOR_WRITE_ENABLE_ALL;
                }
                const REX::W32::D3D11_DEPTH_STENCILOP_DESC keep_op{
                    REX::W32::D3D11_STENCIL_OP_KEEP, REX::W32::D3D11_STENCIL_OP_KEEP,
                    REX::W32::D3D11_STENCIL_OP_KEEP, REX::W32::D3D11_COMPARISON_ALWAYS
                };
                REX::W32::D3D11_DEPTH_STENCIL_DESC depth_desc{};
                depth_desc.depthEnable = 0;
                depth_desc.stencilEnable = 0;
                depth_desc.stencilReadMask = 0xFF;
                depth_desc.stencilWriteMask = 0xFF;
                depth_desc.frontFace = keep_op;
                depth_desc.backFace = keep_op;

                // v4.4: the quad carries its own rasterizer state — culling
                // off and scissor off — so whatever the engine leaves bound
                // (CULL_BACK, an active scissor rect) cannot silently drop
                // it. The corrected winding above is the primary fix; this
                // is the belt to its braces.
                REX::W32::D3D11_RASTERIZER_DESC rs_desc{};
                rs_desc.fillMode = REX::W32::D3D11_FILL_SOLID;
                rs_desc.cullMode = REX::W32::D3D11_CULL_NONE;
                rs_desc.frontCounterClockwise = 0;
                rs_desc.depthClipEnable = 1;
                rs_desc.scissorEnable = 0;

                // NDC rect derived once from the screen-fraction constants:
                // x = 2u-1; y_top = 1-2*top, y_bottom = 1-2*bottom. v6.31:
                // the CB carries a SECOND float4 — the per-draw HDR flag
                // lives in g_flags.x, NOT in g_ndcRect.y. Run 64: the flag
                // used to overwrite the rect's yBottom (both lived in
                // g_ndcRect.y), so LDR targets drew the quad from NDC 0 up
                // — exactly the upper half of the screen. That is why the
                // panel had always been shorter than its constants and the
                // new taller rect exposed it as "half a panel".
                const float cb_data[8] = {
                    2.0f * Panel_Screen_MinX - 1.0f, 1.0f - 2.0f * Panel_Screen_MaxY,
                    2.0f * Panel_Screen_MaxX - 1.0f, 1.0f - 2.0f * Panel_Screen_MinY,
                    0.0f,
                    (Panel_Screen_MaxX - Panel_Screen_MinX) / (Panel_Screen_MaxY - Panel_Screen_MinY),
                    0.0f, 0.0f
                };
                REX::W32::D3D11_BUFFER_DESC cb_desc{};
                cb_desc.byteWidth = sizeof(cb_data);
                // Run 31: DYNAMIC — the draw path rewrites the HDR flag per
                // draw (y), the VS still reads the NDC rect (x, z, w).
                cb_desc.usage = REX::W32::D3D11_USAGE_DYNAMIC;
                cb_desc.cpuAccessFlags = REX::W32::D3D11_CPU_ACCESS_WRITE;
                cb_desc.bindFlags = REX::W32::D3D11_BIND_CONSTANT_BUFFER;

                bool ok = device->CreateSamplerState(&sampler_desc, &sampler) == 0 && sampler &&
                    device->CreateBlendState(&blend_desc, &blend) == 0 && blend &&
                    device->CreateBlendState(&opaque_desc, &opaque) == 0 && opaque &&
                    device->CreateDepthStencilState(&depth_desc, &depth_off) == 0 && depth_off &&
                    device->CreateRasterizerState(&rs_desc, &rs) == 0 && rs;
                REX::W32::D3D11_SUBRESOURCE_DATA init{ cb_data, 0, 0 };
                ok = ok && device->CreateBuffer(&cb_desc, &init, &cb) == 0 && cb;
                if (!ok)
                {
                    logger::warn("Proto v4 composite state creation failed");
                    if (sampler)
                        sampler->Release();
                    if (blend)
                        blend->Release();
                    if (opaque)
                        opaque->Release();
                    if (depth_off)
                        depth_off->Release();
                    if (rs)
                        rs->Release();
                    if (cb)
                        cb->Release();
                    vs->Release();
                    ps->Release();
                    m_failed = true;
                    return false;
                }

                m_vs = vs;
                m_ps = ps;
                m_sampler = sampler;
                m_blend = blend;
                m_opaque = opaque;
                m_depth_off = depth_off;
                m_rs = rs;
                m_cb = cb;
                logger::info("Proto v4.6 composite ready: panel rect {}%..{}% x {}%..{}% of screen",
                    static_cast<int>(Panel_Screen_MinX * 100), static_cast<int>(Panel_Screen_MaxX * 100),
                    static_cast<int>(Panel_Screen_MinY * 100), static_cast<int>(Panel_Screen_MaxY * 100));
                return true;
            }

            // v4.1: draw the panel quad into the CAPTURED call-site target
            // (the format-28 UI composite the visible menu preview uses) —
            // run 20 proved the target bound at DrawInterfaceStart entry is
            // a different, non-visible intermediate. Binding another target
            // means the OM pair and the viewport join the save/restore set.
            void draw(REX::W32::ID3D11DeviceContext* ctx, REX::W32::ID3D11RenderTargetView* rtv)
            {
                OffscreenTarget& target = offscreen_target();
                if (!ctx || !m_vs || !m_ps || !m_rs || !target.srv || !rtv || rtv == target.rtv)
                    return;

                // Size the viewport from the target itself: the panel rect
                // is NDC, so it follows whatever resolution this target has.
                REX::W32::ID3D11Resource* resource = nullptr;
                rtv->GetResource(&resource);
                if (!resource)
                    return;
                REX::W32::D3D11_TEXTURE2D_DESC desc{};
                static_cast<REX::W32::ID3D11Texture2D*>(resource)->GetDesc(&desc);
                resource->Release();

                REX::W32::ID3D11RenderTargetView* prev_rtv = nullptr;
                REX::W32::ID3D11DepthStencilView* prev_dsv = nullptr;
                ctx->OMGetRenderTargets(1, &prev_rtv, &prev_dsv);
                REX::W32::D3D11_VIEWPORT prev_viewports[16] = {};
                UINT prev_viewport_count = 16;
                ctx->RSGetViewports(&prev_viewport_count, prev_viewports);

                REX::W32::ID3D11BlendState* prev_blend = nullptr;
                FLOAT prev_blend_factor[4] = {};
                UINT prev_sample_mask = 0;
                ctx->OMGetBlendState(&prev_blend, prev_blend_factor, &prev_sample_mask);
                if (prev_blend && !m_blend_logged)
                {
                    m_blend_logged = true;
                    REX::W32::D3D11_BLEND_DESC prev_blend_desc{};
                    prev_blend->GetDesc(&prev_blend_desc);
                    const auto& rt0 = prev_blend_desc.renderTarget[0];
                    logger::info("Proto v4.6 engine blend at composite time: enable={} src={} dest={} "
                                 "(v4.5 wrote opaquely into an alpha-composited layer)",
                        rt0.blendEnable, static_cast<int>(rt0.srcBlend), static_cast<int>(rt0.destBlend));
                }
                REX::W32::ID3D11DepthStencilState* prev_ds = nullptr;
                UINT prev_stencil_ref = 0;
                ctx->OMGetDepthStencilState(&prev_ds, &prev_stencil_ref);
                REX::W32::ID3D11RasterizerState* prev_rs = nullptr;
                ctx->RSGetState(&prev_rs);
                if (prev_rs && !m_rs_logged)
                {
                    m_rs_logged = true;
                    REX::W32::D3D11_RASTERIZER_DESC prev_rs_desc{};
                    prev_rs->GetDesc(&prev_rs_desc);
                    logger::info("Proto v4.6 rasterizer bound at composite time: cull={} scissor={} "
                                 "(the v4.1-4.3 quads were back-facing under this state and culled)",
                        static_cast<int>(prev_rs_desc.cullMode),
                        static_cast<int>(prev_rs_desc.scissorEnable));
                }
                REX::W32::ID3D11PixelShader* prev_ps = nullptr;
                ctx->PSGetShader(&prev_ps, nullptr, nullptr);
                REX::W32::ID3D11ShaderResourceView* prev_srv = nullptr;
                ctx->PSGetShaderResources(0, 1, &prev_srv);
                REX::W32::ID3D11SamplerState* prev_sampler = nullptr;
                ctx->PSGetSamplers(0, 1, &prev_sampler);
                REX::W32::ID3D11Buffer* prev_ps_cb = nullptr;
                ctx->PSGetConstantBuffers(0, 1, &prev_ps_cb);
                REX::W32::ID3D11VertexShader* prev_vs = nullptr;
                ctx->VSGetShader(&prev_vs, nullptr, nullptr);
                REX::W32::ID3D11Buffer* prev_vs_cb = nullptr;
                ctx->VSGetConstantBuffers(0, 1, &prev_vs_cb);
                REX::W32::D3D11_PRIMITIVE_TOPOLOGY prev_topology{};
                ctx->IAGetPrimitiveTopology(&prev_topology);

                REX::W32::D3D11_VIEWPORT viewport{ 0.0f, 0.0f,
                    static_cast<float>(desc.width), static_cast<float>(desc.height), 0.0f, 1.0f };
                // Run 31: the studio target's format decides the Reinhard
                // map (HDR world main target, format 10) vs pass-through
                // (LDR UI composite, format 28). v6.31: the flag lives in
                // g_flags.x — the FULL panel rect is written every draw
                // (run 64: the flag used to overwrite g_ndcRect.y = the
                // quad's yBottom, halving the panel).
                const float hdr = desc.format == REX::W32::DXGI_FORMAT_R11G11B10_FLOAT ? 1.0f : 0.0f;
                {
                    REX::W32::D3D11_MAPPED_SUBRESOURCE mapped{};
                    if (ctx->Map(m_cb, 0, REX::W32::D3D11_MAP_WRITE_DISCARD, 0, &mapped) == 0)
                    {
                        auto* out = static_cast<float*>(mapped.data);
                        out[0] = 2.0f * Panel_Screen_MinX - 1.0f;
                        out[1] = 1.0f - 2.0f * Panel_Screen_MaxY;
                        out[2] = 2.0f * Panel_Screen_MaxX - 1.0f;
                        out[3] = 1.0f - 2.0f * Panel_Screen_MinY;
                        out[4] = hdr;
                        out[5] = (Panel_Screen_MaxX - Panel_Screen_MinX) /
                                 (Panel_Screen_MaxY - Panel_Screen_MinY);
                        ctx->Unmap(m_cb, 0);
                    }
                }
                ctx->OMSetRenderTargets(1, &rtv, nullptr);
                if (prev_viewport_count > 0)
                    ctx->RSSetViewports(1, &viewport);
                ctx->OMSetBlendState(m_blend, prev_blend_factor, prev_sample_mask);
                ctx->OMSetDepthStencilState(m_depth_off, prev_stencil_ref);
                ctx->RSSetState(m_rs);
                ctx->PSSetShader(m_ps, nullptr, 0);
                ctx->PSSetShaderResources(0, 1, &target.srv);
                ctx->PSSetSamplers(0, 1, &m_sampler);
                ctx->PSSetConstantBuffers(0, 1, &m_cb);
                ctx->VSSetShader(m_vs, nullptr, 0);
                ctx->VSSetConstantBuffers(0, 1, &m_cb);
                ctx->IASetPrimitiveTopology(REX::W32::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                ctx->Draw(6, 0);
                ++m_draws;

                ctx->OMSetRenderTargets(1, &prev_rtv, prev_dsv);
                if (prev_viewport_count > 0)
                    ctx->RSSetViewports(prev_viewport_count, prev_viewports);
                ctx->OMSetBlendState(prev_blend, prev_blend_factor, prev_sample_mask);
                ctx->OMSetDepthStencilState(prev_ds, prev_stencil_ref);
                ctx->RSSetState(prev_rs);
                ctx->PSSetShader(prev_ps, nullptr, 0);
                ctx->PSSetShaderResources(0, 1, &prev_srv);
                ctx->PSSetSamplers(0, 1, &prev_sampler);
                ctx->PSSetConstantBuffers(0, 1, &prev_ps_cb);
                ctx->VSSetShader(prev_vs, nullptr, 0);
                ctx->VSSetConstantBuffers(0, 1, &prev_vs_cb);
                ctx->IASetPrimitiveTopology(prev_topology);

                if (prev_rtv)
                    prev_rtv->Release();
                if (prev_dsv)
                    prev_dsv->Release();
                if (prev_blend)
                    prev_blend->Release();
                if (prev_ds)
                    prev_ds->Release();
                if (prev_rs)
                    prev_rs->Release();
                if (prev_ps)
                    prev_ps->Release();
                if (prev_srv)
                    prev_srv->Release();
                if (prev_sampler)
                    prev_sampler->Release();
                if (prev_ps_cb)
                    prev_ps_cb->Release();
                if (prev_vs)
                    prev_vs->Release();
                if (prev_vs_cb)
                    prev_vs_cb->Release();

                if (m_draws == 1 || m_draws % Heartbeat_Frames == 0)
                    logger::info("Proto v4.6 composite draw #{} ({}x{} panel rect on screen)", m_draws,
                        desc.width, desc.height);
            }

            // Run 41: the OPAQUE blend used by the proactive P draw guard —
            // blend disabled, all channels written (the studio target is
            // ours alone; unknown engine blend factors were the second
            // black-out suspect).
            [[nodiscard]] REX::W32::ID3D11BlendState* opaque_blend() const { return m_opaque; }

            // Run 42: the CLEAN rasterizer for the proactive P draw — no
            // depth bias (the item call site leaves depthBiasClamp=-100
            // bound, which shoves far-plane-adjacent skinned depth out of
            // range), no scissor, cull off.
            [[nodiscard]] REX::W32::ID3D11RasterizerState* clean_rasterizer() const { return m_rs; }

        private:
            CompositeRenderer() = default;

            REX::W32::ID3D11VertexShader* m_vs = nullptr;
            REX::W32::ID3D11PixelShader* m_ps = nullptr;
            REX::W32::ID3D11SamplerState* m_sampler = nullptr;
            REX::W32::ID3D11BlendState* m_blend = nullptr;
            REX::W32::ID3D11BlendState* m_opaque = nullptr;
            REX::W32::ID3D11DepthStencilState* m_depth_off = nullptr;
            REX::W32::ID3D11RasterizerState* m_rs = nullptr;
            REX::W32::ID3D11Buffer* m_cb = nullptr;
            std::uint32_t m_draws = 0;
            bool m_failed = false;
            bool m_rs_logged = false;
            bool m_blend_logged = false;
        };

        // --- pass redirector (render thread only) --------------------------

        // Bound for the parent-chain walk to the menuObjects roots.
        constexpr std::size_t Ancestry_Max_Depth = 32;

        class PassRedirector
        {
        public:
            static PassRedirector& instance()
            {
                static PassRedirector s_instance;
                return s_instance;
            }

            bool install()
            {
                // The three call-site hooks go through a private local
                // trampoline (skse64 2.2.6's shared branch pool asserts on
                // plugin-sized requests); three 5-byte rewrites need only a
                // few dozen bytes.
                SKSE::GetTrampoline().create(64 * 1024);

                void (*thunks[3])(RE::BSRenderPass*, std::uint32_t, bool, std::uint32_t) = {
                    &thunk_site0,
                    &thunk_site1,
                    &thunk_site2,
                };
                for (std::size_t i = 0; i < 3; ++i)
                {
                    const CallSite& site = k_call_sites[i];
                    std::uintptr_t address = REL::RelocationID(site.id_se, site.id_ae).address() +
                        REL::Relocate(site.offset_se, site.offset_ae, site.offset_ae);
                    // Parse the E8 rel32 BEFORE patching: whatever the call
                    // site pointed at (vanilla SetupAndDrawPass, or Community
                    // Shaders' interposer when LightLimitFix got here first)
                    // is the true original our thunk and replays must run.
                    const std::uintptr_t setup_address = REL::RelocationID(100854, 107644).address();
                    auto* patch_bytes = reinterpret_cast<const std::uint8_t*>(address);
                    if (patch_bytes[0] == 0xE8)
                        s_original_targets[i] =
                            address + 5 + *reinterpret_cast<const std::int32_t*>(address + 1);
                    REL::Relocation<std::uintptr_t> hook{ address };
                    hook.write_call<5>(thunks[i]);
                    logger::info("Proto v3 pass hook {} installed at 0x{:X}; pre-patch target 0x{:X} "
                                 "(SetupAndDrawPass=0x{:X}, {})",
                        i, address, s_original_targets[i], setup_address,
                        s_original_targets[i] == setup_address ? "unhooked" : "interposed, chain restored");
                }
                return true;
            }

            // DrawInterfaceStart entry: open the bracket, snapshot the menu
            // roots passes will be matched against. Logging happens only on
            // root-set changes — the bracket now runs every frame while the
            // panel is open.
            void begin_frame()
            {
                m_in_frame = true;
                ++m_frame_index;
                // Run 36: the proactive P draw fires once per studio frame
                // (latch reset with the clear).
                m_p_drawn_this_frame = false;
                // v6.38: the per-frame composite latch — replay-phase draws
                // set it; end_frame's no-menu-pass fallback consumes it.
                m_composited_this_frame = false;
                // Run 56: the studio-light reference goes stale with the
                // frame — only an item lighting pass seen THIS frame may
                // rebind P's passes.
                m_studio_lights_fresh = false;
                // Run 32 (ghosting fix): clear once per menu frame. The
                // studio target follows the call-site format, and the menu
                // stream (UI composite, format 28) is a DIFFERENT resource
                // from the world main target the P world-stream replays
                // used (format 10) — clearing here cannot wipe world-phase
                // pixels, while NOT clearing let every menu frame stack the
                // item replay and the P snapshot over the previous frame
                // (the user's RenderDoc finding).
                m_cleared = false;

                // Fresh panel open: reset per-open throttles and give a
                // previously failed target creation another chance.
                const std::uint32_t generation = Proto::instance().panel_generation();
                if (generation != m_generation)
                {
                    m_generation = generation;
                    m_content_frames = 0;
                    m_last_logged_replayed = 0;
                    m_session_replays = 0;
                    m_target_failed = false;
                    m_seen_geoms.clear();
                    m_seen_p_geoms.clear();
                    m_p_geoms_logged = 0;
                }

                m_root_count = 0;
                if (RE::UI3DSceneManager* ui3d = RE::UI3DSceneManager::GetSingleton())
                {
                    for (std::size_t i = 0; i < 8; ++i)
                    {
                        RE::NiNode* root = ui3d->menuObjects[i].get();
                        if (root)
                            m_roots[m_root_count++] = root;
                    }
                }

                const bool roots_changed = m_root_count != m_logged_root_count ||
                    !std::equal(m_roots, m_roots + m_root_count, m_logged_roots);
                if (roots_changed)
                {
                    std::copy(m_roots, m_roots + m_root_count, m_logged_roots);
                    m_logged_root_count = m_root_count;
                    logger::info("Proto v3 panel frame #{}: menu roots changed to {}{}", m_frame_index,
                        m_root_count, [this] {
                            std::string names;
                            for (std::size_t i = 0; i < m_root_count; ++i)
                                names += fmt::format(" [{}]={}", i,
                                    m_roots[i]->name.c_str() ? m_roots[i]->name.c_str() : "(null)");
                            return names;
                        }());
                }
            }

            // DrawInterfaceStart return: close the bracket, consume a pending
            // F7 dump, summarize — and (v4.2) composite the panel into the
            // composite target captured during THIS frame's replays. Runs
            // 20/21 proved the entry-time draw lands in the previous
            // frame's pool instance and never reaches the screen: the
            // format-28 UI composite rotates instances every menu frame
            // (recapture lines ~0.35 s apart in run 21). Drawing here —
            // after the original menu draw, before DrawInterfaceStart
            // returns — is inside the same frame the engine's merge reads.
            // Gated on replays having happened this frame, so a stale
            // capture from an earlier menu frame is never used.
            void end_frame()
            {
                m_in_frame = false;

                // Run 30 pacing: the world stream (P's passes) runs BEFORE
                // the menu bracket each rendered frame, so the studio target
                // is cleared by the first P replay of that world phase and
                // the menu-phase item replays draw ON TOP of P without
                // clearing. The reset moves here — after everything a frame
                // will draw — instead of begin_frame, which would wipe the
                // already-drawn P every menu frame.

                if (Proto::instance().take_dump())
                {
                    if (offscreen_target().color)
                        dump_offscreen_to_log_dir();
                    else
                        logger::warn("Proto v3 dump ignored: studio target does not exist");
                }

                // v4.5: the end_frame composite is REMOVED — runs 23/24
                // falsified it (the merge happens inside the original call,
                // before this point; the quad rasterized into an already-
                // merged instance). The composite now runs inside the
                // replay hook, at the last placement on the visible side of
                // that merge. The captured s_panel_rtv stays as evidence
                // only (it logs the call-site format every session).

                if (m_menu_passes == 0 && m_p_passes == 0)
                {
                    if (m_frame_index % Heartbeat_Frames == 0)
                        logger::info("Proto v3 panel frame #{} heartbeat: passes_seen={} menu_passes=0 "
                                     "(content-free frame, target keeps last studio image)",
                            m_frame_index, m_passes_seen);
                }

                // v6.43: STUDIO TARGET SELF-CREATION — the INPUT side of the
                // decoupling. Run 76 (user RenderDoc + log): draw() exits at
                // its !target.srv guard, and draw_p_proactively exits at its
                // !target.rtv guard — the studio offscreen target was only
                // ever created inside replay_after_original (from the call
                // site's desc), and with ZERO passes there is no template
                // and no target: no P draw, no composite, black panel. The
                // target is now created HERE (BEFORE the proactive draw, so
                // the same frame's P lands in it) from the engine's
                // persistent kFRAMEBUFFER desc when absent: same screen
                // resolution; color format pinned to R8G8B8A8_UNORM (28) —
                // the exact representation the item-call-site target used
                // all along, so replays and the composite sampler see the
                // same format; depth normalized to D24_UNORM_S8_UINT
                // (normalize_depth_format's fallback). The replay path's
                // sig-based recreate still runs when a real call-site
                // template shows up with a different desc (e.g. resolution
                // change) — both paths share target_sig, so no thrash.
                {
                    auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
                    auto* rt = renderer ? &renderer->GetRuntimeData() : nullptr;
                    OffscreenTarget& target = offscreen_target();
                    // v6.44: run 77 showed ZERO self-created lines AND zero
                    // failure warns — the block was skipped on a silent
                    // path. Every skip branch now leaves a one-shot trace.
                    if (!m_selfcreate_trace_logged)
                    {
                        m_selfcreate_trace_logged = true;
                        logger::info(
                            "Proto v6.44 self-create trace: renderer={} rt={} context={} forwarder={} "
                            "target.rtv={} targetFailed={} fb.texture={} fb.RTV={}",
                            static_cast<const void*>(renderer), static_cast<const void*>(rt),
                            rt ? static_cast<const void*>(rt->context) : nullptr,
                            rt ? static_cast<const void*>(rt->forwarder) : nullptr,
                            static_cast<const void*>(target.rtv), m_target_failed,
                            (rt && rt->context) ? static_cast<const void*>(rt->renderTargets[RE::RENDER_TARGET::kFRAMEBUFFER].texture) : nullptr,
                            (rt && rt->context) ? static_cast<const void*>(rt->renderTargets[RE::RENDER_TARGET::kFRAMEBUFFER].RTV) : nullptr);
                    }
                    if (rt && rt->context && !target.rtv && !m_target_failed)
                    {
                        auto& fb = rt->renderTargets[RE::RENDER_TARGET::kFRAMEBUFFER];
                        // v6.45: run 78 trace settled it — fb.texture is NULL
                        // while fb.RTV is live (CLib's RenderTargetData
                        // texture/textureCopy pointers are simply not
                        // populated at runtime; the engine only touches these
                        // targets through its views). Read the template desc
                        // from the RTV instead — the same GetResource/GetDesc
                        // walk the replay path has used on the format-28
                        // instances since v4.1.
                        REX::W32::ID3D11Resource* fb_res = nullptr;
                        if (fb.RTV)
                            fb.RTV->GetResource(&fb_res);
                        if (fb_res)
                        {
                            REX::W32::D3D11_TEXTURE2D_DESC fb_desc{};
                            static_cast<REX::W32::ID3D11Texture2D*>(fb_res)->GetDesc(&fb_desc);
                            fb_res->Release();
                            REX::W32::D3D11_TEXTURE2D_DESC template_desc = fb_desc;
                            template_desc.format = REX::W32::DXGI_FORMAT_R8G8B8A8_UNORM;
                            constexpr auto kStudioDepth = REX::W32::DXGI_FORMAT_D24_UNORM_S8_UINT;
                            const TargetSig sig{ template_desc.width, template_desc.height,
                                static_cast<std::uint32_t>(template_desc.format),
                                static_cast<std::uint32_t>(kStudioDepth) };
                            if (!(m_target_failed && sig == m_failed_sig))
                            {
                                if (target.create(rt->forwarder, template_desc, kStudioDepth))
                                {
                                    m_cleared = false;  // fresh surface, needs the first clear
                                    logger::info("Proto v6.43 studio target self-created from kFRAMEBUFFER: "
                                                 "{}x{} format=28 (no pass needed)",
                                        template_desc.width, template_desc.height);
                                }
                                else
                                {
                                    m_target_failed = true;
                                    m_failed_sig = sig;
                                    logger::warn("Proto v6.43 studio target self-creation failed; sticky "
                                                 "for this configuration");
                                }
                            }
                        }
                        else if (!m_fb_texture_null_logged)
                        {
                            m_fb_texture_null_logged = true;
                            logger::warn("Proto v6.44 self-create skipped: kFRAMEBUFFER.RTV is null "
                                         "(no template source at bracket exit)");
                        }
                    }
                }

                // Run 43 (v6.8): the paused inventory does NOT re-draw the
                // same item — no item pass, no OM window from
                // replay_after_original, so P was drawn only on the arm
                // frame (74 bracketed frames, zero draws after). end_frame
                // now DRIVES the proactive draw directly, every bracketed
                // frame: it binds its own studio OM window, clears once per
                // frame, poses, and draws — zero dependence on engine
                // passes. The item-pass window path stays as a redundant
                // trigger (the frame latch prevents double draws).
                if (auto* ui3d = RE::UI3DSceneManager::GetSingleton())
                    draw_p_proactively(ui3d->unk10.get());

                // v6.38: PANEL-VISIBILITY DECOUPLING (user report: the panel
                // only appeared after highlighting a 3D item first). The
                // in-replay composite runs only when a pass reaches the
                // thunk — and the user's run-75 finding settles it: opening
                // the inventory does NOT enter thunk_site at ALL (entering
                // requires highlighting a 3D item), so before the first
                // highlight there is no capture AND no composite. The panel
                // display must bypass the pass path entirely.
                //
                // v6.42: the fallback composite targets the engine's
                // PERSISTENT kFRAMEBUFFER entry —
                // Renderer::GetRuntimeData().renderTargets[kFRAMEBUFFER].RTV
                // — the same engine-ledger slot CS's SetUIBuffer reads.
                // Pure memory read: no OM probing at exit (the v6.40 crash
                // lesson), no captured-pointer lifecycle, never
                // uninitialized while the renderer lives. At bracket exit
                // the menu draw has finished and the menu path renders INTO
                // this framebuffer (vanilla UI composites to kFRAMEBUFFER —
                // CS's SetUIBuffer comment), so a quad drawn now is on the
                // presented frame. CompositeRenderer::draw saves/restores
                // the full OM/state set around our quad (proven in every
                // session since v4.6) — the exit state the engine/CS chain
                // expects is restored before we return.
                if (!m_composited_this_frame)
                {
                    auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
                    auto* rt = renderer ? &renderer->GetRuntimeData() : nullptr;
                    if (rt && rt->context && CompositeRenderer::instance().ensure(rt->forwarder))
                    {
                        auto& fb = rt->renderTargets[RE::RENDER_TARGET::kFRAMEBUFFER];
                        if (fb.RTV)
                        {
                            CompositeRenderer::instance().draw(rt->context, fb.RTV);
                            ++m_end_frame_composites;
                            if (m_end_frame_composites == 1 ||
                                m_end_frame_composites % Heartbeat_Frames == 0)
                                logger::info("Proto v6.42 end_frame composite #{} (kFRAMEBUFFER; zero "
                                             "pass dependence — the panel follows the inventory only)",
                                    m_end_frame_composites);
                        }
                    }
                }

                if (m_menu_passes != 0 || m_p_passes != 0)
                {
                    ++m_content_frames;
                    m_session_replays += m_p_total_replays;
                    m_p_total_replays = 0;
                    if (m_p_total_replays != m_last_logged_replayed ||
                        m_content_frames % Heartbeat_Frames == 1)
                    {
                        m_last_logged_replayed = m_p_total_replays;
                        logger::info("Proto v3 panel frame #{}: passes_seen={} menu_passes={} p_passes={} "
                                     "geoms_logged={}",
                            m_frame_index, m_passes_seen, m_menu_passes, m_p_passes, m_geoms_logged);
                    }
                }
                m_passes_seen = 0;
                m_menu_passes = 0;
                m_p_passes = 0;
                m_menu_lighting_replayed = 0;
            }

            // Render-thread pass observation. Returns true when the pass
            // should be replayed into the private target AFTER the original
            // call has run.
            //
            // Two acceptance paths (run 29 finding): menu-scene passes (the
            // item preview, inside the DrawInterfaceStart bracket) and — new
            // in stage 2 — the display instance P. P's graph does NOT flow
            // through the menu culler (the menu scene collects geometry via
            // the culler's private queue, not scene-graph attachment —
            // capture-report addendum 2), so its passes run in the WORLD
            // pass stream outside the bracket. The world stream hits the
            // same three RenderPassImmediately call sites, so the thunk
            // sees those passes anyway; P is matched by root ancestry and
            // gated on the panel being open, which is what makes the world
            // stream safe to touch (panel closed -> P absent -> zero extra
            // replays; normal world passes never match the P root).
            bool on_pass(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test, std::uint32_t render_flags,
                std::size_t site_index)
            {
                if (!pass || !pass->geometry)
                    return false;

                const bool panel_open = Proto::instance().panel_frame_active();
                const bool p_geom = panel_open && PInstance::instance().is_p_geometry(pass->geometry);
                // Run 54: the thunk reads this right after on_pass returns
                // (same render-thread call) to decide the passthrough.
                m_last_pass_p_geom = p_geom;
                // v6.22's ghost layer RETIRED in v6.64 (stage-2b round 2):
                // the graph lives in CP_StudioHome and the shell dies at
                // relocation, so the world stream no longer contains P
                // passes outside the ~1.5 s build window — which the
                // node-level park keeps out of sight anyway.

                // Outside the menu bracket only P passes are accepted — the
                // studio target must never accumulate world scenery.
                if (!m_in_frame && !p_geom)
                    return false;

                ++m_passes_seen;
                const bool menu = m_in_frame && is_menu_geometry(pass->geometry);
                if (menu)
                {
                    ++m_menu_passes;
                    // Run 53/56: record the item preview's own lights —
                    // they ARE the studio lighting. P's passes carry
                    // dungeon/world lights that are thousands of units from
                    // the menu-space fragments the studio pose produces, so
                    // the PS collapses to ambient-only (run 52: figure fully
                    // formed in the depth buffer, near-black in the RT).
                    // Run 56: reference THIS pass's own sceneLights array —
                    // the exact storage the engine (and CS's light hooks)
                    // use for the item's own draw that frame — instead of
                    // copying pointers into a member array (run 55: copied
                    // pointers outlived the light objects after an item
                    // change and CS's GeometrySetupConstantPointLights
                    // crashed on them).
                    // v6.53: MENU-LIGHT ARMING RETIRED. Highlighting an item
                    // used to overwrite m_studio_light_array with the item's
                    // own light array — which means every "highlight = lit"
                    // observation was the MENU lights drawing the figure
                    // (v6.18), and the self-built lights were never actually
                    // exercised while the menu lights existed. With the
                    // override now unconditional (v6.47), leaving this armed
                    // made the two sources fight: highlight frames drew with
                    // menu lights, no-highlight frames with our lights.
                    // Removed so the self-built lights get a clean test.
                    if (pass->shader && std::to_underlying(pass->shader->shaderType.get()) == 6 &&
                        pass->sceneLights && pass->numLights > 0 && !m_studio_light_array)
                    {
                        m_studio_light_array = pass->sceneLights;
                        m_studio_light_count = pass->numLights;
                        m_studio_lights_fresh = true;
                        logger::info("Proto v6.53 legacy menu-light arming SKIPPED (self-built lights "
                                     "active; {} menu lights seen)",
                            pass->numLights);
                    }
                    // Discovery logging: one line per menu geometry per panel
                    // open; steady-state frames only advance the counters.
                    // p=1 marks stage-2 display-instance P geometry (skinned
                    // content the skin-replay round watches for).
                    if (m_geoms_logged < Menu_Geom_Log_Cap && m_seen_geoms.insert(pass->geometry).second)
                    {
                        ++m_geoms_logged;
                        logger::info(
                            "Proto v3 menu pass: geom={} name=[{}] shader={} passEnum=0x{:X} "
                            "technique=0x{:X} alphaTest={} numLights={} renderFlags=0x{:X} site={} p={}",
                            static_cast<void*>(pass->geometry),
                            pass->geometry->name.c_str() ? pass->geometry->name.c_str() : "(null)",
                            pass->shader ? std::to_underlying(pass->shader->shaderType.get()) : 0u,
                            pass->passEnum, technique, alpha_test, pass->numLights, render_flags,
                            site_index, p_geom ? 1 : 0);
                    }
                }
                else if (p_geom)
                {
                    ++m_p_passes;
                    // Discovery per P geometry per panel open: the skinned
                    // set (body/armor/hair) is exactly what this round must
                    // show.
                    if (m_p_geoms_logged < Menu_Geom_Log_Cap && m_seen_p_geoms.insert(pass->geometry).second)
                    {
                        ++m_p_geoms_logged;
                        logger::info(
                            "Proto v5 P pass: geom={} name=[{}] shader={} passEnum=0x{:X} "
                            "technique=0x{:X} alphaTest={} numLights={} renderFlags=0x{:X} site={} in_menu_frame={}",
                            static_cast<void*>(pass->geometry),
                            pass->geometry->name.c_str() ? pass->geometry->name.c_str() : "(null)",
                            pass->shader ? std::to_underlying(pass->shader->shaderType.get()) : 0u,
                            pass->passEnum, technique, alpha_test, pass->numLights, render_flags,
                            site_index, m_in_frame ? 1 : 0);
                    }
                }

                if (!menu && !p_geom)
                    return false;
                // v2 replays lighting passes only for the MENU stream (they
                // carry the shading; depth/shadow passes write no colour).
                // Run 32: P keeps ALL shader types — its type-6 set is hair
                // only (0Anto92/HAIRLINE/Brows), while the skinned body
                // (CBBE/clothes/shoes/face) renders through BSShader effects
                // (type 8); the run-31 blob props are already excluded by
                // the skinned-only whitelist in is_p_geometry.
                // Run 35: P passes are no longer snapshotted here — they are
                // generated proactively from the geometry's shader property
                // inside the studio OM window (draw_p_proactively). The
                // world-stream passthrough suppression stays: a live P pass
                // must not show the double in the world view.
                if (!pass->shader)
                    return false;
                if (p_geom)
                    return true;
                if (std::to_underlying(pass->shader->shaderType.get()) != 6)
                    return false;

                return true;
            }

            // FR-06 cleanup, running on the render thread at a
            // non-bracketed DrawInterfaceStart after the panel closed:
            // destroy the studio resources once, reset sticky failures and
            // session counters so the next open starts clean.
            void release_target(std::string_view reason)
            {
                OffscreenTarget& target = offscreen_target();
                if (target.color)
                {
                    target.destroy();
                    logger::info("Proto v3 studio target released ({})", reason);
                }
                // v6.41: the captured composite target now PERSISTS across
                // panel open/close — the highlight-decoupling lever that
                // replaced the crashed v6.40 exit-OM capture. The format-28
                // instance pool only reallocates on resolution changes; the
                // replay path already re-captures on any pointer change
                // (v4.1), so a stale pointer self-heals on the first pass —
                // and releasing it here was what forced every panel open to
                // wait for a fresh item-highlight burst before the fallback
                // composite could run. Only an actual resolution change
                // invalidates the texture (the crash guard: the pool frees
                // its textures then — handled by the replay-path pointer
                // check + recapture, never by a dangling Release here).
                m_target_failed = false;
                m_session_replays = 0;
                m_p_drew_this_open = false;
                m_p_draw_logged = false;
                m_p_recipe_logged = false;
                m_p_empty_live_logged = false;
                m_p_no_root_logged = false;
                m_studio_light_array = nullptr;
                m_studio_light_count = 0;
                // v6.66: the raw wrappers must be re-matched every open —
                // the engine rebuilds its ledger on panel cycles (run 87)
                // and a menu transition FREES them outright (run 100
                // crash); holding them across closes held freed memory.
                for (auto*& shell : m_studio_lights)
                    shell = nullptr;
                m_studio_lights_fresh = false;
                m_p_studio_lights_logged = false;
                m_pass_recipes.clear();
            }

            // v3.1: user-initiated close (F6). Runs 17/18 both ended with
            // every F7 pressed AFTER closing the panel — the natural flow
            // treats close as "done, capture now" — so the close itself
            // writes the evidence: one synchronous TGA of the last studio
            // image BEFORE the release destroys the target. Skipped when
            // nothing was replayed this open (empty target) and on
            // force-closes (save loading / new game).
            void dump_and_release(std::string_view reason)
            {
                // v6.63: a P-only open replays nothing (no highlighted item
                // → zero menu passes), but the proactive draw filled the
                // target — that is evidence too (run 96: every close
                // skipped the dump under the replay-only gate).
                if (offscreen_target().color && (m_session_replays > 0 || m_p_drew_this_open))
                    dump_offscreen_to_log_dir();
                release_target(reason);
            }

        private:
            PassRedirector() = default;

            // Run 31 (route 3): a whitelist P pass in the WORLD stream is
            // SUPPRESSED from the passthrough — the world must not show the
            // parked double (PRD 0.5: no panel/P in the world view) — and
            // exists only as the studio replay. Menu passes keep the normal
            // passthrough + replay order (runs 9-26 contract).
            //
            // Run 54: P passes are now suppressed in MENU frames too. The
            // proactive GetRenderPasses calls ENROLL P's passes into the
            // UI3D accumulator's persistent pass lists, and the engine then
            // draws them itself at the call sites — including the menu
            // teardown on inventory exit, where run 53's session CRASHED
            // inside BSLightingShader::SetupGeometry on the
            // studio-light-mutated pass (crash-2026-10-02-22-00-30: P's
            // armor BSTriShape, null light deref). Only the studio draw
            // renders P; the engine never touches these passes again.
            static bool should_suppress_passthrough(bool replay, bool p_geom, bool in_menu_frame)
            {
                // v6.19: while the panel is open the studio is P's only
                // renderer — the engine must not draw its passes anywhere
                // (world frames would show the double; menu frames include
                // the accumulator-cleanup draws that crashed run 53's
                // session). v6.64: the ghost clause is retired with the
                // ghost layer — post-relocation the world stream holds no P
                // passes outside the build window.
                return replay && (p_geom || !in_menu_frame);
            }

            static void thunk_site0(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test,
                std::uint32_t render_flags)
            {
                const bool replay = instance().on_pass(pass, technique, alpha_test, render_flags, 0);
                if (!should_suppress_passthrough(replay, instance().m_last_pass_p_geom,
                        instance().m_in_frame))
                    call_site_original(0, pass, technique, alpha_test, render_flags);
                if (replay)
                    instance().replay_after_original(pass, technique, alpha_test, render_flags, 0);
            }
            static void thunk_site1(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test,
                std::uint32_t render_flags)
            {
                const bool replay = instance().on_pass(pass, technique, alpha_test, render_flags, 1);
                if (!should_suppress_passthrough(replay, instance().m_last_pass_p_geom,
                        instance().m_in_frame))
                    call_site_original(1, pass, technique, alpha_test, render_flags);
                if (replay)
                    instance().replay_after_original(pass, technique, alpha_test, render_flags, 1);
            }
            static void thunk_site2(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test,
                std::uint32_t render_flags)
            {
                const bool replay = instance().on_pass(pass, technique, alpha_test, render_flags, 2);
                if (!should_suppress_passthrough(replay, instance().m_last_pass_p_geom,
                        instance().m_in_frame))
                    call_site_original(2, pass, technique, alpha_test, render_flags);
                if (replay)
                    instance().replay_after_original(pass, technique, alpha_test, render_flags, 2);
            }

            // A pass is a menu-scene pass when its geometry descends from one
            // of the UI3DSceneManager::menuObjects roots (run 6 located the
            // highlighted item under root[1]).
            bool is_menu_geometry(const RE::BSGeometry* geometry) const
            {
                if (m_root_count == 0)
                    return false;
                const RE::NiAVObject* node = geometry;
                for (std::size_t depth = 0; node && depth < Ancestry_Max_Depth; ++depth)
                {
                    for (std::size_t i = 0; i < m_root_count; ++i)
                    {
                        if (node == m_roots[i])
                            return true;
                    }
                    node = node->parent;
                }
                return false;
            }

            // Run 35 (v5.8): PROACTIVE pass construction. Runs 32-34 proved
            // the P pass supply can never be relied on — the world stream
            // dies exactly when the inventory opens (game paused), so the
            // snapshot is empty in the one state that matters. But the
            // engine generates passes through a VIRTUAL on the shader
            // property — BSShaderProperty::GetRenderPasses(geometry,
            // renderMode, accumulator) — which we can call directly for any
            // geometry: it picks the technique and lights and EmplacePasses
            // through BSShader::MakeRenderPass (ID 107497), the same path
            // every live draw uses. The menu scene's own accumulator
            // (UI3DSceneManager::unk10, the one the item preview renders
            // with) supplies the light state; the studio OM window supplies
            // the target. The returned RenderPassArray chains BSRenderPass
            // objects via ->next; each is drawn with BSBatchRenderer::
            // SetupAndDrawPass — the same call_site_original we already
            // invoke. The passes are one-frame objects owned by the
            // property's RenderPassArray (freed by DoClearRenderPasses on
            // the next accumulator run), so we must NOT free them here.
            //
            // Run 43 (v6.8): the draw OWNS ITS OM WINDOW now. Runs up to
            // v6.7 triggered it from replay_after_original — i.e. from an
            // ITEM pass — but the paused inventory never re-draws the same
            // item (the manager keeps loadedModels and just skips drawing),
            // so after the arm frame the window never opened again (run-43
            // log: 74 bracketed frames, zero P draws). end_frame now drives
            // the draw every bracketed frame; this function binds the
            // studio OM, clears once per frame, draws, and restores — no
            // dependency on any engine pass arriving.
            // v6.49: the self-built studio lights, REGISTERED WITH THE
            // ENGINE. Run 82 falsified the radius theory (4096 changed
            // nothing): CS's LightLimitFix feeds its shader light buffer
            // from ShadowSceneNode::activeLights (LightLimitFix.cpp:492) —
            // the world's light ledger — NOT from pass->sceneLights, and
            // the v6.47 shells were never in it. That is also why
            // highlighting works: the item's own lights ride the native
            // pass-array path, ours rode nothing. Fix: create the two
            // NiPointLights exactly as before, then hand them to the
            // ENGINE's own registry — ShadowSceneNode::AddLight(NiLight*)
            // (CLib REL 99691/106325, the non-shadow/never-fade overload) —
            // which builds the BSLight wrapper, fills every field, and
            // files it in activeLights. Both channels now carry our
            // lights: LLF's buffer AND the native pass-assignment path.
            // The manual shells are retired; the wrapper comes back from
            // GetPointLight for the pass-array override.
            // v6.50: shell fetch scans BOTH queues. Run 83: GetPointLight
            // scans activeLights only, but the engine's AddLight files new
            // lights into lightQueueAdd first (the per-frame light update
            // promotes them) — same-frame fetch always missed, the sticky
            // failure then killed the lights for the whole session (run 83
            // log: one warn, rig fell back to the dungeon directional).
            static RE::BSLight* fetch_light_wrapper(RE::ShadowSceneNode* a_node, RE::NiLight* a_light)
            {
                if (!a_node)
                    return nullptr;
                auto& rt = a_node->GetRuntimeData();
                for (auto& e : rt.activeLights)
                    if (e && e->light.get() == a_light)
                        return e.get();
                for (auto& e : rt.lightQueueAdd)
                    if (e && e->light.get() == a_light)
                        return e.get();
                return nullptr;
            }

            void ensure_studio_lights()
            {
                auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
                if (!renderer)
                    return;
                auto* ui3d = RE::UI3DSceneManager::GetSingleton();
                RE::NiNode* host = ui3d ? ui3d->menuObjects[0].get() : nullptr;
                RE::ShadowSceneNode* world_node =
                    RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];
                if (!host || !world_node)
                    return;

                // v6.66/v6.67: NEITHER the world's ShadowSceneNode NOR the
                // UI3D host survives a main-menu transition, and they fail
                // DIFFERENTLY (run 100/101):
                // - run 100: the node is destroyed — the engine FREES the
                //   BSLight wrappers our NiLights were registered with; the
                //   first P draw patched freed shells (lum read as 1.08e21
                //   garbage) and crashed writing through a freed NiLight.
                // - run 101: the node pointer SURVIVES, but the UI3D host
                //   (menuObjects[0]) is rebuilt — our rig node is orphaned
                //   under the dead root, the engine's per-frame light
                //   collection no longer reaches it, so the ledger holds no
                //   wrappers for our lights: the fetch fails SILENTLY and
                //   the figure renders dark.
                // The NiLights/rig are cheap — whenever the host or the node
                // changed, discard the whole rig and re-create under the
                // CURRENT scene (Phase 1 below). Re-creating sidesteps every
                // dangling-parent question an orphaned rig raises.
                if ((m_studio_light_ni[0] || m_studio_rig_node) &&
                    (host != m_studio_lights_host || world_node != m_studio_lights_node))
                {
                    logger::info("Proto v6.67 studio light rig reset: {} changed across a menu/load "
                                 "transition — re-creating under the current scene",
                        host != m_studio_lights_host ? "the UI3D host" : "the ShadowSceneNode");
                    for (auto*& shell : m_studio_lights)
                        shell = nullptr;
                    for (auto& ni : m_studio_light_ni)
                        ni = nullptr;
                    m_studio_rig_node = nullptr;
                    m_studio_light_count = 0;
                    m_studio_light_array = nullptr;
                    m_studio_lights_failed = false;
                    m_fetch_warned = false;
                }

                // Phase 1: create once (no sticky failure — a same-frame
                // queue miss must not kill the lights for the session).
                if (!m_studio_light_ni[0] && !m_studio_lights_failed)
                {
                    // v6.51: a PRIVATE rig node. menuObjects[0] is an
                    // engine root with selective-update flags — v6.50's
                    // parent->Update() cascade was short-circuited by them
                    // (the run-41 lesson, again): the lights' world
                    // transforms never left the origin, so nothing lit.
                    // The rig node is ours, fresh, no flags — and the rig
                    // cascade below now uses UpdateDownwardPass (forced).
                    auto* rig = RE::NiNode::Create();
                    if (!rig)
                    {
                        m_studio_lights_failed = true;
                        logger::warn("Proto v6.51 studio rig node creation failed");
                        return;
                    }
                    rig->name = "CP_StudioLightRig";
                    host->AttachChild(rig);
                    m_studio_rig_node.reset(rig);
                    for (std::size_t i = 0; i < Studio_Light_Count; ++i)
                    {
                        // v6.54: BOTH point lights. Run 87 verdict: the LEFT
                        // screenshot (the user's chosen reference look) was
                        // lit while shell[0] was still the queued POINT-light
                        // wrapper (the v6.52 directional shell had not been
                        // promoted yet) — the point lights + rig placement
                        // ARE the proven-good configuration. The RIGHT (dark)
                        // case appeared exactly when re-fetch picked the
                        // promoted directional shell: its worldDirection
                        // (Rz(-90°) guess) faces away, and lum=10081 made it
                        // dominant — one wrong-facing ultra-bright light
                        // beats two correct point lights to black.
                        // Directional experiment retired; both point again.
                        // v6.56: ENGINE SLOT CONVENTION — sceneLights[0] is
                        // the AMBIENT slot; point lights start at index 1
                        // (LLF: strict lights iterate sceneLights[i+1],
                        // LightLimitFix.cpp:248; the engine SetupGeometry
                        // treats slot 0 the same way). The v6.47-55 layouts
                        // put the KEY at slot 0, where it was silently
                        // treated as ambient — only the slot-1 fill light
                        // ever contributed (the run-71+ 'lit but odd' look).
                        // New arrangement: slot 0 = ambient fill (soft base
                        // light, no position math), slots 1/2 = key/fill
                        // point lights.
                        RE::NiLight* ni = nullptr;
                        if (i == 0)
                        {
                            // Slot 0: ambient base. A point light flagged
                            // ambient — the engine reads its ambient term.
                            auto* amb = RE::NiPointLight::Create();
                            if (!amb)
                            {
                                m_studio_lights_failed = true;
                                logger::warn("Proto v6.56 ambient light create failed");
                                return;
                            }
                            amb->name = "CP_StudioAmbient";
                            auto& ard = amb->GetLightRuntimeData();
                            ard.diffuse = RE::NiColor(0.25f, 0.25f, 0.28f);
                            ard.radius = { 4096.0f, 4096.0f, 4096.0f };
                            ard.fade = 1.0f;
                            amb->SetLightAttenuation(4096.0f);
                            ni = amb;
                        }
                        else
                        {
                            auto* pt = RE::NiPointLight::Create();
                            if (!pt)
                            {
                                m_studio_lights_failed = true;
                                logger::warn("Proto v6.56 point light create failed");
                                return;
                            }
                            pt->name = i == 1 ? "CP_StudioKey" : "CP_StudioFill";
                            // Warm-white key, cool fill; radius covers any
                            // rig placement, fade 2.0 for headroom (v6.48).
                            auto& rd = pt->GetLightRuntimeData();
                            rd.diffuse = i == 1 ? RE::NiColor(1.0f, 0.96f, 0.90f) : RE::NiColor(0.70f, 0.80f, 1.0f);
                            rd.radius = { 4096.0f, 4096.0f, 4096.0f };
                            rd.fade = 2.0f;
                            pt->SetLightAttenuation(4096.0f);
                            ni = pt;
                        }
                        ni->local.translate = { 0.0f, 0.0f, 0.0f };
                        rig->AttachChild(ni);
                        // Engine registration: the wrapper is BUILT here
                        // (filed in lightQueueAdd, promoted to activeLights
                        // by the next light update).
                        world_node->AddLight(ni);
                        m_studio_light_ni[i].reset(ni);
                    }
                    m_studio_lights_node = world_node;
                    m_studio_lights_host = host;
                    logger::info("Proto v6.50 studio lights created and handed to ShadowSceneNode::AddLight "
                                 "(activeLights.size={} lightQueueAdd.size={})",
                        world_node->GetRuntimeData().activeLights.size(),
                        world_node->GetRuntimeData().lightQueueAdd.size());
                }

                // Phase 2: fetch the ENGINE-built wrappers — retried every
                // window until both are found (the queue promotion happens
                // on the engine's light-update tick, at most a frame later).
                if (m_studio_light_count == 0)
                {
                    bool all = true;
                    for (std::size_t i = 0; i < Studio_Light_Count; ++i)
                        if (!m_studio_lights[i])
                            m_studio_lights[i] =
                                fetch_light_wrapper(world_node, m_studio_light_ni[i].get());
                    for (std::size_t i = 0; i < Studio_Light_Count; ++i)
                        if (!m_studio_lights[i])
                            all = false;
                    // v6.67: an incomplete fetch used to be SILENT — run
                    // 101's dark figure had zero log lines to explain it.
                    // Warn once per stall; the reset above or the engine's
                    // next ledger rebuild clears it.
                    if (!all)
                    {
                        if (!m_fetch_warned)
                        {
                            m_fetch_warned = true;
                            logger::warn("Proto v6.67 studio light wrappers not in the ledger yet "
                                         "({}/{} matched) — the figure renders without studio lights "
                                         "until they appear",
                                std::count_if(std::begin(m_studio_lights), std::end(m_studio_lights),
                                    [](const RE::BSLight* s) { return s != nullptr; }),
                                Studio_Light_Count);
                        }
                        return;
                    }
                    m_fetch_warned = false;
                    // v6.53/v6.54: FIX THE SHELL FIELDS the engine left
                    // stale — and RE-PATCH ON EVERY FETCH. The engine
                    // REBUILDS its wrappers whenever the ledger is
                    // repopulated (panel open/close cycles: activeLights
                    // 97→99 in run 87), and every rebuilt shell starts
                    // with lodDimmer=0 again — the run-87 log shows the
                    // patch working on fetch 1 and the panel dark again
                    // on fetch 2. Both light consumers multiply by
                    // lodDimmer (LLF: light.fade *= lodDimmer; native
                    // LOD fade), so 0 = a light that exists but
                    // contributes exactly nothing.
                    bool any_patched = false;
                    for (std::size_t i = 0; i < Studio_Light_Count; ++i)
                    {
                        auto* shell = m_studio_lights[i];
                        if (shell->lodDimmer != 1.0f || shell->luminance != 1.0f)
                        {
                            any_patched = true;
                            logger::info(
                                "Proto v6.54 shell[{}] raw: lodDimmer={:.3f} lum={:.3f} portalStrict={} "
                                "dynamic={} pointLight={} frustrumCull=0x{:X} worldTranslate=({:.1f},{:.1f},{:.1f})",
                                i, shell->lodDimmer, shell->luminance, shell->portalStrict, shell->dynamic,
                                shell->pointLight, shell->frustrumCull, shell->worldTranslate.x,
                                shell->worldTranslate.y, shell->worldTranslate.z);
                            shell->lodDimmer = 1.0f;
                            shell->luminance = 1.0f;
                            shell->frustrumCull = 0;
                        }
                    }
                    m_studio_light_count = Studio_Light_Count;
                    m_studio_light_array = m_studio_lights;
                    logger::info("Proto v6.54 studio light wrappers fetched{} "
                                 "(activeLights.size={} lightQueueAdd.size={})",
                        any_patched ? " and PATCHED (lodDimmer=1, lum=1, no cull)" : " (fields already good)",
                        world_node->GetRuntimeData().activeLights.size(),
                        world_node->GetRuntimeData().lightQueueAdd.size());
                }
            }

            // v6.58: the studio lights are REGISTERED in the world's light
            // ledger (v6.49, required for LLF), so the engine renders them
            // into the WORLD whenever they sit anywhere near gameplay
            // space — the user's run-91 report: dungeon walls lit with the
            // panel's warm key light while the inventory was open, the
            // patch moving as the rig moved. The rig node therefore lives
            // FAR OUT of the world by default (parked), and only returns
            // to the studio anchor for the duration of the P draw window —
            // the one interval our mutate/draw/restore owns. Outside that
            // window (panel-open non-bracket frames, panel closed, world
            // frames) the lights are 100k units away and light nothing.
            static constexpr float Rig_Park_Z = 100000.0f;

            void park_studio_rig()
            {
                if (m_studio_rig_node)
                {
                    m_studio_rig_node->local.translate = { 0.0f, 0.0f, Rig_Park_Z };
                    RE::NiUpdateData data{ 0.0f, RE::NiUpdateData::Flag::kDirty };
                    m_studio_rig_node->UpdateDownwardPass(data, 0);
                }
            }

            void draw_p_proactively(RE::BSShaderAccumulator* accumulator)
            {
                // Stage-2b: the game thread may be inside the relocation
                // window (world detach -> menu-home attach). Skip this
                // frame; the next one draws from the home.
                if (PInstance::instance().relocating())
                    return;
                RE::NiAVObject* p_root = PInstance::instance().root();
                if (!p_root || !accumulator)
                {
                    // Run 54: once-per-open visibility for the silent
                    // swallow — run 53's steady-state frames stopped
                    // drawing with ZERO log lines because the whitelist
                    // root vanished (PInstance back to kNone, cause still
                    // unidentified); this line makes it visible.
                    if (!p_root && !m_p_no_root_logged)
                    {
                        m_p_no_root_logged = true;
                        logger::warn("Proto P draw skipped: whitelist root is null (PInstance state lost?)");
                    }
                    return;
                }
                // Run 36: one P draw per studio frame (replay_after_original
                // may call this for every item pass in a frame).
                if (m_p_drawn_this_frame)
                    return;
                m_p_drawn_this_frame = true;

                // v6.58: bring the light rig back INTO the studio anchor
                // neighborhood for the draw window (the lights are world-
                // registered; parked they sit 100k away and light nothing —
                // see park_studio_rig). The rig's parent is menuObjects[0],
                // whose world is the menu-space identity cascade, so the
                // local translate IS the accumulator-space position.
                if (m_studio_rig_node)
                {
                    const RE::NiPoint3 anchor = PInstance::instance().studio_anchor();
                    m_studio_rig_node->local.translate = anchor;
                    RE::NiUpdateData data{ 0.0f, RE::NiUpdateData::Flag::kDirty };
                    m_studio_rig_node->UpdateDownwardPass(data, 0);
                }

                auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
                auto& runtime = renderer->GetRuntimeData();
                if (!runtime.context || !runtime.forwarder)
                {
                    park_studio_rig();  // v6.58: pulled in above; park back out
                    return;
                }

                // Save the ambient OM pair + viewport + depth state.
                REX::W32::ID3D11RenderTargetView* prev_rtv = nullptr;
                REX::W32::ID3D11DepthStencilView* prev_dsv = nullptr;
                runtime.context->OMGetRenderTargets(1, &prev_rtv, &prev_dsv);
                REX::W32::ID3D11DepthStencilState* prev_ds = nullptr;
                std::uint32_t prev_stencil_ref = 0;
                runtime.context->OMGetDepthStencilState(&prev_ds, &prev_stencil_ref);
                REX::W32::D3D11_VIEWPORT prev_viewport{};
                std::uint32_t prev_viewport_count = 1;
                runtime.context->RSGetViewports(&prev_viewport_count, &prev_viewport);

                // Studio target: persistent, sized from the captured
                // call-site description (recreated on desc change).
                REX::W32::D3D11_TEXTURE2D_DESC desc{};
                if (s_panel_rtv)
                {
                    REX::W32::ID3D11Resource* resource = nullptr;
                    s_panel_rtv->GetResource(&resource);
                    if (resource)
                    {
                        static_cast<REX::W32::ID3D11Texture2D*>(resource)->GetDesc(&desc);
                        resource->Release();
                    }
                }
                OffscreenTarget& target = offscreen_target();
                if (!target.rtv)
                {
                    // No template this frame (first draw may precede any
                    // capture); skip — but the rig was pulled in at the top,
                    // so park it back out before leaving (v6.58).
                    park_studio_rig();
                    if (prev_rtv)
                        prev_rtv->Release();
                    if (prev_dsv)
                        prev_dsv->Release();
                    return;
                }

                PInstance::instance().pose_for_studio();

                runtime.context->OMSetRenderTargets(1, &target.rtv, target.dsv);
                runtime.context->OMSetDepthStencilState(target.ds_state, 0);
                // v6.46: ENGINE DIRTY-BIT GUARD. Run 79 (user screenshots +
                // log): with the self-created target and no menu-item pass
                // context, P's bow prop and garbage-skinned geometry landed
                // ON SCREEN outside the panel — the engine's
                // SetupAndDrawPass re-applies the LEDGER's render target
                // whenever ShaderFlags::DIRTY_RENDERTARGET is set (CS
                // Deferred.cpp drives the same machinery), and with nothing
                // having bound our private target through engine state that
                // frame, the ledger still said kFRAMEBUFFER. The highlight
                // window was clean precisely because the item call-site had
                // just bound the format-28 instance through engine state.
                // Fix: after our raw bind, CLEAR the dirty bit so the pass
                // internals treat the OM as current (our bind stays); after
                // the window's restore, SET it back so the engine rebuilds
                // its own bindings next time it applies state (CS coexists
                // with this exact handshake every frame).
                {
                    auto* shadow_state = RE::BSGraphics::RendererShadowState::GetSingleton();
                    shadow_state->GetRuntimeData().stateUpdateFlags.reset(
                        RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);
                    m_rt_dirty_guard_used = true;
                }
                REX::W32::D3D11_VIEWPORT viewport{ 0.0f, 0.0f,
                    static_cast<float>(target.width), static_cast<float>(target.height), 0.0f, 1.0f };
                runtime.context->RSSetViewports(1, &viewport);
                if (!m_cleared)
                {
                    const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
                    runtime.context->ClearRenderTargetView(target.rtv, clear_color);
                    runtime.context->ClearDepthStencilView(target.dsv,
                        REX::W32::D3D11_CLEAR_DEPTH | REX::W32::D3D11_CLEAR_STENCIL, 1.0f, 0);
                    m_cleared = true;
                }

                // Run 41: opaque blend; run 42: clean rasterizer (the
                // engine's leftovers at the item call site — unknown blend
                // factors, depthBiasClamp=-100 — both black out the pixels).
                struct BlendGuard
                {
                    BlendGuard(REX::W32::ID3D11DeviceContext* a_ctx, REX::W32::ID3D11BlendState* a_opaque) :
                        ctx(a_ctx), opaque(a_opaque)
                    {
                        ctx->OMGetBlendState(&prev_blend, prev_factor, &prev_mask);
                        ctx->OMSetBlendState(opaque, prev_factor, prev_mask);
                    }
                    ~BlendGuard()
                    {
                        ctx->OMSetBlendState(prev_blend, prev_factor, prev_mask);
                        if (prev_blend)
                            prev_blend->Release();
                    }
                    REX::W32::ID3D11DeviceContext* ctx;
                    REX::W32::ID3D11BlendState* opaque;
                    REX::W32::ID3D11BlendState* prev_blend = nullptr;
                    FLOAT prev_factor[4]{};
                    UINT prev_mask = 0;
                } blend_guard(runtime.context, CompositeRenderer::instance().opaque_blend());

                struct RasterGuard
                {
                    RasterGuard(REX::W32::ID3D11DeviceContext* a_ctx, REX::W32::ID3D11RasterizerState* a_clean) :
                        ctx(a_ctx), clean(a_clean)
                    {
                        ctx->RSGetState(&prev_rs);
                        ctx->RSSetState(clean);
                    }
                    ~RasterGuard()
                    {
                        ctx->RSSetState(prev_rs);
                        if (prev_rs)
                            prev_rs->Release();
                    }
                    REX::W32::ID3D11DeviceContext* ctx;
                    REX::W32::ID3D11RasterizerState* clean;
                    REX::W32::ID3D11RasterizerState* prev_rs = nullptr;
                } raster_guard(runtime.context, CompositeRenderer::instance().clean_rasterizer());

                std::vector<RE::BSRenderPass*> passes;
                RE::BSVisit::TraverseScenegraphGeometries(p_root, [&](RE::BSGeometry* geometry) {
                    if (!PInstance::instance().is_p_geometry(geometry))
                        return RE::BSVisit::BSVisitControl::kContinue;
                    auto& geom_rt = geometry->GetGeometryRuntimeData();
                    auto* property = geom_rt.shaderProperty.get();
                    if (!property)
                        return RE::BSVisit::BSVisitControl::kContinue;
                    // renderMode kNormal (0) — the menu accumulator's normal
                    // path, same as the item preview's passes.
                    auto* pass_array = property->GetRenderPasses(geometry,
                        std::to_underlying(RE::BSShaderAccumulator::RENDER_MODE::kNormal), accumulator);
                    if (pass_array)
                    {
                        for (RE::BSRenderPass* pass = pass_array->head; pass; pass = pass->next)
                        {
                            if (pass->geometry == geometry && pass->shader)
                            {
                                passes.push_back(pass);
                                // Run 46: remember the PASS RECIPE — the
                                // paused inventory stops regenerating passes
                                // (run 43), so later frames must rebuild
                                // them from the recorded parameters. All
                                // referenced objects live for the panel
                                // open (actor graph, singleton shaders,
                                // UI3D menu lights). Dedup by full key.
                                const bool known = std::any_of(m_pass_recipes.begin(), m_pass_recipes.end(),
                                    [&](const PPassRecipe& r) {
                                        return r.shader == pass->shader && r.geometry == geometry &&
                                            r.technique == pass->passEnum;
                                    });
                                if (!known)
                                {
                                    m_pass_recipes.push_back(
                                        { pass->shader, property, geometry, pass->passEnum,
                                            static_cast<std::uint8_t>(pass->numLights),
                                            { pass->sceneLights ? pass->sceneLights[0] : nullptr,
                                                pass->sceneLights ? pass->sceneLights[1] : nullptr,
                                                pass->sceneLights ? pass->sceneLights[2] : nullptr,
                                                pass->sceneLights ? pass->sceneLights[3] : nullptr } });
                                }
                            }
                        }
                    }
                    // Run 46 fallback: the generator came up empty for this
                    // geometry (paused-inventory state) — rebuild from a
                    // previously recorded recipe instead.
                    return RE::BSVisit::BSVisitControl::kContinue;
                });

                // Run 46: rebuild passes from recipes when the live
                // generator produced nothing (or too few). Deduplicate the
                // recipes accumulated over the session per panel open.
                if (passes.empty())
                {
                    // Run 46: nothing live — try rebuilding from recorded
                    // recipes. Also LOG the empty live generation once per
                    // session: silent empty runs were the run-46 blind spot.
                    if (!m_p_empty_live_logged)
                    {
                        m_p_empty_live_logged = true;
                        logger::info("Proto v6.11 live GetRenderPasses produced nothing ({} recipes "
                                     "recorded); rebuilding",
                            m_pass_recipes.size());
                    }
                    for (const auto& recipe : m_pass_recipes)
                    {
                        if (!PInstance::instance().is_p_geometry(recipe.geometry))
                            continue;
                        RE::BSLight* lights[4] = { recipe.lights[0], recipe.lights[1], recipe.lights[2],
                            recipe.lights[3] };
                        if (RE::BSRenderPass* rebuilt =
                                recipe.shader->MakeRenderPass(recipe.property, recipe.geometry,
                                    recipe.technique, recipe.num_lights, lights))
                        {
                            if (rebuilt->geometry == recipe.geometry)
                                passes.push_back(rebuilt);
                        }
                    }
                }
                else if (!m_pass_recipes.empty())
                {
                    m_pass_recipes.clear();  // live generation works; recipes stale
                }

                // Run 47: per-frame draw summary (replaces the draw-once
                // log) — the user's RenderDoc report contradicted the
                // single-line log, which was latched forever. This line
                // states EXACTLY what each studio frame drew and from where.
                logger::info("Proto v6.11 P draw: source={} passes={}", passes.empty() && !m_pass_recipes.empty()
                                                                                 ? "recipes-failed"
                                                                                 : (passes.empty() ? "empty"
                                                                                                   : "live/recipes"),
                    passes.size());

                // Run 52 (v6.17): BACK TO THE RUN-35 CONFIGURATION (user
                // direction — "runs 35-42 actually drew the character, it
                // was just out of the frustum"). The v6.13-16 manual draw is
                // falsified: its rd gate blocked every pass on
                // rendererData=null (runs 49-51) — and if the engine's
                // SetupAndDrawPass is what lazily creates those device
                // buffers, the manual path removed the ONLY caller that
                // could ever initialize the geometry. Restore the proven
                // chain, exactly as runs 35-42 drew it: per pass, rebind the
                // studio OM pair (the v6.4 fix — SetupAndDrawPass's internal
                // shadow-state reapplication steals the binding), then call
                // the call-site original — the ENGINE's own SetupAndDrawPass
                // with the pass's own technique encoding (alphaTest =
                // passEnum bit 6, run-31 observation; renderFlags 0x200 =
                // the menu-stream value). The engine then handles technique
                // setup, geometry/device-buffer init, batch dispatch and
                // state; the guards added since (opaque blend, clean
                // rasterizer, forced cascade, near-plane pose) all stay.
                // Site 1's original = the CS-interposed chain the item
                // preview itself flows through, so the menu context matches.
                std::uint32_t drawn = 0;
                for (RE::BSRenderPass* pass : passes)
                {
                    runtime.context->OMSetRenderTargets(1, &target.rtv, target.dsv);
                    runtime.context->OMSetDepthStencilState(target.ds_state, 0);
                    // Run 53/56: studio lighting — point the pass at the
                    // item preview's own light array (recorded THIS frame;
                    // the freshness gate skips frames without a menu
                    // lighting pass, falling back to the pass's own lights),
                    // then zero the shadow-light count (menu lights cast
                    // none). Restored after the draw — these passes live in
                    // the UI3D accumulator's enrolled lists and the engine
                    // must never see our rebind afterwards.
                    RE::BSLight** saved_scene_lights = nullptr;
                    std::uint8_t saved_num_lights = 0;
                    std::uint8_t saved_shadow_lights = 0;
                    // v6.47: SELF-BUILT STUDIO LIGHTS (user direction: drop
                    // the menu-light dependence entirely). Two persistent
                    // NiPointLights (key + fill) created on the engine heap
                    // via CLib's NiPointLight::Create factory, attached under
                    // a UI3D menuObjects root so their world transforms are
                    // live scene-graph members; each wrapped in a minimal
                    // BSLight shell (engine-heap 0x140 bytes + the real
                    // VTABLE_BSLight — the engine only reads fields and
                    // IsShadowLight inside our window). The per-pass override
                    // now points P's passes at THIS array unconditionally —
                    // no freshness gate, no dungeon-light fallback. The rig
                    // below then moves the lights' NODES (the v6.37 verified
                    // path — point lights carry a parent now).
                    ensure_studio_lights();
                    const bool override_lights = m_studio_light_count > 0 && m_studio_light_array;
                    if (override_lights)
                    {
                        saved_scene_lights = pass->sceneLights;
                        saved_num_lights = pass->numLights;
                        saved_shadow_lights = pass->numShadowLights;
                        pass->numLights = m_studio_light_count;
                        pass->numShadowLights = 0;
                        pass->sceneLights = m_studio_light_array;
                        // v6.55: PER-PASS shell patch. Run 88: the engine's
                        // light-update tick keeps rewriting lodDimmer back to
                        // 0 on our shells (they are foreign to its LOD fade
                        // bookkeeping), so a fetch-time patch is always one
                        // tick behind. The pass window is OURS — re-assert
                        // the fields right here, after any engine tick and
                        // before the draw, every pass.
                        for (std::uint32_t i = 0; i < pass->numLights && i < 4; ++i)
                        {
                            if (auto* shell = pass->sceneLights ? pass->sceneLights[i] : nullptr)
                            {
                                shell->lodDimmer = 1.0f;
                                shell->luminance = 1.0f;
                                shell->frustrumCull = 0;
                            }
                        }
                        if (!m_p_studio_lights_logged)
                        {
                            m_p_studio_lights_logged = true;
                            logger::info("Proto v6.47 studio lights applied to P passes: {} SELF-BUILT "
                                         "point light(s) (menu-light dependence removed)",
                                m_studio_light_count);
                        }
                    }
                    // v6.33: frontal light rig on the ACTIVE lights — menu
                    // lights when the freshness gate arms, the pass's own
                    // (dungeon) lights otherwise. The v6.32 pre-loop rig
                    // shared the freshness gate and never fired in steady
                    // state (the item preview does not re-render every menu
                    // frame — run 43), leaving the figure on its dungeon
                    // lights: dim and side-lit (run 65 screenshot). Per-pass
                    // save/mutate/restore: the world's own draws happen
                    // outside this window and never see the repositioning.
                    //
                    // v6.37: WHERE the shader reads light positions from.
                    // CS source (LightLimitFix.cpp:275 + InverseSquare
                    // BSLight_GetLuminance) reads NiLight::world.translate —
                    // the NiAVObject node transform; CS LightEditor moves
                    // lights via parent->local.translate + parent->Update.
                    // BSLight::worldTranslate (v6.33-6.36's target) is the
                    // CULLER's copy: run 70 proved writing it does nothing
                    // to the shading (rig landed exactly, pixels unchanged).
                    // So the rig now mutates the NiLight node itself:
                    // set local.translate relative to its parent and cascade
                    // the update, exactly the LightEditor-verified path.
                    RE::NiPoint3 saved_light_pos[4] = {};
                    RE::NiPoint3 saved_node_pos[4] = {};
                    RE::NiNode* saved_node_parent[4] = {};
                    bool node_mutated[4] = {};
                    const std::uint32_t rig_count = pass->numLights < 4 ? pass->numLights : 4;
                    {
                        if (auto* ui3d = RE::UI3DSceneManager::GetSingleton(); ui3d && ui3d->camera)
                        {
                            const auto& w2c = ui3d->camera->GetRuntimeData().worldToCam;
                            // v6.36: the w2c ROWS are NOT unit vectors (run 69
                            // arithmetic: |row0|=6.27, |row1|=11.15 — they carry
                            // the menu-camera's zoom scale). Normalize; the
                            // PLANE directions were always right (+X right, +Z
                            // up, -Y into the screen).
                            RE::NiPoint3 right{ w2c[0][0], w2c[0][1], w2c[0][2] };
                            RE::NiPoint3 up{ w2c[1][0], w2c[1][1], w2c[1][2] };
                            RE::NiPoint3 forward{ w2c[2][0], w2c[2][1], w2c[2][2] };
                            const float right_len = right.Length();
                            const float up_len = up.Length();
                            const float forward_len = forward.Length();
                            if (right_len > 1e-6f)
                                right *= 1.0f / right_len;
                            if (up_len > 1e-6f)
                                up *= 1.0f / up_len;
                            if (forward_len > 1e-6f)
                                forward *= 1.0f / forward_len;
                            const RE::NiPoint3 anchor = PInstance::instance().studio_anchor();
                            RE::NiUpdateData update_data{ 0.0f, RE::NiUpdateData::Flag::kDirty };
                            for (std::uint32_t i = 0; i < rig_count; ++i)
                            {
                                auto* light = pass->sceneLights ? pass->sceneLights[i] : nullptr;
                                if (!light)
                                    continue;
                                saved_light_pos[i] = light->worldTranslate;
                                const float spread =
                                    (static_cast<float>(i) - (rig_count - 1) * 0.5f) * 45.0f;
                                const RE::NiPoint3 light_target =
                                    anchor - forward * 70.0f + up * 50.0f + right * spread;
                                light->worldTranslate = light_target;  // culler copy — kept for free
                                // v6.37/v6.55: the node path the shaders
                                // actually read. v6.55: write the LIGHT
                                // NODE's own local (not the shared rig
                                // parent's) — the v6.51 shared-parent writes
                                // overwrote each other (both shells ended at
                                // light[1]'s position). Each light node now
                                // owns its placement; the rig parent stays at
                                // identity and only carries the cascade.
                                if (auto* ni_light = light->light.get(); ni_light)
                                {
                                    // v6.59: the rig parent sits AT the
                                    // anchor (v6.58 pull-in), so the node's
                                    // local must be the OFFSET from the
                                    // anchor (spread/up/forward only) —
                                    // writing the full accumulator-space
                                    // target here double-counted the anchor
                                    // and pushed the lights twice as far
                                    // away (the run-92 dim look).
                                    const RE::NiPoint3 local_offset{ right * spread + up * 50.0f -
                                                                     forward * 70.0f };
                                    ni_light->local.translate = local_offset;
                                    if (ni_light->parent)
                                    {
                                        saved_node_pos[i] = ni_light->local.translate;
                                        saved_node_parent[i] = ni_light->parent;
                                        ni_light->parent->UpdateDownwardPass(update_data, 0);
                                        node_mutated[i] = true;
                                    }
                                }
                            }
                            if (!m_p_frontal_logged)
                            {
                                m_p_frontal_logged = true;
                                logger::info(
                                    "Proto v6.37 frontal light rig: {} light(s) repositioned "
                                    "(menu-lights={}); anchor=({:.1f},{:.1f},{:.1f}) "
                                    "row_lengths=({:.2f},{:.2f},{:.2f})",
                                    rig_count, override_lights ? 1 : 0, anchor.x, anchor.y, anchor.z,
                                    right_len, up_len, forward_len);
                                for (std::uint32_t i = 0; i < rig_count; ++i)
                                {
                                    auto* light = pass->sceneLights ? pass->sceneLights[i] : nullptr;
                                    if (!light)
                                        continue;
                                    // v6.37 light census: type (NiRTTI chain tells
                                    // NiDirectionalLight vs NiPointLight), radius,
                                    // fade, parent chain — the facts needed to
                                    // decide between "reposition" and "own light".
                                    const char* rtti_name = "<null>";
                                    const char* rtti_base = "<null>";
                                    float radius_x = 0.0f, fade = 0.0f;
                                    const char* parent_name = "<no node>";
                                    RE::NiPoint3 ni_world{ 0.0f, 0.0f, 0.0f };
                                    if (auto* ni_light = light->light.get()) {
                                        if (const auto* rtti = ni_light->GetRTTI()) {
                                            rtti_name = rtti->name;
                                            rtti_base = rtti->baseRTTI ? rtti->baseRTTI->name : "-";
                                        }
                                        const auto& rd = ni_light->GetLightRuntimeData();
                                        radius_x = rd.radius.x;
                                        fade = rd.fade;
                                        parent_name = ni_light->parent ? ni_light->parent->name.c_str()
                                                                       : "<no parent>";
                                        // v6.51: the ACTUAL shader-side position.
                                        // If this stays at/near the origin while
                                        // bs_new moved, the cascade is still
                                        // being short-circuited.
                                        ni_world = ni_light->world.translate;
                                    }
                                    logger::info(
                                        "  light[{}]: point={} ambient={} dynamic={} lum={:.3f} "
                                        "lodDimmer={:.3f} rtti={} base={} radius.x={:.1f} fade={:.3f} "
                                        "parent='{}' bs_old=({:.1f},{:.1f},{:.1f}) "
                                        "bs_new=({:.1f},{:.1f},{:.1f}) ni_world=({:.1f},{:.1f},{:.1f}) "
                                        "node_moved={}",
                                        i, light->pointLight, light->ambientLight, light->dynamic,
                                        light->luminance, light->lodDimmer, rtti_name, rtti_base, radius_x, fade,
                                        parent_name, saved_light_pos[i].x, saved_light_pos[i].y,
                                        saved_light_pos[i].z, light->worldTranslate.x,
                                        light->worldTranslate.y, light->worldTranslate.z,
                                        ni_world.x, ni_world.y, ni_world.z,
                                        node_mutated[i] ? 1 : 0);
                                }
                            }
                        }
                    }
                    call_site_original(1, pass, pass->passEnum, (pass->passEnum & 0x40) != 0, 0x200);
                    // v6.33: restore the positions FIRST (the same array the
                    // next pass may reuse), then the array swap. v6.37: the
                    // node path restores local.translate + re-cascades too.
                    RE::NiUpdateData restore_data{ 0.0f, RE::NiUpdateData::Flag::kDirty };
                    for (std::uint32_t i = 0; i < rig_count; ++i)
                    {
                        auto* light = pass->sceneLights ? pass->sceneLights[i] : nullptr;
                        if (!light)
                            continue;
                        light->worldTranslate = saved_light_pos[i];
                        // v6.55: restore the LIGHT NODE's own local (matches
                        // the new per-node placement above). For the
                        // self-built lights the saved value IS the fresh one
                        // (each node owns its placement), so the cascade
                        // simply re-affirms it.
                        if (node_mutated[i] && saved_node_parent[i])
                        {
                            saved_node_parent[i]->UpdateDownwardPass(restore_data, 0);
                        }
                    }
                    if (override_lights)
                    {
                        pass->sceneLights = saved_scene_lights;
                        pass->numLights = saved_num_lights;
                        pass->numShadowLights = saved_shadow_lights;
                    }
                    ++drawn;
                }

                // Run 47/52: per-frame draw summary — how many passes were
                // handed to the engine's SetupAndDrawPass this frame. Pixel
                // truth is RenderDoc / the close-dump TGA; the renderer-init
                // heartbeat (PInstance) shows whether the device buffers
                // appear once SetupAndDrawPass processes the geometries.
                logger::info("Proto v6.17 P draw (SetupAndDrawPass): submitted={} of {} passes (source={})",
                    drawn, passes.size(),
                    passes.empty() && !m_pass_recipes.empty() ? "recipes-failed" : "live/recipes");
                // v6.63: the close-dump gate counts a successful proactive
                // draw as studio content (P-only opens must dump too).
                if (drawn > 0)
                    m_p_drew_this_open = true;
                runtime.context->OMSetRenderTargets(1, &prev_rtv, prev_dsv);
                runtime.context->OMSetDepthStencilState(prev_ds, prev_stencil_ref);
                runtime.context->RSSetViewports(prev_viewport_count, &prev_viewport);
                // v6.46: hand the ledger back — let the engine rebuild its
                // own render-target bindings the next time it applies state
                // (the same handshake CS's Deferred uses every frame).
                if (m_rt_dirty_guard_used)
                {
                    m_rt_dirty_guard_used = false;
                    RE::BSGraphics::RendererShadowState::GetSingleton()
                        ->GetRuntimeData()
                        .stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);
                }
                // v6.58: the draw window is over — park the light rig back
                // OUT of the world (100k units up) so the engine's light
                // ticks can never render the studio lights into the world
                // scene (the run-91 dungeon-wall report).
                park_studio_rig();

                if (prev_ds)
                    prev_ds->Release();
                if (prev_rtv)
                    prev_rtv->Release();
                if (prev_dsv)
                    prev_dsv->Release();

                ++m_p_total_replays;  // feeds the close-dump gate too
                if (!m_p_draw_logged)
                {
                    m_p_draw_logged = true;
                    logger::info("Proto v6.8 P drawn proactively: {} passes via GetRenderPasses", passes.size());
                }
            }

            // SetupAndDrawPass with the pass's own recorded state. Run-31
            // logs show technique == passEnum on every observed pass, and
            // the alpha-test flag correlates with passEnum bit 6 (0x40):
            // 0x140C9/0x14049 -> alphaTest, 0x14045/0x14031 -> no. The
            // renderFlags observed on menu-stream passes are 0x200; the
            // kNormal menu path uses the same batch renderer entry, whose
            // CS interposer (when present) the pass hooks restored.
            static void call_site_original_from_pass(RE::BSRenderPass* pass)
            {
                call_site_original(1, pass, pass->passEnum, (pass->passEnum & 0x40) != 0, 0x200);
            }

            // v2.5 order: the replay runs AFTER the original call. The
            // original's internal shadow-state application consumed the dirty
            // flags and left every pipeline slot (OM, textures, constants,
            // shaders) exactly as this pass was drawn with; a second call now
            // needs no state capture or restore — it re-draws 1:1, with our
            // render target as the only difference. The earlier pre-call order
            // had to consume the dirty flags itself and then fight the
            // re-application slot family by slot family (OM in run 11, then
            // the SRV set in run 12).
            // Run 37 (v6.0): the pass arguments are now only the WINDOW
            // trigger — the pass itself is not replayed (the panel shows
            // only P), so the parameters go unnamed.
            void replay_after_original(RE::BSRenderPass*, std::uint32_t, bool, std::uint32_t, std::size_t)
            {
                auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
                if (!renderer)
                    return;
                auto& runtime = renderer->GetRuntimeData();
                if (!runtime.context || !runtime.forwarder)
                    return;

                REX::W32::ID3D11RenderTargetView* prev_rtv = nullptr;
                REX::W32::ID3D11DepthStencilView* prev_dsv = nullptr;
                runtime.context->OMGetRenderTargets(1, &prev_rtv, &prev_dsv);
                REX::W32::D3D11_VIEWPORT prev_viewport{};
                std::uint32_t viewport_count = 1;
                runtime.context->RSGetViewports(&viewport_count, &prev_viewport);

                if (!prev_rtv)
                {
                    if (prev_dsv)
                        prev_dsv->Release();
                    return;
                }

                if (!m_state_logged)
                {
                    m_state_logged = true;
                    log_rtv_identity("engine target at call site", prev_rtv);
                }

                // Evidence snapshot: the texture set the original draw used.
                if (!m_srv_logged)
                {
                    m_srv_logged = true;
                    REX::W32::ID3D11ShaderResourceView* srvs[16] = {};
                    runtime.context->PSGetShaderResources(0, 16, srvs);
                    log_srv_set("PS SRVs at replay (post-original set)", srvs);
                    release_srvs(srvs);
                }

                // Size the private target from the call site's own render
                // target (run 12: format 28 = R8G8B8A8_UNORM, the UI composite
                // — not kMAIN's R11G11B10); the depth buffer matches the
                // engine's bound depth format (normalized to its typed
                // counterpart — engine depth resources are usually typeless)
                // when there is one.
                REX::W32::DXGI_FORMAT depth_format = REX::W32::DXGI_FORMAT_D24_UNORM_S8_UINT;
                if (prev_dsv)
                {
                    REX::W32::ID3D11Resource* depth_resource = nullptr;
                    prev_dsv->GetResource(&depth_resource);
                    if (depth_resource)
                    {
                        REX::W32::D3D11_TEXTURE2D_DESC depth_desc{};
                        static_cast<REX::W32::ID3D11Texture2D*>(depth_resource)->GetDesc(&depth_desc);
                        depth_format = normalize_depth_format(depth_desc.format);
                        depth_resource->Release();
                    }
                }

                REX::W32::D3D11_TEXTURE2D_DESC template_desc{};
                bool template_ok = false;
                {
                    REX::W32::ID3D11Resource* resource = nullptr;
                    prev_rtv->GetResource(&resource);
                    if (resource)
                    {
                        static_cast<REX::W32::ID3D11Texture2D*>(resource)->GetDesc(&template_desc);
                        resource->Release();
                        template_ok = true;
                    }
                }
                if (!template_ok)
                {
                    // Cannot size the studio target from the call site; skip
                    // silently (never observed in runs 9-16).
                    prev_rtv->Release();
                    if (prev_dsv)
                        prev_dsv->Release();
                    return;
                }

                // v4.1: capture the call-site target (AddRef) for the panel
                // composite — run 20 proved the DrawInterfaceStart-entry
                // target is a non-visible intermediate (format 24), while
                // THIS target carries the visible menu preview (format 28,
                // runs 11-19). A pointer change (resolution change
                // reallocating the pool) triggers a recapture.
                if (s_panel_rtv != prev_rtv)
                {
                    if (s_panel_rtv)
                        s_panel_rtv->Release();
                    s_panel_rtv = prev_rtv;
                    s_panel_rtv->AddRef();
                    logger::info("Proto v4.6 panel composite target captured: {}x{} format={}",
                        template_desc.width, template_desc.height, static_cast<int>(template_desc.format));
                }

                // The studio target is persistent: created from the call
                // site's own description, kept across frames, self-recreated
                // when the description changes (resolution change), and
                // released only when the panel closes (FR-06, render thread).
                // A configuration whose creation failed stays failed for this
                // panel open — retry on reopen or desc change only, instead
                // of hammering CreateTexture2D every frame.
                OffscreenTarget& target = offscreen_target();
                const TargetSig sig{ template_desc.width, template_desc.height,
                    static_cast<std::uint32_t>(template_desc.format),
                    static_cast<std::uint32_t>(depth_format) };
                if (!target.rtv || sig != target_sig(target))
                {
                    if (m_target_failed && sig == m_failed_sig)
                    {
                        prev_rtv->Release();
                        if (prev_dsv)
                            prev_dsv->Release();
                        return;
                    }
                    if (!target.create(runtime.forwarder, template_desc, depth_format))
                    {
                        logger::warn("Proto v3 studio target creation failed ({}x{} format={} depth={}); "
                                     "replays disabled until the panel reopens or the format changes",
                            sig.width, sig.height, sig.format, sig.depth_format);
                        m_target_failed = true;
                        m_failed_sig = sig;
                        prev_rtv->Release();
                        if (prev_dsv)
                            prev_dsv->Release();
                        return;
                    }
                    m_target_failed = false;
                }

                // The engine's depth-stencil state at this point is whatever
                // the original draw left; bind our own depth-on state so the
                // pass's depth test runs against the private buffer, and
                // restore the engine's afterwards.
                REX::W32::ID3D11DepthStencilState* prev_ds_state = nullptr;
                std::uint32_t prev_stencil_ref = 0;
                runtime.context->OMGetDepthStencilState(&prev_ds_state, &prev_stencil_ref);

                REX::W32::D3D11_VIEWPORT viewport = prev_viewport;
                viewport.topLeftX = 0.0f;
                viewport.topLeftY = 0.0f;
                viewport.width = static_cast<float>(target.width);
                viewport.height = static_cast<float>(target.height);

                runtime.context->OMSetRenderTargets(1, &target.rtv, target.dsv);
                runtime.context->OMSetDepthStencilState(target.ds_state, 0);
                runtime.context->RSSetViewports(1, &viewport);

                // Clear both buffers once per armed frame; later passes draw
                // on top with depth testing, so intra-item occlusion works.
                if (!m_cleared)
                {
                    const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
                    runtime.context->ClearRenderTargetView(target.rtv, clear_color);
                    runtime.context->ClearDepthStencilView(target.dsv,
                        REX::W32::D3D11_CLEAR_DEPTH | REX::W32::D3D11_CLEAR_STENCIL, 1.0f, 0);
                    m_cleared = true;
                }

                // Run 37 (v6.0) / v6.57: the item pass itself is NOT replayed
                // into the studio, and the REPLAY WINDOW NO LONGER DRAWS OR
                // COMPOSITES. Run 90 verdict (user comparison): the replay
                // window's P draw ran in the item pass's post-original
                // context (its light constants / strict buffer freshly
                // active), producing the dark highlight-window panel — while
                // the no-highlight window (end_frame only) was correct. The
                // user's architecture call: with menu-light arming retired
                // (v6.53), item replay retired (v6.0), and the end_frame
                // kFRAMEBUFFER composite proven (v6.42+), thunk_site has
                // exactly ONE remaining duty — P passthrough
                // suppression — and the panel draws/composites ONLY in
                // end_frame. One path, one context, no cross-contamination.
                // No dirty flags are left, so nothing re-applies and our bind
                // is expected to survive; verify once per frame.
                REX::W32::ID3D11RenderTargetView* post_rtv = nullptr;
                REX::W32::ID3D11DepthStencilView* post_dsv = nullptr;
                runtime.context->OMGetRenderTargets(1, &post_rtv, &post_dsv);
                const bool survived = post_rtv == target.rtv;
                if (!m_binding_logged)
                {
                    m_binding_logged = true;
                    logger::info("Proto v3 replay (post-original) binding survived: {}", survived);
                    if (!survived)
                        log_rtv_identity("post-replay target", post_rtv);
                }
                if (post_rtv)
                    post_rtv->Release();
                if (post_dsv)
                    post_dsv->Release();

                runtime.context->OMSetRenderTargets(1, &prev_rtv, prev_dsv);
                runtime.context->OMSetDepthStencilState(prev_ds_state, prev_stencil_ref);
                runtime.context->RSSetViewports(1, &prev_viewport);

                // v4.5/v6.57: the IN-REPLAY COMPOSITE IS RETIRED. The panel
                // composites ONLY at end_frame into kFRAMEBUFFER (v6.42,
                // proven) — keeping a second composite source here meant
                // highlight windows drew the quad into a replay-window
                // context the run-90 comparison showed dark. Menu-only gate
                // retained in comment for history; world frames never
                // composite here anymore at all.
                if (prev_ds_state)
                    prev_ds_state->Release();
                prev_rtv->Release();
                if (prev_dsv)
                    prev_dsv->Release();

                ++m_p_total_replays;
            }

            RE::NiNode* m_roots[8]{};
            RE::NiNode* m_logged_roots[8]{};
            std::size_t m_root_count = 0;
            std::size_t m_logged_root_count = 0;
            // Menu geometries already logged this panel open (discovery cap).
            std::unordered_set<const RE::BSGeometry*> m_seen_geoms;
            // P geometries already logged this panel open (run 29: P passes
            // arrive in the WORLD stream, outside the menu bracket).
            std::unordered_set<const RE::BSGeometry*> m_seen_p_geoms;
            std::uint32_t m_generation = 0;
            std::uint32_t m_frame_index = 0;
            std::uint32_t m_passes_seen = 0;
            std::uint32_t m_geoms_logged = 0;
            std::uint32_t m_p_geoms_logged = 0;
            std::uint32_t m_menu_passes = 0;
            std::uint32_t m_p_passes = 0;
            std::uint32_t m_menu_lighting_replayed = 0;
            // Replays accumulated since the last end_frame summary; the
            // close-dump gate (m_session_replays) consumes them so a P-only
            // session still writes its evidence TGA.
            std::uint32_t m_p_total_replays = 0;
            std::uint32_t m_last_logged_replayed = 0;
            std::uint32_t m_content_frames = 0;
            std::uint32_t m_session_replays = 0;
            // v6.63: the proactive P draw submitted at least one pass this
            // open — the close-dump gate counts it as studio content.
            bool m_p_drew_this_open = false;
            bool m_cleared = false;
            bool m_target_failed = false;
            TargetSig m_failed_sig{};
            bool m_state_logged = false;
            bool m_srv_logged = false;
            bool m_binding_logged = false;
            bool m_in_frame = false;
            // v6.38: set by the in-replay composite, consumed by end_frame's
            // no-menu-pass fallback — one composite per studio frame.
            bool m_composited_this_frame = false;
            std::uint32_t m_end_frame_composites = 0;
            // v6.44: one-shot traces for the self-create block's skip paths
            // (run 77: the block skipped with zero log lines — silence made
            // the skip path unidentifiable).
            bool m_selfcreate_trace_logged = false;
            bool m_fb_texture_null_logged = false;
            // v6.46: the dirty-bit guard fired this studio window (paired
            // set-back at the restore).
            bool m_rt_dirty_guard_used = false;
            // v6.47: the self-built studio lights. Shells live for the
            // session (render thread only); the NiLights are owned by the
            // scene graph (menuObjects[0]) AND these pointers (the shell's
            // NiPointer holds one ref, these hold another — detach would
            // need both cleared; despawn scope is session end, where leak-
            // on-exit is acceptable for the proto).
            RE::BSLight* m_studio_lights[Studio_Light_Count] = {};
            // v6.66: the ShadowSceneNode the lights were last registered
            // with — a changed pointer means the world was rebuilt (menu
            // transition / load) and the old wrappers are gone.
            RE::ShadowSceneNode* m_studio_lights_node = nullptr;
            // v6.67: the UI3D host the rig was last created under — a
            // changed pointer means the menu scene was rebuilt and the old
            // rig is orphaned (its lights no longer reach the ledger).
            RE::NiNode* m_studio_lights_host = nullptr;
            // v6.67: the incomplete-fetch warn latch (cleared on success).
            bool m_fetch_warned = false;
            RE::NiPointer<RE::NiLight> m_studio_light_ni[Studio_Light_Count];
            // v6.51: the private rig node the lights hang from — fresh,
            // flag-free, so forced cascades actually move them.
            RE::NiPointer<RE::NiNode> m_studio_rig_node;
            bool m_studio_lights_failed = false;
            bool m_p_draw_logged = false;
            bool m_p_binding_logged = false;
            bool m_p_drawn_this_frame = false;
            bool m_p_recipe_logged = false;
            bool m_p_empty_live_logged = false;
            // Run 54: the pass just classified by on_pass — the thunks read
            // it (same render-thread call) to keep the engine from ever
            // drawing P's passes (world double + menu teardown crash).
            bool m_last_pass_p_geom = false;
            // Run 54: once-per-open marker for the no-root early return in
            // draw_p_proactively — a silent kNone here is what swallowed
            // run 53's steady-state draws without a single log line.
            bool m_p_no_root_logged = false;
            // v6.33: once-per-session marker for the frontal light rig log.
            bool m_p_frontal_logged = false;
            // Run 53/56: the item preview's menu lights — the studio
            // lighting for P's passes (whose own lights are dungeon/world
            // lights, thousands of units from the menu-space fragments the
            // pose produces). Run 56: we now reference the ITEM PASS'S OWN
            // sceneLights ARRAY (the exact storage the engine — and CS's
            // light hooks — use for the item's own draw that frame)
            // instead of copying BSLight pointers into a member array:
            // run 55's session CRASHED in CS's
            // GeometrySetupConstantPointLights after the copied pointers
            // outlived the light objects (item selection changed mid
            // session). The freshness flag gates the override to frames
            // where a menu lighting pass actually arrived.
            RE::BSLight** m_studio_light_array = nullptr;
            std::uint8_t m_studio_light_count = 0;
            bool m_studio_lights_fresh = false;
            bool m_p_studio_lights_logged = false;

            // Run 46: pass recipes recorded on successful live generation —
            // the paused inventory stops regenerating passes, so later
            // frames rebuild them via BSShader::MakeRenderPass (ID 107497).
            // All referenced objects live for the panel open; cleared on
            // release_target.
            struct PPassRecipe
            {
                RE::BSShader* shader;
                RE::BSShaderProperty* property;
                RE::BSGeometry* geometry;
                std::uint32_t technique;
                std::uint8_t num_lights;
                RE::BSLight* lights[4];
            };
            std::vector<PPassRecipe> m_pass_recipes;
        };
    }

    namespace
    {
        // Set by install_hook() before any menu frame can run the thunk;
        // Detours relocates the overwritten prologue into its own trampoline,
        // so calling this runs the true original function.
        DrawInterfaceStart_t s_original_draw_interface_start = nullptr;

        // Render-thread detour. DrawInterfaceStart runs once per rendered
        // frame (the HUD is a menu too); the panel state decides whether
        // this frame is bracketed for studio redirection.
        void draw_interface_start_thunk(std::int64_t a1)
        {
            DrawInterfaceStart_t const original = s_original_draw_interface_start;
            if (!original)
            {
                // Detour not fully installed; bail out without recursing.
                return;
            }

            // The P build state machine is paced by REAL rendered frames:
            // each DrawInterfaceStart queues at most one game-thread step
            // (run-28 finding — a self-rescheduling task drains within one
            // game frame and never lets the engine load anything).
            PInstance::instance().pump();

            if (!Proto::instance().panel_frame_active())
            {
                // Panel closed: run FR-06 cleanup here on the render thread
                // — a user close writes one evidence TGA first, a
                // force-close releases silently — then draw untouched. The
                // retired P graphs (stage 2) also drain here: this frame
                // provably runs no pass hooks, so no in-flight pass can
                // still read the detached scene graph.
                if (Proto::instance().take_release_pending())
                {
                    if (Proto::instance().take_dump_on_close())
                        PassRedirector::instance().dump_and_release("panel closed");
                    else
                        PassRedirector::instance().release_target("panel force-closed");
                }
                PInstance::instance().drain_retired();
                original(a1);
                return;
            }

            // Panel open: bracket the menu draw. The pass hooks replay menu
            // lighting passes into the persistent studio target while it is
            // open; the panel composite runs at end_frame into THIS frame's
            // composite instance (v4.2); the menu frame itself renders
            // normally.
            PassRedirector::instance().begin_frame();
            original(a1);
            PassRedirector::instance().end_frame();
        }
    }

    bool Proto::install_hook()
    {
        // Detours-based entry detour (same mechanism as Community Shaders'
        // stl::detour_thunk): the overwritten prologue bytes are relocated to
        // a trampoline, so the thunk can call the original function body. A
        // raw write_call<5> on the entry is NOT viable here — the entry is a
        // 5-byte jmp whose bytes would be lost and the thunk would recurse
        // into itself (the main-menu stack overflow seen in the first run).
        static DrawInterfaceStart_t original =
            reinterpret_cast<DrawInterfaceStart_t>(REL::RelocationID(79947, 82084).address());

        DetourRestoreAfterWith();
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        if (DetourAttach(reinterpret_cast<PVOID*>(&original),
                reinterpret_cast<PVOID>(draw_interface_start_thunk)) != NO_ERROR ||
            DetourTransactionCommit() != NO_ERROR)
        {
            DetourTransactionAbort();
            logger::warn("Proto DetourAttach failed; the panel has no effect");
            return false;
        }
        s_original_draw_interface_start = original;
        logger::info("Proto DrawInterfaceStart detour installed at 0x{:X}",
            REL::RelocationID(79947, 82084).address());
        return true;
    }

    bool Proto::install_pass_hooks()
    {
        return PassRedirector::instance().install();
    }
}
