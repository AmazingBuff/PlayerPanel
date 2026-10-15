//
// Created by AmazingBuff on 2026/10/13.
//

#include "render/studio/figure_passes.h"

#include "render/studio/figure_draw_order.h"
#include "render/studio/figure_pipeline_state.h"

#include "RE/B/BSBatchRenderer.h"
#include "RE/B/BSGeometry.h"
#include "RE/B/BSShaderAccumulator.h"
#include "RE/B/BSShaderProperty.h"
#include "RE/B/BSVisit.h"
#include "RE/N/NiAlphaProperty.h"
#include "RE/N/NiCamera.h"
#include "RE/R/Renderer.h"
#include "RE/R/RendererShadowState.h"

#include <vector>

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // One pass, with the studio rig swapped in around it and the pass's own light bookkeeping put
    // back afterwards. Mutating the pass is mutating engine state (the pass objects are shared), so
    // every field written here is restored before the call returns.
    void submit_pass(RE::BSRenderPass* a_pass, bool a_alpha_test, bool a_inject_lights, RE::BSLight** a_light_slots, std::size_t a_light_count)
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

        // ShaderSetup only calls SetupGeometryAlphaBlending when renderFlags includes bit 0x4.
        // The UI's 0x200 alone assumes the accumulator has already supplied that state.
        static constexpr std::uint32_t s_render_flags = 0x200 | 0x4;
        RE::BSBatchRenderer::SetupAndDrawPass(a_pass, a_pass->passEnum, a_alpha_test, s_render_flags);

        if (a_inject_lights)
        {
            a_pass->sceneLights = saved_scene_lights;
            a_pass->numLights = saved_num_lights;
            a_pass->numShadowLights = saved_shadow_lights;
        }
    }
}

FigurePassStats draw_figure_passes(FigurePassRequest const& a_request)
{
    FigurePassStats stats;
    if (!a_request.root || !a_request.accumulator || !a_request.context || !a_request.target || !a_request.states)
        return stats;

    RE::BSGraphics::RendererShadowState::FLAT_RUNTIME_DATA& shadow =
        RE::BSGraphics::RendererShadowState::GetSingleton()->GetRuntimeData();
    FigurePipelineState pipeline_state(shadow);
    shadow.stateUpdateFlags.reset(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);

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

    constexpr float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    a_request.context->ClearRenderTargetView(rtv, clear_color);
    a_request.context->ClearDepthStencilView(a_request.target->dsv(), REX::W32::D3D11_CLEAR_DEPTH | REX::W32::D3D11_CLEAR_STENCIL, 1.0f, 0);

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

    std::vector<FigureGeometry> geometries;
    std::uint32_t blended_count = 0;
    std::uint32_t tested_count = 0;
    RE::NiCamera const* const camera = a_request.camera;
    RE::BSVisit::TraverseScenegraphGeometries(a_request.root, [&](RE::BSGeometry* a_geometry)
    {
        const RE::BSGeometry::GEOMETRY_RUNTIME_DATA& geom_rt = a_geometry->GetGeometryRuntimeData();
        if (!geom_rt.shaderProperty)
            return RE::BSVisit::BSVisitControl::kContinue;

        RE::NiAlphaProperty const* const alpha = geom_rt.alphaProperty.get();
        const bool alpha_blend = alpha && alpha->GetAlphaBlending();
        const bool alpha_test = alpha && alpha->GetAlphaTesting();
        static constexpr std::uint16_t s_no_sorter = 0x2000;
        float depth = 0.0f;
        if (camera)
        {
            const RE::NiPoint3 offset = a_geometry->worldBound.center - camera->world.translate;
            const auto& w2c = camera->GetRuntimeData().worldToCam;
            depth = offset.x * w2c[2][0] + offset.y * w2c[2][1] + offset.z * w2c[2][2];
        }
        geometries.push_back({ alpha_blend, alpha_test, alpha && (alpha->alphaFlags & s_no_sorter) != 0, depth, a_geometry });
        blended_count += alpha_blend ? 1 : 0;
        tested_count += alpha_test ? 1 : 0;
        return RE::BSVisit::BSVisitControl::kContinue;
    });
    order_figure_geometry(geometries);

    // Pass lists belong to their shader property/accumulator. Generate and consume one geometry at
    // a time after ordering; do not retain pass pointers across another GetRenderPasses call.
    for (FigureGeometry const& item : geometries)
    {
        const RE::BSGeometry::GEOMETRY_RUNTIME_DATA& geom_rt = item.geometry->GetGeometryRuntimeData();
        if (const RE::BSShaderProperty::RenderPassArray* pass_array = geom_rt.shaderProperty->GetRenderPasses(item.geometry, std::to_underlying(RE::BSShaderAccumulator::RENDER_MODE::kNormal), a_request.accumulator))
        {
            for (RE::BSRenderPass* pass = pass_array->head; pass; pass = pass->next)
            {
                if (pass->geometry != item.geometry || !pass->shader)
                    continue;
                // Diagnostics only - never gate the draw on rendererData: the engine's
                // SetupAndDrawPass lazily creates the device buffers (run 95), and a gate here would
                // remove the only caller that could ever initialize them (run 52).
                if (!geom_rt.rendererData)
                    ++stats.renderer_data_null;
                pipeline_state.prepare(viewport, item.alpha_blend, item.alpha_test,
                    geom_rt.shaderProperty->flags.any(RE::BSShaderProperty::EShaderPropertyFlag::kTwoSided));
                submit_pass(pass, item.alpha_test, stats.inject_lights, a_request.light_slots, a_request.light_count);
                ++stats.drawn;
            }
        }
    }

    if (a_request.check_render_target)
        logger::info("FIGURE transparency geometries={} blended={} tested={} render-flags=0x204 depth=opaque-write/alpha-read",
            geometries.size(), blended_count, tested_count);

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

    return stats;
}

PLUGIN_NAMESPACE_END
