//
// C-G5-E2-B B3/B4: the source-graph post-process boundary (2026-11-12 pre-registration).
//
// B3: the 58423 return-side `out` block must still hold its return snapshot when the pinned
// graph's 63856 apply entry re-reads it (any component drift means a writer between 58423 and the
// memcpy path).
// B4: poseLocal (63856's first argument) must equal the same snapshot component-wise.
//
// The stored snapshot is keyed per thread on the out-block pointer. Only pairs whose
// `hkbContext+0x00` (character) equals the pinned graph's characterInstance are admitted.
// Audit fact (audit 210/220/222): 58315 builds the context with `character = &graph->characterInstance`
// and explicitly zeroes `ctx->behavior (+0x08)`, so `ctx->behavior == graph` can never hold. The
// first v97/v97a runs used that impossible identity and invalidated themselves. Stale, missing,
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
        // a_expected_stride / a_actual_stride are in vec4 units per track. The 58423 out snapshot
        // is [translation, rotation] (2), while poseLocal is an hkQsTransform array
        // [translation, rotation, scale] (3). v97c compared both at stride 2, so poseLocal track 1
        // read track 0's scale (1,1,1) — the exact 0/1 sample pattern of the v97b/v97c runs.
        std::uint32_t compare(std::uint8_t a_phase, std::uint32_t a_track_count,
            ProbeVec4 const* a_expected, std::uint32_t a_expected_stride,
            ProbeVec4 const* a_actual, std::uint32_t a_actual_stride) noexcept
        {
            std::uint32_t bad = 0;
            for (std::uint32_t track = 0; track < a_track_count; ++track)
            {
                for (std::uint8_t segment = 0; segment < 2; ++segment)
                {
                    ProbeVec4 const& expected = a_expected[track * a_expected_stride + segment];
                    ProbeVec4 const& actual = a_actual[track * a_actual_stride + segment];
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
        void const* character;
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

        Outcome on_return(std::uint32_t a_tid, void const* a_out, void const* a_character,
            std::uint16_t a_tracks, ProbeVec4 const* a_snapshot) noexcept
        {
            Slot* const slot = find_or_insert(a_tid);
            if (!slot)
                return Outcome::e_no_slot;
            Outcome const outcome = slot->pending ? Outcome::e_overwritten : Outcome::e_stored;
            slot->tid = a_tid;
            slot->out = a_out;
            slot->character = a_character;
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
        ProbeB3B4Slot const* match(std::uint32_t a_tid, void const* a_character) const noexcept
        {
            for (Slot const& slot : m_slots)
            {
                if (slot.tid == a_tid && slot.tid != 0 && slot.pending && slot.character == a_character)
                    return &slot;
            }
            return nullptr;
        }

        // v97c: consume the pending snapshot at the apply. Without this the slot stayed pending
        // forever, so every later 58423 return counted as `b3-multi` even when an apply had
        // already used the previous snapshot (v97b run: multi=1161 of 1167). Consumption makes
        // `multi` mean exactly "two returns before one apply", as the contract intends.
        ProbeB3B4Slot const* consume(std::uint32_t a_tid, void const* a_character) noexcept
        {
            for (Slot& slot : m_slots)
            {
                if (slot.tid == a_tid && slot.tid != 0 && slot.pending && slot.character == a_character)
                {
                    slot.pending = false;
                    return &slot;
                }
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

    // Reads `ctx->character` (+0x00) from an hkbContext built by 58315. The entry recorder passes
    // 58423's second argument; null or implausible values are reported as absent. The identity is
    // compared against `&pinned->characterInstance` (== pinned + 0xC0), never against the graph.
    [[nodiscard]] inline void const* probe_b3b4_context_character(void const* a_ctx) noexcept
    {
        if (a_ctx == nullptr || !plausible_object_pointer(reinterpret_cast<std::uintptr_t>(a_ctx)))
            return nullptr;
        auto const* const character = *reinterpret_cast<void const* const*>(a_ctx);
        return plausible_object_pointer(reinterpret_cast<std::uintptr_t>(character)) ? character : nullptr;
    }
}
PLUGIN_NAMESPACE_END
