//
// Created by AmazingBuff on 2026/10/6.
//

#include "light.h"

PLUGIN_NAMESPACE_BEGIN

namespace
{
    constexpr char Studio_Light_Node_Name[] = "StudioLight";
}

StudioLight& StudioLight::instance()
{
    static StudioLight s_instance;
    return s_instance;
}

RE::NiPointer<RE::NiNode> StudioLight::light_node()
{
    return m_light_node;
}

RE::BSLight** StudioLight::lights()
{
    return m_lights;
}

bool StudioLight::refresh()
{
    // A paused game (main menu, savegame load, inventory) never runs light
    // updates, so anything registered during a pause is dead weight piling up
    // in lightQueueAdd: freeze the miss counters and wait for an unpause.
    RE::UI* ui = RE::UI::GetSingleton();
    const bool paused = !ui || ui->GameIsPaused();

    const RE::ShadowSceneNode::RUNTIME_DATA& rt = m_scene_node->GetRuntimeData();

    bool all_fetched = true;
    for (std::size_t i = 0; i < Studio_Light_Count; ++i)
    {
        RE::NiLight* const ni_light = m_ni_lights[i].get();
        if (!ni_light)
            continue;

        // Scan BOTH queues: the rig is detached, so the engine never runs
        // its scene tick over our NiLights and never promotes the shells
        // from lightQueueAdd to activeLights. But the AddLight-overload
        // shell is fully built and bound the moment it is filed (v6.50,
        // run 84) — fetching straight from the queue is the m0
        // double-queue fetch, validated through runs 84-93.
        RE::BSLight* ledger_shell = nullptr;
        for (const RE::NiPointer<RE::BSLight>& entry : rt.activeLights)
        {
            if (entry && entry->light.get() == ni_light)
            {
                ledger_shell = entry.get();
                break;
            }
        }
        if (!ledger_shell)
        {
            for (const RE::NiPointer<RE::BSLight>& entry : rt.lightQueueAdd)
            {
                if (entry && entry->light.get() == ni_light)
                {
                    ledger_shell = entry.get();
                    break;
                }
            }
        }

        if (ledger_shell)
        {
            if (m_tracked_shells[i] != ledger_shell)
            {
                if (m_tracked_shells[i])
                    logger::info(
                        "CP-LIGHT slot {}: ledger shell replaced {} -> {}",
                        i,
                        static_cast<const void*>(m_tracked_shells[i]),
                        static_cast<const void*>(ledger_shell));
                else
                    logger::info(
                        "CP-LIGHT slot {}: ledger shell fetched {} (after {} miss frames)",
                        i,
                        static_cast<const void*>(ledger_shell),
                        m_miss_frames[i]);
            }

            // The engine tick keeps resetting these on foreign shells (v6.54):
            // re-apply the freshness patches every frame.
            ledger_shell->lodDimmer = 1.0f;
            ledger_shell->luminance = 1.0f;
            ledger_shell->frustrumCull = 0;

            m_tracked_shells[i] = ledger_shell;
            m_lights[i] = ledger_shell;
            m_miss_frames[i] = 0;
        }
        else
        {
            if (m_tracked_shells[i])
                logger::info("CP-LIGHT slot {}: ledger shell dropped", i);

            m_tracked_shells[i] = nullptr;
            m_lights[i] = nullptr;
            if (!paused)
                ++m_miss_frames[i];
            all_fetched = false;

            if (!paused && m_miss_frames[i])
            {
                if (RE::BSLight* shell = m_scene_node->AddLight(ni_light, m_create_params))
                {
                    shell->lodDimmer = 1.0f;
                    shell->luminance = 1.0f;
                    shell->frustrumCull = 0;
                    logger::info(
                        "CP-LIGHT slot {}: fallback re-registered after {} miss frames (shell {} filed to lightQueueAdd)",
                        i,
                        m_miss_frames[i],
                        static_cast<const void*>(shell));
                }
            }
        }
    }

    return all_fetched;
}

void StudioLight::park()
{
    if (!m_light_node)
        return;

    m_light_node->local.translate = { 0.0f, 0.0f, Studio_Light_Pos_Z };

    RE::NiUpdateData update_data{
        .time = 0.0f,
        .flags = RE::NiUpdateData::Flag::kDirty
    };
    m_light_node->UpdateDownwardPass(update_data, 0);
}

