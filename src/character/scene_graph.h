//
// Created by AmazingBuff on 2026/10/19.
//
#pragma once
#include "animation/controller.h"
#include "dac/session.h"
#include "animation/pose.h"
#include "render/dx11/d3d11_util.h"
#include "render/dx11/common_states.h"
PLUGIN_NAMESPACE_BEGIN
struct PreviewFrame
{
    bool paused;
    bool open;
    bool opening;
    bool closing;
    bool usable;
};
class SceneGraph
{
public:
    enum class Command : std::uint8_t { e_none, e_capture, e_toggle_draw };
    explicit SceneGraph(RE::Actor& source);
    ~SceneGraph();
    void request(Command command);
    [[nodiscard]] bool update(PreviewFrame const& frame, std::uint64_t revision);
    bool draw(RE::UI3DSceneManager const* ui3d, CommonStates const& states, RenderTarget const& render_target);
    [[nodiscard]] RE::FormID source_id() const { return m_source_id; }
private:
    struct MonitorState
    {
        RE::ActorHandle source;
        animation::SourceObservation observed;
        std::uint64_t equipment_revision = 0;
        std::uint64_t request_generation = 0;
        std::uint64_t source_graph_generation = 0;
        RE::NiAVObject* source_root = nullptr;
        void const* source_graph = nullptr;
        std::string dirty_reason;
        EquipmentRefreshGate refresh;
        bool capture_pending = true;
        bool session_active = false;
        bool draw_enabled = true;
    };
    bool update_source_graph(RE::Actor& source);
    void observe_source(RE::Actor& source);
    void schedule_animation(RE::Actor& source, std::uint64_t revision, std::string reason);
    void close_session();
    void update_monitor(RE::Actor& source, std::uint64_t revision, bool paused, bool open, bool opening, bool closing, bool usable);
    bool capture_due(RE::Actor& source, std::uint64_t revision, bool manual);
    void capture_result(bool accepted, bool opening);
    void toggle_draw();
    [[nodiscard]] RE::NiPointer<RE::NiAVObject> capture_graph(RE::Actor& source);
    void retire_graph(RE::NiPointer<RE::NiAVObject> graph);
    void replace_graph(RE::NiPointer<RE::NiAVObject> graph);
    void pose();
    void calibrate_from_posed_bound();
    [[nodiscard]] bool drawable() const { return m_usable && m_monitor.draw_enabled && m_graph_object && m_animation.ready(); }
    struct SnapshotNode
    {
        RE::NiAVObject* object;
        RE::NiTransform world;
        RE::NiBound bound;
    };
    MonitorState m_monitor;
    dac::Session m_dac;
    animation::AnimationController m_animation;
    RE::FormID m_source_id;
    std::uint32_t m_capture_id = 0;
    std::atomic<Command> m_command{ Command::e_none };
    bool m_usable = false;
    RE::NiPointer<RE::NiAVObject> m_graph_object;
    RE::NiPoint3 m_anchor;
    bool m_om_check_logged = false;
    bool m_empty_draw_reported = false;
    RE::NiTransform m_snapshot_root_world;
    RE::NiPoint3 m_snapshot_center;
    std::vector<SnapshotNode> m_snapshot_nodes;
    bool m_recentre_pending = false;
    RE::NiTransform m_calibrated_root;
};
PLUGIN_NAMESPACE_END
