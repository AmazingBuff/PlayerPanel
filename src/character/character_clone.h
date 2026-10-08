//
// Created by AmazingBuff on 2026/10/5.
//

#pragma once

#include "render/dx11/d3d11_util.h"
#include "render/dx11/common_states.h"

#include <chrono>

PLUGIN_NAMESPACE_BEGIN

// Owns a studio graph from Actor assembly or an audited, controller-free snapshot.
// Neither graph is hosted under engine-managed menu roots.
class CharacterClone
{
public:
    explicit CharacterClone(RE::Actor* actor);
    explicit CharacterClone(RE::NiPointer<RE::NiAVObject> graph);
    ~CharacterClone();

    // Actor mode advances assembly and detaches once; snapshots are already ready.
    bool detach_graph();

    void pose();

    bool draw(const RE::UI3DSceneManager* ui3d, const CommonStates& states, const RenderTarget& render_target);
    void rotate_snapshot();
private:
    enum class CloneState : uint8_t
    {
        e_none,
        e_generated,   // shell actor placed; biped graph still assembling in the world
        e_ready,       // studio graph ready; Actor mode retains a disabled shell
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

    struct SnapshotNode
    {
        RE::NiAVObject* object;
        RE::NiTransform world;
        RE::NiBound bound;
    };

    bool m_is_snapshot;
    float m_snapshot_angle;
    uint32_t m_draw_frames;
    RE::NiTransform m_snapshot_root_world;
    RE::NiPoint3 m_snapshot_center;
    std::vector<SnapshotNode> m_snapshot_nodes;
};

PLUGIN_NAMESPACE_END
