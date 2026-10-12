//
// Created by AmazingBuff on 2026/10/13.
//

#include "render/studio/figure_passes.h"

#include "RE/B/BSBatchRenderer.h"
#include "RE/B/BSGeometry.h"
#include "RE/B/BSShaderAccumulator.h"
#include "RE/B/BSShaderProperty.h"
#include "RE/B/BSVisit.h"
#include "RE/R/Renderer.h"
#include "RE/R/RendererShadowState.h"

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // One pass, with the studio rig swapped in around it and the pass's own light bookkeeping put
    // back afterwards. Mutating the pass is mutating engine state (the pass objects are shared), so
    // every field written here is restored before the call returns.
    void submit_pass(RE::BSRenderPass* a_pass, bool a_inject_lights, RE::BSLight** a_light_slots, std::size_t a_light_count)
    {
        RE::BSLight** saved_scene_lights = nullptr;
        std::uint8_t saved_num_lights = 0;
        std::uint8_t saved_shadow_lights = 0;
        if (a_inject_lights)
        {
            saved_scene_lights = a_pass->sceneLights;
            saved_num_lights = a_pass->numLights;
            saved_shadow_lights = a_pass->numShadowLights;

            a_pass->numLights = static_cast<std::uint8_t>(a_light_count);
            a_pass->numShadowLights = 0;
            a_pass->sceneLights = a_light_slots;

            for (std::size_t i = 0; i < a_light_count; ++i)
            {
                if (RE::BSLight* shell = a_light_slots[i])
                {
                    shell->lodDimmer = 1.0f;
                    shell->luminance = 1.0f;
                    shell->frustrumCull = 0;
                }
            }
        }

        RE::BSBatchRenderer::SetupAndDrawPass(a_pass, a_pass->passEnum, (a_pass->passEnum & 0x40) != 0, 0x200);

        if (a_inject_lights)
        {
            a_pass->sceneLights = saved_scene_lights;
            a_pass->numLights = saved_num_lights;
            a_pass->numShadowLights = saved_shadow_lights;
        }
    }
}

FigurePassStats draw_figure_passes(FigurePassRequest const& a_request,
    std::function<void(RE::BSRenderPass*)> const& a_before_pass,
    std::function<void(std::uint32_t)> const& a_after_batch)
{
    FigurePassStats stats;
    if (!a_request.root || !a_request.accumulator || !a_request.context || !a_request.target || !a_request.states)
        return stats;

    RE::BSGraphics::RendererShadowState::GetSingleton()->GetRuntimeData().stateUpdateFlags.reset(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);

    REX::W32::D3D11_VIEWPORT viewport{
        .topLeftX = 0.0f,
        .topLeftY = 0.0f,
        .width = static_cast<float>(a_request.target->width()),
        .height = static_cast<float>(a_request.target->height()),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    a_request.context->RSSetViewports(1, &viewport);

    REX::W32::ID3D11RenderTargetView* const rtv = a_request.target->rtv();
    a_request.context->OMSetRenderTargets(1, &rtv, a_request.target->dsv());
    a_request.context->OMSetDepthStencilState(a_request.states->depth_default(), 0);

    constexpr float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    a_request.context->ClearRenderTargetView(rtv, clear_color);
    a_request.context->ClearDepthStencilView(a_request.target->dsv(), REX::W32::D3D11_CLEAR_DEPTH | REX::W32::D3D11_CLEAR_STENCIL, 1.0f, 0);

    a_request.context->OMSetBlendState(a_request.states->opaque(), nullptr, 0xFFFFFFFF);
    a_request.context->RSSetState(a_request.states->cull_none());

    // Injection engages only when every slot of the rig is ledger-served; otherwise the passes draw
    // with whatever lights the accumulator already carried.
    stats.inject_lights = a_request.light_slots != nullptr && a_request.light_count != 0;
    if (stats.inject_lights)
    {
        for (std::size_t i = 0; i < a_request.light_count; ++i)
        {
            if (!a_request.light_slots[i])
            {
                stats.inject_lights = false;
                break;
            }
        }
    }

    RE::BSVisit::TraverseScenegraphGeometries(a_request.root, [&](RE::BSGeometry* a_geometry)
    {
        const RE::BSGeometry::GEOMETRY_RUNTIME_DATA& geom_rt = a_geometry->GetGeometryRuntimeData();
        if (!geom_rt.shaderProperty)
            return RE::BSVisit::BSVisitControl::kContinue;

        if (const RE::BSShaderProperty::RenderPassArray* pass_array = geom_rt.shaderProperty->GetRenderPasses(a_geometry, std::to_underlying(RE::BSShaderAccumulator::RENDER_MODE::kNormal), a_request.accumulator))
        {
            for (RE::BSRenderPass* pass = pass_array->head; pass; pass = pass->next)
            {
                if (pass->geometry != a_geometry || !pass->shader)
                    continue;
                // Diagnostics only - never gate the draw on rendererData: the engine's
                // SetupAndDrawPass lazily creates the device buffers (run 95), and a gate here would
                // remove the only caller that could ever initialize them (run 52).
                if (!geom_rt.rendererData)
                    ++stats.renderer_data_null;
                if (a_before_pass)
                    a_before_pass(pass);
                submit_pass(pass, stats.inject_lights, a_request.light_slots, a_request.light_count);
                ++stats.drawn;
            }
        }
        return RE::BSVisit::BSVisitControl::kContinue;
    });

    if (a_after_batch)
        a_after_batch(stats.drawn);

    // What the context holds after the batch. When the shadow state says DIRTY_RENDERTARGET,
    // SetupAndDrawPass re-applies the engine's ledger target mid-batch (v6.45), which empties the
    // studio target without any log line of its own - this decides "rerouted" from "drew-nothing".
    if (a_request.check_render_target && stats.drawn > 0)
    {
        stats.render_target_checked = true;
        REX::W32::ID3D11RenderTargetView* bound = nullptr;
        a_request.context->OMGetRenderTargets(1, &bound, nullptr);
        stats.render_target_matched = bound == rtv;
        stats.render_target_bound = bound;
        if (bound)
            bound->Release();
    }
    else
    {
        stats.render_target_matched = true;
    }

    RE::BSGraphics::RendererShadowState::GetSingleton()->GetRuntimeData().stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);

    return stats;
}

PLUGIN_NAMESPACE_END
