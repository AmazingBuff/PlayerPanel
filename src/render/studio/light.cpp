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
    RE::ShadowSceneNode* scene_node = m_scene_node;
    if (!scene_node)
        return false;

    // REPAIR ONLY -- never discard a slot that is already correct.
    //
    // AddLight above returns a shell that is bound on the spot, so a slot
    // holding a shell bound to our NiLight is good and must be kept. An earlier
    // version re-looked-up every slot each frame and cleared any lookup miss:
    // GetPointLight only scans activeLights, and a freshly registered light
    // reaches activeLights on the next light update, so that version wiped the
    // shells AddLight had just handed us ("3/3 shells bound immediately"
    // followed 20 s later by "bound=0"). A miss here means "not visible in the
    // ledger yet", not "dead".
    // Re-registering is expensive and repeated failures would file duplicates,
    // so attempt a repair at most once per this many frames.
    constexpr std::uint32_t Refresh_Retry_Interval = 30;
    ++m_refresh_ticks;

    bool all_ours = true;
    for (std::size_t i = 0; i < Studio_Light_Count; ++i)
    {
        RE::NiLight* const ni_light = m_ni_lights[i].get();
        if (!ni_light)
            continue;

        RE::BSLight* shell = m_lights[i];
        if (shell && shell->light.get() == ni_light)
            continue;  // already ours: keep it

        // The slot is empty, or holds a shell the engine recycled, or holds one
        // whose light pointer is null (allocated but unbound). All three are
        // unusable and cannot be repaired in place, so RE-REGISTER: the
        // returning AddLight overload binds a fresh shell on the spot. This is
        // the only recovery for "shells=3/3 but bound=0".
        if (m_refresh_ticks % Refresh_Retry_Interval == 0)
        {
            RE::BSLight* found = scene_node->GetPointLight(ni_light);
            if (!found || found->light.get() != ni_light)
            {
                found = scene_node->AddLight(ni_light, m_create_params);
                found->lodDimmer = 1.0f;
                found->luminance = 1.0f;
                found->frustrumCull = 0;
            }
            shell = found;
        }

        m_lights[i] = (shell && shell->light.get() == ni_light) ? shell : nullptr;
        if (!m_lights[i])
            all_ours = false;
    }

    return all_ours;
}

bool StudioLight::init(const RE::NiPointer<RE::NiNode>& menu, RE::ShadowSceneNode* scene_node)
{
    if (m_menu_node == menu && m_scene_node == scene_node)
        return true;

    if (!menu || !scene_node || !m_light_node)
    {
        logger::warn("Studio lights not attached (menu={} scene={} rig={})", static_cast<void*>(menu.get()), static_cast<void*>(scene_node), static_cast<void*>(m_light_node.get()));
        return false;
    }

    menu->AttachChild(m_light_node.get());

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

        m_lights[i] = scene_node->AddLight(ni_light, m_create_params);
        if (m_lights[i] && m_lights[i]->light.get() != ni_light)
        {
            // The engine returned a shell bound to something else: never hand
            // that to the shader. Drop it and let refresh() retry.
            logger::warn("Studio light {}: AddLight returned a foreign shell; ignoring it", i);
            m_lights[i] = nullptr;
        }
    }

    uint32_t bound = 0;
    for (RE::BSLight* shell : m_lights)
    {
        if (shell && shell->light)
        {
            bound++;
            shell->lodDimmer = 1.0f;
            shell->luminance = 1.0f;
            shell->frustrumCull = 0;
        }
    }

    m_menu_node = menu;
    m_scene_node = scene_node;

    logger::info("Studio lights has been added to menu ({}/{} shells bound immediately)", bound, static_cast<std::uint32_t>(Studio_Light_Count));

    return true;
}

void StudioLight::clear_lights()
{
    for (RE::BSLight*& light : m_lights)
        light = nullptr;

    for (RE::NiPointer<RE::NiLight>& light : m_ni_lights)
    {
        if (light && m_scene_node)
            m_scene_node->RemoveLight(light.get());
    }

    m_menu_node = nullptr;
    m_scene_node = nullptr;

    logger::info("Studio lights has been cleared");
}

StudioLight::StudioLight() : m_lights{}, m_create_params{}, m_refresh_ticks(0), m_menu_node(nullptr), m_scene_node(nullptr)
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
        light->local.translate = { 0.0f, 0.0f, 0.0f };
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

}

PLUGIN_NAMESPACE_END