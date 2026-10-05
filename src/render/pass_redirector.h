#pragma once
// The pass redirector: pass classification for the three
// RenderPassImmediately call-site thunks, the frame bracket
// (begin_frame/end_frame), the P matcher, and the studio draw/composite
// orchestration. (2026-10-05 src migration: split out of tools/m0_proto, behavior byte-identical)

#include <cstddef>
#include <unordered_set>

#include <RE/B/BSLight.h>
#include <RE/S/ShadowSceneNode.h>

#include "render/offscreen_target.h"
#include "render/render_internal.h"

PLUGIN_NAMESPACE_BEGIN
class PassRedirector
{
public:
    static PassRedirector& instance()
    {
        static PassRedirector s_instance;
        return s_instance;
    }

    bool install();
    void begin_frame();
    void end_frame();
    bool on_pass(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test,
        std::uint32_t render_flags, std::size_t site_index);
    void release_target(std::string_view reason);
    void dump_and_release(std::string_view reason);

private:
    PassRedirector() = default;

    // Run 31 (route 3): a whitelist P pass in the WORLD stream is
    // SUPPRESSED from the passthrough — the world must not show the
    // parked double (PRD 0.5: no panel/P in the world view) — and
    // exists only as the studio replay. Menu passes keep the normal
    // passthrough + replay order (runs 9-26 contract).
    //
    // Run 54: P passes are now suppressed in MENU frames too. The
    // proactive GetRenderPasses calls ENROLL P's passes into the
    // UI3D accumulator's persistent pass lists, and the engine then
    // draws them itself at the call sites — including the menu
    // teardown on inventory exit, where run 53's session CRASHED
    // inside BSLightingShader::SetupGeometry on the
    // studio-light-mutated pass (crash-2026-10-02-22-00-30: P's
    // armor BSTriShape, null light deref). Only the studio draw
    // renders P; the engine never touches these passes again.
    static bool should_suppress_passthrough(bool replay, bool p_geom, bool in_menu_frame)
    {
        // v6.19: while the panel is open the studio is P's only
        // renderer — the engine must not draw its passes anywhere
        // (world frames would show the double; menu frames include
        // the accumulator-cleanup draws that crashed run 53's
        // session). v6.64: the ghost clause is retired with the
        // ghost layer — post-relocation the world stream holds no P
        // passes outside the build window.
        return replay && (p_geom || !in_menu_frame);
    }

    static void thunk_site0(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test,
        std::uint32_t render_flags)
    {
        const bool replay = instance().on_pass(pass, technique, alpha_test, render_flags, 0);
        if (!should_suppress_passthrough(replay, instance().m_last_pass_p_geom,
                instance().m_in_frame))
            call_site_original(0, pass, technique, alpha_test, render_flags);
        if (replay)
            instance().replay_after_original(pass, technique, alpha_test, render_flags, 0);
    }
    static void thunk_site1(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test,
        std::uint32_t render_flags)
    {
        const bool replay = instance().on_pass(pass, technique, alpha_test, render_flags, 1);
        if (!should_suppress_passthrough(replay, instance().m_last_pass_p_geom,
                instance().m_in_frame))
            call_site_original(1, pass, technique, alpha_test, render_flags);
        if (replay)
            instance().replay_after_original(pass, technique, alpha_test, render_flags, 1);
    }
    static void thunk_site2(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test,
        std::uint32_t render_flags)
    {
        const bool replay = instance().on_pass(pass, technique, alpha_test, render_flags, 2);
        if (!should_suppress_passthrough(replay, instance().m_last_pass_p_geom,
                instance().m_in_frame))
            call_site_original(2, pass, technique, alpha_test, render_flags);
        if (replay)
            instance().replay_after_original(pass, technique, alpha_test, render_flags, 2);
    }

    bool is_menu_geometry(const RE::BSGeometry* geometry) const;
    static RE::BSLight* fetch_light_wrapper(RE::ShadowSceneNode* a_node, RE::NiLight* a_light);
    void reset_light_rig(const char* a_reason);
    void ensure_studio_lights();
    void park_studio_rig();
    void draw_p_proactively(RE::BSShaderAccumulator* accumulator);
    static void call_site_original_from_pass(RE::BSRenderPass* pass);
    void replay_after_original(RE::BSRenderPass*, std::uint32_t, bool, std::uint32_t, std::size_t);

    // v6.58: the studio lights are REGISTERED in the world's light
    // ledger (v6.49, required for LLF), so the engine renders them
    // into the WORLD whenever they sit anywhere near gameplay
    // space — the user's run-91 report: dungeon walls lit with the
    // panel's warm key light while the inventory was open, the
    // patch moving as the rig moved. The rig node therefore lives
    // FAR OUT of the world by default (parked), and only returns
    // to the studio anchor for the duration of the P draw window —
    // the one interval our mutate/draw/restore owns. Outside that
    // window (panel-open non-bracket frames, panel closed, world
    // frames) the lights are 100k units away and light nothing.
    static constexpr float Rig_Park_Z = 100000.0f;

