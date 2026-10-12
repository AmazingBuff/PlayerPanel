//
// Created by AmazingBuff on 2026/10/13.
//

#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

namespace RE
{
    class Actor;
}

PLUGIN_NAMESPACE_BEGIN

// AFP2 (PRD 0.9 section 6.6): the application stage - the behaviour graph's own final output pose
// written into the Actor's own skeleton.
//
// This is the half the engine does not do while the panel owns the clock (HKX42 measured the node
// locals frozen under a manual tick), and it is deliberately strict:
//
//   - it writes only a mapping it has verified against the engine's *own* applied node locals through
//     `score_pose_order`/`decide_pose_order`, and blocks with the reason when that fails. An order
//     nothing verified may not drive a skeleton (R00), and a blocked stage says so instead of writing
//     something plausible;
//   - the verification therefore happens while the world is RUNNING (the engine applies every frame,
//     so its locals are the reference), and the write happens while the panel pauses the world (the
//     engine applies nothing). That is R03 followed by R04, encoded rather than hoped for;
//   - it resolves every node from the engine's live bone table on the frame it writes: no node
//     pointer is kept across frames (HKX43's dangling read came from exactly that), and no per-frame
//     walk of the scene graph is done (each step of such a walk costs a page query - HKX35);
//   - it writes local transforms and recomputes the subtree's worlds once, which is the only write
//     path this codebase has measured reaching a draw (HKX2/S2P4).
class FinalPoseApply
{
public:
    // F3: off -> tick -> tick+apply -> tick+apply+force -> step-once -> off. One key carries R02's
    // push/freeze control, R04's A/B pair and the declared single step, because this setup has three
    // keys and not nine.
    //
    // `tick+apply+force` is the same push and the same write with one field of the update block set -
    // the flag the engine's own update data uses to generate a pose every frame instead of skipping
    // (see PuppetDriver::set_force_generate). Two adjacent F3 states therefore A/B that flag alone,
    // inside one session and one placement, which is the only way the difference is not confounded
    // with the engine's own frame-to-frame decisions.
    enum class Drive : std::uint8_t
    {
        e_off,
        e_tick,
        e_tick_apply,
        e_tick_apply_force,
        e_step
    };

    static FinalPoseApply& instance();
    [[nodiscard]] static char const* drive_name(Drive a_drive);

    // The F3 control: advances the cycle and pushes the resulting gate into PuppetDriver.
    void cycle();
    [[nodiscard]] Drive drive() const { return m_drive.load(std::memory_order_acquire); }
    [[nodiscard]] bool apply_enabled() const
    {
        const Drive current = drive();
        return current == Drive::e_tick_apply || current == Drive::e_tick_apply_force;
    }
    [[nodiscard]] bool force_generate() const { return drive() == Drive::e_tick_apply_force; }

    // Once per rendered frame, inside the render bracket, after the drivers ticked and before the
    // draw: verifies the mapping while the world runs, writes the pose while it is paused.
    void tick(bool a_paused);

    // A2: what the stage did, for the mark's report and for a reader who needs the last frame.
    struct Stats
    {
        bool applied = false;            // a pose was written on the frame this describes
        std::uint32_t written = 0;
        std::uint32_t unreachable = 0;   // pose indices the chosen mapping could not resolve to a node
        std::uint32_t frames = 0;
        std::uint32_t verifications = 0;
        float max_rotation = 0.0f;       // what the write changed on that frame
        float max_position = 0.0f;
    };

    [[nodiscard]] Stats const& stats() const { return m_stats; }
    [[nodiscard]] char const* mapping() const { return m_mapping; }
    [[nodiscard]] char const* blocked() const { return m_blocked; }
    [[nodiscard]] std::uint32_t generation() const { return m_generation; }
    [[nodiscard]] bool verified() const { return m_verified; }

private:
    FinalPoseApply() = default;

    // Scores both index orders against the engine's applied node locals and, on a match, keeps the
    // winning mapping as pose-index -> bone-table-index plus the ratio that justified it.
    void verify(RE::Actor& a_puppet);
    // Writes one frame of the pose through the kept mapping.
    void apply(RE::Actor& a_puppet);
    void block(char const* a_reason);

    std::atomic<Drive> m_drive{ Drive::e_tick };
    // A step is one frame of work as well as one advance: R02 wants a single step to advance exactly
    // one declared dt AND apply once, so the stage remembers that the next paused frame is that one.
    bool m_step_pending = false;
    // The identity the mapping belongs to. A new 3D root or a new graph invalidates it wholesale, and
    // a failed verification waits for that event instead of retrying every frame.
    void const* m_root = nullptr;
    void const* m_graph = nullptr;
    bool m_seen_identity = false;
    bool m_verify_attempted = false;
    // pose index -> index into the engine's live boneNodes table, for the mapping that was verified.
    // Indices the mapping cannot reach are -1 and counted, never silently skipped.
    std::vector<std::int32_t> m_targets;
    char const* m_mapping = "none";
    char const* m_blocked = "not-armed";
    bool m_verified = false;
    std::uint32_t m_generation = 0;
    std::uint32_t m_log_countdown = 0;
    Stats m_stats;
};

PLUGIN_NAMESPACE_END
