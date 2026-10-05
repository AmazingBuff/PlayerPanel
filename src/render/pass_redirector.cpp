//
// Pass redirector core: call-site hook installation, the frame bracket,
// pass classification, target lifecycle, and the passthrough suppression
// thunks. (2026-10-05 src migration: split out of tools/m0_proto, behavior byte-identical)
//

#include "panel/panel.h"
#include "pinstance/pinstance.h"
#include "render/composite.h"
#include "render/evidence.h"
#include "render/offscreen_target.h"
#include "render/pass_redirector.h"
#include "render/render_internal.h"

#include <unordered_set>

PLUGIN_NAMESPACE_BEGIN
namespace
{
    // The three RenderPassImmediately call sites Community Shaders and the
    // stage-0 spike hook: RelocationID + the AE call-instruction offsets
    // inside those functions. SetupAndDrawPass is RELOCATION_ID(100854,
    // 107644); the runtime is gated to AE 1.6.1170, so the AE column is
    // the live one.
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

    // Per-open discovery cap for menu-geometry log lines: each menu
    // geometry is logged once per panel open; afterwards only counters
    // advance (a persistent frame would otherwise spam the log at frame
    // rate).
    constexpr std::size_t Menu_Geom_Log_Cap = 256;

    // --- pass redirector (render thread only) --------------------------

    // Bound for the parent-chain walk to the menuObjects roots.
    constexpr std::size_t Ancestry_Max_Depth = 32;
}

void call_site_original(std::size_t site_index, RE::BSRenderPass* pass, std::uint32_t technique,
    bool alpha_test, std::uint32_t render_flags)
{
    const std::uintptr_t target = s_original_targets[site_index];
    if (target)
        reinterpret_cast<RenderPassImmediately_t>(target)(pass, technique, alpha_test, render_flags);
}

bool PassRedirector::install()
{
    // The three call-site hooks go through a private local
    // trampoline (skse64 2.2.6's shared branch pool asserts on
    // plugin-sized requests); three 5-byte rewrites need only a
    // few dozen bytes.
    SKSE::GetTrampoline().create(64 * 1024);

    void (*thunks[3])(RE::BSRenderPass*, std::uint32_t, bool, std::uint32_t) = {
        &thunk_site0,
        &thunk_site1,
        &thunk_site2,
    };
    for (std::size_t i = 0; i < 3; ++i)
    {
        const CallSite& site = k_call_sites[i];
        std::uintptr_t address = REL::RelocationID(site.id_se, site.id_ae).address() +
            REL::Relocate(site.offset_se, site.offset_ae, site.offset_ae);
        // Parse the E8 rel32 BEFORE patching: whatever the call
        // site pointed at (vanilla SetupAndDrawPass, or Community
        // Shaders' interposer when LightLimitFix got here first)
        // is the true original our thunk and replays must run.
        const std::uintptr_t setup_address = REL::RelocationID(100854, 107644).address();
        auto* patch_bytes = reinterpret_cast<const std::uint8_t*>(address);
        if (patch_bytes[0] == 0xE8)
            s_original_targets[i] =
                address + 5 + *reinterpret_cast<const std::int32_t*>(address + 1);
        REL::Relocation<std::uintptr_t> hook{ address };
        hook.write_call<5>(thunks[i]);
        logger::info("Proto v3 pass hook {} installed at 0x{:X}; pre-patch target 0x{:X} "
                     "(SetupAndDrawPass=0x{:X}, {})",
            i, address, s_original_targets[i], setup_address,
            s_original_targets[i] == setup_address ? "unhooked" : "interposed, chain restored");
    }
    return true;
}

