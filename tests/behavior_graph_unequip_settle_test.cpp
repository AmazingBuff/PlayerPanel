//
// Created by AmazingBuff on 2026/10/31.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/unequip_settle_policy.h"

#include <cassert>
#include <cstdint>

using test_plugin::animation::behavior_graph::UnequipSettleGate;
using test_plugin::animation::behavior_graph::UnequipSettleVerdict;

namespace
{
    // v52: the candidate idled at default=10 (its authored initial) until WeapEquip moved it to 1
    // at step 49 with the node stack settling at 97.
    void settled_tail_opens_the_injection()
    {
        UnequipSettleGate gate;
        for (std::uint32_t step = 0; step < 49; ++step)
            gate.observe(step, 10, 97);
        assert(!gate.reached_target());
        assert(gate.verdict() == UnequipSettleVerdict::e_no_settle);
        for (std::uint32_t step = 49; step <= 120; ++step)
            gate.observe(step, 1, 97);
        assert(gate.reached_target());
        assert(gate.target_step() == 49);
        assert(gate.settled_step() == 51);
        assert(gate.baseline_nodes() == 97);
        assert(gate.last_default() == 1);
        assert(gate.verdict() == UnequipSettleVerdict::e_settled);
    }

    void disturbance_restarts_the_stability_run()
    {
        UnequipSettleGate gate;
        // Nodes still moving after the state reached the target: no run of three equal samples.
        for (std::uint32_t step = 0; step < 10; ++step)
            gate.observe(step, 1, 90 + static_cast<std::int32_t>(step % 3));
        assert(gate.reached_target());
        assert(gate.verdict() == UnequipSettleVerdict::e_no_settle);
        for (std::uint32_t step = 10; step <= 20; ++step)
            gate.observe(step, 1, 97);
        assert(gate.settled_step() == 12);
        assert(gate.verdict() == UnequipSettleVerdict::e_settled);
        // A node change re-opens the run and the baseline follows the newest triple.
        gate.observe(21, 1, 96);
        gate.observe(22, 1, 96);
        gate.observe(23, 1, 96);
        assert(gate.settled_step() == 23);
        assert(gate.baseline_nodes() == 96);
        assert(gate.verdict() == UnequipSettleVerdict::e_settled);
    }

    void no_settle_when_the_target_never_holds()
    {
        UnequipSettleGate gate;
        // Oscillating between the unarmed and equipped values never stabilizes on the target.
        for (std::uint32_t step = 0; step <= 40; ++step)
            gate.observe(step, step % 2 == 0 ? 0 : 1, 97);
        assert(gate.reached_target());
        assert(gate.verdict() == UnequipSettleVerdict::e_no_settle);

        // A window that stabilizes early and then drifts off the target is no-settle too: the
        // injection must not fire from a state that left the settled reading.
        UnequipSettleGate drifted;
        for (std::uint32_t step = 0; step <= 30; ++step)
            drifted.observe(step, 1, 97);
        assert(drifted.verdict() == UnequipSettleVerdict::e_settled);
        for (std::uint32_t step = 31; step <= 34; ++step)
            drifted.observe(step, 12, 60);
        assert(drifted.last_default() == 12);
        assert(drifted.verdict() == UnequipSettleVerdict::e_no_settle);
    }
}

int main()
{
    settled_tail_opens_the_injection();
    disturbance_restarts_the_stability_run();
    no_settle_when_the_target_never_holds();
    return 0;
}
