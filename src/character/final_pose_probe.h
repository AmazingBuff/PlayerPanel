//
// Created by AmazingBuff on 2026/10/13.
//

#pragma once

#include "character/animation_source.h"

#include "RE/H/hkQsTransform.h"
#include "RE/N/NiMatrix3.h"
#include "RE/N/NiPoint3.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
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
// moved since the previous mark, and calibrates both candidate orders against the node locals. A
// wrong assumption therefore prints a number instead of moving a skeleton - which is the bar R00
// sets before any package may write a pose.
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

    // One (index order, output slot) reading of the calibration: how many bones were comparable and
    // how many of them the candidate reproduces within the declared one-degree bar.
    struct Candidate
    {
        char const* order = "none";
        int slot = -1;
        std::uint32_t sampled = 0;
        std::uint32_t compared = 0;
        std::uint32_t named = 0;   // skeleton order only: bones that resolved to a node by exact name
        std::uint32_t within = 0;
        float max_rotation = 0.0f;
        float max_position = 0.0f;
    };

    FinalPoseProbe() = default;

    void subject(char const* a_label, RE::TESObjectREFR* a_subject, SubjectState& a_state);
    // The order calibration: every candidate (order x output slot) is scored against the node locals
    // the engine itself wrote, one log line each, and the best of them is returned for the verdict.
    Candidate calibrate(RE::BShkbAnimationGraph& a_graph, SubjectState const& a_state, char const* a_label);

    SubjectState m_puppet;
    SubjectState m_player;
    std::atomic_bool m_mark_requested{ false };
    std::uint32_t m_marks = 0;
};

PLUGIN_NAMESPACE_END
