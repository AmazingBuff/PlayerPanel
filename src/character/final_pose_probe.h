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
#include <cstring>
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

// One transform as plain numbers, used for both sides of every comparison: what a pose implies for a
// bone, and what the node actually holds. The calibration compares the graph's own output pose
// against what the engine applied, so both sides can be handed over as data - which is exactly what
// lets the decision itself be unit-tested. A mapping rule that can only be exercised inside the game
// is a rule nobody can check.
//
// `present` is what keeps an index honest: an entry without a node is *not comparable*, and dropping
// it instead would shift every later index of that candidate onto the wrong bone.
struct TransformSample
{
    RE::NiMatrix3 rotate;
    RE::NiPoint3 translate;
    float scale = 1.0f;
    bool present = false;
};

// How many outliers a score names. The count is always exact; the list is a sample, because a wrong
// order can put every bone outside the bar and the log is read by a human.
inline constexpr std::size_t Score_Outlier_Cap = 8;

// What one index order scored against the node transforms.
struct OrderScore
{
    std::uint32_t compared = 0;
    std::uint32_t within = 0;      // bones whose rotation landed inside the tolerance
    std::uint32_t beyond = 0;      // ... and the ones that did not
    float max_rotation = 0.0f;
    float max_position = 0.0f;
    // R03 asks for the three spaces separately: a pose carries three scales where a NIF node carries
    // one, so this is measured for the record rather than used as a gate.
    float max_scale_deviation = 0.0f;
    // Indices (into the candidate table) of the bones beyond the bar, capped. The caller turns them
    // into names, because naming is a fact about the skeleton, not about the comparison.
    std::vector<std::uint32_t> outliers;

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

// One pose sample as plain numbers, read out of the engine's SSE quads with memcpy rather than with
// an aligned vector load. It reads the same three quads `read_pose` (animation_source.h) does, but
// this one is pointed at engine memory whose offset and alignment are exactly what the validation
// chain establishes - a misaligned load would fault inside the instrument that exists to turn a wrong
// assumption into a printed number.
[[nodiscard]] inline PoseSample read_pose_unaligned(RE::hkQsTransform const& a_pose)
{
    float raw[12];
    std::memcpy(raw, &a_pose, sizeof(raw));

    PoseSample sample{};
    sample.position[0] = raw[0];
    sample.position[1] = raw[1];
    sample.position[2] = raw[2];
    sample.quaternion[0] = raw[4];
    sample.quaternion[1] = raw[5];
    sample.quaternion[2] = raw[6];
    sample.quaternion[3] = raw[7];
    sample.scale = raw[8];
    return sample;
}

// The local transform one pose sample implies, in the convention this codebase measured (HKX2:
// `direct`, and the pose's own scale kept as one number).
[[nodiscard]] inline TransformSample pose_local(PoseSample const& a_sample)
{
    const RE::NiTransform local = ni_local_from_pose(a_sample);
    TransformSample sample;
    sample.rotate = local.rotate;
    sample.translate = local.translate;
    sample.scale = a_sample.scale;
    sample.present = true;
    return sample;
}

// Everything one pose implies, as a table: this is the "expected" side of a comparison, and it is
// built without touching the game so the rules below can be tested against fabricated skeletons.
inline void pose_locals(std::span<PoseSample const> a_pose, std::span<TransformSample> a_out)
{
    const std::size_t count = std::min(a_pose.size(), a_out.size());
    for (std::size_t i = 0; i < count; ++i)
        a_out[i] = pose_local(a_pose[i]);
}

// Composes a local table into world transforms along a parent table: world[i] = world[parent] *
// local[i]. R03 asks for the local and the world space separately, because they disagree exactly
// where the two hierarchies differ - a CME/helper branch hangs a bone somewhere the animation
// skeleton does not. A parent that is missing, out of range or not before its child is treated as a
// root rather than trusted: the loop is bounded by the table, so a cyclic table cannot hang it.
inline void compose_worlds(std::span<TransformSample const> a_locals, std::span<std::int16_t const> a_parents,
    std::span<TransformSample> a_out)
{
    const std::size_t count = std::min(a_locals.size(), a_out.size());
    for (std::size_t i = 0; i < count; ++i)
    {
        RE::NiTransform world;
        const std::int16_t parent = i < a_parents.size() ? a_parents[i] : -1;
        if (parent >= 0 && static_cast<std::size_t>(parent) < i && a_out[static_cast<std::size_t>(parent)].present)
        {
            const TransformSample& parent_world = a_out[static_cast<std::size_t>(parent)];
            RE::NiTransform parent_transform;
            parent_transform.rotate = parent_world.rotate;
            parent_transform.translate = parent_world.translate;
            parent_transform.scale = parent_world.scale;

            RE::NiTransform local;
            local.rotate = a_locals[i].rotate;
            local.translate = a_locals[i].translate;
            local.scale = a_locals[i].scale;
            world = parent_transform * local;
        }
        else
        {
            world.rotate = a_locals[i].rotate;
            world.translate = a_locals[i].translate;
            world.scale = a_locals[i].scale;
        }

        a_out[i].rotate = world.rotate;
        a_out[i].translate = world.translate;
        a_out[i].scale = world.scale;
        a_out[i].present = a_locals[i].present;
    }
}

// Aligns a composed world table with the worlds a node tree actually holds. The composition starts at
// each root bone's own local, while a node's world also carries everything above the skeleton - the
// Actor's placement, its root node, the CME branches a modded body hangs - so comparing the two raw
// convicts every bone of an offset that has nothing to do with the pose (measured: 0 of 92 bones
// inside one degree, worst translation 2044 units, on both subjects). One delta per root chain - the
// transform that maps the composed root onto the node's own root world - removes exactly that, so what
// the comparison then measures is the skeleton's structure rather than where the Actor stands.
inline void align_worlds_to_root(std::span<TransformSample const> a_composed, std::span<std::int16_t const> a_parents,
    std::span<TransformSample const> a_node_worlds, std::span<TransformSample> a_out)
{
    const std::size_t count = std::min(a_composed.size(), a_out.size());
    std::vector<RE::NiTransform> root_delta(count);

    for (std::size_t i = 0; i < count; ++i)
    {
        RE::NiTransform delta;  // identity unless this chain has a root whose node world is known
        const std::int16_t parent = i < a_parents.size() ? a_parents[i] : -1;
        const bool has_parent = parent >= 0 && static_cast<std::size_t>(parent) < i;
        if (has_parent)
        {
            delta = root_delta[static_cast<std::size_t>(parent)];
        }
        else if (i < a_node_worlds.size() && a_node_worlds[i].present && a_composed[i].present)
        {
            RE::NiTransform composed;
            composed.rotate = a_composed[i].rotate;
            composed.translate = a_composed[i].translate;
            composed.scale = a_composed[i].scale;
            RE::NiTransform node;
            node.rotate = a_node_worlds[i].rotate;
            node.translate = a_node_worlds[i].translate;
            node.scale = a_node_worlds[i].scale;
            // node_world = delta * composed  =>  delta = node_world * composed^-1
            delta = node * composed.Invert();
        }
        root_delta[i] = delta;

        RE::NiTransform composed;
        composed.rotate = a_composed[i].rotate;
        composed.translate = a_composed[i].translate;
        composed.scale = a_composed[i].scale;
        const RE::NiTransform world = delta * composed;
        a_out[i].rotate = world.rotate;
        a_out[i].translate = world.translate;
        a_out[i].scale = world.scale;
        a_out[i].present = a_composed[i].present;
    }
}

// The comparison itself: expected[i] against actual[i], index by index. Rotation is what gates - a
// bone counts as reproduced when it lands inside the tolerance - while translation and scale are
// measured for the record, because a pose carries three scales where a NIF node carries one and a
// systematic offset is a fact about the spaces rather than a verdict about the order. Indices whose
// actual side is absent are skipped and do not enter `compared`.
[[nodiscard]] inline OrderScore score_transforms(std::span<TransformSample const> a_expected,
    std::span<TransformSample const> a_actual, float a_tolerance_degrees)
{
    OrderScore score;
    const std::size_t comparable = std::min(a_expected.size(), a_actual.size());
    for (std::size_t i = 0; i < comparable; ++i)
    {
        if (!a_actual[i].present || !a_expected[i].present)
            continue;

        const float degrees = rotation_angle_degrees(a_expected[i].rotate, a_actual[i].rotate);
        const float position = (a_expected[i].translate - a_actual[i].translate).Length();
        const float scale = std::abs(a_expected[i].scale - a_actual[i].scale);

        ++score.compared;
        if (degrees <= a_tolerance_degrees)
            ++score.within;
        else
        {
            ++score.beyond;
            if (score.outliers.size() < Score_Outlier_Cap)
                score.outliers.push_back(static_cast<std::uint32_t>(i));
        }
        score.max_rotation = std::max(score.max_rotation, degrees);
        score.max_position = std::max(score.max_position, position);
        score.max_scale_deviation = std::max(score.max_scale_deviation, scale);
    }
    return score;
}

// Scores one index order: pose[i] is compared with locals[i], index by index.
[[nodiscard]] inline OrderScore score_pose_order(std::span<PoseSample const> a_pose, std::span<TransformSample const> a_locals,
    float a_tolerance_degrees)
{
    // A small stack of the expected side would be cheaper, but the table has to be index-aligned with
    // the candidate (an absent node still occupies its index), so it is built whole.
    std::vector<TransformSample> expected(a_locals.size());
    const std::size_t count = std::min(a_pose.size(), expected.size());
    for (std::size_t i = 0; i < count; ++i)
        expected[i] = pose_local(a_pose[i]);
    return score_transforms(expected, a_locals, a_tolerance_degrees);
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

// The character's third-person graph: the one whose holder is that character and which has a root
// node. Shared by the instrument and the application stage so both read the same graph the same way.
[[nodiscard]] RE::BShkbAnimationGraph* find_third_person_graph(RE::TESObjectREFR& a_subject);

// Every node of a tree by exact name, for the name bridge. Exposed because the application stage needs
// the same bridge the instrument measured - and a second copy of a name rule is a second thing to be
// wrong. Read-only: nothing is retained and no name is copied out of the engine's storage.
[[nodiscard]] std::unordered_map<std::string_view, RE::NiAVObject*> nodes_by_name(RE::NiAVObject& a_root);

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
    // A0: call once per rendered frame inside the render bracket BEFORE the drivers tick. The mark
    // then holds the frame's state as the engine left it, so the report taken after the tick (A1) can
    // say whether this frame's manual tick is what changed the output - which no cross-mark delta can
    // separate from the engine having run in between (R02 asks for exactly this pair).
    void sample_before_tick();
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
        std::vector<PoseSample> pose_local;
        std::vector<BoneState> bones;
        // How many node locals moved since the previous mark: the verdict carries it because a
        // calibration against a frozen tree measures nothing, and the reader has to see that on the
        // same line as the number it qualifies.
        std::uint32_t nodes_moved = 0;
        // The panel's own generation notion, which R02/R06 ask for next to the graph and the bone
        // table: it counts how many times this subject's 3D root or its graph changed identity, and
        // that is exactly the event that invalidates a mapping or a pose held across frames. A tree
        // the engine rebuilt is therefore numbered here rather than inferred from `rebound` counts.
        std::uint32_t generation = 0;
        void const* root = nullptr;
        void const* graph = nullptr;
    };

