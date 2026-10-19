//
// Created by AmazingBuff on 2026/10/08.
//
#pragma once
#include "character/isolation_watch.h"
PLUGIN_NAMESPACE_BEGIN
class SceneGraphCopy
{
public:
    enum class Command : uint8_t { e_none, e_capture, e_toggle_draw };
    static SceneGraphCopy& instance();
    void request(Command command);
    [[nodiscard]] Command take_command();
    void reset_for_load();
    void note_world_change(char const* reason);
    void process_boundary(bool open);
    [[nodiscard]] RE::NiPointer<RE::NiAVObject> capture(RE::Actor& source);
    void retire(RE::NiPointer<RE::NiAVObject> graph);
private:
    static constexpr size_t Parked_Graph_Cap = 12;
    SceneGraphCopy();
    ~SceneGraphCopy();
    void log_parked() const;
    std::atomic<Command> m_command;
    uint32_t m_capture_id;
    bool m_cap_logged;
    std::vector<RE::NiPointer<RE::NiAVObject>*> m_parked;
    IsolationWatch m_isolation;
};
PLUGIN_NAMESPACE_END
