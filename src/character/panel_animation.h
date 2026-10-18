//
// Created by AmazingBuff on 2026/10/16.
//

#pragma once

#include <memory>
#include <cstdint>
#include <cstddef>
#include <string>

namespace RE
{
    class Actor;
    class NiAVObject;
}

PLUGIN_NAMESPACE_BEGIN

class PanelAnimation
{
public:
    PanelAnimation();
    ~PanelAnimation();
    void reset();
    // Clears the persisted sequence plans: a load boundary rebuilds the project name table, so
    // the plans' member indexes no longer refer to the animations they were collected for.
    void invalidate_sequence_plans();
    void observe_equipment(RE::Actor& source, std::uint64_t revision);
    void prepare(RE::Actor& source, RE::NiAVObject& root, bool equipment_changed);
    void tick(RE::Actor& source, RE::NiAVObject& root);
    [[nodiscard]] bool ready() const;
    [[nodiscard]] std::string name() const;
    // Advances every ready clip against the graph; returns the number of nodes the strongest
    // driver actually wrote this call (0 = nothing drove the figure yet).
    std::size_t advance(RE::NiAVObject& root);
    // True while an entry action or a weapon transition is playing: the framing calibration waits
    // for the idle the move hands over to, not for the move's own body (round 18).
    [[nodiscard]] bool transitioning() const;

private:
    class State;
    std::unique_ptr<State> m_state;
};

PLUGIN_NAMESPACE_END
