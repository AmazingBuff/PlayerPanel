//
// Created by AmazingBuff on 2026/10/13.
//

#pragma once

#include "character/animation_source.h"
#include "character/snapshot_transform.h"

#include "RE/H/hkQsTransform.h"
#include "RE/N/NiMatrix3.h"
#include "RE/N/NiPoint3.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace RE
{
    class BShkbAnimationGraph;
    class NiAVObject;
    class TESObjectREFR;
}

PLUGIN_NAMESPACE_BEGIN

// The bar for "this index reproduces that bone", taken from the HKX28 protocol rather than tuned
// after seeing a result: R00 requires the tolerances to be fixed before a package is handed over.
inline constexpr float Pose_Match_Degrees = 1.0f;
// ... and the two conditions under which a scoring is allowed to become an application. A high ratio
// over three bones is a coincidence, and a mapping nothing checked may not drive a skeleton (R00:
// an unavailable or unverified output is named and blocked, never faked).
inline constexpr std::uint32_t Pose_Match_Min_Compared = 8;
inline constexpr float Pose_Match_Min_Ratio = 0.9f;

// The engine's node local for one candidate index, as plain numbers. The calibration compares the
// graph's own output pose against what the engine applied to the nodes, so both sides can be handed
// over as data - which is exactly what lets the decision itself be unit-tested. A mapping rule that
// can only be exercised inside the game is a rule nobody can check.
//
// `present` is what keeps an index honest: a table entry without a node is *not comparable*, and
// dropping it instead would shift every later index of that candidate onto the wrong bone.
struct NodeLocal
{
    RE::NiMatrix3 rotate;
    RE::NiPoint3 translate;
    bool present = false;
};

// What one index order scored against those node locals.
struct OrderScore
{
    std::uint32_t compared = 0;
    std::uint32_t within = 0;      // bones whose rotation landed inside the tolerance
    float max_rotation = 0.0f;
    float max_position = 0.0f;

    [[nodiscard]] float ratio() const
    {
        return compared != 0 ? static_cast<float>(within) / static_cast<float>(compared) : 0.0f;
    }
};

// The position a pose sample carries, as an engine point. It lives here with the scoring rules
// because both are pure: no engine object is touched, so the unit test can exercise them.
[[nodiscard]] inline RE::NiPoint3 pose_position(PoseSample const& a_sample)
{
    return RE::NiPoint3{ a_sample.position[0], a_sample.position[1], a_sample.position[2] };
}

// Scores one index order: pose[i] is compared with locals[i], index by index, and a bone counts as
// reproduced when its rotation is within the tolerance. Translation is measured and reported but
// never gates: a pose carries three scales where a NIF node carries one, so a systematic offset in a
// translation is a fact about the spaces, not a verdict about the order. Indices without a node are
// skipped and do not enter `compared`.
[[nodiscard]] inline OrderScore score_pose_order(std::span<PoseSample const> a_pose, std::span<NodeLocal const> a_locals,
    float a_tolerance_degrees)
{
    OrderScore score;
    const std::size_t comparable = std::min(a_pose.size(), a_locals.size());
    for (std::size_t i = 0; i < comparable; ++i)
    {
        // An index without a node says nothing about this order either way, so it is skipped rather
        // than counted as a mismatch (which would let a sparse table convict a correct order).
        if (!a_locals[i].present)
            continue;

        const RE::NiMatrix3 rotation = ni_local_from_pose(a_pose[i], false).rotate;
        const float degrees = rotation_angle_degrees(rotation, a_locals[i].rotate);
        const float position = (pose_position(a_pose[i]) - a_locals[i].translate).Length();

        ++score.compared;
        if (degrees <= a_tolerance_degrees)
            ++score.within;
        score.max_rotation = std::max(score.max_rotation, degrees);
        score.max_position = std::max(score.max_position, position);
    }
    return score;
}

// The decision the application stage needs, and the reason when it must not write.
struct OrderDecision
{
    bool match = false;
    std::size_t best = 0;
    char const* reason = "no-candidate";
    OrderScore score;
};

// Picks the best of the scored candidates and says whether it may be written. `reason` is one of
// `matched`, `no-candidate`, `insufficient-compared`, `below-ratio` - the caller prints it as it is,
// so a blocked application says which condition blocked it instead of failing silently.
[[nodiscard]] inline OrderDecision decide_pose_order(std::span<OrderScore const> a_scores)
{
    OrderDecision decision;

    for (std::size_t i = 0; i < a_scores.size(); ++i)
    {
        if (a_scores[i].compared == 0)
            continue;
        if (decision.score.compared == 0 || a_scores[i].ratio() > decision.score.ratio() ||
            (a_scores[i].ratio() == decision.score.ratio() && a_scores[i].compared > decision.score.compared))
        {
            decision.best = i;
            decision.score = a_scores[i];
        }
    }

    if (decision.score.compared == 0)
    {
        decision.reason = "no-candidate";
        return decision;
    }
    // A ratio over a handful of bones is a coincidence, and an order nothing verified may not drive a
    // skeleton: both conditions are reported as the block they are.
    if (decision.score.compared < Pose_Match_Min_Compared)
    {
        decision.reason = "insufficient-compared";
        return decision;
    }
    if (decision.score.ratio() < Pose_Match_Min_Ratio)
    {
        decision.reason = "below-ratio";
        return decision;
    }

    decision.match = true;
    decision.reason = "matched";
    return decision;
}

