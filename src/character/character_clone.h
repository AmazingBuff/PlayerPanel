//
// Created by AmazingBuff on 2026/10/5.
//

#pragma once

#include "render/dx11/d3d11_util.h"
#include "render/dx11/common_states.h"

#include <chrono>

PLUGIN_NAMESPACE_BEGIN

// Photostudio figure: a duplicated actor whose biped graph is detached from
// the world scene graph and solely owned by this object. The graph is never
// hosted under any engine scene: the menu-scene roots are engine-managed and
// rebuilt across menu/load transitions while clones hosted under them dangle
// (runs 100-102; crash-2026-10-07-17-20-42 / -18-01-37). The studio draws
// the detached graph manually every frame instead.
class CharacterClone
{
public:
    explicit CharacterClone(RE::Actor* actor);
    ~CharacterClone();

    // Detach the biped graph from the world cell and kill the shell actor.
    // Idempotent and self-paced: returns false while the clone is still
    // assembling in the world; true once the graph is solely owned and the
    // shell is gone. Runs inside the render bracket, serialized with the
    // world renderer job on the same thread.
    bool detach_graph();

    void pose();

    void draw(const RE::UI3DSceneManager* ui3d, const CommonStates& states, const RenderTarget& render_target);
private:
    enum class CloneState : uint8_t
    {
        e_none,
        e_generated,   // shell actor placed; biped graph still assembling in the world
        e_ready,       // graph detached, solely owned here; shell actor dead
        e_discarded    // assembly stalled (boneless low-LOD graph, run 104); unusable
    };

private:
    RE::NiPointer<RE::TESObjectREFR> m_clone;
    std::atomic<CloneState> m_clone_state;

    // Sole owner of the detached graph once e_ready; null before that.
    RE::NiPointer<RE::NiAVObject> m_graph_object;

    RE::NiPoint3 m_anchor;
    uint32_t m_wait_frames;
    bool m_dressed;
    bool m_bind_map_logged;
    uint32_t m_bind_map_stall;

    // Post-dressing settle gate (phase 3): the geometry census must hold
    // steady across the armor-attach race before the graph may detach.
    uint32_t m_settle_last_geoms = 0;
    uint32_t m_settle_last_skinned = 0;
    uint32_t m_settle_stable = 0;
    bool m_geom_names_logged = false;
};

PLUGIN_NAMESPACE_END
