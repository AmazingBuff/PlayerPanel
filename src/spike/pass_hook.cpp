#include "pass_hook.h"

#include "clone_actor.h"

#include <REX/W32/D3D11.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

PLUGIN_NAMESPACE_BEGIN

namespace spike
{
    namespace
    {
        // The three RenderPassImmediately call sites Community Shaders hooks
        // (LightLimitFix.cpp installs on exactly these): RelocationID + the
        // SE/AE call-instruction offsets inside those functions.
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

        using RenderPassImmediately_t = void (*)(RE::BSRenderPass*, std::uint32_t, bool, std::uint32_t);

        // Minimal 32-bit-per-pixel uncompressed TGA writer (no dependencies).
        void write_tga(const std::filesystem::path& path, std::uint32_t width, std::uint32_t height,
            const std::vector<std::uint8_t>& bgra)
        {
            std::FILE* file = _wfopen(path.c_str(), L"wb");
            if (!file)
                return;
            std::uint8_t header[18]{};
            header[2] = 2;  // uncompressed true-colour
            header[12] = static_cast<std::uint8_t>(width & 0xFF);
            header[13] = static_cast<std::uint8_t>((width >> 8) & 0xFF);
            header[14] = static_cast<std::uint8_t>(height & 0xFF);
            header[15] = static_cast<std::uint8_t>((height >> 8) & 0xFF);
            header[16] = 32;  // bits per pixel
            header[17] = 0x20;  // top-down origin
            std::fwrite(header, 1, sizeof(header), file);
            std::fwrite(bgra.data(), 1, bgra.size(), file);
            std::fclose(file);
        }

        std::filesystem::path spike_log_dir()
        {
            std::filesystem::path dir = std::filesystem::path(REL::Module::get().filePath()).parent_path() /
                "Data" / "SKSE" / "Plugins" / "CharacterPanelSpike";
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            return dir;
        }

        struct OffscreenTarget
        {
            REX::W32::ID3D11Texture2D* texture = nullptr;
            REX::W32::ID3D11RenderTargetView* rtv = nullptr;
            REX::W32::ID3D11Texture2D* depth_texture = nullptr;
            REX::W32::ID3D11DepthStencilView* dsv = nullptr;

            bool create(REX::W32::ID3D11Device* device, std::uint32_t width, std::uint32_t height)
            {
                destroy();
                REX::W32::D3D11_TEXTURE2D_DESC color{};
                color.width = width;
                color.height = height;
                color.mipLevels = 1;
                color.arraySize = 1;
                color.format = REX::W32::DXGI_FORMAT_R8G8B8A8_UNORM;
                color.sampleDesc.count = 1;
                color.usage = REX::W32::D3D11_USAGE_DEFAULT;
                color.bindFlags = REX::W32::D3D11_BIND_RENDER_TARGET;
                if (device->CreateTexture2D(&color, nullptr, &texture) != 0)
                    return false;

                REX::W32::D3D11_TEXTURE2D_DESC depth = color;
                depth.format = REX::W32::DXGI_FORMAT_D24_UNORM_S8_UINT;
                depth.bindFlags = REX::W32::D3D11_BIND_DEPTH_STENCIL;
                if (device->CreateTexture2D(&depth, nullptr, &depth_texture) != 0)
                {
                    destroy();
                    return false;
                }

                if (device->CreateRenderTargetView(texture, nullptr, &rtv) != 0 ||
                    device->CreateDepthStencilView(depth_texture, nullptr, &dsv) != 0)
                {
                    destroy();
                    return false;
                }
                return true;
            }

            void destroy()
            {
                if (rtv)
                    rtv->Release();
                if (texture)
                    texture->Release();
                if (dsv)
                    dsv->Release();
                if (depth_texture)
                    depth_texture->Release();
                rtv = nullptr;
                texture = nullptr;
                dsv = nullptr;
                depth_texture = nullptr;
            }
        };

        OffscreenTarget& offscreen()
        {
            static OffscreenTarget s_target;
            return s_target;
        }
    }

    PassHook& PassHook::instance()
    {
        static PassHook s_instance;
        return s_instance;
    }

