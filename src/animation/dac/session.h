//
// Created by AmazingBuff on 2026/10/20.
//
#pragma once
#include "animation/dac/idle_goal.h"
#include <cstdint>
#include <memory>
#include <string>
namespace RE { class Actor; }
PLUGIN_NAMESPACE_BEGIN
namespace dac
{
    void install();
    void reset_world(); // invalidate before PreLoadGame/NewGame releases old-world actors
    class Session
    {
    public:
        explicit Session(RE::Actor& source);
        ~Session();
        Session(Session const&) = delete;
        Session& operator=(Session const&) = delete;
        void request(std::uint64_t generation);
        void dispatch(std::string event, std::uint64_t generation);
        void prepare_idle(std::uint64_t generation, bool retain_ready);
        [[nodiscard]] IdleReadiness idle_readiness(std::uint64_t generation) const;
        void observe();
        [[nodiscard]] std::uint64_t revision() const;
        // Diagnostic: the source's active Smooth Moveset markers as file-local ids ("802", "806!"
        // marks an inactive instance), read on the calling thread like the other UI-task probes.
        [[nodiscard]] std::string smooth_markers() const;
        class State;
    private:
        std::shared_ptr<State> m_state;
    };
}
PLUGIN_NAMESPACE_END
