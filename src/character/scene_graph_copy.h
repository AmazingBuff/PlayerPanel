//
// Created by AmazingBuff on 2026/10/08.
//

#pragma once

#include "character/animation_source.h"
#include "character/idle_driver.h"

#include <string>

PLUGIN_NAMESPACE_BEGIN

class CharacterClone;

class SceneGraphCopy
{
public:
    enum class Command : uint8_t
    {
        e_none,
        e_capture,
        e_toggle_draw,
        e_rotate,
        e_release,
        e_anim_probe,
        e_idle_toggle
    };

    static SceneGraphCopy& instance();
    void request(Command command);
    void reset_for_load();
    // Only these two methods access the snapshot; both run on the render callback.
    void process_requests();
    CharacterClone* drawable() const;

    // S2 probe stages, called by CharacterClone::draw() in the order PRD 0.6 §6.3 fixes: the
    // studio's base placement, the controlled bone transform, the engine's pass preparation and
    // the actual draw. Each stage records its own measurement; the report prints one frame's
    // four stages together. They are no-ops unless F2 armed the probe on the drawn snapshot.
    void probe_at_base_pose(CharacterClone& clone);
    void probe_at_pass_submit(CharacterClone& clone, RE::BSRenderPass* pass);
    void probe_at_draw_done(CharacterClone& clone, uint32_t drawn);
    // PRD FR-03's driver: breathes and shifts the copy's weight while the panel is open. Its own
    // stage, so the studio's base placement is what it animates around and the probe can stay a
    // diagnostic that owns the figure while it is armed.
    void animate_at_base_pose(CharacterClone& clone);

private:
    // Freeing a native clone's graph crashed at every point that was tried
    // (crash-2026-10-08-23-01-11 in capture(), 23-07-35 at a clean frame boundary,
    // 23-12-02 at the main-menu boundary), always inside the engine's fade-node
    // teardown. A replaced or released snapshot is therefore parked and deliberately
    // never destroyed while the process lives; the cap bounds how many are parked.
    static constexpr size_t Parked_Graph_Cap = 12;

    // One read-only sample of the skin matrix slot that belongs to the probe bone. The layout is
    // the 3x4 row-major stride of 48 bytes this file already assumes, so row 0 is at [0..2] and
    // its w components (3, 7, 11) hold the slot's translation. The sample never writes.
    //
    // Both buffers are read: the engine keeps a second one for the same slot, and a shader that
    // reads it on some frames makes a single-buffer reading look like a coin flip.
    struct ProbeSlotSample
    {
        bool sampled = false;
        uint32_t slot = 0;
        uint32_t num_matrices = 0;
        uint32_t frame_id = 0;
        const void* buffer = nullptr;
        float values[12] = {};
        bool previous_sampled = false;
        const void* previous_buffer = nullptr;
        float previous_values[12] = {};
    };

    SceneGraphCopy();
    ~SceneGraphCopy();
    void capture();
    // S2-P0: decide whether independent animation is a data problem (drive the copy's own
    // bones) or an architectural one, by writing one bone of the copy and watching the
    // rendered figure. Toggled with F2; changes nothing outside the draw window.
    void toggle_animation_probe();
    void toggle_idle();
    void select_probe_target(RE::NiAVObject& root);
    // The cached target belongs to the graph it was picked from: a re-capture or a session boundary
    // retires that graph, so the probe would otherwise keep swinging (and reporting on) a parked
    // copy while the panel draws a different one.
    void reset_probe_target();
    bool sample_probe_slot(ProbeSlotSample& sample) const;
    // Names the engine-side quantity a sampled slot resembles, from a candidate set built only from
    // node and bind data. The labels this replaced compared a single candidate pair and could not
    // tell "the slot holds something else" from "the slot holds neither of my two guesses".
    std::string classify_probe_slot(float const* values) const;
    // Dumps the target skin's slots raw, next to each slot's bone and that bone's own world
    // transform, once per F2 arm. Ten candidates all missed by ~2.0, which says the reading layout
    // (or the slot's indexing) is wrong rather than the candidate set being incomplete.
    void dump_probe_skin();
    void report_probe_trace();
    void retire(std::unique_ptr<CharacterClone> snapshot);
    void log_parked() const;

private:
    std::atomic<Command> m_command;
    std::atomic_uint32_t m_generation;
    uint32_t m_seen_generation;
    uint32_t m_capture_id;
    bool m_draw_enabled;
    uint32_t m_frame;
    bool m_cap_logged;
    bool m_anim_probe_enabled;
    IdleDriver m_idle;
    // The animation the copy plays when the source character has one the engine can hand over; the
    // procedural idle is the fallback when it does not.
    ClipPlayer m_clip;
    RE::NiAVObject* m_probe_bone_cache;
    // The geometry whose skin owns the probe bone: the pass that submits it is the draw under
    // measurement (T2), and its slot is the one sampled at T1-T3.
    RE::BSGeometry* m_probe_target_geometry;
    // The descendant furthest from the swing axis. A witness on the axis cannot move, so its
    // displacement would not judge the write.
    RE::NiAVObject* m_probe_witness;
    float m_probe_witness_radius;
    uint32_t m_probe_subtree_nodes;
    // How much of the figure the swung subtree touches: the geometry that T2 waits for is the
    // largest skin owning the bone, and the count is what names the difference between a swing
    // that moves one limb and one that moves the body.
    uint32_t m_probe_affected_geometries;
    uint32_t m_probe_skinned_geometries;
    uint32_t m_probe_geometry_bones;
    RE::NiTransform m_probe_bone_world_pose;
    std::string m_probe_geometry;
    std::string m_probe_root;
    std::string m_probe_parent_chain;
    std::string m_probe_skin_report;
    // The slot's bind-pose transform (static skin data) and the orientation the previous frame's
    // swing produced: both are candidate quantities the sampled slot may actually hold, and both
    // come from the engine rather than from a value this code wrote.
    RE::NiMatrix3 m_probe_bind_rotate;
    bool m_probe_bind_valid;
    RE::NiMatrix3 m_previous_bone_rotate;
    bool m_previous_bone_valid;
    bool m_probe_dump_done;
    // T0-T3 trace of the newest probe frame, printed together so the reader can see whether the
    // write survived base placement and whether the engine's own buffer followed it.
    uint32_t m_trace_frame;
    uint32_t m_trace_capture;
    RE::NiTransform m_trace_bone_before;
    RE::NiTransform m_trace_bone_after;
    RE::NiPoint3 m_trace_witness_before;
    RE::NiPoint3 m_trace_witness_after;
    ProbeSlotSample m_trace_slot_pre;
    ProbeSlotSample m_trace_slot_submit;
    ProbeSlotSample m_trace_slot_final;
    std::string m_trace_pass_geometry;
    uint32_t m_trace_drawn;
    bool m_trace_submit_sampled;
    bool m_trace_complete;
    float m_probe_move_min;
    float m_probe_move_max;
    uint32_t m_probe_samples;
    bool m_probe_invalid_logged;
    uint32_t m_probe_frame_report;
    std::unique_ptr<CharacterClone> m_snapshot;
    // Raw on purpose: these snapshots own graphs that must outlive every boundary.
    std::vector<CharacterClone*> m_parked;
};

PLUGIN_NAMESPACE_END
