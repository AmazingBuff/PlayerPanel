//
// C-G5-E2-B engine-free cases: the foot-IK out-block reader, the entry/return pairing, and the
// rewrite comparison over two snapshots of the same block.
//
// Build:
//   cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests\probe_liveness_e2b_test.cpp
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/probe_liveness_policy.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace test_plugin::animation::behavior_graph;

namespace
{
    constexpr std::size_t kTracks = 4;

    struct OutBlock
    {
        // Word +0x32 count, word +0x34 signed track offset; dword +4 is the >2 gate.
        std::array<std::byte, 0x40> head{};
        std::array<std::byte, kTracks * 0x30> tracks{};

        OutBlock()
        {
            std::int32_t const gate = 3;
            std::uint16_t const count = kTracks;
            std::int16_t const offset = static_cast<std::int16_t>(0x40);
            std::memcpy(head.data() + 4, &gate, sizeof(gate));
            std::memcpy(head.data() + 0x32, &count, sizeof(count));
            std::memcpy(head.data() + 0x34, &offset, sizeof(offset));
        }

        void set(std::uint32_t a_index, std::uint8_t a_segment, float a_value)
        {
            std::memcpy(tracks.data() + a_index * 0x30 + a_segment * sizeof(ProbeVec4)
                + (a_segment == 0 ? a_index % 3 * sizeof(float) : (a_index % 4) * sizeof(float)),
                &a_value, sizeof(a_value));
        }

        float get(std::uint32_t a_index, std::uint8_t a_segment, std::uint32_t a_component) const
        {
            float v = 0.0f;
            std::memcpy(&v, tracks.data() + a_index * 0x30 + a_segment * sizeof(ProbeVec4)
                + a_component * sizeof(float), sizeof(v));
            return v;
        }
    };

    void in_flight_pairs_entry_and_return_by_thread()
    {
        ProbeFootikInFlight ledger;
        ProbeFootikInFlight::Entry entry{};
        entry.out = reinterpret_cast<void const*>(0x1000);
        entry.tracks = kTracks;
        entry.tsc = 1;
        assert(ledger.on_entry(11, entry) == ProbeFootikInFlight::Outcome::e_stored);
        assert(ledger.in_flight() == 1);
        ProbeFootikInFlight::Entry out{};
        assert(!ledger.on_return(99, out));
        assert(ledger.on_return(11, out));
        assert(out.out == entry.out && out.tracks == kTracks);
        assert(ledger.in_flight() == 0);
        // Re-entry overwrites and counts.
        (void)ledger.on_entry(11, entry);
        assert(ledger.on_entry(11, entry) == ProbeFootikInFlight::Outcome::e_overwritten);
        assert(ledger.overwritten() == 1);
        ledger.reset();
        assert(ledger.in_flight() == 0 && ledger.overwritten() == 0);
    }

    void out_block_reader_maps_tracks_and_honours_the_gate()
    {
        OutBlock block;
        for (std::uint32_t i = 0; i < kTracks; ++i)
        {
            for (std::uint32_t c = 0; c < 3; ++c)
            {
                float const v = 0.1f + static_cast<float>(i) + c * 0.01f;
                std::memcpy(block.tracks.data() + i * 0x30 + c * sizeof(float), &v, sizeof(v));
            }
            for (std::uint32_t c = 0; c < 4; ++c)
            {
                float const v = 1.0f + static_cast<float>(i) + c * 0.01f;
                std::memcpy(block.tracks.data() + i * 0x30 + sizeof(ProbeVec4) + c * sizeof(float),
                    &v, sizeof(v));
            }
        }
        std::array<ProbeVec4, kTracks * 2> snap{};
        std::uint16_t count = 0;
        void const* slot_ptr = block.head.data();
        assert(probe_read_footik_out(&slot_ptr, snap.data(), kTracks, count));
        assert(snap[0].x == 0.1f);
        // snap layout: [track*2] = trans, [track*2+1] = rot. snap[1] is track 0's rot,
        // snap[3] is track 1's rot; track 1's trans lives in snap[2].
        assert(snap[2].z == 0.1f + 1.0f + 0.02f);
        assert(snap[3].w == 1.0f + 1.0f + 0.03f);
        // A closed gate (count<=2) reports zero tracks without failing.
        std::memcpy(block.head.data() + 4, "\002", 1);
        std::memcpy(block.head.data() + 5, "\0", 1);
        std::memcpy(block.head.data() + 6, "\0", 1);
        std::memcpy(block.head.data() + 7, "\0", 1);
        void const* slot_ptr2 = block.head.data();
        assert(probe_read_footik_out(&slot_ptr2, snap.data(), kTracks, count));
    }

    void rewrite_comparison_finds_a_single_changed_component()
    {
        std::array<ProbeVec4, kTracks * 2> a{};
        std::array<ProbeVec4, kTracks * 2> b{};
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            a[i] = { 1.0f, 2.0f, 3.0f, 4.0f };
            b[i] = a[i];
        }
        // One component beyond both bounds: 1% of 2.0 is 0.02.
        b[3].y = 2.02f;
        std::uint32_t rewritten = 0;
        float worst = 0.0f;
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            float const ea[4] = { a[i].x, a[i].y, a[i].z, a[i].w };
            float const eb[4] = { b[i].x, b[i].y, b[i].z, b[i].w };
            for (std::uint8_t c = 0; c < 4; ++c)
            {
                float const delta = probe_mix_delta(ea[c], eb[c]);
                if (delta != 0.0f)
                {
                    ++rewritten;
                    if (delta > worst)
                        worst = delta;
                }
            }
        }
        assert(rewritten == 1);
        // 0.5f + 0.015f quantises, so compare with the same tolerance shape as the product.
        assert(worst > 0.019f && worst < 0.021f);
    }
}

int main()
{
    in_flight_pairs_entry_and_return_by_thread();
    out_block_reader_maps_tracks_and_honours_the_gate();
    rewrite_comparison_finds_a_single_changed_component();
    std::puts("3 probe-liveness e2b cases passed (engine-free)");
}
