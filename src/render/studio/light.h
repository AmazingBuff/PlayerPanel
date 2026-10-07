//
// Created by AmazingBuff on 2026/10/6.
//

#pragma once

PLUGIN_NAMESPACE_BEGIN

static constexpr size_t Studio_Light_Count = 3;
static constexpr float Studio_Light_Pos_Z = 100000.0f;

// Photostudio light rig. The NiLight rig node is DETACHED — hosted under no
// engine scene — so the menu-root rebuilds (runs 100-102) can never orphan
// or dangle it. The shells live in the ShadowSceneNode ledger (AddLight),
// the shader reads NiLight::world.translate (v6.37), and place() cascades
// the detached rig manually inside the draw window.
class StudioLight
{
public:
    static StudioLight& instance();

    RE::NiPointer<RE::NiNode> light_node();
    RE::BSLight** lights();

    // Pull the rig to the studio anchor and cascade it. Call inside the
    // draw window, before the passes are generated: the detached rig gets
    // no engine tick, so this is the only thing keeping NiLight world
    // transforms correct.
    void place(const RE::NiPoint3& anchor);

    // Park the rig far out of gameplay space. The shells live in the world
    // light ledger, so the engine renders them into the world whenever the
    // rig sits near gameplay (v6.58) — and with a detached rig the anchor
    // IS gameplay space. Call on every draw-window exit.
    void park();

    // Register the NiLights with the ShadowSceneNode ledger (AddLight) and
    // refresh the renderer-facing shells. Idempotent per scene node.
    bool init(RE::ShadowSceneNode* scene_node);

    // Re-fetch the engine's BSLight shells and report whether all of them are
    // now bound to this rig's NiLights. Call every frame: a rig registered
    // while the game is paused stays unbound until the next light update.
    bool refresh();
private:
    StudioLight();
    ~StudioLight();
private:
    RE::NiPointer<RE::NiLight> m_ni_lights[Studio_Light_Count];
    RE::NiPointer<RE::NiNode> m_light_node;

    // Renderer-facing slots: a non-null entry is always ledger-held and bound
    // to its NiLight as of the last refresh.
    RE::BSLight* m_lights[Studio_Light_Count];

    // Shell identity per slot. Address comparisons only; a recorded shell is
    // dereferenced solely after being re-fetched from the ledger that frame.
    RE::BSLight* m_tracked_shells[Studio_Light_Count];
    std::uint32_t m_miss_frames[Studio_Light_Count];
    std::uint32_t m_fallback_counts[Studio_Light_Count];

    // AddLight is kept only as a low-frequency fallback while a slot has no
    // ledger shell; the shells it returns are never handed to the renderer.
    RE::ShadowSceneNode::LIGHT_CREATE_PARAMS m_create_params;
    std::uint32_t m_refresh_ticks;

    std::uint32_t m_stable_frames;
    std::uint32_t m_window_start_tick;
    bool m_window_open;

    RE::ShadowSceneNode* m_last_ui3d_node;

    RE::ShadowSceneNode* m_scene_node;
};


PLUGIN_NAMESPACE_END