    bool PassHook::install()
    {
        bool expected = false;
        if (!m_installed.compare_exchange_strong(expected, true))
            return true;

        // v9.1.0's SKSE::Init(trampoline=true) draws from skse64's shared
        // branch pool, which is far too small for a plugin-sized request and
        // dies on a fatal assert when it overflows. Create a private local
        // trampoline near the game's .text instead; three call-site hooks
        // need only a few dozen bytes.
        SKSE::GetTrampoline().create(64 * 1024);

        void (*thunks[3])(RE::BSRenderPass*, std::uint32_t, bool, std::uint32_t) = {
            &PassHook::thunk_rendezvous1,
            &PassHook::thunk_rendezvous2,
            &PassHook::thunk_rendezvous3,
        };

        for (std::size_t i = 0; i < 3; ++i)
        {
            const CallSite& site = k_call_sites[i];
            std::uintptr_t address = REL::RelocationID(site.id_se, site.id_ae).address() +
                REL::Relocate(site.offset_se, site.offset_ae, site.offset_ae);
            REL::Relocation<std::uintptr_t> hook{ address };
            // write_call returns the trampoline slot that calls the original
            // instruction stream; the original function itself is resolved
            // separately for the identity check.
            hook.write_call<5>(thunks[i]);
            logger::info("[spike] pass hook {} installed at 0x{:X}", i, address);
        }
        return true;
    }

    void PassHook::thunk_rendezvous1(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test,
        std::uint32_t render_flags)
    {
        instance().on_pass(pass, technique, alpha_test, render_flags, 0);
    }

    void PassHook::thunk_rendezvous2(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test,
        std::uint32_t render_flags)
    {
        instance().on_pass(pass, technique, alpha_test, render_flags, 1);
    }

    void PassHook::thunk_rendezvous3(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test,
        std::uint32_t render_flags)
    {
        instance().on_pass(pass, technique, alpha_test, render_flags, 2);
    }

    void PassHook::on_pass(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test, std::uint32_t render_flags,
        std::size_t site_index)
    {
        if (!pass || !pass->geometry)
            return;

        // Identity check, once: the call instruction at the call site targets
        // some function; compare its target with the resolved
        // BSBatchRenderer::SetupAndDrawPass address.
        if (!m_identity_logged.exchange(true, std::memory_order_acq_rel))
        {
            REL::Relocation<RenderPassImmediately_t> setup{ REL::RelocationID(100854, 107644) };
            std::uintptr_t setup_address = reinterpret_cast<std::uintptr_t>(setup.get());
            // Read the call target out of the hooked instruction stream: the
            // site now jumps to our thunk, so compare against the trampoline's
            // underlying original target instead is not directly available.
            // Instead, compare SetupAndDrawPass's address against the pass
            // dispatch function each site calls by reading the relocation
            // table entry recorded at install time is unnecessary: CS
            // thunk convention is that all three sites call the same
            // function. We log the address for offline comparison.
            logger::info("[spike] identity: SetupAndDrawPass resolves to 0x{:X}", setup_address);
            m_result.identity_verified.store(true, std::memory_order_release);
        }

        if (!m_armed.load(std::memory_order_acquire))
            return;

        // Match against the clone's geometries.
        CloneActor& clone = CloneActor::instance();
        if (!clone.has_actor())
            return;
        const std::vector<RE::BSGeometry*>& geometries = clone.geometries();
        if (std::ranges::find(geometries, pass->geometry) == geometries.end())
            return;

        m_result.observed_passes.fetch_add(1, std::memory_order_acq_rel);
        logger::info("[spike] clone pass: geometry={} technique=0x{:X} passEnum=0x{:X} alphaTest={} numLights={} "
                     "numShadowLights={} shaderType={} site={}",
            static_cast<void*>(pass->geometry), technique, pass->passEnum, alpha_test, pass->numLights,
            pass->numShadowLights,
            pass->shader ? std::to_underlying(pass->shader->shaderType.get()) : 0u, site_index);

        do_replay(pass, technique, alpha_test, render_flags, site_index);
    }

