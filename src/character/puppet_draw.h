//
// Created by AmazingBuff on 2026/10/13.
//

#pragma once

#include "render/dx11/common_states.h"
#include "render/dx11/d3d11_util.h"

#include "RE/N/NiBound.h"
#include "RE/N/NiTransform.h"

#include <cstdint>

namespace RE
{
    class NiAVObject;
    class UI3DSceneManager;
}

PLUGIN_NAMESPACE_BEGIN

// R01 (PRD 0.9 §6.6, AFP9): draw the puppet Actor's OWN third-person 3D into the studio target.
//
// Until now the renderer drew the detached copy (CharacterClone::draw); the pose the application
// stage writes (FinalPoseApply) never reached the screen. This module closes exactly that gap and
// nothing else:
//
//   - the root is the puppet's own Get3D(false) tree - no detach, no data3D sever, no second figure;
//     an identity line (actor form, root, graph, generation) proves what was drawn;
//   - the studio placement is written as the root's LOCAL (plus the derived worlds for the current
//     frame) and is restored on the way out. The local is what matters: this actor's subtree is
//     re-derived from `local` every frame, so a world-only write is wiped (AFP9e measured the body
//     staying behind while the axe - a rigid attachment - followed), and the derived worlds are what
//     the engine's skinning reads;
//   - the placement is applied in `begin_frame`, BEFORE the puppet's graph update, because the skin
//     state is computed during that update. Applying it afterwards reaches rigid attachments only;
//   - A4 (pre-pass) and A5 (post-draw) samples of a fixed bone set prove the draw did not overwrite
//     the pose the application stage wrote;
//   - world isolation: the Actor reference is never moved, the camera is untouched, the pause is
//     never lifted, and every exit path restores the tree it borrowed.
//
// Physical simulation, FaceGen and body-morph sync stay out of scope: A3 reports `skipped`.
class PuppetDraw
{
public:
    static PuppetDraw& instance();

    // F6: the R01 toggle. ON is the default, because the panel is meant to show the figure the moment
    // the inventory opens and this is the route that draws it without a capture step first; F6 switches
    // to the detached copy. When the puppet is unavailable either way, it falls back and says so.
    void toggle();
    [[nodiscard]] bool enabled() const { return m_enabled.load(std::memory_order_acquire); }

    // Called once per rendered frame BEFORE the puppet's graph update: puts the figure on the studio
    // anchor when this frame will draw it, and remembers what it borrowed.
    void begin_frame();

    // The panel-open draw branch, inside its own D3D state capture. Returns true when the studio
    // target carries the puppet's own figure (the caller may composite it); false means the caller
    // draws the copy as before.
    bool draw(RE::UI3DSceneManager const& a_ui3d, CommonStates const& a_states, RenderTarget& a_target);

    // Called once per rendered frame after the draw branch: restores exactly what begin_frame()
    // changed, whatever the draw did.
    void end_frame();

    // True between begin_frame() and end_frame() of the frame being built: this frame's studio
    // placement owns the puppet's transform, so nothing else may re-assert the park over it.
    [[nodiscard]] bool framing_active() const { return m_framed_root != nullptr; }

private:
    PuppetDraw() = default;

    // Forensic image capture while the panel is being diagnosed: the first drawn frame and every
    // Dump_Period_Frames after it, capped so a long session cannot fill the dump directory.
    static constexpr std::uint32_t Dump_Period_Frames = 300;
    static constexpr std::uint32_t Max_Dumps = 20;

    std::atomic_bool m_enabled{ true };
    // One-shot per identity: the R01 provenance line prints when the drawn root or the graph
    // changes, not once per session - a reload or re-assembly must re-prove itself.
    void const* m_logged_root = nullptr;
    void const* m_logged_graph = nullptr;
    bool m_fallback_warned = false;

    // The borrowed state: non-null only between begin_frame() and end_frame().
    RE::NiAVObject* m_framed_root = nullptr;
    RE::NiTransform m_saved_local;
    RE::NiTransform m_saved_world;
    RE::NiTransform m_saved_previous;
    RE::NiBound m_saved_bound;
    // What begin_frame() wrote, kept for the reconciliation line in end_frame().
    RE::NiTransform m_applied_local;
    RE::NiTransform m_applied_world;
    std::int32_t m_dump_countdown = 1;
    std::uint32_t m_dumps = 0;
};

PLUGIN_NAMESPACE_END
