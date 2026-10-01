//
// Created by AmazingBuff on 2026/09/29.
//

#include "proto.h"

#include <RE/Skyrim.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>
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

        // --- composite (v4, work package 3) --------------------------------

        // M0 calibration constants: the panel occupies a fixed screen
        // fraction (58%..88% horizontally, 12%..68% vertically from the
        // top) and shows the WHOLE studio target squeezed into it (uv
        // 0..1). Framing and zoom are studio-camera concerns (M1), not
        // composite concerns; the rect moves behind a config only if run
        // 20 shows placement matters for verification.
        constexpr float Panel_Screen_MinX = 0.58f;
        constexpr float Panel_Screen_MaxX = 0.88f;
        constexpr float Panel_Screen_MinY = 0.12f;
        constexpr float Panel_Screen_MaxY = 0.68f;

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
cbuffer PanelCB : register(b0) { float4 g_ndcRect; }
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
        constexpr std::string_view Composite_PS = R"(
Texture2D g_tex : register(t0);
SamplerState g_samp : register(s0);
float4 ps_main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target {
    return g_tex.Sample(g_samp, uv);
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
                // x = 2u-1; y_top = 1-2*top, y_bottom = 1-2*bottom.
                const float cb_data[4] = {
                    2.0f * Panel_Screen_MinX - 1.0f, 1.0f - 2.0f * Panel_Screen_MaxY,
                    2.0f * Panel_Screen_MaxX - 1.0f, 1.0f - 2.0f * Panel_Screen_MinY
                };
                REX::W32::D3D11_BUFFER_DESC cb_desc{};
                cb_desc.byteWidth = sizeof(cb_data);
                cb_desc.usage = REX::W32::D3D11_USAGE_IMMUTABLE;
                cb_desc.bindFlags = REX::W32::D3D11_BIND_CONSTANT_BUFFER;

                bool ok = device->CreateSamplerState(&sampler_desc, &sampler) == 0 && sampler &&
                    device->CreateBlendState(&blend_desc, &blend) == 0 && blend &&
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

        private:
            CompositeRenderer() = default;

            REX::W32::ID3D11VertexShader* m_vs = nullptr;
            REX::W32::ID3D11PixelShader* m_ps = nullptr;
            REX::W32::ID3D11SamplerState* m_sampler = nullptr;
            REX::W32::ID3D11BlendState* m_blend = nullptr;
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
                m_passes_seen = 0;
                m_geoms_logged = 0;
                m_menu_passes = 0;
                m_menu_lighting_replayed = 0;
                m_cleared = false;
                ++m_frame_index;

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

                if (m_menu_passes == 0)
                {
                    if (m_frame_index % Heartbeat_Frames == 0)
                        logger::info("Proto v3 panel frame #{} heartbeat: passes_seen={} menu_passes=0 "
                                     "(content-free frame, target keeps last studio image)",
                            m_frame_index, m_passes_seen);
                    return;
                }

                ++m_content_frames;
                m_session_replays += m_menu_lighting_replayed;
                if (m_menu_lighting_replayed != m_last_logged_replayed ||
                    m_content_frames % Heartbeat_Frames == 1)
                {
                    m_last_logged_replayed = m_menu_lighting_replayed;
                    logger::info("Proto v3 panel frame #{}: passes_seen={} menu_passes={} "
                                 "lighting_replayed={} geoms_logged={}",
                        m_frame_index, m_passes_seen, m_menu_passes, m_menu_lighting_replayed,
                        m_geoms_logged);
                }
            }

            // Render-thread pass observation. Returns true when the pass is a
            // menu-scene BSLightingShader pass and should be replayed into
            // the private target AFTER the original call has run.
            bool on_pass(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test, std::uint32_t render_flags,
                std::size_t site_index)
            {
                if (!m_in_frame || !pass || !pass->geometry)
                    return false;

                ++m_passes_seen;
                const bool menu = is_menu_geometry(pass->geometry);
                if (menu)
                {
                    ++m_menu_passes;
                    // Discovery logging: one line per menu geometry per panel
                    // open; steady-state frames only advance the counters.
                    if (m_geoms_logged < Menu_Geom_Log_Cap && m_seen_geoms.insert(pass->geometry).second)
                    {
                        ++m_geoms_logged;
                        logger::info(
                            "Proto v3 menu pass: geom={} name=[{}] shader={} passEnum=0x{:X} "
                            "technique=0x{:X} alphaTest={} numLights={} renderFlags=0x{:X} site={}",
                            static_cast<void*>(pass->geometry),
                            pass->geometry->name.c_str() ? pass->geometry->name.c_str() : "(null)",
                            pass->shader ? std::to_underlying(pass->shader->shaderType.get()) : 0u,
                            pass->passEnum, technique, alpha_test, pass->numLights, render_flags,
                            site_index);
                    }
                }

                if (!menu)
                    return false;
                // v2 replays lighting passes only: they carry the shading
                // (spike: depth/shadow passes write no colour). Other menu
                // passes are counted and logged for the next iteration.
                if (!pass->shader || std::to_underlying(pass->shader->shaderType.get()) != 6)
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
                // v4.1: the captured composite target belongs to the session
                // too — release it so a changed resolution is recaptured on
                // the next open.
                if (s_panel_rtv)
                {
                    s_panel_rtv->Release();
                    s_panel_rtv = nullptr;
                }
                m_target_failed = false;
                m_session_replays = 0;
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
                if (offscreen_target().color && m_session_replays > 0)
                    dump_offscreen_to_log_dir();
                release_target(reason);
            }

        private:
            PassRedirector() = default;

            static void thunk_site0(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test,
                std::uint32_t render_flags)
            {
                const bool replay = instance().on_pass(pass, technique, alpha_test, render_flags, 0);
                call_site_original(0, pass, technique, alpha_test, render_flags);
                if (replay)
                    instance().replay_after_original(pass, technique, alpha_test, render_flags, 0);
            }
            static void thunk_site1(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test,
                std::uint32_t render_flags)
            {
                const bool replay = instance().on_pass(pass, technique, alpha_test, render_flags, 1);
                call_site_original(1, pass, technique, alpha_test, render_flags);
                if (replay)
                    instance().replay_after_original(pass, technique, alpha_test, render_flags, 1);
            }
            static void thunk_site2(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test,
                std::uint32_t render_flags)
            {
                const bool replay = instance().on_pass(pass, technique, alpha_test, render_flags, 2);
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

            // v2.5 order: the replay runs AFTER the original call. The
            // original's internal shadow-state application consumed the dirty
            // flags and left every pipeline slot (OM, textures, constants,
            // shaders) exactly as this pass was drawn with; a second call now
            // needs no state capture or restore — it re-draws 1:1, with our
            // render target as the only difference. The earlier pre-call order
            // had to consume the dirty flags itself and then fight the
            // re-application slot family by slot family (OM in run 11, then
            // the SRV set in run 12).
            void replay_after_original(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test,
                std::uint32_t render_flags, std::size_t site_index)
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

                call_site_original(site_index, pass, technique, alpha_test, render_flags);

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

                // v4.5: composite HERE — inside the original call, with the
                // call-site instance still bound. The passthrough wrote the
                // item's pixels into THIS target microseconds ago and they
                // ARE visible on screen, so whatever merges this instance
                // reads it after this point; a quad drawn now rides the same
                // merge. Runs 23/24 falsified every later placement: at
                // end_frame the merge has already happened inside the
                // original call (the run-24 rasterizer line — cull=1,
                // CullNone — also proves rasterization was never the
                // blocker, retracting the v4.4 winding diagnosis).
                if (CompositeRenderer::instance().ensure(runtime.forwarder))
                    CompositeRenderer::instance().draw(runtime.context, prev_rtv);

                if (prev_ds_state)
                    prev_ds_state->Release();
                prev_rtv->Release();
                if (prev_dsv)
                    prev_dsv->Release();

                ++m_menu_lighting_replayed;
            }

            RE::NiNode* m_roots[8]{};
            RE::NiNode* m_logged_roots[8]{};
            std::size_t m_root_count = 0;
            std::size_t m_logged_root_count = 0;
            // Menu geometries already logged this panel open (discovery cap).
            std::unordered_set<const RE::BSGeometry*> m_seen_geoms;
            std::uint32_t m_generation = 0;
            std::uint32_t m_frame_index = 0;
            std::uint32_t m_passes_seen = 0;
            std::uint32_t m_geoms_logged = 0;
            std::uint32_t m_menu_passes = 0;
            std::uint32_t m_menu_lighting_replayed = 0;
            std::uint32_t m_last_logged_replayed = 0;
            std::uint32_t m_content_frames = 0;
            std::uint32_t m_session_replays = 0;
            bool m_cleared = false;
            bool m_target_failed = false;
            TargetSig m_failed_sig{};
            bool m_state_logged = false;
            bool m_srv_logged = false;
            bool m_binding_logged = false;
            bool m_in_frame = false;
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

            if (!Proto::instance().panel_frame_active())
            {
                // Panel closed: run FR-06 cleanup here on the render thread
                // — a user close writes one evidence TGA first, a
                // force-close releases silently — then draw untouched.
                if (Proto::instance().take_release_pending())
                {
                    if (Proto::instance().take_dump_on_close())
                        PassRedirector::instance().dump_and_release("panel closed");
                    else
                        PassRedirector::instance().release_target("panel force-closed");
                }
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
