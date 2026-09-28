//
// Created by AmazingBuff on 2026/09/28.
//

#include "proto.h"

#include <RE/Skyrim.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>
#include <REX/W32/D3D11.h>

#include <Windows.h>
#include <detours/detours.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "proto_config.h"

using namespace std::literals;
namespace logger = SKSE::log;

// M0 prototype: verifies the capture report's finding 1 — the engine's
// current accumulator is a plain global slot, so a private frame can run by
// swapping it in, accumulating the UI3D menu scene into our own color/depth
// target, and restoring. The swap brackets MenuManager::DrawInterfaceStart
// (REL::RelocationID(79947, 82084)) on the render thread, the same entry the
// engine uses per menu frame; no culling-process fields are touched (the
// report flags BSCullingProcess extension offsets as unverified on 1.6.1170).

namespace CharacterPanelProto
{
    namespace
    {
        using DrawInterfaceStart_t = void (*)(std::int64_t);

        struct SwapTarget
        {
            REX::W32::ID3D11Texture2D* color = nullptr;
            REX::W32::ID3D11RenderTargetView* rtv = nullptr;
            REX::W32::ID3D11Texture2D* depth_texture = nullptr;
            REX::W32::ID3D11DepthStencilView* dsv = nullptr;
            std::uint32_t width = 0;
            std::uint32_t height = 0;

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
                rtv = nullptr;
                color = nullptr;
                dsv = nullptr;
                depth_texture = nullptr;
            }
        };

        SwapTarget& swap_target()
        {
            static SwapTarget s_target;
            return s_target;
        }

        bool create_swap_target(REX::W32::ID3D11Device* device, std::uint32_t width, std::uint32_t height)
        {
            SwapTarget& target = swap_target();
            target.destroy();

            // Size from the engine's kMAIN pool and reuse its HDR format: the
            // menu scene shaders emit scene-linear values shaped for it, the
            // same lesson as the stage-0 spike's readback.
            REX::W32::DXGI_FORMAT format = REX::W32::DXGI_FORMAT_R11G11B10_FLOAT;
            {
                auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
                if (renderer)
                {
                    auto& runtime = renderer->GetRuntimeData();
                    auto& main_pool = runtime.renderTargets[std::to_underlying(RE::RENDER_TARGET::kMAIN)];
                    if (main_pool.texture)
                    {
                        REX::W32::D3D11_TEXTURE2D_DESC desc{};
                        main_pool.texture->GetDesc(&desc);
                        width = desc.width;
                        height = desc.height;
                        format = desc.format;
                    }
                }
            }

            REX::W32::D3D11_TEXTURE2D_DESC color_desc{};
            color_desc.width = width;
            color_desc.height = height;
            color_desc.mipLevels = 1;
            color_desc.arraySize = 1;
            color_desc.format = format;
            color_desc.sampleDesc.count = 1;
            color_desc.usage = REX::W32::D3D11_USAGE_DEFAULT;
            color_desc.bindFlags = REX::W32::D3D11_BIND_RENDER_TARGET | REX::W32::D3D11_BIND_SHADER_RESOURCE;
            if (device->CreateTexture2D(&color_desc, nullptr, &target.color) != 0)
                return false;
            if (device->CreateRenderTargetView(target.color, nullptr, &target.rtv) != 0)
            {
                target.destroy();
                return false;
            }

            REX::W32::D3D11_TEXTURE2D_DESC depth_desc{};
            depth_desc.width = width;
            depth_desc.height = height;
            depth_desc.mipLevels = 1;
            depth_desc.arraySize = 1;
            depth_desc.format = REX::W32::DXGI_FORMAT_D24_UNORM_S8_UINT;
            depth_desc.sampleDesc.count = 1;
            depth_desc.usage = REX::W32::D3D11_USAGE_DEFAULT;
            depth_desc.bindFlags = REX::W32::D3D11_BIND_DEPTH_STENCIL;
            if (device->CreateTexture2D(&depth_desc, nullptr, &target.depth_texture) != 0 ||
                device->CreateDepthStencilView(target.depth_texture, nullptr, &target.dsv) != 0)
            {
                target.destroy();
                return false;
            }
            target.width = width;
            target.height = height;
            return true;
        }

