//
// Created by AmazingBuff on 2026/10/5.
//

#pragma once

#include "render/dx11/d3d11_util.h"
#include "render/dx11/common_states.h"

PLUGIN_NAMESPACE_BEGIN

class CharacterClone
{
public:
    explicit CharacterClone(RE::Actor* actor);
    ~CharacterClone();

    bool attach_graph(const RE::NiPointer<RE::NiNode>& host);

    void pose();

    void draw(const RE::UI3DSceneManager* ui3d, const CommonStates& states, const RenderTarget& render_target);
private:
    enum class CloneState : uint8_t
    {
        e_none,
        e_generated,
        e_graph
    };

private:
    std::uint32_t m_wait_frames;
    bool m_light_warned;

    RE::NiPointer<RE::TESObjectREFR> m_clone;
    std::atomic<CloneState> m_clone_state;

    RE::NiPointer<RE::NiNode> m_character_node;
    RE::NiPointer<RE::NiAVObject> m_graph_object;

    RE::NiPoint3 m_anchor;
};

PLUGIN_NAMESPACE_END
