//
// Stage 0 spike for docs/plans/panel-render-2b.md — disposable, do not build a feature on it.
//
// First form (calling the engine's draw entry ourselves from the present hook) crashed inside
// BSBatchRenderer::SetupAndDrawPass: by present time the frame's accumulation is over, and that
// entry depends on state that only holds while the engine is accumulating.
//
// This form stays inside the engine's own render: it detours SetupAndDrawPass with the SKSE
// trampoline, so it sees every pass the engine draws, with valid state, and can redirect one of them
// into a target the plugin owns. The questions it answers, in one run:
//
//   1. which technique, light set and pass state the engine really uses for the preview's body;
//   2. whether that pass can be redirected into a private target - the gate for the whole plan.
//
// Status on 1.6.1170 (crash dump 2026-09-27 15:32): the entry RELOCATION_ID(100854, 107644) points
// at does not behave as clib-ng's BSBatchRenderer.h claims - callers such as the water-displacement
// imagespace effect pass a hash-like uint32 in the second argument, not a BSRenderPass*, and another
// plugin already owns the entry with a JMP into its own trampoline. install() refuses both
// conditions, so the spike cannot engage on this runtime; the engine-pass plan
// (docs/plans/panel-render-2b.md) needs a different interception point.
//
// It is armed from the game thread (remembering one geometry of the preview) and fires once. After
// that the detour only forwards, so a shipped build would carry no per-frame cost beyond a compare.
// One deliberate artefact: the frame it fires on loses that pass from the world image.
//

#pragma once

#include <REX/W32/D3D11.h>

PLUGIN_NAMESPACE_BEGIN

class EnginePassSpike
{
public:
    static EnginePassSpike& instance();

    EnginePassSpike(EnginePassSpike const&) = delete;
    EnginePassSpike& operator=(EnginePassSpike const&) = delete;

    // Writes the detour. Call once at load; a branch cannot quietly be taken back afterwards.
    static void install();

    // Game-thread frame tick: arms the spike once the preview's 3D carries a skinned geometry.
    static void on_frame();

private:
    EnginePassSpike() = default;
    ~EnginePassSpike() = default;
};

PLUGIN_NAMESPACE_END