        void dump_swap_result_to_log_dir()
        {
            auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
            SwapTarget& target = swap_target();
            if (!renderer || !target.color)
                return;

            auto* context = renderer->GetRuntimeData().context;
            if (!context)
                return;

            // GPU -> CPU copy of the swapped-frame color target.
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
            if (!renderer->GetRuntimeData().forwarder ||
                renderer->GetRuntimeData().forwarder->CreateTexture2D(&staging_desc, nullptr, &staging) != 0)
                return;
            context->CopyResource(staging, target.color);

            REX::W32::D3D11_MAPPED_SUBRESOURCE mapped{};
            HRESULT const hr = context->Map(staging, 0, REX::W32::D3D11_MAP_READ, 0, &mapped);
            if (FAILED(hr))
            {
                staging->Release();
                return;
            }

            // Decode R11G11B10_FLOAT rows and tone-map to an 8-bit BGRA TGA,
            // written under the SKSE log directory like the probe captures.
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

            std::vector<std::uint8_t> bgra;
            bgra.reserve(static_cast<std::size_t>(desc.width) * desc.height * 4);
            for (std::uint32_t y = 0; y < desc.height; ++y)
            {
                auto const* row = reinterpret_cast<std::uint8_t const*>(mapped.data) + y * mapped.rowPitch;
                for (std::uint32_t x = 0; x < desc.width; ++x)
                {
                    std::uint32_t packed = 0;
                    std::memcpy(&packed, row + static_cast<std::size_t>(x) * 4, sizeof(packed));
                    float const r = decode_r11f(packed & 0x7FF);
                    float const g = decode_r11f((packed >> 11) & 0x7FF);
                    float const b = decode_r11f((packed >> 22) & 0x3FF);
                    // Simple reinhard tone map + gamma for a viewable TGA.
                    auto tone = [](float v) {
                        v = v / (1.0f + v);
                        return static_cast<std::uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
                    };
                    bgra.push_back(tone(b));
                    bgra.push_back(tone(g));
                    bgra.push_back(tone(r));
                    bgra.push_back(255);
                }
            }
            context->Unmap(staging, 0);
            staging->Release();

            std::filesystem::path dir = SKSE::log::log_directory().value_or(std::filesystem::path(".")) /
                "CharacterPanelProto";
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            for (std::uint32_t index = 0; index < 1000; ++index)
            {
                std::filesystem::path file = dir / fmt::format("proto-swap-{:03}.tga", index);
                if (std::filesystem::exists(file))
                    continue;
                std::FILE* out = std::fopen(file.string().c_str(), "wb");
                if (!out)
                    return;
                std::uint8_t header[18] = {};
                header[2] = 2;  // uncompressed true-color
                header[12] = static_cast<std::uint8_t>(desc.width & 0xFF);
                header[13] = static_cast<std::uint8_t>((desc.width >> 8) & 0xFF);
                header[14] = static_cast<std::uint8_t>(desc.height & 0xFF);
                header[15] = static_cast<std::uint8_t>((desc.height >> 8) & 0xFF);
                header[16] = 32;
                header[17] = 0x20;  // top-down origin
                std::fwrite(header, 1, sizeof(header), out);
                std::fwrite(bgra.data(), 1, bgra.size(), out);
                std::fclose(out);
                logger::info("Proto swap dump written: {}", file.string());
                return;
            }
            logger::warn("Proto swap dump directory is full; TGA not written");
        }

        // --- swap state (render thread only) ---

        RE::BSShaderAccumulator* g_saved_accumulator = nullptr;
        std::uint32_t g_swap_runs = 0;

        // Run 1 established that the inventory 3D path never consults the
        // engine's global current-accumulator slot, so a bare swap draws
        // nothing. Run 2 drives the secondary accumulator directly, using
        // only CLib-bound, probe-verified entry points:
        //
        //   Renderer::StartAccumulating(camera, secondary, flags)  (99790)
        //     — sets up the secondary's batch renderer and publishes it as
        //       current; 130-byte wrapper ending in SetCurrentAccumulator.
        //   culler->Process2(camera, menuObject, visibleSet)       (vtable 17)
        //     — the engine's own per-frame menu-scene cull; with
        //       useVirtualAppend=true each visible geometry lands in the
        //       secondary via AppendVirtual.
        //   secondary->FinishAccumulating()                        (vtable 26)
        //     — flushes the accumulated batches into the bound render target.
        //
        // The private D3D11 target is bound around the whole sequence so any
        // draws land in the readback texture instead of the engine's targets.