// DrawInterfaceStart entry: open the bracket, snapshot the menu
// roots passes will be matched against. Logging happens only on
// root-set changes — the bracket now runs every frame while the
// panel is open.
void PassRedirector::begin_frame()
{
    m_in_frame = true;
    ++m_frame_index;
    // Run 36: the proactive P draw fires once per studio frame
    // (latch reset with the clear).
    m_p_drawn_this_frame = false;
    // v6.38: the per-frame composite latch — replay-phase draws
    // set it; end_frame's no-menu-pass fallback consumes it.
    m_composited_this_frame = false;
    // Run 56: the studio-light reference goes stale with the
    // frame — only an item lighting pass seen THIS frame may
    // rebind P's passes.
    m_studio_lights_fresh = false;
    // Run 32 (ghosting fix): clear once per menu frame. The
    // studio target follows the call-site format, and the menu
    // stream (UI composite, format 28) is a DIFFERENT resource
    // from the world main target the P world-stream replays
    // used (format 10) — clearing here cannot wipe world-phase
    // pixels, while NOT clearing let every menu frame stack the
    // item replay and the P snapshot over the previous frame
    // (the user's RenderDoc finding).
    m_cleared = false;

    // Fresh panel open: reset per-open throttles and give a
    // previously failed target creation another chance.
    const std::uint32_t generation = Proto::instance().panel_generation();
    if (generation != m_generation)
    {
        m_generation = generation;
        m_content_frames = 0;
        m_last_logged_replayed = 0;
        m_session_replays = 0;
        m_target_failed = false;
        m_seen_geoms.clear();
        m_seen_p_geoms.clear();
        m_p_geoms_logged = 0;
    }

    m_root_count = 0;
    if (RE::UI3DSceneManager* ui3d = RE::UI3DSceneManager::GetSingleton())
    {
        for (std::size_t i = 0; i < 8; ++i)
        {
            RE::NiNode* root = ui3d->menuObjects[i].get();
            if (root)
                m_roots[m_root_count++] = root;
        }
    }

    const bool roots_changed = m_root_count != m_logged_root_count ||
        !std::equal(m_roots, m_roots + m_root_count, m_logged_roots);
    if (roots_changed)
    {
        std::copy(m_roots, m_roots + m_root_count, m_logged_roots);
        m_logged_root_count = m_root_count;
        logger::info("Proto v3 panel frame #{}: menu roots changed to {}{}", m_frame_index,
            m_root_count, [this] {
                std::string names;
                for (std::size_t i = 0; i < m_root_count; ++i)
                    names += fmt::format(" [{}]={}", i,
                        m_roots[i]->name.c_str() ? m_roots[i]->name.c_str() : "(null)");
                return names;
            }());
    }
}

