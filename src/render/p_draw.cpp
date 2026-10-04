//
// The proactive P draw: per-frame studio rendering of the whitelisted P
// graph via the engine's own pass generator and SetupAndDrawPass chain.
// (2026-10-05 src migration: split out of tools/m0_proto, behavior byte-identical)
//

#include "pinstance/pinstance.h"
#include "render/composite.h"
#include "render/offscreen_target.h"
#include "render/pass_redirector.h"
#include "render/render_internal.h"

#include <RE/Skyrim.h>
#include <RE/B/BSLight.h>
#include <RE/R/RendererShadowState.h>
#include <REX/W32/D3D11.h>
#include <fmt/format.h>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

using namespace std::literals;
namespace logger = SKSE::log;

namespace CharacterPanelProto
{
    void PassRedirector::draw_p_proactively(RE::BSShaderAccumulator* accumulator)
    {
        // Stage-2b: the game thread may be inside the relocation
        // window (world detach -> menu-home attach). Skip this
        // frame; the next one draws from the home.
        if (PInstance::instance().relocating())
            return;
        RE::NiAVObject* p_root = PInstance::instance().root();
        if (!p_root || !accumulator)
        {
            // Run 54: once-per-open visibility for the silent
            // swallow — run 53's steady-state frames stopped
            // drawing with ZERO log lines because the whitelist
            // root vanished (PInstance back to kNone, cause still
            // unidentified); this line makes it visible.
            if (!p_root && !m_p_no_root_logged)
            {
                m_p_no_root_logged = true;
                logger::warn("Proto P draw skipped: whitelist root is null (PInstance state lost?)");
            }
            return;
        }
        // Run 36: one P draw per studio frame (replay_after_original
        // may call this for every item pass in a frame).
        if (m_p_drawn_this_frame)
            return;
        m_p_drawn_this_frame = true;

        // v6.58: bring the light rig back INTO the studio anchor
        // neighborhood for the draw window (the lights are world-
        // registered; parked they sit 100k away and light nothing —
        // see park_studio_rig). The rig's parent is menuObjects[0],
        // whose world is the menu-space identity cascade, so the
        // local translate IS the accumulator-space position.
        if (m_studio_rig_node)
        {
            const RE::NiPoint3 anchor = PInstance::instance().studio_anchor();
            m_studio_rig_node->local.translate = anchor;
            RE::NiUpdateData data{ 0.0f, RE::NiUpdateData::Flag::kDirty };
            m_studio_rig_node->UpdateDownwardPass(data, 0);
        }

        auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
        auto& runtime = renderer->GetRuntimeData();
        if (!runtime.context || !runtime.forwarder)
        {
            park_studio_rig();  // v6.58: pulled in above; park back out
            return;
        }

        // Save the ambient OM pair + viewport + depth state.
        REX::W32::ID3D11RenderTargetView* prev_rtv = nullptr;
        REX::W32::ID3D11DepthStencilView* prev_dsv = nullptr;
        runtime.context->OMGetRenderTargets(1, &prev_rtv, &prev_dsv);
        REX::W32::ID3D11DepthStencilState* prev_ds = nullptr;
        std::uint32_t prev_stencil_ref = 0;
        runtime.context->OMGetDepthStencilState(&prev_ds, &prev_stencil_ref);
        REX::W32::D3D11_VIEWPORT prev_viewport{};
        std::uint32_t prev_viewport_count = 1;
        runtime.context->RSGetViewports(&prev_viewport_count, &prev_viewport);

        // Studio target: persistent, sized from the captured
        // call-site description (recreated on desc change).
        REX::W32::D3D11_TEXTURE2D_DESC desc{};
        if (s_panel_rtv)
        {
            REX::W32::ID3D11Resource* resource = nullptr;
            s_panel_rtv->GetResource(&resource);
            if (resource)
            {
                static_cast<REX::W32::ID3D11Texture2D*>(resource)->GetDesc(&desc);
                resource->Release();
            }
        }
        OffscreenTarget& target = offscreen_target();
        if (!target.rtv)
        {
            // No template this frame (first draw may precede any
            // capture); skip — but the rig was pulled in at the top,
            // so park it back out before leaving (v6.58).
            park_studio_rig();
            if (prev_rtv)
                prev_rtv->Release();
            if (prev_dsv)
                prev_dsv->Release();
            return;
        }

        PInstance::instance().pose_for_studio();

        runtime.context->OMSetRenderTargets(1, &target.rtv, target.dsv);
        runtime.context->OMSetDepthStencilState(target.ds_state, 0);
        // v6.46: ENGINE DIRTY-BIT GUARD. Run 79 (user screenshots +
        // log): with the self-created target and no menu-item pass
        // context, P's bow prop and garbage-skinned geometry landed
        // ON SCREEN outside the panel — the engine's
        // SetupAndDrawPass re-applies the LEDGER's render target
        // whenever ShaderFlags::DIRTY_RENDERTARGET is set (CS
        // Deferred.cpp drives the same machinery), and with nothing
        // having bound our private target through engine state that
        // frame, the ledger still said kFRAMEBUFFER. The highlight
        // window was clean precisely because the item call-site had
        // just bound the format-28 instance through engine state.
        // Fix: after our raw bind, CLEAR the dirty bit so the pass
        // internals treat the OM as current (our bind stays); after
        // the window's restore, SET it back so the engine rebuilds
        // its own bindings next time it applies state (CS coexists
        // with this exact handshake every frame).
        {
            auto* shadow_state = RE::BSGraphics::RendererShadowState::GetSingleton();
            shadow_state->GetRuntimeData().stateUpdateFlags.reset(
                RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);
            m_rt_dirty_guard_used = true;
        }
        REX::W32::D3D11_VIEWPORT viewport{ 0.0f, 0.0f,
            static_cast<float>(target.width), static_cast<float>(target.height), 0.0f, 1.0f };
        runtime.context->RSSetViewports(1, &viewport);
        if (!m_cleared)
        {
            const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            runtime.context->ClearRenderTargetView(target.rtv, clear_color);
            runtime.context->ClearDepthStencilView(target.dsv,
                REX::W32::D3D11_CLEAR_DEPTH | REX::W32::D3D11_CLEAR_STENCIL, 1.0f, 0);
            m_cleared = true;
        }

        // Run 41: opaque blend; run 42: clean rasterizer (the
        // engine's leftovers at the item call site — unknown blend
        // factors, depthBiasClamp=-100 — both black out the pixels).
        struct BlendGuard
        {
            BlendGuard(REX::W32::ID3D11DeviceContext* a_ctx, REX::W32::ID3D11BlendState* a_opaque) :
                ctx(a_ctx), opaque(a_opaque)
            {
                ctx->OMGetBlendState(&prev_blend, prev_factor, &prev_mask);
                ctx->OMSetBlendState(opaque, prev_factor, prev_mask);
            }
            ~BlendGuard()
            {
                ctx->OMSetBlendState(prev_blend, prev_factor, prev_mask);
                if (prev_blend)
                    prev_blend->Release();
            }
            REX::W32::ID3D11DeviceContext* ctx;
            REX::W32::ID3D11BlendState* opaque;
            REX::W32::ID3D11BlendState* prev_blend = nullptr;
            FLOAT prev_factor[4]{};
            UINT prev_mask = 0;
        } blend_guard(runtime.context, CompositeRenderer::instance().opaque_blend());

        struct RasterGuard
        {
            RasterGuard(REX::W32::ID3D11DeviceContext* a_ctx, REX::W32::ID3D11RasterizerState* a_clean) :
                ctx(a_ctx), clean(a_clean)
            {
                ctx->RSGetState(&prev_rs);
                ctx->RSSetState(clean);
            }
            ~RasterGuard()
            {
                ctx->RSSetState(prev_rs);
                if (prev_rs)
                    prev_rs->Release();
            }
            REX::W32::ID3D11DeviceContext* ctx;
            REX::W32::ID3D11RasterizerState* clean;
            REX::W32::ID3D11RasterizerState* prev_rs = nullptr;
        } raster_guard(runtime.context, CompositeRenderer::instance().clean_rasterizer());

        std::vector<RE::BSRenderPass*> passes;
        RE::BSVisit::TraverseScenegraphGeometries(p_root, [&](RE::BSGeometry* geometry) {
            if (!PInstance::instance().is_p_geometry(geometry))
                return RE::BSVisit::BSVisitControl::kContinue;
            auto& geom_rt = geometry->GetGeometryRuntimeData();
            auto* property = geom_rt.shaderProperty.get();
            if (!property)
                return RE::BSVisit::BSVisitControl::kContinue;
            // renderMode kNormal (0) — the menu accumulator's normal
            // path, same as the item preview's passes.
            auto* pass_array = property->GetRenderPasses(geometry,
                std::to_underlying(RE::BSShaderAccumulator::RENDER_MODE::kNormal), accumulator);
            if (pass_array)
            {
                for (RE::BSRenderPass* pass = pass_array->head; pass; pass = pass->next)
                {
                    if (pass->geometry == geometry && pass->shader)
                    {
                        passes.push_back(pass);
                        // Run 46: remember the PASS RECIPE — the
                        // paused inventory stops regenerating passes
                        // (run 43), so later frames must rebuild
                        // them from the recorded parameters. All
                        // referenced objects live for the panel
                        // open (actor graph, singleton shaders,
                        // UI3D menu lights). Dedup by full key.
                        const bool known = std::any_of(m_pass_recipes.begin(), m_pass_recipes.end(),
                            [&](const PPassRecipe& r) {
                                return r.shader == pass->shader && r.geometry == geometry &&
                                    r.technique == pass->passEnum;
                            });
                        if (!known)
                        {
                            m_pass_recipes.push_back(
                                { pass->shader, property, geometry, pass->passEnum,
                                    static_cast<std::uint8_t>(pass->numLights),
                                    { pass->sceneLights ? pass->sceneLights[0] : nullptr,
                                        pass->sceneLights ? pass->sceneLights[1] : nullptr,
                                        pass->sceneLights ? pass->sceneLights[2] : nullptr,
                                        pass->sceneLights ? pass->sceneLights[3] : nullptr } });
                        }
                    }
                }
            }
            // Run 46 fallback: the generator came up empty for this
            // geometry (paused-inventory state) — rebuild from a
            // previously recorded recipe instead.
            return RE::BSVisit::BSVisitControl::kContinue;
        });

        // Run 46: rebuild passes from recipes when the live
        // generator produced nothing (or too few). Deduplicate the
        // recipes accumulated over the session per panel open.
        if (passes.empty())
        {
            // Run 46: nothing live — try rebuilding from recorded
            // recipes. Also LOG the empty live generation once per
            // session: silent empty runs were the run-46 blind spot.
            if (!m_p_empty_live_logged)
            {
                m_p_empty_live_logged = true;
                logger::info("Proto v6.11 live GetRenderPasses produced nothing ({} recipes "
                             "recorded); rebuilding",
                    m_pass_recipes.size());
            }
            for (const auto& recipe : m_pass_recipes)
            {
                if (!PInstance::instance().is_p_geometry(recipe.geometry))
                    continue;
                RE::BSLight* lights[4] = { recipe.lights[0], recipe.lights[1], recipe.lights[2],
                    recipe.lights[3] };
                if (RE::BSRenderPass* rebuilt =
                        recipe.shader->MakeRenderPass(recipe.property, recipe.geometry,
                            recipe.technique, recipe.num_lights, lights))
                {
                    if (rebuilt->geometry == recipe.geometry)
                        passes.push_back(rebuilt);
                }
            }
        }
        else if (!m_pass_recipes.empty())
        {
            m_pass_recipes.clear();  // live generation works; recipes stale
        }

        // Run 47: per-frame draw summary (replaces the draw-once
        // log) — the user's RenderDoc report contradicted the
        // single-line log, which was latched forever. This line
        // states EXACTLY what each studio frame drew and from where.
        logger::info("Proto v6.11 P draw: source={} passes={}", passes.empty() && !m_pass_recipes.empty()
                                                                         ? "recipes-failed"
                                                                         : (passes.empty() ? "empty"
                                                                                           : "live/recipes"),
            passes.size());

        // Run 52 (v6.17): BACK TO THE RUN-35 CONFIGURATION (user
        // direction — "runs 35-42 actually drew the character, it
        // was just out of the frustum"). The v6.13-16 manual draw is
        // falsified: its rd gate blocked every pass on
        // rendererData=null (runs 49-51) — and if the engine's
        // SetupAndDrawPass is what lazily creates those device
        // buffers, the manual path removed the ONLY caller that
        // could ever initialize the geometry. Restore the proven
        // chain, exactly as runs 35-42 drew it: per pass, rebind the
        // studio OM pair (the v6.4 fix — SetupAndDrawPass's internal
        // shadow-state reapplication steals the binding), then call
        // the call-site original — the ENGINE's own SetupAndDrawPass
        // with the pass's own technique encoding (alphaTest =
        // passEnum bit 6, run-31 observation; renderFlags 0x200 =
        // the menu-stream value). The engine then handles technique
        // setup, geometry/device-buffer init, batch dispatch and
        // state; the guards added since (opaque blend, clean
        // rasterizer, forced cascade, near-plane pose) all stay.
        // Site 1's original = the CS-interposed chain the item
        // preview itself flows through, so the menu context matches.
        std::uint32_t drawn = 0;
        for (RE::BSRenderPass* pass : passes)
        {
            runtime.context->OMSetRenderTargets(1, &target.rtv, target.dsv);
            runtime.context->OMSetDepthStencilState(target.ds_state, 0);
            // Run 53/56: studio lighting — point the pass at the
            // item preview's own light array (recorded THIS frame;
            // the freshness gate skips frames without a menu
            // lighting pass, falling back to the pass's own lights),
            // then zero the shadow-light count (menu lights cast
            // none). Restored after the draw — these passes live in
            // the UI3D accumulator's enrolled lists and the engine
            // must never see our rebind afterwards.
            RE::BSLight** saved_scene_lights = nullptr;
            std::uint8_t saved_num_lights = 0;
            std::uint8_t saved_shadow_lights = 0;
            // v6.47: SELF-BUILT STUDIO LIGHTS (user direction: drop
            // the menu-light dependence entirely). Two persistent
            // NiPointLights (key + fill) created on the engine heap
            // via CLib's NiPointLight::Create factory, attached under
            // a UI3D menuObjects root so their world transforms are
            // live scene-graph members; each wrapped in a minimal
            // BSLight shell (engine-heap 0x140 bytes + the real
            // VTABLE_BSLight — the engine only reads fields and
            // IsShadowLight inside our window). The per-pass override
            // now points P's passes at THIS array unconditionally —
            // no freshness gate, no dungeon-light fallback. The rig
            // below then moves the lights' NODES (the v6.37 verified
            // path — point lights carry a parent now).
            ensure_studio_lights();
            const bool override_lights = m_studio_light_count > 0 && m_studio_light_array;
            if (override_lights)
            {
                saved_scene_lights = pass->sceneLights;
                saved_num_lights = pass->numLights;
                saved_shadow_lights = pass->numShadowLights;
                pass->numLights = m_studio_light_count;
                pass->numShadowLights = 0;
                pass->sceneLights = m_studio_light_array;
                // v6.55: PER-PASS shell patch. Run 88: the engine's
                // light-update tick keeps rewriting lodDimmer back to
                // 0 on our shells (they are foreign to its LOD fade
                // bookkeeping), so a fetch-time patch is always one
                // tick behind. The pass window is OURS — re-assert
                // the fields right here, after any engine tick and
                // before the draw, every pass.
                for (std::uint32_t i = 0; i < pass->numLights && i < 4; ++i)
                {
                    if (auto* shell = pass->sceneLights ? pass->sceneLights[i] : nullptr)
                    {
                        shell->lodDimmer = 1.0f;
                        shell->luminance = 1.0f;
                        shell->frustrumCull = 0;
                    }
                }
                if (!m_p_studio_lights_logged)
                {
                    m_p_studio_lights_logged = true;
                    logger::info("Proto v6.47 studio lights applied to P passes: {} SELF-BUILT "
                                 "point light(s) (menu-light dependence removed)",
                        m_studio_light_count);
                }
            }
            // v6.33: frontal light rig on the ACTIVE lights — menu
            // lights when the freshness gate arms, the pass's own
            // (dungeon) lights otherwise. The v6.32 pre-loop rig
            // shared the freshness gate and never fired in steady
            // state (the item preview does not re-render every menu
            // frame — run 43), leaving the figure on its dungeon
            // lights: dim and side-lit (run 65 screenshot). Per-pass
            // save/mutate/restore: the world's own draws happen
            // outside this window and never see the repositioning.
            //
            // v6.37: WHERE the shader reads light positions from.
            // CS source (LightLimitFix.cpp:275 + InverseSquare
            // BSLight_GetLuminance) reads NiLight::world.translate —
            // the NiAVObject node transform; CS LightEditor moves
            // lights via parent->local.translate + parent->Update.
            // BSLight::worldTranslate (v6.33-6.36's target) is the
            // CULLER's copy: run 70 proved writing it does nothing
            // to the shading (rig landed exactly, pixels unchanged).
            // So the rig now mutates the NiLight node itself:
            // set local.translate relative to its parent and cascade
            // the update, exactly the LightEditor-verified path.
            RE::NiPoint3 saved_light_pos[4] = {};
            RE::NiPoint3 saved_node_pos[4] = {};
            RE::NiNode* saved_node_parent[4] = {};
            bool node_mutated[4] = {};
            const std::uint32_t rig_count = pass->numLights < 4 ? pass->numLights : 4;
            {
                if (auto* ui3d = RE::UI3DSceneManager::GetSingleton(); ui3d && ui3d->camera)
                {
                    const auto& w2c = ui3d->camera->GetRuntimeData().worldToCam;
                    // v6.36: the w2c ROWS are NOT unit vectors (run 69
                    // arithmetic: |row0|=6.27, |row1|=11.15 — they carry
                    // the menu-camera's zoom scale). Normalize; the
                    // PLANE directions were always right (+X right, +Z
                    // up, -Y into the screen).
                    RE::NiPoint3 right{ w2c[0][0], w2c[0][1], w2c[0][2] };
                    RE::NiPoint3 up{ w2c[1][0], w2c[1][1], w2c[1][2] };
                    RE::NiPoint3 forward{ w2c[2][0], w2c[2][1], w2c[2][2] };
                    const float right_len = right.Length();
                    const float up_len = up.Length();
                    const float forward_len = forward.Length();
                    if (right_len > 1e-6f)
                        right *= 1.0f / right_len;
                    if (up_len > 1e-6f)
                        up *= 1.0f / up_len;
                    if (forward_len > 1e-6f)
                        forward *= 1.0f / forward_len;
                    const RE::NiPoint3 anchor = PInstance::instance().studio_anchor();
                    RE::NiUpdateData update_data{ 0.0f, RE::NiUpdateData::Flag::kDirty };
                    for (std::uint32_t i = 0; i < rig_count; ++i)
                    {
                        auto* light = pass->sceneLights ? pass->sceneLights[i] : nullptr;
                        if (!light)
                            continue;
                        saved_light_pos[i] = light->worldTranslate;
                        const float spread =
                            (static_cast<float>(i) - (rig_count - 1) * 0.5f) * 45.0f;
                        const RE::NiPoint3 light_target =
                            anchor - forward * 70.0f + up * 50.0f + right * spread;
                        light->worldTranslate = light_target;  // culler copy — kept for free
                        // v6.37/v6.55: the node path the shaders
                        // actually read. v6.55: write the LIGHT
                        // NODE's own local (not the shared rig
                        // parent's) — the v6.51 shared-parent writes
                        // overwrote each other (both shells ended at
                        // light[1]'s position). Each light node now
                        // owns its placement; the rig parent stays at
                        // identity and only carries the cascade.
                        if (auto* ni_light = light->light.get(); ni_light)
                        {
                            // v6.59: the rig parent sits AT the
                            // anchor (v6.58 pull-in), so the node's
                            // local must be the OFFSET from the
                            // anchor (spread/up/forward only) —
                            // writing the full accumulator-space
                            // target here double-counted the anchor
                            // and pushed the lights twice as far
                            // away (the run-92 dim look).
                            const RE::NiPoint3 local_offset{ right * spread + up * 50.0f -
                                                             forward * 70.0f };
                            ni_light->local.translate = local_offset;
                            if (ni_light->parent)
                            {
                                saved_node_pos[i] = ni_light->local.translate;
                                saved_node_parent[i] = ni_light->parent;
                                ni_light->parent->UpdateDownwardPass(update_data, 0);
                                node_mutated[i] = true;
                            }
                        }
                    }
                    if (!m_p_frontal_logged)
                    {
                        m_p_frontal_logged = true;
                        logger::info(
                            "Proto v6.37 frontal light rig: {} light(s) repositioned "
                            "(menu-lights={}); anchor=({:.1f},{:.1f},{:.1f}) "
                            "row_lengths=({:.2f},{:.2f},{:.2f})",
                            rig_count, override_lights ? 1 : 0, anchor.x, anchor.y, anchor.z,
                            right_len, up_len, forward_len);
                        for (std::uint32_t i = 0; i < rig_count; ++i)
                        {
                            auto* light = pass->sceneLights ? pass->sceneLights[i] : nullptr;
                            if (!light)
                                continue;
                            // v6.37 light census: type (NiRTTI chain tells
                            // NiDirectionalLight vs NiPointLight), radius,
                            // fade, parent chain — the facts needed to
                            // decide between "reposition" and "own light".
                            const char* rtti_name = "<null>";
                            const char* rtti_base = "<null>";
                            float radius_x = 0.0f, fade = 0.0f;
                            const char* parent_name = "<no node>";
                            RE::NiPoint3 ni_world{ 0.0f, 0.0f, 0.0f };
                            if (auto* ni_light = light->light.get()) {
                                if (const auto* rtti = ni_light->GetRTTI()) {
                                    rtti_name = rtti->name;
                                    rtti_base = rtti->baseRTTI ? rtti->baseRTTI->name : "-";
                                }
                                const auto& rd = ni_light->GetLightRuntimeData();
                                radius_x = rd.radius.x;
                                fade = rd.fade;
                                parent_name = ni_light->parent ? ni_light->parent->name.c_str()
                                                               : "<no parent>";
                                // v6.51: the ACTUAL shader-side position.
                                // If this stays at/near the origin while
                                // bs_new moved, the cascade is still
                                // being short-circuited.
                                ni_world = ni_light->world.translate;
                            }
                            logger::info(
                                "  light[{}]: point={} ambient={} dynamic={} lum={:.3f} "
                                "lodDimmer={:.3f} rtti={} base={} radius.x={:.1f} fade={:.3f} "
                                "parent='{}' bs_old=({:.1f},{:.1f},{:.1f}) "
                                "bs_new=({:.1f},{:.1f},{:.1f}) ni_world=({:.1f},{:.1f},{:.1f}) "
                                "node_moved={}",
                                i, light->pointLight, light->ambientLight, light->dynamic,
                                light->luminance, light->lodDimmer, rtti_name, rtti_base, radius_x, fade,
                                parent_name, saved_light_pos[i].x, saved_light_pos[i].y,
                                saved_light_pos[i].z, light->worldTranslate.x,
                                light->worldTranslate.y, light->worldTranslate.z,
                                ni_world.x, ni_world.y, ni_world.z,
                                node_mutated[i] ? 1 : 0);
                        }
                    }
                }
            }
            call_site_original(1, pass, pass->passEnum, (pass->passEnum & 0x40) != 0, 0x200);
            // v6.33: restore the positions FIRST (the same array the
            // next pass may reuse), then the array swap. v6.37: the
            // node path restores local.translate + re-cascades too.
            RE::NiUpdateData restore_data{ 0.0f, RE::NiUpdateData::Flag::kDirty };
            for (std::uint32_t i = 0; i < rig_count; ++i)
            {
                auto* light = pass->sceneLights ? pass->sceneLights[i] : nullptr;
                if (!light)
                    continue;
                light->worldTranslate = saved_light_pos[i];
                // v6.55: restore the LIGHT NODE's own local (matches
                // the new per-node placement above). For the
                // self-built lights the saved value IS the fresh one
                // (each node owns its placement), so the cascade
                // simply re-affirms it.
                if (node_mutated[i] && saved_node_parent[i])
                {
                    saved_node_parent[i]->UpdateDownwardPass(restore_data, 0);
                }
            }
            if (override_lights)
            {
                pass->sceneLights = saved_scene_lights;
                pass->numLights = saved_num_lights;
                pass->numShadowLights = saved_shadow_lights;
            }
            ++drawn;
        }

        // Run 47/52: per-frame draw summary — how many passes were
        // handed to the engine's SetupAndDrawPass this frame. Pixel
        // truth is RenderDoc / the close-dump TGA; the renderer-init
        // heartbeat (PInstance) shows whether the device buffers
        // appear once SetupAndDrawPass processes the geometries.
        logger::info("Proto v6.17 P draw (SetupAndDrawPass): submitted={} of {} passes (source={})",
            drawn, passes.size(),
            passes.empty() && !m_pass_recipes.empty() ? "recipes-failed" : "live/recipes");
        // v6.63: the close-dump gate counts a successful proactive
        // draw as studio content (P-only opens must dump too).
        if (drawn > 0)
            m_p_drew_this_open = true;
        runtime.context->OMSetRenderTargets(1, &prev_rtv, prev_dsv);
        runtime.context->OMSetDepthStencilState(prev_ds, prev_stencil_ref);
        runtime.context->RSSetViewports(prev_viewport_count, &prev_viewport);
        // v6.46: hand the ledger back — let the engine rebuild its
        // own render-target bindings the next time it applies state
        // (the same handshake CS's Deferred uses every frame).
        if (m_rt_dirty_guard_used)
        {
            m_rt_dirty_guard_used = false;
            RE::BSGraphics::RendererShadowState::GetSingleton()
                ->GetRuntimeData()
                .stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);
        }
        // v6.58: the draw window is over — park the light rig back
        // OUT of the world (100k units up) so the engine's light
        // ticks can never render the studio lights into the world
        // scene (the run-91 dungeon-wall report).
        park_studio_rig();

        if (prev_ds)
            prev_ds->Release();
        if (prev_rtv)
            prev_rtv->Release();
        if (prev_dsv)
            prev_dsv->Release();

        ++m_p_total_replays;  // feeds the close-dump gate too
        if (!m_p_draw_logged)
        {
            m_p_draw_logged = true;
            logger::info("Proto v6.8 P drawn proactively: {} passes via GetRenderPasses", passes.size());
        }
    }

    // SetupAndDrawPass with the pass's own recorded state. Run-31
    // logs show technique == passEnum on every observed pass, and
    // the alpha-test flag correlates with passEnum bit 6 (0x40):
    // 0x140C9/0x14049 -> alphaTest, 0x14045/0x14031 -> no. The
    // renderFlags observed on menu-stream passes are 0x200; the
    // kNormal menu path uses the same batch renderer entry, whose
    // CS interposer (when present) the pass hooks restored.
    void PassRedirector::call_site_original_from_pass(RE::BSRenderPass* pass)
    {
        call_site_original(1, pass, pass->passEnum, (pass->passEnum & 0x40) != 0, 0x200);
    }
}