        void run_swapped_frame(std::int64_t a1, DrawInterfaceStart_t original)
        {
            auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
            RE::UI3DSceneManager* ui3d = RE::UI3DSceneManager::GetSingleton();
            if (!renderer || !ui3d)
            {
                original(a1);
                return;
            }

            auto& runtime = renderer->GetRuntimeData();
            if (!runtime.context || !runtime.forwarder ||
                !create_swap_target(runtime.forwarder, runtime.renderWindows[0].windowWidth,
                    runtime.renderWindows[0].windowHeight))
            {
                logger::warn("Proto swap target unavailable; running unswapped frame");
                original(a1);
                return;
            }
            SwapTarget& target = swap_target();

            RE::BSShaderAccumulator* secondary = ui3d->unk18.get();
            RE::NiCamera* camera = ui3d->camera.get();
            RE::BSCullingProcess* culler = const_cast<RE::BSCullingProcess*>(ui3d->cullingProcess);
            if (!secondary || !camera || !culler)
            {
                logger::warn("Proto UI3D objects unavailable; running unswapped frame");
                original(a1);
                return;
            }

            // Collect live menu-object roots; the cull walks each one.
            std::vector<RE::NiAVObject*> roots;
            for (auto const& menu_object : ui3d->menuObjects)
            {
                if (menu_object)
                    roots.push_back(menu_object.get());
            }
            if (roots.empty())
            {
                logger::warn("Proto found no menu objects; running unswapped frame");
                original(a1);
                return;
            }

            // Visible-set buffer for the cull. 4096 geometry slots is well
            // above the menu scene's object count (probe: 8 objects).
            constexpr std::size_t Visible_Capacity = 4096;
            static std::vector<RE::BSGeometry*> s_visible_storage(Visible_Capacity, nullptr);
            RE::NiVisibleArray visible_set;
            visible_set.array = s_visible_storage.data();
            visible_set.currentSize = 0;
            visible_set.allocatedSize = Visible_Capacity;
            visible_set.growBy = 1024;

            // Bind our color/depth target for the direct-drive draws.
            REX::W32::ID3D11RenderTargetView* prev_rtv = nullptr;
            REX::W32::ID3D11DepthStencilView* prev_dsv = nullptr;
            runtime.context->OMGetRenderTargets(1, &prev_rtv, &prev_dsv);
            REX::W32::D3D11_VIEWPORT prev_viewport{};
            std::uint32_t viewport_count = 1;
            runtime.context->RSGetViewports(&viewport_count, &prev_viewport);

            runtime.context->OMSetRenderTargets(1, &target.rtv, target.dsv);
            REX::W32::D3D11_VIEWPORT viewport = prev_viewport;
            viewport.width = static_cast<float>(target.width);
            viewport.height = static_cast<float>(target.height);
            viewport.topLeftX = 0.0f;
            viewport.topLeftY = 0.0f;
            runtime.context->RSSetViewports(1, &viewport);
            const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            runtime.context->ClearRenderTargetView(target.rtv, clear_color);
            runtime.context->ClearDepthStencilView(target.dsv,
                REX::W32::D3D11_CLEAR_DEPTH | REX::W32::D3D11_CLEAR_STENCIL, 1.0f, 0);

            g_saved_accumulator = RE::BSShaderAccumulator::GetCurrentAccumulator();
            logger::info(
                "Proto direct-drive frame #{}: secondary {} camera {} roots {} saved-current {}",
                g_swap_runs + 1,
                static_cast<void*>(secondary),
                static_cast<void*>(camera),
                roots.size(),
                static_cast<void*>(g_saved_accumulator));

            // 1) Engine-sanctioned accumulator startup: batch renderer setup
            //    plus publishing the secondary as current.
            RE::BSGraphics::Renderer::StartAccumulating(camera, secondary, 0);

            // 2) Cull each menu-object root. Run 5 called UpdateWorldData on
            //    the roots and NO bound changed (radius stayed 0) — either the
            //    update needs the engine's downward pass, or the roots are
            //    container nodes whose geometry lives deeper. Run 6 runs the
            //    engine's own UpdateDownwardPass on each root, then censuses
            //    each subtree (node count, geometry count by RTTI name, bound
            //    radii) so the next log shows where the geometry actually
            //    lives and whether any bound is populated.
            RE::NiUpdateData update_data{ .time = 0.0f, .flags = RE::NiUpdateData::Flag::kNone };

            auto census = [](RE::NiAVObject* node, std::uint32_t depth, auto& census_ref,
                             std::uint32_t& nodes, std::uint32_t& geometries, float& max_radius,
                             bool& names_logged, std::string& names) -> void {
                if (!node || depth > 6 || nodes > 512)
                    return;
                ++nodes;
                if (node->worldBound.radius > max_radius)
                    max_radius = node->worldBound.radius;
                if (!names_logged && names.size() < 160)
                {
                    names += node->name.c_str() ? fmt::format("{} ", node->name.c_str()) : "(null) ";
                    if (nodes >= 8)
                        names_logged = true;
                }
                char const* rtti_name = node->GetRTTI() ? node->GetRTTI()->GetName() : "";
                if (std::strstr(rtti_name, "TriShape") || std::strstr(rtti_name, "Geometry") ||
                    std::strstr(rtti_name, "Particles"))
                    ++geometries;
                if (auto* as_node = node->AsNode())
                {
                    for (auto const& child : as_node->GetChildren())
                        census_ref(child.get(), depth + 1, census_ref, nodes, geometries, max_radius, names_logged, names);
                }
            };

            for (RE::NiAVObject* root : roots)
            {
                root->UpdateWorldData(&update_data);
                root->UpdateDownwardPass(update_data, 0);
            }
            for (std::size_t i = 0; i < roots.size() && i < 3; ++i)
            {
                std::uint32_t nodes = 0;
                std::uint32_t geometries = 0;
                float max_radius = 0.0f;
                bool names_logged = false;
                std::string names;
                census(roots[i], 0, census, nodes, geometries, max_radius, names_logged, names);
                logger::info(
                    "Proto census root[{}] name=[{}] nodes={} geometries={} max_bound_radius={} names: {}",
                    i, roots[i]->name.c_str() ? roots[i]->name.c_str() : "(null)", nodes, geometries, max_radius, names);
            }
            // Run 6 located the geometry: root[1] holds 14 TriShapes with
            // valid child bounds (max radius 37.4) — yet Process2 culled
            // everything. Run 7 bypasses the shared culler entirely: collect
            // the subtree's geometry leaves straight into the visible array
            // and feed RegisterObjectArray. This isolates stage B+C (does the
            // accumulator ingest and draw geometry handed to it?) from the
            // cull, whose unverified extension layout remains the report's
            // standing caveat.
            std::uint32_t collected = 0;
            auto collect_geometry = [&](auto&& collect_ref, RE::NiAVObject* node, std::uint32_t depth) -> void {
                if (!node || depth > 6 || collected >= Visible_Capacity)
                    return;
                char const* rtti_name = node->GetRTTI() ? node->GetRTTI()->GetName() : "";
                if (std::strstr(rtti_name, "TriShape") || std::strstr(rtti_name, "Geometry") ||
                    std::strstr(rtti_name, "Particles"))
                {
                    auto* geometry = static_cast<RE::BSGeometry*>(node);
                    if (geometry && !geometry->GetAppCulled() && collected < Visible_Capacity)
                        s_visible_storage[collected++] = geometry;
                }
                if (auto* as_node = node->AsNode())
                {
                    for (auto const& child : as_node->GetChildren())
                        collect_ref(collect_ref, child.get(), depth + 1);
                }
            };
            for (RE::NiAVObject* root : roots)
                collect_geometry(collect_geometry, root, 0);
            visible_set.currentSize = collected;
            logger::info("Proto stage A(cull-bypass): collected {} geometries", collected);

            // 3) Ingest the collected geometries and flush the batches.
            if (visible_set.currentSize > 0)
                secondary->RegisterObjectArray(visible_set);
            secondary->FinishAccumulating();

            // 4) Restore the engine's current accumulator and targets before
            //    the real menu draw runs.
            RE::BSShaderAccumulator::SetCurrentAccumulator(g_saved_accumulator);
            runtime.context->OMSetRenderTargets(1, &prev_rtv, prev_dsv);
            runtime.context->RSSetViewports(1, &prev_viewport);

            auto& secondary_data = secondary->GetRuntimeData();
            logger::info(
                "Proto direct-drive done: pass={} bucket={} active={} camera_world=({},{},{})",
                secondary_data.currentPass,
                secondary_data.currentBucket,
                secondary_data.currentActive,
                camera->world.translate.x,
                camera->world.translate.y,
                camera->world.translate.z);

            if (prev_rtv)
                prev_rtv->Release();
            if (prev_dsv)
                prev_dsv->Release();

            dump_swap_result_to_log_dir();
            ++g_swap_runs;

            original(a1);
        }
    }

    namespace
    {
        // Set by install_hook() before any menu frame can run the thunk;
        // Detours relocates the overwritten prologue into its own trampoline,
        // so calling this runs the true original function.
        DrawInterfaceStart_t s_original_draw_interface_start = nullptr;

        // Render-thread detour. DrawInterfaceStart runs once per rendered
        // menu frame; the armed flag decides whether this frame is the swap.
        void draw_interface_start_thunk(std::int64_t a1)
        {
            DrawInterfaceStart_t const original = s_original_draw_interface_start;
            if (!original)
            {
                // Detour not fully installed; bail out without recursing.
                return;
            }

            if (!Proto::instance().take_armed())
                original(a1);
            else
                run_swapped_frame(a1, original);
        }
    }

    bool Proto::take_armed()
    {
        bool expected = true;
        return m_armed.compare_exchange_strong(expected, false, std::memory_order_acq_rel);
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
            logger::warn("Proto DetourAttach failed; F6 arm has no effect");
            return false;
        }
        s_original_draw_interface_start = original;
        logger::info("Proto DrawInterfaceStart detour installed at 0x{:X}",
            REL::RelocationID(79947, 82084).address());
        return true;
    }
}
