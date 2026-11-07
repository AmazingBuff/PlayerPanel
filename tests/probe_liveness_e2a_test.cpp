//
// C-G5-E2-A engine-free cases: the 64112 mix (trans lerp, sign-corrected nlerp with the
// zero-length convention), the tolerance shape, the tally, and the per-track comparison.
//
// Build:
//   cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests\probe_liveness_e2a_test.cpp
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/probe_liveness_policy.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

using namespace test_plugin::animation::behavior_graph;

namespace
{
    constexpr float kEpsilon = 1.0e-6f;

    bool near(float a_left, float a_right, float a_epsilon = kEpsilon)
    {
        return std::fabs(a_left - a_right) <= a_epsilon;
    }

    void mix_matches_endpoints_and_midpoint()
    {
        ProbeVec4 const a{ 1.0f, 2.0f, 3.0f, 0.0f };
        ProbeVec4 const b{ 5.0f, 6.0f, 7.0f, 0.0f };
        ProbeVec4 const at_zero = probe_mix_trans(a, b, 0.0f);
        assert(near(at_zero.x, 1.0f) && near(at_zero.y, 2.0f) && near(at_zero.z, 3.0f));
        ProbeVec4 const at_one = probe_mix_trans(a, b, 1.0f);
        assert(near(at_one.x, 5.0f) && near(at_one.y, 6.0f) && near(at_one.z, 7.0f));
        ProbeVec4 const half = probe_mix_trans(a, b, 0.5f);
        assert(near(half.x, 3.0f) && near(half.y, 4.0f) && near(half.z, 5.0f));
    }

    void rot_mix_normalises_and_handles_sign()
    {
        // Two unit quaternions with a positive dot: the nlerp result must stay unit length.
        ProbeVec4 const a{ 1.0f, 0.0f, 0.0f, 0.0f };
        ProbeVec4 const b{ 0.0f, 1.0f, 0.0f, 0.0f };
        ProbeVec4 const mixed = probe_mix_rot(a, b, 0.5f);
        float const length = std::sqrt(probe_dot4(mixed, mixed));
        assert(near(length, 1.0f, 2.0e-6f));

        // Negative dot: B is negated first, so the same pair must land on the same direction as A.
        ProbeVec4 const opposite{ -1.0f, 0.0f, 0.0f, 0.0f };
        ProbeVec4 const corrected = probe_mix_rot(a, opposite, 0.5f);
        assert(near(corrected.x, 1.0f, 2.0e-6f));
        assert(near(corrected.y, 0.0f) && near(corrected.z, 0.0f) && near(corrected.w, 0.0f));

        // Zero-length convention: 64112 writes the zero vector when the mixed length is zero.
        ProbeVec4 const zero{ 0.0f, 0.0f, 0.0f, 0.0f };
        ProbeVec4 const degenerate = probe_mix_rot(zero, zero, 0.5f);
        assert(degenerate.x == 0.0f && degenerate.y == 0.0f && degenerate.z == 0.0f
            && degenerate.w == 0.0f);
    }

    void tolerance_switches_at_one_hundred_units()
    {
        // Below 100 units the absolute 2e-6 applies.
        assert(probe_mix_delta(1.0f, 1.0f + 1.0e-6f) == 0.0f);
        assert(probe_mix_delta(1.0f, 1.0f + 3.0e-6f) > 0.0f);
        // At 200 units the relative 1e-5 applies: 1.5e-3 passes, 3e-3 fails.
        assert(probe_mix_delta(200.0f, 200.0f + 1.5e-3f) == 0.0f);
        assert(probe_mix_delta(200.0f, 200.0f + 3.0e-3f) > 0.0f);
        // Below the boundary the absolute rule still applies. 50.0f has a 3.8e-6 ulp, so adding
        // 5e-6 is representable and lands above the 2e-6 tolerance (100.0f would quantise 3e-6
        // away entirely, which is a test-input problem rather than an implementation one).
        assert(probe_mix_delta(50.0f, 50.0f + 5.0e-6f) > 0.0f);
        assert(probe_mix_delta(50.0f, 50.0f + 1.0e-6f) == 0.0f);
    }

