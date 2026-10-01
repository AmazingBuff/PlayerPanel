//
// Created by AmazingBuff on 2026/09/29.
//

#include "proto.h"

#include <RE/Skyrim.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>
#include <REX/W32/D3D11.h>

#include <Windows.h>
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
            REX::W32::ID3D11Texture2D* depth_texture = nullptr;
            REX::W32::ID3D11DepthStencilView* dsv = nullptr;
            REX::W32::ID3D11DepthStencilState* ds_state = nullptr;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            REX::W32::DXGI_FORMAT format = REX::W32::DXGI_FORMAT_UNKNOWN;
            REX::W32::DXGI_FORMAT depth_format = REX::W32::DXGI_FORMAT_UNKNOWN;

            void destroy()
            {
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

        // --- pass redirector (render thread only) --------------------------

        // Per-open discovery cap for menu-geometry log lines: each menu
        // geometry is logged once per panel open; afterwards only counters
        // advance (a persistent frame would otherwise spam the log at frame
        // rate).
        constexpr std::size_t Menu_Geom_Log_Cap = 256;
        // Slow log cadence for both summary flavors — one line per N frames
        // (~30 s at 60 fps). Content summaries additionally fire whenever
        // the replay count changes (item switches).
        constexpr std::uint32_t Heartbeat_Frames = 1800;
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
            // F7 dump, and summarize — but only on state changes or the slow
            // throttles, never unconditionally at frame rate.
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

            // Panel open: bracket the menu draw. While the bracket is open
            // the pass hooks replay menu lighting passes into the persistent
            // studio target; the menu frame itself renders normally.
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
