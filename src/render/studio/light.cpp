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

bool StudioLight::init(const RE::NiPointer<RE::NiNode>& menu, RE::ShadowSceneNode* scene_node)
{
    if (m_menu_node == menu && m_scene_node == scene_node)
        return true;

    menu->AttachChild(m_light_node.get());

    for (const RE::NiPointer<RE::NiLight>& m_ni_light : m_ni_lights)
        scene_node->AddLight(m_ni_light.get());

    for (std::size_t i = 0; i < Studio_Light_Count; ++i)
    {
        RE::ShadowSceneNode::RUNTIME_DATA& rt = scene_node->GetRuntimeData();
        for (RE::NiPointer<RE::BSLight>& e : rt.activeLights)
        {
            if (e && e->light == m_ni_lights[i])
                m_lights[i] = e.get();
        }

        for (RE::NiPointer<RE::BSLight>& e : rt.lightQueueAdd)
        {
            if (e && e->light == m_ni_lights[i])
                m_lights[i] = e.get();
        }
    }

    for (RE::BSLight* shell : m_lights)
    {
        if (shell->lodDimmer != 1.0f || shell->luminance != 1.0f)
        {
            shell->lodDimmer = 1.0f;
            shell->luminance = 1.0f;
            shell->frustrumCull = 0;
        }
    }

    m_menu_node = menu;
    m_scene_node = scene_node;

    logger::info("Studio lights has been added to menu");

    return true;
}

void StudioLight::clear_lights()
{
    for (RE::BSLight*& light : m_lights)
        light = nullptr;
    m_menu_node = nullptr;
    m_scene_node = nullptr;

    logger::info("Studio lights has been cleared");
}

StudioLight::StudioLight() : m_lights(nullptr), m_menu_node(nullptr), m_scene_node(nullptr)
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

    logger::info("Studio light node has been created");
}

StudioLight::~StudioLight()
{

}

PLUGIN_NAMESPACE_END