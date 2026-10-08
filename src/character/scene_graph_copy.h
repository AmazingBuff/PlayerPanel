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
    SceneGraphCopy();
    ~SceneGraphCopy();
    void capture();

private:
    std::atomic<Command> m_command;
    std::atomic_uint32_t m_generation;
    uint32_t m_seen_generation;
    uint32_t m_capture_id;
    bool m_draw_enabled;
    std::unique_ptr<CharacterClone> m_snapshot;
};

PLUGIN_NAMESPACE_END
