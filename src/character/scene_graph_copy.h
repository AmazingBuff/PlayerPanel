//
// Created by AmazingBuff on 2026/10/08.
//

#pragma once

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
        e_anim_probe
    };

    static SceneGraphCopy& instance();
    void request(Command command);
    void reset_for_load();
    // Only these two methods access the snapshot; both run on the render callback.
    void process_requests();
    CharacterClone* drawable() const;
    // Poses one bone of the current copy when the F2 probe is on; render-callback only.
    void apply_animation_probe(CharacterClone& clone);

private:
    // Freeing a native clone's graph crashed at every point that was tried
    // (crash-2026-10-08-23-01-11 in capture(), 23-07-35 at a clean frame boundary,
    // 23-12-02 at the main-menu boundary), always inside the engine's fade-node
    // teardown. A replaced or released snapshot is therefore parked and deliberately
    // never destroyed while the process lives; the cap bounds how many are parked.
    static constexpr size_t Parked_Graph_Cap = 12;

    SceneGraphCopy();
    ~SceneGraphCopy();
    void capture();
    // S2-P0: decide whether independent animation is a data problem (drive the copy's own
    // bones) or an architectural one, by writing one bone of the copy and watching the
    // rendered figure. Toggled with F2; changes nothing outside the draw window.
    void toggle_animation_probe();
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
    RE::NiAVObject* m_probe_bone_cache;
    RE::NiAVObject* m_probe_child_cache;
    RE::NiPoint3 m_probe_bone_origin;
    RE::NiPoint3 m_probe_child_origin;
    RE::NiTransform m_probe_bone_world_pose;
    std::string m_probe_skin_report;
    float m_probe_move_min;
    float m_probe_move_max;
    uint32_t m_probe_samples;
    bool m_probe_invalid_logged;
    uint32_t m_probe_rebuilt_skins;
    uint32_t m_probe_rebuilt_slots;
    float m_probe_slot_value_after_draw;
    uint32_t m_probe_corrupt_slot;
    std::string m_probe_slot_geometry;
    uint32_t m_probe_frame_report;
    std::unique_ptr<CharacterClone> m_snapshot;
    // Raw on purpose: these snapshots own graphs that must outlive every boundary.
    std::vector<CharacterClone*> m_parked;
};

PLUGIN_NAMESPACE_END