// One standard track of a behaviour graph's own output buffer (`hkbGeneratorOutput`, reached through
// `BShkbAnimationGraph::generatorOutputs`). Every field is filled only after the step it describes
// was actually read, and `reason` names the step that stopped the read, so a buffer this build does
// not understand reports which assumption failed instead of a zero that reads like real data.
struct OutputTrackView
{
    char const* reason = "not-read";
    void const* output = nullptr;
    void const* data = nullptr;
    std::int32_t num_tracks = 0;
    std::int16_t num_data = 0;
    std::int16_t capacity = 0;
    std::int16_t element_size = 0;
    std::int16_t data_offset = 0;
    std::uint8_t type = 0;
    std::uint8_t flags = 0;
    float on_fraction = 0.0f;
    bool output_readable = false;
    bool tracks_readable = false;
    bool exists = false;         // the master header lists a track at this index
    bool header_readable = false;
    bool valid = false;          // onFraction > 0, which is the engine's own "this track is live"
    bool data_readable = false;
};

// Reads the pose track (standard track 2) out of one of the graph's two output buffers. Read-only and
// fully page-guarded: the buffer belongs to the graph update, so nothing here is retained across
// frames and no virtual function is called. `a_pose`/`a_count` are set only when the whole chain -
// output object, tracks blob, track header, element size, data span - validated.
[[nodiscard]] bool read_pose_track(RE::BShkbAnimationGraph& a_graph, int a_slot, OutputTrackView& a_view,
    RE::hkQsTransform const*& a_pose, std::int32_t& a_count);

// AFP1 (PRD 0.9 section 6.6): the first instrument of the same-Actor route.
//
// That route draws the puppet Actor's own third-person 3D and, while the panel pauses the world,
// applies the behaviour graph's *final* output pose to that Actor's own skeleton. Two questions
// decide whether it can work at all, and neither is answered yet:
//
//   1. whether the graph's output buffer really holds a readable final pose after our manual graph
//      tick, and which of the two buffers is the current one (the graph keeps two, so the engine can
//      interpolate between them);
//   2. which index order that pose uses - the engine's own `boneNodes` table or the animation
//      skeleton - because the engine applied it during normal play and the manual route has to
//      reproduce exactly that.
//
// This probe answers both from inside the game without writing anything: on each mark it copies the
// pose track and the node locals the engine applied, prints them beside each other, prints what
// moved since the previous mark, scores both candidate orders through `score_pose_order`, and prints
// the decision `decide_pose_order` reaches. A wrong assumption therefore prints a number instead of
// moving a skeleton - which is the bar R00 sets before any package may write a pose.
class FinalPoseProbe
{
public:
    static FinalPoseProbe& instance();

    // The mark control: the next rendered frame takes one read-only sample of both subjects.
    void request_mark();
    // Call once per rendered frame inside the render bracket, after the drivers have ticked, so the
    // sample sees the frame the panel is about to draw.
    void tick(bool a_paused);

private:
    // A node local as this probe owns it: plain numbers plus the node's identity. The pointer is
    // never dereferenced after the mark that read it (HKX43's dangling read came from keeping one),
    // it only says whether the same node is still at this index.
    struct BoneState
    {
        void const* node = nullptr;
        RE::NiMatrix3 rotate;
        RE::NiPoint3 translate;
    };

    // What one subject looked like at the previous mark.
    struct SubjectState
    {
        std::vector<PoseSample> pose[2];
        std::vector<BoneState> bones;
        // How many node locals moved since the previous mark: the verdict carries it because a
        // calibration against a frozen tree measures nothing, and the reader has to see that on the
        // same line as the number it qualifies.
        std::uint32_t nodes_moved = 0;
    };

    // One (index order, output slot) reading of the calibration: which order and buffer it was, plus
    // the score itself. `named` is the skeleton order's own reach - bones that resolved to a node by
    // exact name - and is what tells a failed ratio apart from a failed bridge.
    struct Candidate
    {
        char const* order = "none";
        int slot = -1;
        std::uint32_t sampled = 0;
        std::uint32_t named = 0;
        OrderScore score;
    };

    // Everything one calibration produced: the candidate the decision chose, and the decision itself
    // (which is what the application stage will act on, and what the verdict line prints).
    struct Calibration
    {
        Candidate candidate;
        OrderDecision decision;
    };

    FinalPoseProbe() = default;

    void subject(char const* a_label, RE::TESObjectREFR* a_subject, SubjectState& a_state);
    // The order calibration: every candidate (order x output slot) is scored against the node locals
    // the engine itself wrote, one log line each, and the best of them is judged by
    // `decide_pose_order` - the same call the application stage makes, so the log says whether that
    // stage would be allowed to write.
    Calibration calibrate(RE::BShkbAnimationGraph& a_graph, SubjectState const& a_state, char const* a_label);

    SubjectState m_puppet;
    SubjectState m_player;
    std::atomic_bool m_mark_requested{ false };
    std::uint32_t m_marks = 0;
};

PLUGIN_NAMESPACE_END
