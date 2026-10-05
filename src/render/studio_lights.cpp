//
// The self-built studio light rig: engine-registered NiPointLights with
// their BSLight wrappers, rig parking, and stall self-healing.
// (2026-10-05 src migration: split out of tools/m0_proto, behavior byte-identical)
//

#include "pinstance/pinstance.h"
#include "render/pass_redirector.h"
#include "render/render_internal.h"

#include <RE/B/BSLight.h>
#include <RE/B/BSShaderManager.h>
#include <RE/N/NiPointLight.h>
#include <RE/S/ShadowSceneNode.h>

PLUGIN_NAMESPACE_BEGIN
// v6.50: shell fetch scans BOTH queues. Run 83: GetPointLight
// scans activeLights only, but the engine's AddLight files new
// lights into lightQueueAdd first (the per-frame light update
// promotes them) — same-frame fetch always missed, the sticky
// failure then killed the lights for the whole session (run 83
// log: one warn, rig fell back to the dungeon directional).
RE::BSLight* PassRedirector::fetch_light_wrapper(RE::ShadowSceneNode* a_node, RE::NiLight* a_light)
{
    if (!a_node)
        return nullptr;
    auto& rt = a_node->GetRuntimeData();
    for (auto& e : rt.activeLights)
        if (e && e->light.get() == a_light)
            return e.get();
    for (auto& e : rt.lightQueueAdd)
        if (e && e->light.get() == a_light)
            return e.get();
    return nullptr;
}

// v6.68: discard the whole self-built light rig so Phase 1
// re-creates it under the CURRENT host/node. Triggered by a
// pointer change (v6.67) or by a stalled wrapper fetch — the
// transition can cut the rig out of the host's children (or
// out of the ledger) while every pointer we hold stays
// identical (run 102: 0/3 matched with all caches matching),
// so the fetch stall is the authoritative signal.
void PassRedirector::reset_light_rig(const char* a_reason)
{
    logger::info(
        "Proto v6.68 studio light rig reset: {} — re-creating under the current scene",
        a_reason);
    for (auto*& shell : m_studio_lights)
        shell = nullptr;
    for (auto& ni : m_studio_light_ni)
        ni = nullptr;
    m_studio_rig_node = nullptr;
    m_studio_light_count = 0;
    m_studio_light_array = nullptr;
    m_studio_lights_failed = false;
    m_fetch_warned = false;
    m_fetch_stall_windows = 0;
}

