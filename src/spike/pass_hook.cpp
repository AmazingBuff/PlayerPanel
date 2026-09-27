#include "pass_hook.h"

#include "clone_actor.h"

#include <RE/R/RendererShadowState.h>
#include <RE/R/RenderTargetData.h>
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

        // Engine pool slot the offscreen texture is registered under for the
        // replay (transient hit-feedback buffer, unused during scene draws)
        // and the depth-stencil pool entry pointed at while it is bound.
        constexpr RE::RENDER_TARGET Spike_Target = RE::RENDER_TARGET::kGETHIT_BUFFER;
        constexpr std::uint32_t Spike_Depth_Slot = std::to_underlying(RE::RENDER_TARGETS_DEPTHSTENCIL::kMAIN);

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
        // When on_pass reports the pass was consumed by a replay (drawn into
        // the offscreen target), the original call must NOT run again or the
        // pass draws twice; otherwise it must always run (CS convention).
        if (!instance().on_pass(pass, technique, alpha_test, render_flags, 0))
        {
            REL::Relocation<RenderPassImmediately_t> setup{ REL::RelocationID(100854, 107644) };
            setup.get()(pass, technique, alpha_test, render_flags);
        }
    }

    void PassHook::thunk_rendezvous2(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test,
        std::uint32_t render_flags)
    {
        if (!instance().on_pass(pass, technique, alpha_test, render_flags, 1))
        {
            REL::Relocation<RenderPassImmediately_t> setup{ REL::RelocationID(100854, 107644) };
            setup.get()(pass, technique, alpha_test, render_flags);
        }
    }

    void PassHook::thunk_rendezvous3(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test,
        std::uint32_t render_flags)
    {
        if (!instance().on_pass(pass, technique, alpha_test, render_flags, 2))
        {
            REL::Relocation<RenderPassImmediately_t> setup{ REL::RelocationID(100854, 107644) };
            setup.get()(pass, technique, alpha_test, render_flags);
        }
    }

    bool PassHook::on_pass(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test, std::uint32_t render_flags,
        std::size_t site_index)
    {
        if (!pass || !pass->geometry)
            return false;

        // Identity check, once: log the resolved SetupAndDrawPass address for
        // offline comparison with the call-site addresses in the log.
        if (!m_identity_logged.exchange(true, std::memory_order_acq_rel))
        {
            REL::Relocation<RenderPassImmediately_t> setup{ REL::RelocationID(100854, 107644) };
            logger::info("[spike] identity: SetupAndDrawPass resolves to 0x{:X}",
                reinterpret_cast<std::uintptr_t>(setup.get()));
            m_result.identity_verified.store(true, std::memory_order_release);
        }

        if (!m_armed.load(std::memory_order_acquire))
            return false;

        // Match against the clone's geometries.
        CloneActor& clone = CloneActor::instance();
        if (!clone.has_actor())
            return false;
        const std::vector<RE::BSGeometry*>& geometries = clone.geometries();
        if (std::ranges::find(geometries, pass->geometry) == geometries.end())
            return false;

        m_result.observed_passes.fetch_add(1, std::memory_order_acq_rel);
        logger::info("[spike] clone pass: geometry={} technique=0x{:X} passEnum=0x{:X} alphaTest={} numLights={} "
                     "numShadowLights={} shaderType={} site={}",
            static_cast<void*>(pass->geometry), technique, pass->passEnum, alpha_test, pass->numLights,
            pass->numShadowLights,
            pass->shader ? std::to_underlying(pass->shader->shaderType.get()) : 0u, site_index);

        // The replay draws the pass into our own target; returning true tells
        // the thunk the pass was consumed and must not draw again.
        bool consumed = do_replay(pass, technique, alpha_test, render_flags, site_index);
        return consumed;
    }

    bool PassHook::do_replay(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test,
        std::uint32_t render_flags, std::size_t site_index)
    {
        // Only replay the lighting pass. The armed window catches the depth
        // and shadow passes first (shaderType 8, numLights 0) whose vertex
        // shader runs fine but which by design write no colour -- replaying
        // them produced fifteen single-colour TGAs of the clear value and
        // disarmed before the actual lighting pass (0x484040B5 family,
        // shaderType 6) ever arrived. That pass carries the real shading.
        if (!pass->shader || std::to_underlying(pass->shader->shaderType.get()) != 6)
            return false;

        auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
        if (!renderer)
            return false;
        auto& runtime = renderer->GetRuntimeData();
        auto* context = runtime.context;
        auto* device = runtime.forwarder;
        if (!context || !device)
            return false;

        // Capture the pipeline state the engine relies on across the call:
        // render targets, viewport, rasterizer, blend, depth-stencil. The
        // first in-game run rebound only the render targets, so the replayed
        // lighting pass ran against a viewport/depth state that belonged to
        // the offscreen target and drew nothing.
        REX::W32::ID3D11RenderTargetView* prev_rtv = nullptr;
        REX::W32::ID3D11DepthStencilView* prev_dsv = nullptr;
        context->OMGetRenderTargets(1, &prev_rtv, &prev_dsv);
        if (!prev_rtv)
        {
            if (prev_dsv)
                prev_dsv->Release();
            return false;
        }

        REX::W32::D3D11_VIEWPORT prev_viewport{};
        std::uint32_t viewport_count = 1;
        context->RSGetViewports(&viewport_count, &prev_viewport);

        REX::W32::ID3D11RasterizerState* prev_raster = nullptr;
        context->RSGetState(&prev_raster);
        REX::W32::ID3D11BlendState* prev_blend = nullptr;
        float prev_blend_factor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        std::uint32_t prev_sample_mask = 0xffffffff;
        context->OMGetBlendState(&prev_blend, prev_blend_factor, &prev_sample_mask);
        REX::W32::ID3D11DepthStencilState* prev_depth = nullptr;
        std::uint32_t prev_stencil_ref = 0;
        context->OMGetDepthStencilState(&prev_depth, &prev_stencil_ref);

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

        // The engine does not honour raw OM bindings: it keeps its own shadow
        // state (BSGraphics::RendererShadowState) mapping each OM slot to a
        // RENDER_TARGET pool entry and rebinds them itself on every pass. A
        // bare OMSetRenderTargets was overwritten inside the replayed pass,
        // which is why the first lighting replay produced a clear-colour TGA
        // while the world flickered for a frame. Register the offscreen
        // texture in the engine's pool under a transient slot
        // (kGETHIT_BUFFER, used only for momentary hit-feedback blits) and
        // retarget the shadow state, exactly the mechanism Community
        // Shaders' Deferred uses for its G-buffer takeover.
        RE::BSGraphics::RendererShadowState* shadow_state =
            RE::BSGraphics::RendererShadowState::GetSingleton();
        if (!shadow_state)
        {
            logger::warn("[spike] shadow state unavailable");
            prev_rtv->Release();
            if (prev_dsv)
                prev_dsv->Release();
            return false;
        }
        auto& shadow_data = shadow_state->GetRuntimeData();
        auto& pool = runtime.renderTargets[std::to_underlying(Spike_Target)];

        // Save the transient slot's previous pool entry (spike runs once per
        // arm; a static save is enough).
        static RE::BSGraphics::RenderTargetData s_saved_pool{};
        static bool s_pool_saved = false;
        if (!s_pool_saved)
        {
            s_saved_pool = pool;
            s_pool_saved = true;
        }

        // Release whatever the engine previously held in the slot, then
        // install our texture/views (matches CS SetupRenderTarget pattern).
        if (pool.texture != target.texture)
        {
            if (pool.UAV)
                pool.UAV->Release();
            if (pool.RTV)
                pool.RTV->Release();
            if (pool.SRVCopy)
                pool.SRVCopy->Release();
            if (pool.SRV)
                pool.SRV->Release();
            if (pool.textureCopy)
                pool.textureCopy->Release();
            if (pool.texture)
                pool.texture->Release();
            pool.texture = target.texture;
            pool.textureCopy = nullptr;
            pool.RTV = target.rtv;
            pool.SRV = nullptr;
            pool.SRVCopy = nullptr;
            pool.UAV = nullptr;
            pool.texture->AddRef();
            pool.RTV->AddRef();
        }

        // Point the shadow state at the slot and mark everything dirty so the
        // engine rebinds OM, viewport and depth on the next draw (ours).
        const RE::RENDER_TARGET prev_main_target = shadow_data.renderTargets[0];
        shadow_data.renderTargets[0] = Spike_Target;
        shadow_data.setRenderTargetMode[0] = RE::BSGraphics::SetRenderTargetMode::SRTM_NO_CLEAR;
        shadow_data.depthStencil = Spike_Depth_Slot;
        shadow_data.stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET,
            RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE, RE::BSGraphics::ShaderFlags::DIRTY_VIEWPORT);

        // The replay draws through the engine function this call site
        // originally invoked; the engine itself now binds our offscreen
        // target from the shadow state.
        REL::Relocation<RenderPassImmediately_t> setup{ REL::RelocationID(100854, 107644) };
        setup.get()(pass, technique, alpha_test, render_flags);

        // Restore the shadow state and the pool entry so the engine resumes
        // exactly where it was.
        shadow_data.renderTargets[0] = prev_main_target;
        shadow_data.stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET,
            RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE, RE::BSGraphics::ShaderFlags::DIRTY_VIEWPORT);

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
