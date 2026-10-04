#pragma once
// Shared render-layer internals: the RenderPassImmediately call-site
// plumbing, the shared pre-patch target table, and log cadence constants
// used by more than one render TU. (2026-10-05 src migration: split out of tools/m0_proto, behavior byte-identical)

#include <cstdint>
#include <RE/B/BSRenderPass.h>

PLUGIN_NAMESPACE_BEGIN
    using DrawInterfaceStart_t = void (*)(std::int64_t);
    using RenderPassImmediately_t = void (*)(RE::BSRenderPass*, std::uint32_t, bool, std::uint32_t);

    // Pre-patch call targets, parsed from the E8 rel32 before our
    // write_call. The three call sites are hook-sharing real estate:
    // Community Shaders' LightLimitFix patches the same instructions, and
    // run 11 (garbage shield textures while geometry stayed correct) is
    // consistent with our patch having bypassed its interposer — CS's
    // replaced shaders then draw with stale constant buffers. Both the
    // passthrough and the replay must run the true pre-patch target.
    inline std::uintptr_t s_original_targets[3]{};

    void call_site_original(std::size_t site_index, RE::BSRenderPass* pass, std::uint32_t technique,
        bool alpha_test, std::uint32_t render_flags);

    // Slow log cadence for both summary flavors — one line per N frames
    // (~30 s at 60 fps). Content summaries additionally fire whenever
    // the replay count changes (item switches).
    constexpr std::uint32_t Heartbeat_Frames = 1800;

    // v6.47: self-built studio lights — the panel's lighting no longer
    // depends on menu-item lights at all. v6.56: slot 0 = ambient base
    // (engine convention: point lights start at sceneLights[1]), slots
    // 1/2 = key/fill point lights.
    constexpr std::size_t Studio_Light_Count = 3;

    // v6.68: how many consecutive draw windows an incomplete wrapper
    // fetch may stall before the whole light rig is discarded and
    // re-created (run 102: the transition cut the rig out of the host's
    // children with every pointer cache matching — the stall is the
    // only observable). ~0.5 s of dark figure per recovery attempt.
    constexpr std::uint32_t Fetch_Stall_Reset_Windows = 30;
PLUGIN_NAMESPACE_END
