//
// Created by AmazingBuff on 2026/10/13.
//

#pragma once

#include "render/dx11/common_states.h"
#include "render/dx11/d3d11_util.h"

#include <cstddef>
#include <cstdint>

namespace RE
{
    class BSLight;
    class BSRenderPass;
    class BSShaderAccumulator;
    class NiAVObject;
    class NiCamera;
}

PLUGIN_NAMESPACE_BEGIN

// SceneGraph supplies the detached figure and placement. This batch owns only render-target
// binding, material/pass submission, studio-light injection and engine state restoration.
struct FigurePassRequest
{
    RE::NiAVObject* root = nullptr;
    // The accumulator the passes are generated into (the UI3D one, ui3d->unk10). Its render mode and
    // its own camera decide how the figure projects.
    RE::BSShaderAccumulator* accumulator = nullptr;
    RE::NiCamera const* camera = nullptr;
    REX::W32::ID3D11DeviceContext* context = nullptr;
    RenderTarget const* target = nullptr;
    CommonStates const* states = nullptr;
    // The studio light rig's shells. Injection engages only when slots are present and every one of
    // them is non-null, because a partial rig would light the figure with a mixture of the studio and
    // whatever the pass already carried.
    RE::BSLight** light_slots = nullptr;
    std::size_t light_count = 0;
    // Read the bound render target back after the batch, once. The caller owns the "once" decision
    // (its own flag), the seam owns the readback, because only the seam knows the batch has ended.
    bool check_render_target = false;
};

struct FigurePassStats
{
    std::uint32_t drawn = 0;
    std::uint32_t renderer_data_null = 0;
    bool inject_lights = false;
    bool render_target_checked = false;
    bool render_target_matched = false;
    // The target the context actually held after the batch, for the caller's own log line. Only
    // meaningful when `render_target_checked`.
    void const* render_target_bound = nullptr;
};

// Binds and clears the studio target, draws opaque/cutout geometry before the sorted alpha stage,
// and uses each geometry's original NiAlphaProperty for native blend/threshold setup. Alpha-blended
// surfaces test depth without writing it. Device bindings and the engine material cache are restored
// around the batch. The returned statistics describe
// the completed batch without exposing per-pass callbacks.
//
// The caller must already be inside a D3D state capture (SceneGraph's callers are): the viewport,
// render targets, depth/blend/rasterizer states this sets are restored by that capture, not here.
[[nodiscard]] FigurePassStats draw_figure_passes(FigurePassRequest const& a_request);

PLUGIN_NAMESPACE_END