// DrawInterfaceStart return: close the bracket, consume a pending
// F7 dump, summarize — and (v4.2) composite the panel into the
// composite target captured during THIS frame's replays. Runs
// 20/21 proved the entry-time draw lands in the previous
// frame's pool instance and never reaches the screen: the
// format-28 UI composite rotates instances every menu frame
// (recapture lines ~0.35 s apart in run 21). Drawing here —
// after the original menu draw, before DrawInterfaceStart
// returns — is inside the same frame the engine's merge reads.
// Gated on replays having happened this frame, so a stale
// capture from an earlier menu frame is never used.
void PassRedirector::end_frame()
{
    m_in_frame = false;

    // Run 30 pacing: the world stream (P's passes) runs BEFORE
    // the menu bracket each rendered frame, so the studio target
    // is cleared by the first P replay of that world phase and
    // the menu-phase item replays draw ON TOP of P without
    // clearing. The reset moves here — after everything a frame
    // will draw — instead of begin_frame, which would wipe the
    // already-drawn P every menu frame.

    if (Proto::instance().take_dump())
    {
        if (offscreen_target().color)
            dump_offscreen_to_log_dir();
        else
            logger::warn("Proto v3 dump ignored: studio target does not exist");
    }

    // v4.5: the end_frame composite is REMOVED — runs 23/24
    // falsified it (the merge happens inside the original call,
    // before this point; the quad rasterized into an already-
    // merged instance). The composite now runs inside the
    // replay hook, at the last placement on the visible side of
    // that merge. The captured s_panel_rtv stays as evidence
    // only (it logs the call-site format every session).

    if (m_menu_passes == 0 && m_p_passes == 0)
    {
        if (m_frame_index % Heartbeat_Frames == 0)
            logger::info("Proto v3 panel frame #{} heartbeat: passes_seen={} menu_passes=0 "
                         "(content-free frame, target keeps last studio image)",
                m_frame_index, m_passes_seen);
    }

    // v6.43: STUDIO TARGET SELF-CREATION — the INPUT side of the
    // decoupling. Run 76 (user RenderDoc + log): draw() exits at
    // its !target.srv guard, and draw_p_proactively exits at its
    // !target.rtv guard — the studio offscreen target was only
    // ever created inside replay_after_original (from the call
    // site's desc), and with ZERO passes there is no template
    // and no target: no P draw, no composite, black panel. The
    // target is now created HERE (BEFORE the proactive draw, so
    // the same frame's P lands in it) from the engine's
    // persistent kFRAMEBUFFER desc when absent: same screen
    // resolution; color format pinned to R8G8B8A8_UNORM (28) —
    // the exact representation the item-call-site target used
    // all along, so replays and the composite sampler see the
    // same format; depth normalized to D24_UNORM_S8_UINT
    // (normalize_depth_format's fallback). The replay path's
    // sig-based recreate still runs when a real call-site
    // template shows up with a different desc (e.g. resolution
    // change) — both paths share target_sig, so no thrash.
    {
        auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
        auto* rt = renderer ? &renderer->GetRuntimeData() : nullptr;
        OffscreenTarget& target = offscreen_target();
        // v6.44: run 77 showed ZERO self-created lines AND zero
        // failure warns — the block was skipped on a silent
        // path. Every skip branch now leaves a one-shot trace.
        if (!m_selfcreate_trace_logged)
        {
            m_selfcreate_trace_logged = true;
            logger::info(
                "Proto v6.44 self-create trace: renderer={} rt={} context={} forwarder={} "
                "target.rtv={} targetFailed={} fb.texture={} fb.RTV={}",
                static_cast<const void*>(renderer), static_cast<const void*>(rt),
                rt ? static_cast<const void*>(rt->context) : nullptr,
                rt ? static_cast<const void*>(rt->forwarder) : nullptr,
                static_cast<const void*>(target.rtv), m_target_failed,
                (rt && rt->context) ? static_cast<const void*>(rt->renderTargets[RE::RENDER_TARGET::kFRAMEBUFFER].texture) : nullptr,
                (rt && rt->context) ? static_cast<const void*>(rt->renderTargets[RE::RENDER_TARGET::kFRAMEBUFFER].RTV) : nullptr);
        }
        if (rt && rt->context && !target.rtv && !m_target_failed)
        {
            auto& fb = rt->renderTargets[RE::RENDER_TARGET::kFRAMEBUFFER];
            // v6.45: run 78 trace settled it — fb.texture is NULL
            // while fb.RTV is live (CLib's RenderTargetData
            // texture/textureCopy pointers are simply not
            // populated at runtime; the engine only touches these
            // targets through its views). Read the template desc
            // from the RTV instead — the same GetResource/GetDesc
            // walk the replay path has used on the format-28
            // instances since v4.1.
            REX::W32::ID3D11Resource* fb_res = nullptr;
            if (fb.RTV)
                fb.RTV->GetResource(&fb_res);
            if (fb_res)
            {
                REX::W32::D3D11_TEXTURE2D_DESC fb_desc{};
                static_cast<REX::W32::ID3D11Texture2D*>(fb_res)->GetDesc(&fb_desc);
                fb_res->Release();
                REX::W32::D3D11_TEXTURE2D_DESC template_desc = fb_desc;
                template_desc.format = REX::W32::DXGI_FORMAT_R8G8B8A8_UNORM;
                constexpr auto kStudioDepth = REX::W32::DXGI_FORMAT_D24_UNORM_S8_UINT;
                const TargetSig sig{ template_desc.width, template_desc.height,
                    static_cast<std::uint32_t>(template_desc.format),
                    static_cast<std::uint32_t>(kStudioDepth) };
                if (!(m_target_failed && sig == m_failed_sig))
                {
                    if (target.create(rt->forwarder, template_desc, kStudioDepth))
                    {
                        m_cleared = false;  // fresh surface, needs the first clear
                        logger::info("Proto v6.43 studio target self-created from kFRAMEBUFFER: "
                                     "{}x{} format=28 (no pass needed)",
                            template_desc.width, template_desc.height);
                    }
                    else
                    {
                        m_target_failed = true;
                        m_failed_sig = sig;
                        logger::warn("Proto v6.43 studio target self-creation failed; sticky "
                                     "for this configuration");
                    }
                }
            }
            else if (!m_fb_texture_null_logged)
            {
                m_fb_texture_null_logged = true;
                logger::warn("Proto v6.44 self-create skipped: kFRAMEBUFFER.RTV is null "
                             "(no template source at bracket exit)");
            }
        }
    }

    // Run 43 (v6.8): the paused inventory does NOT re-draw the
    // same item — no item pass, no OM window from
    // replay_after_original, so P was drawn only on the arm
    // frame (74 bracketed frames, zero draws after). end_frame
    // now DRIVES the proactive draw directly, every bracketed
    // frame: it binds its own studio OM window, clears once per
    // frame, poses, and draws — zero dependence on engine
    // passes. The item-pass window path stays as a redundant
    // trigger (the frame latch prevents double draws).
    if (auto* ui3d = RE::UI3DSceneManager::GetSingleton())
        draw_p_proactively(ui3d->unk10.get());

    // v6.38: PANEL-VISIBILITY DECOUPLING (user report: the panel
    // only appeared after highlighting a 3D item first). The
    // in-replay composite runs only when a pass reaches the
    // thunk — and the user's run-75 finding settles it: opening
    // the inventory does NOT enter thunk_site at ALL (entering
    // requires highlighting a 3D item), so before the first
    // highlight there is no capture AND no composite. The panel
    // display must bypass the pass path entirely.
    //
    // v6.42: the fallback composite targets the engine's
    // PERSISTENT kFRAMEBUFFER entry —
    // Renderer::GetRuntimeData().renderTargets[kFRAMEBUFFER].RTV
    // — the same engine-ledger slot CS's SetUIBuffer reads.
    // Pure memory read: no OM probing at exit (the v6.40 crash
    // lesson), no captured-pointer lifecycle, never
    // uninitialized while the renderer lives. At bracket exit
    // the menu draw has finished and the menu path renders INTO
    // this framebuffer (vanilla UI composites to kFRAMEBUFFER —
    // CS's SetUIBuffer comment), so a quad drawn now is on the
    // presented frame. CompositeRenderer::draw saves/restores
    // the full OM/state set around our quad (proven in every
    // session since v4.6) — the exit state the engine/CS chain
    // expects is restored before we return.
    if (!m_composited_this_frame)
    {
        auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
        auto* rt = renderer ? &renderer->GetRuntimeData() : nullptr;
        if (rt && rt->context && CompositeRenderer::instance().ensure(rt->forwarder))
        {
            auto& fb = rt->renderTargets[RE::RENDER_TARGET::kFRAMEBUFFER];
            if (fb.RTV)
            {
                CompositeRenderer::instance().draw(rt->context, fb.RTV);
                ++m_end_frame_composites;
                if (m_end_frame_composites == 1 ||
                    m_end_frame_composites % Heartbeat_Frames == 0)
                    logger::info("Proto v6.42 end_frame composite #{} (kFRAMEBUFFER; zero "
                                 "pass dependence — the panel follows the inventory only)",
                        m_end_frame_composites);
            }
        }
    }

    if (m_menu_passes != 0 || m_p_passes != 0)
    {
        ++m_content_frames;
        m_session_replays += m_p_total_replays;
        m_p_total_replays = 0;
        if (m_p_total_replays != m_last_logged_replayed ||
            m_content_frames % Heartbeat_Frames == 1)
        {
            m_last_logged_replayed = m_p_total_replays;
            logger::info("Proto v3 panel frame #{}: passes_seen={} menu_passes={} p_passes={} "
                         "geoms_logged={}",
                m_frame_index, m_passes_seen, m_menu_passes, m_p_passes, m_geoms_logged);
        }
    }
    m_passes_seen = 0;
    m_menu_passes = 0;
    m_p_passes = 0;
    m_menu_lighting_replayed = 0;
}

