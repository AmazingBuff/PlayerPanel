//
// Created by AmazingBuff on 2026/10/6.
//

#pragma once

PLUGIN_NAMESPACE_BEGIN

static constexpr size_t Studio_Light_Count = 3;
static constexpr float Studio_Light_Pos_Z = 100000.0f;

class StudioLight
{
public:
    static StudioLight& instance();

    RE::NiPointer<RE::NiNode> light_node();
    RE::BSLight** lights();

    bool init(const RE::NiPointer<RE::NiNode>& menu, RE::ShadowSceneNode* scene_node);

    // Re-fetch the engine's BSLight shells and report whether all of them are
    // now bound to this rig's NiLights. Call every frame: a rig registered
    // while the game is paused stays unbound until the next light update.
    bool refresh();

    void clear_lights();
private:
    StudioLight();
    ~StudioLight();
private:
    RE::NiPointer<RE::NiLight> m_ni_lights[Studio_Light_Count];
    RE::NiPointer<RE::NiNode> m_light_node;

    RE::BSLight* m_lights[Studio_Light_Count];

    // Kept so refresh() can re-register a light whose shell went unusable.
    RE::ShadowSceneNode::LIGHT_CREATE_PARAMS m_create_params;
    std::uint32_t m_refresh_ticks;

    RE::NiPointer<RE::NiNode> m_menu_node;
    RE::ShadowSceneNode* m_scene_node;
};


PLUGIN_NAMESPACE_END