    // A0's own sample. It reads the same two arrays the report reads but keeps them separately, so
    // the report path stays exactly as it was measured and the pair can only ever be compared, never
    // confused. `reason` is the pose track's own verdict per buffer.
    struct PreSample
    {
        bool valid = false;
        bool paused = false;
        std::vector<PoseSample> pose[2];
        std::vector<BoneState> bones;
        // `hkbCharacter::poseLocal`, the pose the character itself holds - a second, differently
        // ordered source of the same information (HKX2 measured it boneNodes-ordered while the graph's
        // output pose is skeleton-ordered). It is sampled because the paused puppet has no output pose
        // at all: if this one still moves under the panel's own clock, the route has a source while
        // paused; if it is frozen too, the missing piece is the engine's evaluation, not our read.
        std::vector<PoseSample> pose_local;
        void const* output[2] = { nullptr, nullptr };
        std::int32_t pose_count[2] = { 0, 0 };
        char const* reason[2] = { "not-read", "not-read" };
        std::uint32_t bones_present = 0;
    };

    // One (index order, output slot) reading of the calibration: which order and buffer it was, plus
    // the score itself. `named` is the skeleton order's own reach - bones that resolved to a node by
    // exact name - and is what tells a failed ratio apart from a failed bridge.
    //
    // Both spaces are kept: `score` is the local one, which is what a write would have to reproduce
    // and therefore what the decision gates on, while `world` is the same pose composed along the
    // animation skeleton's parent table and compared with the nodes' own worlds (R03 asks for both,
    // because they disagree exactly where the two hierarchies do). `world.compared == 0` means this
    // order has no parent table to compose with, not that the world space agreed.
    struct Candidate
    {
        char const* order = "none";
        int slot = -1;
        std::uint32_t sampled = 0;
        std::uint32_t named = 0;
        OrderScore score;
        OrderScore world;
    };

