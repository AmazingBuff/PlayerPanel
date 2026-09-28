//
// Created by AmazingBuff on 2026/09/28.
//

#include "proto.h"

#include <RE/Skyrim.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>
#include <REX/W32/D3D11.h>

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
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
        bool g_swap_active = false;
        std::uint32_t g_swap_runs = 0;

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

            // The UI3D secondary accumulator is the one the inventory menu
            // scene actually renders through (capture report, finding 2).
            RE::BSShaderAccumulator* secondary = ui3d->unk18.get();
            if (!secondary)
            {
                logger::warn("Proto secondary accumulator unavailable; running unswapped frame");
                original(a1);
                return;
            }

            g_saved_accumulator = RE::BSShaderAccumulator::GetCurrentAccumulator();
            RE::BSShaderAccumulator::SetCurrentAccumulator(secondary);
            g_swap_active = true;
            logger::info(
                "Proto swap frame #{}: current {} -> secondary {} (camera {})",
                g_swap_runs + 1,
                static_cast<void*>(g_saved_accumulator),
                static_cast<void*>(secondary),
                static_cast<void*>(ui3d->camera.get()));

            // Run the engine's own menu interface draw. Its inventory 3D path
            // pulls the UI3D scene, culls it, and submits through the current
            // accumulator; with the secondary swapped in as current, the
            // accumulator's own StartAccumulating binds what we prepared.
            secondary->renderMode = RE::BSShaderAccumulator::RENDER_MODE::kNormal;
            original(a1);

            g_swap_active = false;
            RE::BSShaderAccumulator::SetCurrentAccumulator(g_saved_accumulator);
            logger::info(
                "Proto swap frame done: restored current={}, pass={} bucket={} active={}",
                static_cast<void*>(g_saved_accumulator),
                [&] { auto& d = secondary->GetRuntimeData(); return d.currentPass; }(),
                [&] { auto& d = secondary->GetRuntimeData(); return d.currentBucket; }(),
                [&] { auto& d = secondary->GetRuntimeData(); return d.currentActive; }());

            dump_swap_result_to_log_dir();
            ++g_swap_runs;
        }
    }

    namespace
    {
        // Render-thread trampoline. DrawInterfaceStart runs once per rendered
        // menu frame; the armed flag decides whether this frame is the swap.
        // The hook redirects one call instruction inside DrawInterfaceStart;
        // the original function is the same relocated address, entered after
        // the patched prologue range via the trampoline slot.
        void draw_interface_start_thunk(std::int64_t a1)
        {
            static DrawInterfaceStart_t const original =
                reinterpret_cast<DrawInterfaceStart_t>(REL::RelocationID(79947, 82084).address());

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
        // A private trampoline near the game's .text; the shared SKSE pool is
        // too small (same lesson as the stage-0 spike's pass hook).
        static bool trampoline_ready = false;
        if (!trampoline_ready)
        {
            SKSE::GetTrampoline().create(64 * 1024);
            trampoline_ready = true;
        }

        REL::Relocation<std::uintptr_t> target{ REL::RelocationID(79947, 82084) };
        target.write_call<5>(draw_interface_start_thunk);
        logger::info("Proto DrawInterfaceStart call hook installed at 0x{:X}", target.address());
        return true;
    }
}