void PassRedirector::ensure_studio_lights()
{
    auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
    if (!renderer)
        return;
    auto* ui3d = RE::UI3DSceneManager::GetSingleton();
    RE::NiNode* host = ui3d ? ui3d->menuObjects[0].get() : nullptr;
    RE::ShadowSceneNode* world_node =
        RE::BSShaderManager::State::GetSingleton().shadowSceneNode[0];
    if (!host || !world_node)
        return;

    // v6.66/v6.67/v6.68: NEITHER the world's ShadowSceneNode NOR
    // the UI3D host survives a main-menu transition intact, and
    // they fail in DIFFERENT ways (runs 100-102):
    // - run 100: the node is destroyed — the engine FREES the
    //   BSLight wrappers our NiLights were registered with; the
    //   first P draw patched freed shells (lum read as 1.08e21
    //   garbage) and crashed writing through a freed NiLight.
    // - run 101: the node pointer SURVIVES, but the UI3D host
    //   (menuObjects[0]) is rebuilt — our rig node is orphaned
    //   under the dead root, the engine's per-frame light
    //   collection no longer reaches it, so the ledger holds no
    //   wrappers for our lights: the fetch fails SILENTLY and
    //   the figure renders dark.
    // - run 102: EVERY pointer cache matches (host, node,
    //   NiLights alive) and the fetch still reports 0/3 — the
    //   transition stripped the rig from the host's children
    //   (or equivalent) without changing any pointer we hold.
    // Pointer comparisons are therefore NOT the ground truth;
    // they stay as cheap fast-path checks, and the stalled
    // fetch itself (Phase 2 below) is the authoritative trigger:
    // a rig that has not reached the ledger for a while is
    // discarded and re-created under the CURRENT scene, which
    // re-enters the host's children and the ledger by
    // construction.
    if ((m_studio_light_ni[0] || m_studio_rig_node) &&
        (host != m_studio_lights_host || world_node != m_studio_lights_node))
    {
        reset_light_rig(host != m_studio_lights_host
                ? "the UI3D host changed across a menu/load transition"
                : "the ShadowSceneNode changed across a menu/load transition");
    }

    // Phase 1: create once (no sticky failure — a same-frame
    // queue miss must not kill the lights for the session).
    if (!m_studio_light_ni[0] && !m_studio_lights_failed)
    {
        // v6.51: a PRIVATE rig node. menuObjects[0] is an
        // engine root with selective-update flags — v6.50's
        // parent->Update() cascade was short-circuited by them
        // (the run-41 lesson, again): the lights' world
        // transforms never left the origin, so nothing lit.
        // The rig node is ours, fresh, no flags — and the rig
        // cascade below now uses UpdateDownwardPass (forced).
        auto* rig = RE::NiNode::Create();
        if (!rig)
        {
            m_studio_lights_failed = true;
            logger::warn("Proto v6.51 studio rig node creation failed");
            return;
        }
        rig->name = "CP_StudioLightRig";
        host->AttachChild(rig);
        m_studio_rig_node.reset(rig);
        for (std::size_t i = 0; i < Studio_Light_Count; ++i)
        {
            // v6.54: BOTH point lights. Run 87 verdict: the LEFT
            // screenshot (the user's chosen reference look) was
            // lit while shell[0] was still the queued POINT-light
            // wrapper (the v6.52 directional shell had not been
            // promoted yet) — the point lights + rig placement
            // ARE the proven-good configuration. The RIGHT (dark)
            // case appeared exactly when re-fetch picked the
            // promoted directional shell: its worldDirection
            // (Rz(-90°) guess) faces away, and lum=10081 made it
            // dominant — one wrong-facing ultra-bright light
            // beats two correct point lights to black.
            // Directional experiment retired; both point again.
            // v6.56: ENGINE SLOT CONVENTION — sceneLights[0] is
            // the AMBIENT slot; point lights start at index 1
            // (LLF: strict lights iterate sceneLights[i+1],
            // LightLimitFix.cpp:248; the engine SetupGeometry
            // treats slot 0 the same way). The v6.47-55 layouts
            // put the KEY at slot 0, where it was silently
            // treated as ambient — only the slot-1 fill light
            // ever contributed (the run-71+ 'lit but odd' look).
            // New arrangement: slot 0 = ambient fill (soft base
            // light, no position math), slots 1/2 = key/fill
            // point lights.
            RE::NiLight* ni = nullptr;
            if (i == 0)
            {
                // Slot 0: ambient base. A point light flagged
                // ambient — the engine reads its ambient term.
                auto* amb = RE::NiPointLight::Create();
                if (!amb)
                {
                    m_studio_lights_failed = true;
                    logger::warn("Proto v6.56 ambient light create failed");
                    return;
                }
                amb->name = "CP_StudioAmbient";
                auto& ard = amb->GetLightRuntimeData();
                ard.diffuse = RE::NiColor(0.25f, 0.25f, 0.28f);
                ard.radius = { 4096.0f, 4096.0f, 4096.0f };
                ard.fade = 1.0f;
                amb->SetLightAttenuation(4096.0f);
                ni = amb;
            }
            else
            {
                auto* pt = RE::NiPointLight::Create();
                if (!pt)
                {
                    m_studio_lights_failed = true;
                    logger::warn("Proto v6.56 point light create failed");
                    return;
                }
                pt->name = i == 1 ? "CP_StudioKey" : "CP_StudioFill";
                // Warm-white key, cool fill; radius covers any
                // rig placement, fade 2.0 for headroom (v6.48).
                auto& rd = pt->GetLightRuntimeData();
                rd.diffuse = i == 1 ? RE::NiColor(1.0f, 0.96f, 0.90f) : RE::NiColor(0.70f, 0.80f, 1.0f);
                rd.radius = { 4096.0f, 4096.0f, 4096.0f };
                rd.fade = 2.0f;
                pt->SetLightAttenuation(4096.0f);
                ni = pt;
            }
            ni->local.translate = { 0.0f, 0.0f, 0.0f };
            rig->AttachChild(ni);
            // Engine registration: the wrapper is BUILT here
            // (filed in lightQueueAdd, promoted to activeLights
            // by the next light update).
            world_node->AddLight(ni);
            m_studio_light_ni[i].reset(ni);
        }
        m_studio_lights_node = world_node;
        m_studio_lights_host = host;
        logger::info("Proto v6.50 studio lights created and handed to ShadowSceneNode::AddLight "
                     "(activeLights.size={} lightQueueAdd.size={})",
            world_node->GetRuntimeData().activeLights.size(),
            world_node->GetRuntimeData().lightQueueAdd.size());
    }

    // Phase 2: fetch the ENGINE-built wrappers — retried every
    // window until both are found (the queue promotion happens
    // on the engine's light-update tick, at most a frame later).
    if (m_studio_light_count == 0)
    {
        bool all = true;
        for (std::size_t i = 0; i < Studio_Light_Count; ++i)
            if (!m_studio_lights[i])
                m_studio_lights[i] =
                    fetch_light_wrapper(world_node, m_studio_light_ni[i].get());
        for (std::size_t i = 0; i < Studio_Light_Count; ++i)
            if (!m_studio_lights[i])
                all = false;
        // v6.67: an incomplete fetch used to be SILENT — run
        // 101's dark figure had zero log lines to explain it.
        // Warn once per stall; the reset above or the engine's
        // next ledger rebuild clears it.
        if (!all)
        {
            if (!m_fetch_warned)
            {
                m_fetch_warned = true;
                logger::warn("Proto v6.67 studio light wrappers not in the ledger yet "
                             "({}/{} matched) — the figure renders without studio lights "
                             "until they appear",
                    std::count_if(std::begin(m_studio_lights), std::end(m_studio_lights),
                        [](const RE::BSLight* s) { return s != nullptr; }),
                    Studio_Light_Count);
            }
            // v6.68: the pointers can all look unchanged while
            // the transition cut the rig out of the host's
            // children anyway (run 102: 0/3 with every cache
            // matching) — a sustained stall IS the ground
            // truth. Re-create and let the fresh rig re-enter
            // the host's children and the ledger by
            // construction.
            if (++m_fetch_stall_windows >= Fetch_Stall_Reset_Windows)
                reset_light_rig(
                    "wrapper fetch stalled — the rig no longer reaches the ledger");
            return;
        }
        m_fetch_warned = false;
        m_fetch_stall_windows = 0;
        // v6.53/v6.54: FIX THE SHELL FIELDS the engine left
        // stale — and RE-PATCH ON EVERY FETCH. The engine
        // REBUILDS its wrappers whenever the ledger is
        // repopulated (panel open/close cycles: activeLights
        // 97→99 in run 87), and every rebuilt shell starts
        // with lodDimmer=0 again — the run-87 log shows the
        // patch working on fetch 1 and the panel dark again
        // on fetch 2. Both light consumers multiply by
        // lodDimmer (LLF: light.fade *= lodDimmer; native
        // LOD fade), so 0 = a light that exists but
        // contributes exactly nothing.
        bool any_patched = false;
        for (std::size_t i = 0; i < Studio_Light_Count; ++i)
        {
            auto* shell = m_studio_lights[i];
            if (shell->lodDimmer != 1.0f || shell->luminance != 1.0f)
            {
                any_patched = true;
                logger::info(
                    "Proto v6.54 shell[{}] raw: lodDimmer={:.3f} lum={:.3f} portalStrict={} "
                    "dynamic={} pointLight={} frustrumCull=0x{:X} worldTranslate=({:.1f},{:.1f},{:.1f})",
                    i, shell->lodDimmer, shell->luminance, shell->portalStrict, shell->dynamic,
                    shell->pointLight, shell->frustrumCull, shell->worldTranslate.x,
                    shell->worldTranslate.y, shell->worldTranslate.z);
                shell->lodDimmer = 1.0f;
                shell->luminance = 1.0f;
                shell->frustrumCull = 0;
            }
        }
        m_studio_light_count = Studio_Light_Count;
        m_studio_light_array = m_studio_lights;
        logger::info("Proto v6.54 studio light wrappers fetched{} "
                     "(activeLights.size={} lightQueueAdd.size={})",
            any_patched ? " and PATCHED (lodDimmer=1, lum=1, no cull)" : " (fields already good)",
            world_node->GetRuntimeData().activeLights.size(),
            world_node->GetRuntimeData().lightQueueAdd.size());
    }
}

void PassRedirector::park_studio_rig()
{
    if (m_studio_rig_node)
    {
        m_studio_rig_node->local.translate = { 0.0f, 0.0f, Rig_Park_Z };
        RE::NiUpdateData data{ 0.0f, RE::NiUpdateData::Flag::kDirty };
        m_studio_rig_node->UpdateDownwardPass(data, 0);
    }
}
PLUGIN_NAMESPACE_END