    // Everything one calibration produced: the candidate the decision chose, and the decision itself
    // (which is what the application stage will act on, and what the verdict line prints).
    struct Calibration
    {
        Candidate candidate;
        OrderDecision decision;
        // The nodes the chosen candidate's outliers sit on, in outlier order. R03/R04 need to know
        // whether a bone nothing reproduced is one a target mesh reads (which blocks) or a helper
        // branch (which does not), and that question is about the skin palette, not about the score.
        std::vector<RE::NiAVObject*> outlier_nodes;
    };

    FinalPoseProbe() = default;

    void subject(char const* a_label, RE::TESObjectREFR* a_subject, SubjectState& a_state);
    // What the graph itself says about the frame the census counted, which the pose report cannot see:
    // whether the behaviour graph is active at all (a deactivated one generates nothing however often
    // it is ticked), whether it is linked, how many node clones it carries, and how far the engine has
    // interpolated between its two output buffers. The paused puppet's empty pose is the whole
    // question AFP3 was opened for, and "deactivated" / "never generated" / "generated and consumed"
    // are three different answers - so the state that separates them is printed rather than inferred
    // from an element count, and the fields no source names yet are printed raw.
    void report_state(char const* a_label, RE::BShkbAnimationGraph const& a_graph);
    // A0's sampler: output buffers and node locals, nothing printed.
    PreSample sample_only(RE::TESObjectREFR* a_subject);
    // The A0 -> A1 pair for one subject: what this frame's tick changed, in the output and in the
    // nodes, with the same identity rule the mark deltas use (a rebuilt tree reports rebound, never a
    // phantom move).
    void report_frame_delta(char const* a_label, PreSample const& a_pre, SubjectState const& a_now);
    // The order calibration: every candidate (order x output slot) is scored against the node locals
    // the engine itself wrote, one log line each, and the best of them is judged by
    // `decide_pose_order` - the same call the application stage makes, so the log says whether that
    // stage would be allowed to write.
    Calibration calibrate(RE::BShkbAnimationGraph& a_graph, SubjectState const& a_state, char const* a_label);

    SubjectState m_puppet;
    SubjectState m_player;
    PreSample m_puppet_pre;
    PreSample m_player_pre;
    std::atomic_bool m_mark_requested{ false };
    std::uint32_t m_marks = 0;
};

PLUGIN_NAMESPACE_END
