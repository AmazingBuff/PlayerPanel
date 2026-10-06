//
// Created by AmazingBuff on 2026/10/6.
//

#pragma once

PLUGIN_NAMESPACE_BEGIN

static constexpr size_t Studio_Light_Count = 3;

class StudioLight
{
public:
    static StudioLight& instance();

    RE::NiPointer<RE::NiNode> light_node();
    RE::BSLight** lights();

    bool init(const RE::NiPointer<RE::NiNode>& menu, RE::ShadowSceneNode* scene_node);

    void clear_lights();
private:
    StudioLight();
    ~StudioLight();
private:
    RE::NiPointer<RE::NiLight> m_ni_lights[Studio_Light_Count];
    RE::NiPointer<RE::NiNode> m_light_node;

    RE::BSLight* m_lights[Studio_Light_Count];

    RE::NiPointer<RE::NiNode> m_menu_node;
    RE::ShadowSceneNode* m_scene_node;
};


PLUGIN_NAMESPACE_END