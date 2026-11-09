//
// Created by AmazingBuff on 2026/10/28.
//
#pragma once

#include <cstddef>
#include <cstdint>

namespace RE
{
    class Actor;
    class BShkbAnimationGraph;
}

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // G3-A: read the player's own graph during normal play and log only what changes.
    // Called once per post-UI frame; it never writes, never dispatches events and never
    // touches the retained candidate. Off unless [Experiment] SourceTimeline is on.
    void sample_source_timeline(RE::Actor& actor);

    // Publishes a fresh bounded window request (F7); the sampler owns all recording state.
    void arm_source_timeline();

    // C-G5-E3 (M1): the experiment arms the timeline on its own pinned source graph, before its
    // dispatches, so the source trajectory and the candidate steps share one window. The sampler
    // re-validates the pointer against the manager every frame (comparison only, never a
    // dereference of an unowned graph) and aborts on replacement, exactly like the v66 pinning.
    // Returns false, and logs why, when the mode is not `concurrent`, the graph is null, or this
    // process already used its one window.
    [[nodiscard]] bool arm_source_timeline_concurrent(RE::BShkbAnimationGraph const& a_source_graph,
        std::uint32_t a_actor_id, std::uint64_t a_request);

    // C-G5-E3 (M1): request the concurrent window's close (display boundary / expiry / world
    // reset); the sampler emits the single close row with the line count.
    void close_source_timeline_window(char const* a_reason);

    // C-G5-E3 (M2): drain the settle timestamps published by the window for the pairing ledger.
    // Called from the same render thread that runs the sampler, so the ring stays single-threaded
    // in practice. Returns the number of timestamps written to `a_out`.
    std::size_t take_source_settles(std::uint64_t* a_out, std::size_t a_capacity);

    // C-G5-E3 (M2): settles observed in the current window that the ledger has not yet drained.
    [[nodiscard]] std::size_t source_settle_backlog();

    // Queues cancellation across a world boundary, consumed before the next source read.
    void invalidate_source_timeline();
}
PLUGIN_NAMESPACE_END