// Render-thread pass observation. Returns true when the pass
// should be replayed into the private target AFTER the original
// call has run.
//
// Two acceptance paths (run 29 finding): menu-scene passes (the
// item preview, inside the DrawInterfaceStart bracket) and — new
// in stage 2 — the display instance P. P's graph does NOT flow
// through the menu culler (the menu scene collects geometry via
// the culler's private queue, not scene-graph attachment —
// capture-report addendum 2), so its passes run in the WORLD
// pass stream outside the bracket. The world stream hits the
// same three RenderPassImmediately call sites, so the thunk
// sees those passes anyway; P is matched by root ancestry and
// gated on the panel being open, which is what makes the world
// stream safe to touch (panel closed -> P absent -> zero extra
// replays; normal world passes never match the P root).
bool PassRedirector::on_pass(RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test, std::uint32_t render_flags,
    std::size_t site_index)
{
    if (!pass || !pass->geometry)
        return false;

    const bool panel_open = Proto::instance().panel_frame_active();
    const bool p_geom = panel_open && PInstance::instance().is_p_geometry(pass->geometry);
    // Run 54: the thunk reads this right after on_pass returns
    // (same render-thread call) to decide the passthrough.
    m_last_pass_p_geom = p_geom;
    // v6.22's ghost layer RETIRED in v6.64 (stage-2b round 2):
    // the graph lives in CP_StudioHome and the shell dies at
    // relocation, so the world stream no longer contains P
    // passes outside the ~1.5 s build window — which the
    // node-level park keeps out of sight anyway.

    // Outside the menu bracket only P passes are accepted — the
    // studio target must never accumulate world scenery.
    if (!m_in_frame && !p_geom)
        return false;

    ++m_passes_seen;
    const bool menu = m_in_frame && is_menu_geometry(pass->geometry);
    if (menu)
    {
        ++m_menu_passes;
        // Run 53/56: record the item preview's own lights —
        // they ARE the studio lighting. P's passes carry
        // dungeon/world lights that are thousands of units from
        // the menu-space fragments the studio pose produces, so
        // the PS collapses to ambient-only (run 52: figure fully
        // formed in the depth buffer, near-black in the RT).
        // Run 56: reference THIS pass's own sceneLights array —
        // the exact storage the engine (and CS's light hooks)
        // use for the item's own draw that frame — instead of
        // copying pointers into a member array (run 55: copied
        // pointers outlived the light objects after an item
        // change and CS's GeometrySetupConstantPointLights
        // crashed on them).
        // v6.53: MENU-LIGHT ARMING RETIRED. Highlighting an item
        // used to overwrite m_studio_light_array with the item's
        // own light array — which means every "highlight = lit"
        // observation was the MENU lights drawing the figure
        // (v6.18), and the self-built lights were never actually
        // exercised while the menu lights existed. With the
        // override now unconditional (v6.47), leaving this armed
        // made the two sources fight: highlight frames drew with
        // menu lights, no-highlight frames with our lights.
        // Removed so the self-built lights get a clean test.
        if (pass->shader && std::to_underlying(pass->shader->shaderType.get()) == 6 &&
            pass->sceneLights && pass->numLights > 0 && !m_studio_light_array)
        {
            m_studio_light_array = pass->sceneLights;
            m_studio_light_count = pass->numLights;
            m_studio_lights_fresh = true;
            logger::info("Proto v6.53 legacy menu-light arming SKIPPED (self-built lights "
                         "active; {} menu lights seen)",
                pass->numLights);
        }
        // Discovery logging: one line per menu geometry per panel
        // open; steady-state frames only advance the counters.
        // p=1 marks stage-2 display-instance P geometry (skinned
        // content the skin-replay round watches for).
        if (m_geoms_logged < Menu_Geom_Log_Cap && m_seen_geoms.insert(pass->geometry).second)
        {
            ++m_geoms_logged;
            logger::info(
                "Proto v3 menu pass: geom={} name=[{}] shader={} passEnum=0x{:X} "
                "technique=0x{:X} alphaTest={} numLights={} renderFlags=0x{:X} site={} p={}",
                static_cast<void*>(pass->geometry),
                pass->geometry->name.c_str() ? pass->geometry->name.c_str() : "(null)",
                pass->shader ? std::to_underlying(pass->shader->shaderType.get()) : 0u,
                pass->passEnum, technique, alpha_test, pass->numLights, render_flags,
                site_index, p_geom ? 1 : 0);
        }
    }
    else if (p_geom)
    {
        ++m_p_passes;
        // Discovery per P geometry per panel open: the skinned
        // set (body/armor/hair) is exactly what this round must
        // show.
        if (m_p_geoms_logged < Menu_Geom_Log_Cap && m_seen_p_geoms.insert(pass->geometry).second)
        {
            ++m_p_geoms_logged;
            logger::info(
                "Proto v5 P pass: geom={} name=[{}] shader={} passEnum=0x{:X} "
                "technique=0x{:X} alphaTest={} numLights={} renderFlags=0x{:X} site={} in_menu_frame={}",
                static_cast<void*>(pass->geometry),
                pass->geometry->name.c_str() ? pass->geometry->name.c_str() : "(null)",
                pass->shader ? std::to_underlying(pass->shader->shaderType.get()) : 0u,
                pass->passEnum, technique, alpha_test, pass->numLights, render_flags,
                site_index, m_in_frame ? 1 : 0);
        }
    }

    if (!menu && !p_geom)
        return false;
    // v2 replays lighting passes only for the MENU stream (they
    // carry the shading; depth/shadow passes write no colour).
    // Run 32: P keeps ALL shader types — its type-6 set is hair
    // only (0Anto92/HAIRLINE/Brows), while the skinned body
    // (CBBE/clothes/shoes/face) renders through BSShader effects
    // (type 8); the run-31 blob props are already excluded by
    // the skinned-only whitelist in is_p_geometry.
    // Run 35: P passes are no longer snapshotted here — they are
    // generated proactively from the geometry's shader property
    // inside the studio OM window (draw_p_proactively). The
    // world-stream passthrough suppression stays: a live P pass
    // must not show the double in the world view.
    if (!pass->shader)
        return false;
    if (p_geom)
        return true;
    if (std::to_underlying(pass->shader->shaderType.get()) != 6)
        return false;

    return true;
}