    void tally_counts_failures_and_keeps_four_samples()
    {
        ProbeMixTally tally{};
        tally.add_frame(116);
        assert(tally.frames == 1 && tally.tracks == 116);
        for (std::uint32_t i = 0; i < 6; ++i)
            assert(!tally.add_component(i, 0, 1.0f, 1.0f + 0.001f * static_cast<float>(i + 1)));
        assert(tally.failed == 6);
        assert(tally.sample_count == ProbeMixTally::Max_Samples);
        assert(tally.max_delta >= 0.006f - 1.0e-6f);
        assert(!tally.clean());
        // A passing component does not touch the counters.
        ProbeMixTally const clean_before = tally;
        (void)clean_before;
        assert(tally.add_component(7, 1, 2.0f, 2.0f));
        assert(tally.failed == 6);
        tally.reset();
        assert(tally.frames == 0 && tally.tracks == 0 && tally.failed == 0 && tally.sample_count == 0);
        assert(tally.clean());
    }

    void non_finite_is_a_failure()
    {
        ProbeMixTally tally{};
        assert(!tally.add_component(0, 0, std::numeric_limits<float>::quiet_NaN(), 1.0f));
        assert(tally.non_finite);
        assert(!tally.clean());
    }

    // ---- per-track comparison against a hand-built track --------------------------------

    struct Track
    {
        std::array<std::byte, 0x30> bytes{};

        void set_trans(float a_x, float a_y, float a_z)
        {
            std::memcpy(bytes.data() + 0x00, &a_x, sizeof(a_x));
            std::memcpy(bytes.data() + 0x04, &a_y, sizeof(a_y));
            std::memcpy(bytes.data() + 0x08, &a_z, sizeof(a_z));
        }

        void set_rot(float a_x, float a_y, float a_z, float a_w)
        {
            std::memcpy(bytes.data() + 0x10, &a_x, sizeof(a_x));
            std::memcpy(bytes.data() + 0x14, &a_y, sizeof(a_y));
            std::memcpy(bytes.data() + 0x18, &a_z, sizeof(a_z));
            std::memcpy(bytes.data() + 0x1c, &a_w, sizeof(a_w));
        }
    };