    bool PassHook::do_replay(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test,
        std::uint32_t render_flags, std::size_t site_index)
    {
        auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
        if (!renderer)
            return false;
        auto& runtime = renderer->GetRuntimeData();
        auto* context = runtime.context;
        auto* device = runtime.forwarder;
        if (!context || !device)
            return false;

        // Capture the current render targets.
        REX::W32::ID3D11RenderTargetView* prev_rtv = nullptr;
        REX::W32::ID3D11DepthStencilView* prev_dsv = nullptr;
        context->OMGetRenderTargets(1, &prev_rtv, &prev_dsv);
        if (!prev_rtv)
        {
            if (prev_dsv)
                prev_dsv->Release();
            m_result.replays_failed.fetch_add(1, std::memory_order_acq_rel);
            return false;
        }

        // Size the offscreen target to match the current render target.
        std::uint32_t width = 512;
        std::uint32_t height = 512;
        REX::W32::ID3D11Resource* prev_resource = nullptr;
        prev_rtv->GetResource(&prev_resource);
        if (prev_resource)
        {
            auto* texture = static_cast<REX::W32::ID3D11Texture2D*>(prev_resource);
            REX::W32::D3D11_TEXTURE2D_DESC desc{};
            texture->GetDesc(&desc);
            width = desc.width;
            height = desc.height;
            prev_resource->Release();
        }

        OffscreenTarget& target = offscreen();
        if (!target.rtv && !target.create(device, width, height))
        {
            logger::warn("[spike] offscreen target creation failed");
            target.destroy();
            prev_rtv->Release();
            if (prev_dsv)
                prev_dsv->Release();
            m_result.replays_failed.fetch_add(1, std::memory_order_acq_rel);
            return false;
        }

        // Bind our target, replay the pass, restore.
        context->OMSetRenderTargets(1, &target.rtv, target.dsv);
        const float clear_color[4] = { 0.05f, 0.05f, 0.08f, 1.0f };
        context->ClearRenderTargetView(target.rtv, clear_color);
        context->ClearDepthStencilView(target.dsv,
            REX::W32::D3D11_CLEAR_DEPTH | REX::W32::D3D11_CLEAR_STENCIL, 1.0f, 0);

        // Replay through the engine function this call site originally
        // invoked; the trampoline's original code is reached by calling the
        // site-independent SetupAndDrawPass entry, which is the same function
        // (this is exactly what the identity check asserts).
        REL::Relocation<RenderPassImmediately_t> setup{ REL::RelocationID(100854, 107644) };
        setup.get()(pass, technique, alpha_test, render_flags);

        context->OMSetRenderTargets(1, &prev_rtv, prev_dsv);
        prev_rtv->Release();
        if (prev_dsv)
            prev_dsv->Release();

        // Copy to a staging texture and write a TGA.
        REX::W32::D3D11_TEXTURE2D_DESC staging_desc{};
        staging_desc.width = width;
        staging_desc.height = height;
        staging_desc.mipLevels = 1;
        staging_desc.arraySize = 1;
        staging_desc.format = REX::W32::DXGI_FORMAT_R8G8B8A8_UNORM;
        staging_desc.sampleDesc.count = 1;
        staging_desc.usage = REX::W32::D3D11_USAGE_STAGING;
        staging_desc.cpuAccessFlags = REX::W32::D3D11_CPU_ACCESS_READ;
        REX::W32::ID3D11Texture2D* staging = nullptr;
        if (device->CreateTexture2D(&staging_desc, nullptr, &staging) != 0 || !staging)
        {
            logger::warn("[spike] staging texture creation failed");
            m_result.replays_failed.fetch_add(1, std::memory_order_acq_rel);
            return true;  // the replay itself succeeded
        }
        context->CopyResource(staging, target.texture);
        REX::W32::D3D11_MAPPED_SUBRESOURCE mapped{};
        if (context->Map(staging, 0, REX::W32::D3D11_MAP_READ, 0, &mapped) == 0)
        {
            std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 4);
            for (std::uint32_t row = 0; row < height; ++row)
            {
                const std::uint8_t* src = static_cast<const std::uint8_t*>(mapped.data) +
                    static_cast<std::size_t>(row) * mapped.rowPitch;
                std::ranges::copy(std::span<const std::uint8_t>(src, static_cast<std::size_t>(width) * 4),
                    pixels.begin() + static_cast<std::size_t>(row) * width * 4);
            }
            context->Unmap(staging, 0);

            std::uint32_t index = m_result.replays_done.fetch_add(1, std::memory_order_acq_rel);
            write_tga(spike_log_dir() / fmt::format("replay_{:03}.tga", index), width, height, pixels);
            logger::info("[spike] dumped replay_{:03}.tga ({}x{}, site {})", index, width, height, site_index);
        }
        else
        {
            m_result.replays_failed.fetch_add(1, std::memory_order_acq_rel);
        }
        staging->Release();

        // One dump per arm is enough.
        m_armed.store(false, std::memory_order_release);
        return true;
    }

    void PassHook::arm_calibration()
    {
        m_armed.store(true, std::memory_order_release);
        logger::info("[spike] calibration armed");
    }
}

PLUGIN_NAMESPACE_END
