//
// Created by AmazingBuff on 2026/10/08.
//

#pragma once

#include "character/panel_animation.h"
#include "character/panel_animation_policy.h"
#include "character/isolation_watch.h"

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
    void capture(bool equipment_changed = false);
    void retire(std::unique_ptr<CharacterClone> snapshot);
    void log_parked() const;

private:
    std::atomic<Command> m_command;
    std::atomic_uint32_t m_generation;
    uint32_t m_seen_generation;
    uint32_t m_capture_id;
    bool m_draw_enabled;
    bool m_cap_logged;
    bool m_open_pending = false;
    EquipmentRefreshGate m_equipment_refresh;
    bool m_was_open = false;
    PanelAnimation m_panel_animation;
    std::unique_ptr<CharacterClone> m_snapshot;
    // Retired snapshots deliberately remain alive until process exit.
    std::vector<CharacterClone*> m_parked;
    IsolationWatch m_isolation;
};

PLUGIN_NAMESPACE_END