    void compare_track_accepts_the_explicit_identity()
    {
        // Values derived by hand from the 64112 formula with w = 0.25:
        //   trans = (1-0.25)*(1,2,3) + 0.25*(5,6,7)          = (2, 3, 4)
        //   rot   = normalize(0.75*(1,0,0,0) + 0.25*(0,1,0,0)) = (0.948683, 0.316228, 0, 0)
        Track slot1{};
        Track slot0{};
        Track pose{};
        slot1.set_trans(1.0f, 2.0f, 3.0f);
        slot0.set_trans(5.0f, 6.0f, 7.0f);
        slot1.set_rot(1.0f, 0.0f, 0.0f, 0.0f);
        slot0.set_rot(0.0f, 1.0f, 0.0f, 0.0f);
        pose.set_trans(2.0f, 3.0f, 4.0f);
        pose.set_rot(0.94868326f, 0.31622776f, 0.0f, 0.0f);

        ProbeMixTally clean{};
        assert(probe_compare_track(pose.bytes.data(), slot1.bytes.data(), slot0.bytes.data(), 0.25f, 0,
            clean) == 0);
        assert(clean.clean());

        // Corrupt rotation w only: exactly one component fails, on channel 7.
        pose.set_rot(0.94868326f, 0.31622776f, 0.0f, 0.01f);
        ProbeMixTally dirty{};
        std::uint32_t const failed = probe_compare_track(pose.bytes.data(), slot1.bytes.data(),
            slot0.bytes.data(), 0.25f, 0, dirty);
        assert(failed == 1);
        assert(dirty.failed == 1);
        assert(dirty.sample_count == 1);
        assert(dirty.samples[0].track == 0);
        assert(dirty.samples[0].channel == 7);

        // Corrupt one translation component: exactly one failure, on channel 1.
        Track pose_x{};
        pose_x.set_trans(2.0f, 3.5f, 4.0f);
        pose_x.set_rot(0.94868326f, 0.31622776f, 0.0f, 0.0f);
        ProbeMixTally trans_dirty{};
        assert(probe_compare_track(pose_x.bytes.data(), slot1.bytes.data(), slot0.bytes.data(), 0.25f, 0,
            trans_dirty) == 1);
        assert(trans_dirty.samples[0].channel == 1);
    }
    void track_count_only_caps_by_participating_slots()
    {
        // Fallback (w = 1): slot1 is inactive, so its zero count must not force n to zero. The v73
        // run read slot0=116, slot1=0, w=1.0 on all 276 frames and therefore compared nothing.
        assert(probe_mix_track_count(116, 1.0f, 116, 0) == 116);
        // w = 0: slot0 is inactive and slot1 caps instead.
        assert(probe_mix_track_count(116, 0.0f, 0, 116) == 116);
        // A genuine two-slot blend is capped by both slots.
        assert(probe_mix_track_count(116, 0.5f, 116, 0) == 0);
        assert(probe_mix_track_count(116, 0.5f, 50, 116) == 50);
        // The clamped count still caps when the slots are larger.
        assert(probe_mix_track_count(50, 0.5f, 116, 116) == 50);
        // Branch classification: only a strict interior weight blends two slots.
        assert(probe_mix_mode(1.0f) == ProbeMixMode::e_fallback);
        assert(probe_mix_mode(0.0f) == ProbeMixMode::e_fallback);
        assert(probe_mix_mode(0.5f) == ProbeMixMode::e_main);
    }
    void failure_attribution_prefers_the_matching_slot()
    {
        ProbeMixTally tally{};
        tally.classify_failure(5.0f, 5.0f, 1.0f);   // actual equals slot0
        assert(tally.near_slot0 == 1 && tally.near_slot1 == 0 && tally.near_neither == 0);
        tally.classify_failure(1.0f, 5.0f, 1.0f);   // actual equals slot1
        assert(tally.near_slot1 == 1);
        tally.classify_failure(3.0f, 5.0f, 1.0f);   // closer to neither
        assert(tally.near_neither == 1);
        tally.reset();
        assert(tally.near_slot0 == 0 && tally.near_slot1 == 0 && tally.near_neither == 0);
        assert(tally.fail_frames == 0 && tally.fail_frames_ik == 0 && tally.ik_ready_frames == 0);
    }
    void real_ik_gate_requires_all_four_conditions()
    {
        assert(probe_ik_gate_open(116, 1, 1, true));
        assert(!probe_ik_gate_open(0, 1, 1, true));
        assert(!probe_ik_gate_open(116, 0, 1, true));
        assert(!probe_ik_gate_open(116, 1, 0, true));
        assert(!probe_ik_gate_open(116, 1, 1, false));
        // The payload-overwrite gate used by v75/v76 is a different object entirely; the four
        // inputs above are the ones 63581 actually reads before calling 58423.
        assert(!probe_ik_gate_open(116, 0, 1, true));
    }
    void slot_view_rejects_missing_or_implausible_pointers()
    {
        assert(!probe_slot_view(nullptr, Probe_Mix_Slot0_Container_Offset).valid);
        std::array<std::byte, 0x40> graph{};
        assert(!probe_slot_view(graph.data(), Probe_Mix_Slot0_Container_Offset).valid);
        // A container pointer to a slot whose track offset points into its own storage.
        std::array<std::byte, 0x80> slot{};
        std::int32_t const gate = 3;
        std::uint16_t const count = 7;
        std::int16_t const track_offset = 0x40;
        std::memcpy(slot.data() + 4, &gate, sizeof(gate));
        std::memcpy(slot.data() + 0x32, &count, sizeof(count));
        std::memcpy(slot.data() + 0x34, &track_offset, sizeof(track_offset));
        void const* slot_ptr = slot.data();
        std::array<std::byte, 0x40> graph2{};
        void const* container = &slot_ptr;
        std::memcpy(graph2.data() + Probe_Mix_Slot0_Container_Offset, &container, sizeof(container));
        ProbeSlotView const view = probe_slot_view(graph2.data(), Probe_Mix_Slot0_Container_Offset);
        assert(view.valid);
        assert(view.count == 7);
        assert(view.tracks == slot.data() + 0x40);
    }
}

int main()
{
    mix_matches_endpoints_and_midpoint();
    rot_mix_normalises_and_handles_sign();
    tolerance_switches_at_one_hundred_units();
    tally_counts_failures_and_keeps_four_samples();
    non_finite_is_a_failure();
    compare_track_accepts_the_explicit_identity();
    track_count_only_caps_by_participating_slots();
    failure_attribution_prefers_the_matching_slot();
    real_ik_gate_requires_all_four_conditions();
    slot_view_rejects_missing_or_implausible_pointers();
    std::puts("10 probe-liveness e2a cases passed (engine-free)");
}
