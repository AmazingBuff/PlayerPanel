//
// The replay window (v2.5 order): post-original OM capture, studio target
// sizing, and the binding-survival verification. Since v6.57 this window
// no longer draws or composites. (2026-10-05 src migration: split out of tools/m0_proto, behavior byte-identical)
//

#include "render/offscreen_target.h"
#include "render/pass_redirector.h"
#include "render/render_internal.h"

PLUGIN_NAMESPACE_BEGIN
namespace
{
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

    TargetSig target_sig(const OffscreenTarget& target)
    {
        return { target.width, target.height, static_cast<std::uint32_t>(target.format),
            static_cast<std::uint32_t>(target.depth_format) };
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
void PassRedirector::replay_after_original(RE::BSRenderPass*, std::uint32_t, bool, std::uint32_t, std::size_t)
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
PLUGIN_NAMESPACE_END
