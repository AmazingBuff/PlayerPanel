//
// Created by AmazingBuff on 2026/10/08.
//

#pragma once

#include "character/animation_source.h"
#include "character/idle_driver.h"
#include "character/isolation_watch.h"
#include "character/pose_idle_pool.h"

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
        e_toggle_draw
    };

    static SceneGraphCopy& instance();
    void request(Command command);
    void reset_for_load();
    void note_world_change(char const* reason);
    void process_requests();
    CharacterClone* drawable() const;
    void animate_at_base_pose(CharacterClone& clone);
    [[nodiscard]] std::string animation_driver() const;

private:
    // This bounds bookkeeping only. Native graph teardown is unsafe; retired allocations leak.
    static constexpr size_t Parked_Graph_Cap = 12;

    SceneGraphCopy();
    ~SceneGraphCopy();
    void capture();
    void retire(std::unique_ptr<CharacterClone> snapshot);
    void log_parked() const;
    // The pose-idle sampler: while the world runs, files the engine's own idle for the pose the
    // player stands in; at panel open, picks the clip for the CURRENT stance.
    void sample_pose_idle();
    [[nodiscard]] RE::hkbClipGenerator const* pick_pose_idle(RE::Actor& a_player, char const*& a_pick) const;

private:
    std::atomic<Command> m_command;
    std::atomic_uint32_t m_generation;
    uint32_t m_seen_generation;
    uint32_t m_capture_id;
    bool m_draw_enabled;
    bool m_cap_logged;
    IdleDriver m_idle;
    ClipPlayer m_clip;
    std::string m_driver_note;
    std::unique_ptr<CharacterClone> m_snapshot;
    // Retired snapshots deliberately remain alive until process exit.
    std::vector<CharacterClone*> m_parked;
    IsolationWatch m_isolation;
    PoseIdlePool m_pose_pool;
    std::uint32_t m_pose_sample_frames = 0;
};

PLUGIN_NAMESPACE_END
