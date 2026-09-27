#pragma once
// SPIKE CODE - throwaway calibration module for the Stage-0 gate.
// Creates a live clone of the player that the engine renders normally, so
// real render passes for its geometries flow through the engine pipeline and
// our RenderPassImmediately call-site hook can observe and replay them.

#include <RE/Skyrim.h>

#include <atomic>
#include <vector>

PLUGIN_NAMESPACE_BEGIN

namespace spike
{
    class CloneActor
    {
    public:
        static CloneActor& instance();

        // Game thread only. Places a clone of the player a short distance in
        // front of the camera, wearing the player's current equipment. The
        // clone is deliberately NOT app-culled: the engine must render it so
        // its passes reach the hook.
        void request_spawn();

        // Game thread only. Removes the clone again.
        void request_despawn();

        // Game thread only, called once per frame from the task queue.
        void on_frame();

        [[nodiscard]] bool has_actor() const { return m_actor != nullptr; }

        // Borrowed pointers, valid only while has_actor(); collected on the
        // game thread immediately after creation, read on the render thread
        // only while the frame latch (see pass_hook) holds the world still.
        [[nodiscard]] const std::vector<RE::BSGeometry*>& geometries() const { return m_geometries; }

        [[nodiscard]] RE::Actor* actor() const { return m_actor; }

    private:
        CloneActor() = default;

        void spawn();
        void position_and_dress();
        void despawn();

        enum class Request : std::uint8_t
        {
            kNone,
            kSpawn,
            kDespawn,
        };

        std::atomic<Request> m_request{ Request::kNone };
        RE::ObjectRefHandle m_handle{};
        RE::Actor* m_actor{ nullptr };
        std::vector<RE::BSGeometry*> m_geometries;
        bool m_dressed{ false };
        std::uint32_t m_frames_since_place{ 0 };
    };
}

PLUGIN_NAMESPACE_END
