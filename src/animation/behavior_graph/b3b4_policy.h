//
// C-G5-E2-B B3/B4: the source-graph post-process boundary (2026-11-12 pre-registration).
//
// B3: the 58423 return-side `out` block must still hold its return snapshot when the pinned
// graph's 63856 apply entry re-reads it (any component drift means a writer between 58423 and the
// memcpy path).
// B4: poseLocal (63856's first argument) must equal the same snapshot component-wise.
//
// The stored snapshot is keyed per thread on the out-block pointer. Only pairs whose
// `ctx->behavior` equals the pinned graph are admitted, per the pre-registration. Stale, missing,
// multi-return and read-failure outcomes are counted, never silently resolved.
//
#pragma once

#include "animation/behavior_graph/probe_liveness_policy.h"

#include <array>
#include <cstdint>
#include <cstring>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    inline constexpr std::size_t Probe_B3B4_Track_Capacity = 116;
    inline constexpr std::size_t Probe_B3B4_Max_Threads = 8;
    inline constexpr std::size_t Probe_B3B4_Max_Samples = 8;

    struct ProbeB3B4Sample
    {
        std::uint32_t track;
        std::uint8_t channel;
        float expected;
        float actual;
        float delta;
    };

    struct ProbeB3B4Tally
    {
        // Shared outcome counters; b3_checked counts out re-reads, b4_checked poseLocal reads.
        std::uint64_t b3_checked{ 0 };
        std::uint64_t b3_equal{ 0 };
        std::uint64_t b3_failed{ 0 };
        float b3_max_delta{ 0.0f };
        std::uint64_t b4_checked{ 0 };
        std::uint64_t b4_equal{ 0 };
        std::uint64_t b4_failed{ 0 };
        float b4_max_delta{ 0.0f };
        std::uint32_t sample_count{ 0 };
        float max_delta{ 0.0f };
        std::array<ProbeB3B4Sample, Probe_B3B4_Max_Samples> samples{};
        bool read_failed{ false };
        bool non_finite{ false };

        void reset() noexcept
        {
            b3_checked = 0;
            b3_equal = 0;
            b3_failed = 0;
            b3_max_delta = 0.0f;
            b4_checked = 0;
            b4_equal = 0;
            b4_failed = 0;
            b4_max_delta = 0.0f;
            sample_count = 0;
            max_delta = 0.0f;
            samples = {};
            read_failed = false;
            non_finite = false;
        }

        // Returns the number of failing components. The first Probe_B3B4_Max_Samples failures of
        // the window are kept verbatim; later ones only feed the counters. a_phase selects the
        // counter set: 0 = B3 (out re-read), 1 = B4 (poseLocal). The phase max uses the worst
        // failing component of THIS call, not a cross-call accumulator, so a later clean call
        // cannot lower or mask an earlier phase's worst delta.
        std::uint32_t compare(std::uint8_t a_phase, std::uint32_t a_track_count, ProbeVec4 const* a_expected,
            ProbeVec4 const* a_actual) noexcept
        {
            std::uint32_t bad = 0;
            for (std::uint32_t track = 0; track < a_track_count; ++track)
            {
                for (std::uint8_t segment = 0; segment < 2; ++segment)
                {
                    ProbeVec4 const& expected = a_expected[track * 2 + segment];
                    ProbeVec4 const& actual = a_actual[track * 2 + segment];
                    float const* e = &expected.x;
                    float const* a = &actual.x;
                    std::uint8_t const base = segment == 0 ? Probe_Mix_Channel_Trans_X : Probe_Mix_Channel_Rot_X;
                    for (std::uint8_t c = 0; c < 4; ++c)
                    {
                        if (!std::isfinite(e[c]) || !std::isfinite(a[c]))
                        {
                            non_finite = true;
                            ++bad;
                            continue;
                        }
                        float const delta = probe_mix_delta(e[c], a[c]);
                        if (delta == 0.0f)
                            continue;
                        ++bad;
                        if (delta > max_delta)
                            max_delta = delta;
                        if (sample_count < samples.size())
                            samples[sample_count++] = ProbeB3B4Sample{ track,
                                static_cast<std::uint8_t>(base + c), e[c], a[c], delta };
                    }
                }
            }
            float& phase_max = a_phase == 0 ? b3_max_delta : b4_max_delta;
            float const call_worst = bad > 0 ? max_delta : 0.0f;
            max_delta = 0.0f;
            if (call_worst > phase_max)
                phase_max = call_worst;
            if (a_phase == 0)
            {
                ++b3_checked;
                if (bad == 0) ++b3_equal; else ++b3_failed;
            }
            else
            {
                ++b4_checked;
                if (bad == 0) ++b4_equal; else ++b4_failed;
            }
            return bad;
        }
    };

    // One thread's pending 58423 return snapshot, keyed on the out-block pointer.
    struct ProbeB3B4Slot
    {
        std::uint32_t tid;
        void const* out;
        void const* behavior;
        std::uint16_t tracks;
        bool pending;
        std::array<ProbeVec4, Probe_B3B4_Track_Capacity * 2> snapshot;
    };

    class ProbeB3B4Ledger
    {
    public:
        enum class Outcome : std::uint8_t
        {
            e_stored,
            e_overwritten,
            e_no_slot
        };

        Outcome on_return(std::uint32_t a_tid, void const* a_out, void const* a_behavior,
            std::uint16_t a_tracks, ProbeVec4 const* a_snapshot) noexcept
        {
            Slot* const slot = find_or_insert(a_tid);
            if (!slot)
                return Outcome::e_no_slot;
            Outcome const outcome = slot->pending ? Outcome::e_overwritten : Outcome::e_stored;
            slot->tid = a_tid;
            slot->out = a_out;
            slot->behavior = a_behavior;
            slot->tracks = a_tracks;
            slot->pending = true;
            std::memcpy(slot->snapshot.data(), a_snapshot,
                static_cast<std::size_t>(a_tracks) * 2 * sizeof(ProbeVec4));
            return outcome;
        }

        // Returns the matching slot for this thread's apply entry, or nullptr when the pending
        // snapshot belongs to a different out block (or graph). The slot is NOT consumed here:
        // the same snapshot serves the B3 re-read and the B4 comparison, and it expires only by
        // the next 58423 return on the same thread.
        ProbeB3B4Slot const* match(std::uint32_t a_tid, void const* a_behavior) const noexcept
        {
            for (Slot const& slot : m_slots)
            {
                if (slot.tid == a_tid && slot.tid != 0 && slot.pending && slot.behavior == a_behavior)
                    return &slot;
            }
            return nullptr;
        }

        void reset() noexcept { m_slots = {}; m_overwritten = 0; }

        [[nodiscard]] std::uint64_t overwritten() const noexcept { return m_overwritten; }

    private:
        using Slot = ProbeB3B4Slot;

        Slot* find_or_insert(std::uint32_t a_tid) noexcept
        {
            for (Slot& slot : m_slots)
                if (slot.tid == a_tid && slot.tid != 0)
                    return &slot;
            for (Slot& slot : m_slots)
            {
                if (slot.tid == 0)
                {
                    slot.tid = a_tid;
                    return &slot;
                }
            }
            return nullptr;
        }

        std::array<Slot, Probe_B3B4_Max_Threads> m_slots{};
        std::uint64_t m_overwritten{ 0 };
    };

    // Reads `ctx->behavior` (+0x08) from an hkbContext built by 58315. The entry recorder passes
    // 58423's second argument; null or implausible values are reported as absent.
    [[nodiscard]] inline void const* probe_b3b4_context_behavior(void const* a_ctx) noexcept
    {
        if (a_ctx == nullptr || !plausible_object_pointer(reinterpret_cast<std::uintptr_t>(a_ctx)))
            return nullptr;
        auto const* const behavior = *reinterpret_cast<void const* const*>(
            static_cast<std::byte const*>(a_ctx) + 0x08);
        return plausible_object_pointer(reinterpret_cast<std::uintptr_t>(behavior)) ? behavior : nullptr;
    }
}
PLUGIN_NAMESPACE_END