// FR-06 cleanup, running on the render thread at a
// non-bracketed DrawInterfaceStart after the panel closed:
// destroy the studio resources once, reset sticky failures and
// session counters so the next open starts clean.
void PassRedirector::release_target(std::string_view reason)
{
    OffscreenTarget& target = offscreen_target();
    if (target.color)
    {
        target.destroy();
        logger::info("Proto v3 studio target released ({})", reason);
    }
    // v6.41: the captured composite target now PERSISTS across
    // panel open/close — the highlight-decoupling lever that
    // replaced the crashed v6.40 exit-OM capture. The format-28
    // instance pool only reallocates on resolution changes; the
    // replay path already re-captures on any pointer change
    // (v4.1), so a stale pointer self-heals on the first pass —
    // and releasing it here was what forced every panel open to
    // wait for a fresh item-highlight burst before the fallback
    // composite could run. Only an actual resolution change
    // invalidates the texture (the crash guard: the pool frees
    // its textures then — handled by the replay-path pointer
    // check + recapture, never by a dangling Release here).
    m_target_failed = false;
    m_session_replays = 0;
    m_p_drew_this_open = false;
    m_p_draw_logged = false;
    m_p_recipe_logged = false;
    m_p_empty_live_logged = false;
    m_p_no_root_logged = false;
    m_studio_light_array = nullptr;
    m_studio_light_count = 0;
    // v6.66: the raw wrappers must be re-matched every open —
    // the engine rebuilds its ledger on panel cycles (run 87)
    // and a menu transition FREES them outright (run 100
    // crash); holding them across closes held freed memory.
    for (auto*& shell : m_studio_lights)
        shell = nullptr;
    m_studio_lights_fresh = false;
    m_p_studio_lights_logged = false;
    m_pass_recipes.clear();
}