    RE::NiNode* m_roots[8]{};
    RE::NiNode* m_logged_roots[8]{};
    std::size_t m_root_count = 0;
    std::size_t m_logged_root_count = 0;
    // Menu geometries already logged this panel open (discovery cap).
    std::unordered_set<const RE::BSGeometry*> m_seen_geoms;
    // P geometries already logged this panel open (run 29: P passes
    // arrive in the WORLD stream, outside the menu bracket).
    std::unordered_set<const RE::BSGeometry*> m_seen_p_geoms;
    std::uint32_t m_generation = 0;
    std::uint32_t m_frame_index = 0;
    std::uint32_t m_passes_seen = 0;
    std::uint32_t m_geoms_logged = 0;
    std::uint32_t m_p_geoms_logged = 0;
    std::uint32_t m_menu_passes = 0;
    std::uint32_t m_p_passes = 0;
    std::uint32_t m_menu_lighting_replayed = 0;
    // Replays accumulated since the last end_frame summary; the
    // close-dump gate (m_session_replays) consumes them so a P-only
    // session still writes its evidence TGA.
    std::uint32_t m_p_total_replays = 0;
    std::uint32_t m_last_logged_replayed = 0;
    std::uint32_t m_content_frames = 0;
    std::uint32_t m_session_replays = 0;
    // v6.63: the proactive P draw submitted at least one pass this
    // open — the close-dump gate counts it as studio content.
    bool m_p_drew_this_open = false;
    bool m_cleared = false;
    bool m_target_failed = false;
    TargetSig m_failed_sig{};
    bool m_state_logged = false;
    bool m_srv_logged = false;
    bool m_binding_logged = false;
    bool m_in_frame = false;
    // v6.38: set by the in-replay composite, consumed by end_frame's
    // no-menu-pass fallback — one composite per studio frame.
    bool m_composited_this_frame = false;
    std::uint32_t m_end_frame_composites = 0;
    // v6.44: one-shot traces for the self-create block's skip paths
    // (run 77: the block skipped with zero log lines — silence made
    // the skip path unidentifiable).
    bool m_selfcreate_trace_logged = false;
    bool m_fb_texture_null_logged = false;
    // v6.46: the dirty-bit guard fired this studio window (paired
    // set-back at the restore).
    bool m_rt_dirty_guard_used = false;
    // v6.47: the self-built studio lights. Shells live for the
    // session (render thread only); the NiLights are owned by the
    // scene graph (menuObjects[0]) AND these pointers (the shell's
    // NiPointer holds one ref, these hold another — detach would
    // need both cleared; despawn scope is session end, where leak-
    // on-exit is acceptable for the proto).
    RE::BSLight* m_studio_lights[Studio_Light_Count] = {};
    // v6.66: the ShadowSceneNode the lights were last registered
    // with — a changed pointer means the world was rebuilt (menu
    // transition / load) and the old wrappers are gone.
    RE::ShadowSceneNode* m_studio_lights_node = nullptr;
    // v6.67: the UI3D host the rig was last created under — a
    // changed pointer means the menu scene was rebuilt and the old
    // rig is orphaned (its lights no longer reach the ledger).
    RE::NiNode* m_studio_lights_host = nullptr;
    // v6.67: the incomplete-fetch warn latch (cleared on success).
    bool m_fetch_warned = false;
    // v6.68: consecutive incomplete-fetch windows — at
    // Fetch_Stall_Reset_Windows the whole rig is re-created.
    std::uint32_t m_fetch_stall_windows = 0;
    RE::NiPointer<RE::NiLight> m_studio_light_ni[Studio_Light_Count];
    // v6.51: the private rig node the lights hang from — fresh,
    // flag-free, so forced cascades actually move them.
    RE::NiPointer<RE::NiNode> m_studio_rig_node;
    bool m_studio_lights_failed = false;
    bool m_p_draw_logged = false;
    bool m_p_binding_logged = false;
    bool m_p_drawn_this_frame = false;
    bool m_p_recipe_logged = false;
    bool m_p_empty_live_logged = false;
    // Run 54: the pass just classified by on_pass — the thunks read
    // it (same render-thread call) to keep the engine from ever
    // drawing P's passes (world double + menu teardown crash).
    bool m_last_pass_p_geom = false;
    // Run 54: once-per-open marker for the no-root early return in
    // draw_p_proactively — a silent kNone here is what swallowed
    // run 53's steady-state draws without a single log line.
    bool m_p_no_root_logged = false;
    // v6.33: once-per-session marker for the frontal light rig log.
    bool m_p_frontal_logged = false;
    // Run 53/56: the item preview's menu lights — the studio
    // lighting for P's passes (whose own lights are dungeon/world
    // lights, thousands of units from the menu-space fragments the
    // pose produces). Run 56: we now reference the ITEM PASS'S OWN
    // sceneLights ARRAY (the exact storage the engine — and CS's
    // light hooks — use for the item's own draw that frame)
    // instead of copying BSLight pointers into a member array:
    // run 55's session CRASHED in CS's
    // GeometrySetupConstantPointLights after the copied pointers
    // outlived the light objects (item selection changed mid
    // session). The freshness flag gates the override to frames
    // where a menu lighting pass actually arrived.
    RE::BSLight** m_studio_light_array = nullptr;
    std::uint8_t m_studio_light_count = 0;
    bool m_studio_lights_fresh = false;
    bool m_p_studio_lights_logged = false;

    // Run 46: pass recipes recorded on successful live generation —
    // the paused inventory stops regenerating passes, so later
    // frames rebuild them via BSShader::MakeRenderPass (ID 107497).
    // All referenced objects live for the panel open; cleared on
    // release_target.
    struct PPassRecipe
    {
        RE::BSShader* shader;
        RE::BSShaderProperty* property;
        RE::BSGeometry* geometry;
        std::uint32_t technique;
        std::uint8_t num_lights;
        RE::BSLight* lights[4];
    };
    std::vector<PPassRecipe> m_pass_recipes;
};
PLUGIN_NAMESPACE_END