bool StudioLight::init(RE::ShadowSceneNode* scene_node)
{
    if (scene_node && m_scene_node == scene_node)
        return true;

    if (!scene_node || !m_light_node)
    {
        logger::warn("Studio lights not registered (scene={} rig={})", static_cast<void*>(scene_node), static_cast<void*>(m_light_node.get()));
        return false;
    }

    // The rig stays DETACHED: no menu-root hosting. The shader reads
    // NiLight::world.translate (v6.37) and place() cascades the rig inside
    // the draw window, so an engine scene parent would buy nothing while
    // exposing the rig to the menu-root rebuilds (runs 100-102).

    // Use the AddLight overload that RETURNS the BSLight it builds. The
    // convenience overload (void AddLight(NiLight*)) files the shell in
    // lightQueueAdd and discards it, and the engine only binds that queued
    // shell to its NiLight on the next light update -- which does not run while
    // the inventory pause is up, i.e. exactly when the panel is open. That is
    // why the rig measured bound=0 for whole sessions and the figure stayed lit
    // by the cell. This overload hands the shell back immediately.
    m_create_params = {};
    m_create_params.dynamic = true;
    m_create_params.shadowLight = false;
    m_create_params.portalStrict = false;
    m_create_params.affectLand = false;
    m_create_params.affectWater = false;
    m_create_params.neverFades = true;

    for (std::size_t i = 0; i < Studio_Light_Count; ++i)
    {
        RE::NiLight* const ni_light = m_ni_lights[i].get();
        if (!ni_light)
            continue;

        // Register the NiLight so the engine learns about it promptly. The
        // returned shell is NOT renderer-facing: it is filed into
        // lightQueueAdd and may never reach the ledger (probe 2026-10-07).
        // refresh() fetches whatever the ledger actually serves -- an
        // engine-gathered shell or this one once (if ever) promoted.
        if (RE::BSLight* shell = scene_node->AddLight(ni_light, m_create_params))
        {
            shell->lodDimmer = 1.0f;
            shell->luminance = 1.0f;
            shell->frustrumCull = 0;
            logger::info(
                "CP-LIGHT slot {}: registered, shell {} queued (ledger stays the authority)",
                i,
                static_cast<const void*>(shell));
        }

        m_lights[i] = nullptr;
        m_tracked_shells[i] = nullptr;
        m_miss_frames[i] = 0;
    }

    m_scene_node = scene_node;

    // The pointer values are logged so ledger-probe runs can confirm the
    // ShadowSceneNode identity stays constant across menu/save transitions.
    logger::info(
        "Studio lights registered on the ledger (scene={} -- ledger fetch will populate the slots)",
        static_cast<void*>(scene_node));

    return true;
}

void StudioLight::place(const RE::NiPoint3& anchor)
{
    if (!m_light_node)
        return;

    // Detached rig: no engine tick touches it, so the draw window is the
    // only place the cascade runs. Key/fill offsets are fixed locals under
    // the rig (menu camera world is identity, v6.28) — pending in-game
    // calibration, same as the m0 per-pass spread/up/forward values.
    m_light_node->local.translate = anchor;

    RE::NiUpdateData update_data{
        .time = 0.0f,
        .flags = RE::NiUpdateData::Flag::kDirty
    };
    m_light_node->UpdateDownwardPass(update_data, 0);
}

StudioLight::StudioLight() :
    m_lights{},
    m_tracked_shells{},
    m_miss_frames{},
    m_fallback_counts{},
    m_create_params{},
    m_refresh_ticks(0),
    m_stable_frames(0),
    m_window_start_tick(0),
    m_window_open(false),
    m_last_ui3d_node(nullptr),
    m_scene_node(nullptr)
{
    RE::NiNode* light_node = RE::NiNode::Create();
    light_node->name = Studio_Light_Node_Name;

    for (std::size_t i = 0; i < Studio_Light_Count; ++i)
    {
        RE::NiLight* light = nullptr;
        if (i == 0)
        {
            // Slot 0: ambient base. A point light flagged
            // ambient — the engine reads its ambient term.
            RE::NiPointLight* amb = RE::NiPointLight::Create();

            amb->name = "StudioAmbient";
            RE::NiLight::LIGHT_RUNTIME_DATA& ard = amb->GetLightRuntimeData();
            ard.diffuse = RE::NiColor(0.25f, 0.25f, 0.28f);
            ard.radius = { 4096.0f, 4096.0f, 4096.0f };
            ard.fade = 1.0f;
            amb->SetLightAttenuation(4096.0f);
            light = amb;
        }
        else
        {
            RE::NiPointLight* pt = RE::NiPointLight::Create();

            pt->name = i == 1 ? "CP_StudioKey" : "CP_StudioFill";
            // Warm-white key, cool fill; radius covers any
            // rig placement, fade 2.0 for headroom (v6.48).
            RE::NiLight::LIGHT_RUNTIME_DATA& rd = pt->GetLightRuntimeData();
            rd.diffuse = i == 1 ? RE::NiColor(1.0f, 0.96f, 0.90f) : RE::NiColor(0.70f, 0.80f, 1.0f);
            rd.radius = { 4096.0f, 4096.0f, 4096.0f };
            rd.fade = 2.0f;
            pt->SetLightAttenuation(4096.0f);
            light = pt;
        }
        // Fixed rig-local offsets: the menu camera world is identity (v6.28)
        // and looks down -Y (run 44), so +Y = toward the camera, +Z = up.
        // Key/fill flank the figure above camera side; pending calibration.
        switch (i)
        {
            case 1: light->local.translate = { 40.0f, 55.0f, 60.0f }; break;   // key
            case 2: light->local.translate = { -40.0f, 55.0f, 60.0f }; break;  // fill
            default: light->local.translate = { 0.0f, 0.0f, 30.0f }; break;    // ambient base
        }
        light_node->AttachChild(light);

        m_ni_lights[i].reset(light);
    }
    m_light_node.reset(light_node);

    m_light_node->local.translate = { 0.0f, 0.0f, Studio_Light_Pos_Z };

    RE::NiUpdateData update_data{
        .time = 0.0f,
        .flags = RE::NiUpdateData::Flag::kDirty
    };
    m_light_node->UpdateDownwardPass(update_data, 0);

    logger::info("Studio light node has been created");
}

StudioLight::~StudioLight()
{
    // The rig is detached (no engine scene host) — releasing the NiPointers
    // frees the node tree; the ledger shells die with the scene node.
    m_scene_node = nullptr;
    m_light_node = nullptr;
}

PLUGIN_NAMESPACE_END
