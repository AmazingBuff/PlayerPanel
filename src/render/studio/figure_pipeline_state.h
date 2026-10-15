//
// Created by AmazingBuff on 2026/10/15.
//

#pragma once

#include "RE/R/RendererShadowState.h"

PLUGIN_NAMESPACE_BEGIN

class FigurePipelineState
{
public:
    explicit FigurePipelineState(RE::BSGraphics::RendererShadowState::FLAT_RUNTIME_DATA& state) :
        m_ref_state(state), m_saved_state(state) {}

    ~FigurePipelineState()
    {
        m_ref_state.viewPort = m_saved_state.viewPort;
        m_ref_state.depthStencilDepthMode = m_saved_state.depthStencilDepthMode;
        m_ref_state.depthStencilDepthModePrevious = m_saved_state.depthStencilDepthModePrevious;
        m_ref_state.depthStencilStencilMode = m_saved_state.depthStencilStencilMode;
        m_ref_state.stencilRef = m_saved_state.stencilRef;
        m_ref_state.rasterStateCullMode = m_saved_state.rasterStateCullMode;
        m_ref_state.alphaBlendMode = m_saved_state.alphaBlendMode;
        m_ref_state.alphaBlendAlphaToCoverage = m_saved_state.alphaBlendAlphaToCoverage;
        m_ref_state.alphaBlendWriteMode = m_saved_state.alphaBlendWriteMode;
        m_ref_state.alphaBlendModeExtra = m_saved_state.alphaBlendModeExtra;
        m_ref_state.alphaTestEnabled = m_saved_state.alphaTestEnabled;
        m_ref_state.alphaTestRef = m_saved_state.alphaTestRef;
        m_ref_state.stateUpdateFlags |= m_saved_state.stateUpdateFlags;
        invalidate_material_state();
        m_ref_state.stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);
    }

    FigurePipelineState(FigurePipelineState const&) = delete;
    FigurePipelineState& operator=(FigurePipelineState const&) = delete;

    void prepare(REX::W32::D3D11_VIEWPORT const& viewport, bool alpha_blend, bool alpha_test, bool two_sided)
    {
        m_ref_state.viewPort = viewport;
        m_ref_state.depthStencilDepthMode = alpha_blend ? RE::BSGraphics::DepthStencilDepthMode::kTest :
            RE::BSGraphics::DepthStencilDepthMode::kTestWrite;
        m_ref_state.depthStencilStencilMode = RE::BSGraphics::DEPTH_STENCIL_STENCIL_MODE_DISABLED;
        m_ref_state.stencilRef = 0;
        m_ref_state.rasterStateCullMode = two_sided ? RE::BSGraphics::RASTER_STATE_CULL_MODE_NONE :
            RE::BSGraphics::RASTER_STATE_CULL_MODE_BACK;
        m_ref_state.alphaBlendMode = 0;
        m_ref_state.alphaBlendAlphaToCoverage = 0;
        m_ref_state.alphaBlendWriteMode = 1;
        m_ref_state.alphaBlendModeExtra = 0;
        m_ref_state.alphaTestEnabled = alpha_test;
        m_ref_state.alphaTestRef = 0.0f;
        invalidate_material_state();
        m_ref_state.stateUpdateFlags.reset(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);
    }

private:
    void invalidate_material_state()
    {
        m_ref_state.stateUpdateFlags.set(
            RE::BSGraphics::ShaderFlags::DIRTY_VIEWPORT,
            RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE,
            RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_STENCILREF_MODE,
            RE::BSGraphics::ShaderFlags::DIRTY_RASTER_CULL_MODE,
            RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND,
            RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_TEST_REF,
            RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_ENABLE_TEST);
    }

    // The outer D3D capture restores device bindings. This scope restores the corresponding engine
    // cache and forces its alpha-reference buffer to be uploaded again on the next native draw.
    RE::BSGraphics::RendererShadowState::FLAT_RUNTIME_DATA& m_ref_state;
    RE::BSGraphics::RendererShadowState::FLAT_RUNTIME_DATA const m_saved_state;
};

PLUGIN_NAMESPACE_END
