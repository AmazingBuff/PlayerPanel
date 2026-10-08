//
// Created by AmazingBuff on 2026/10/08.
//

#pragma once

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
        e_release
    };

    static SceneGraphCopy& instance();
    void request(Command command);
    void reset_for_load();
    // Only these two methods access the snapshot; both run on the render callback.
    void process_requests();
    CharacterClone* drawable() const;

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
    std::unique_ptr<CharacterClone> m_snapshot;
    // Raw on purpose: these snapshots own graphs that must outlive every boundary.
    std::vector<CharacterClone*> m_parked;
};

PLUGIN_NAMESPACE_END