// v3.1: user-initiated close (F6). Runs 17/18 both ended with
// every F7 pressed AFTER closing the panel — the natural flow
// treats close as "done, capture now" — so the close itself
// writes the evidence: one synchronous TGA of the last studio
// image BEFORE the release destroys the target. Skipped when
// nothing was replayed this open (empty target) and on
// force-closes (save loading / new game).
void PassRedirector::dump_and_release(std::string_view reason)
{
    // v6.63: a P-only open replays nothing (no highlighted item
    // → zero menu passes), but the proactive draw filled the
    // target — that is evidence too (run 96: every close
    // skipped the dump under the replay-only gate).
    if (offscreen_target().color && (m_session_replays > 0 || m_p_drew_this_open))
        dump_offscreen_to_log_dir();
    release_target(reason);
}

// A pass is a menu-scene pass when its geometry descends from one
// of the UI3DSceneManager::menuObjects roots (run 6 located the
// highlighted item under root[1]).
bool PassRedirector::is_menu_geometry(const RE::BSGeometry* geometry) const
{
    if (m_root_count == 0)
        return false;
    const RE::NiAVObject* node = geometry;
    for (std::size_t depth = 0; node && depth < Ancestry_Max_Depth; ++depth)
    {
        for (std::size_t i = 0; i < m_root_count; ++i)
        {
            if (node == m_roots[i])
                return true;
        }
        node = node->parent;
    }
    return false;
}
PLUGIN_NAMESPACE_END
