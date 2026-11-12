//
// C-G5-E2-B B3/B4 engine-free cases: the per-thread return-snapshot ledger, the phase-split
// component comparison, the ctx->behavior accessor and the tally counters.
//
// Build:
//   cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests\probe_liveness_b3b4_test.cpp
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/b3b4_policy.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>

#ifdef _WIN32
#include <Windows.h>
#endif

using namespace test_plugin::animation::behavior_graph;

namespace
{
    constexpr std::size_t kTracks = 4;

    void fill(ProbeVec4* a_tracks, float a_seed)
    {
        for (std::size_t i = 0; i < kTracks * 2; ++i)
        {
            float const value = a_seed + static_cast<float>(i) * 0.01f;
            a_tracks[i] = { value, value + 0.1f, value + 0.2f, value + 0.3f };
        }
    }

    void ledger_stores_one_return_per_thread_and_matches_behavior()
    {
        ProbeB3B4Ledger ledger;
        std::array<ProbeVec4, kTracks * 2> snap{};
        fill(snap.data(), 1.0f);
        void const* const out = reinterpret_cast<void const*>(0x1000);
        void const* const graph = reinterpret_cast<void const*>(0x2000);
        assert(ledger.on_return(7, out, graph, kTracks, snap.data())
            == ProbeB3B4Ledger::Outcome::e_stored);
        assert(ledger.match(7, graph) != nullptr);
        assert(ledger.match(7, reinterpret_cast<void const*>(0x3000)) == nullptr);
        assert(ledger.match(8, graph) == nullptr);
        // A second return on the same thread overwrites and counts.
        assert(ledger.on_return(7, out, graph, kTracks, snap.data())
            == ProbeB3B4Ledger::Outcome::e_overwritten);
        ledger.reset();
        assert(ledger.match(7, graph) == nullptr);
        assert(ledger.overwritten() == 0);
    }

    void tally_splits_phases_and_keeps_samples()
    {
        ProbeB3B4Tally tally;
        std::array<ProbeVec4, kTracks * 2> expected{};
        std::array<ProbeVec4, kTracks * 2> actual{};
        fill(expected.data(), 1.0f);
        fill(actual.data(), 1.0f);
        assert(tally.compare(0, kTracks, expected.data(), actual.data()) == 0);
        assert(tally.b3_checked == 1 && tally.b3_equal == 1 && tally.b3_failed == 0);
        assert(tally.b4_checked == 0);
        // One component beyond both bounds: vec 1's y is 1.11, so 2.02 is a 0.91 drift.
        actual[1].y = 2.02f;
        assert(tally.compare(1, kTracks, expected.data(), actual.data()) == 1);
        assert(tally.b4_checked == 1 && tally.b4_failed == 1);
        assert(tally.sample_count == 1);
        assert(tally.samples[0].track == 0 && tally.samples[0].channel == Probe_Mix_Channel_Rot_X + 1);
        assert(tally.b4_max_delta > 0.90f && tally.b4_max_delta < 0.92f);
        tally.reset();
        assert(tally.b3_checked == 0 && tally.b4_checked == 0 && tally.sample_count == 0);
    }

    void tally_flags_non_finite()
    {
        ProbeB3B4Tally tally;
        std::array<ProbeVec4, 2> expected{};
        std::array<ProbeVec4, 2> actual{};
        // fill() writes kTracks*2 entries; these two-entry arrays are filled manually so the
        // compare below reads exactly what was written, never past the end.
        for (std::size_t i = 0; i < expected.size(); ++i)
        {
            float const v = 1.0f + static_cast<float>(i) * 0.01f;
            expected[i] = { v, v + 0.1f, v + 0.2f, v + 0.3f };
            actual[i] = expected[i];
        }
        actual[0].z = std::numeric_limits<float>::quiet_NaN();
        (void)tally.compare(0, 1, expected.data(), actual.data());
        assert(tally.non_finite);
        assert(tally.b3_failed == 1);
    }

    void context_behavior_reads_plus_8()
    {
        alignas(16) std::array<std::byte, 0x50> ctx{};
        void const* const graph = reinterpret_cast<void const*>(0x40000);
        std::memcpy(ctx.data() + 0x00, &graph, sizeof(graph));
        assert(probe_b3b4_context_character(ctx.data()) == graph);
        assert(probe_b3b4_context_character(nullptr) == nullptr);
        // An implausible character pointer is reported absent, never dereferenced.
        void const* const bad = reinterpret_cast<void const*>(0x20);
        std::memcpy(ctx.data() + 0x00, &bad, sizeof(bad));
        assert(probe_b3b4_context_character(ctx.data()) == nullptr);
    }

#ifdef _WIN32
    void* g_thunk[4] = { nullptr, nullptr, nullptr, nullptr };

    void footik_recorder(void* a, void* b, void* c, std::uint32_t d) noexcept
    {
        g_thunk[0] = a;
        g_thunk[1] = b;
        g_thunk[2] = c;
        g_thunk[3] = reinterpret_cast<void*>(static_cast<std::uintptr_t>(d));
    }

    // The v97/v97a runs never reached B3/B4 because 58423's entry thunk forwarded only rcx and
    // the return address, so `a_ctx` was garbage. Execute the production bytes to prove all four
    // original argument slots reach the recorder before ever deploying again.
    void footik_thunk_forwards_four_arguments()
    {
        auto* const ret = static_cast<std::uint8_t*>(
            VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        assert(ret != nullptr);
        *ret = 0xC3;
        FlushInstructionCache(GetCurrentProcess(), ret, 1);
        auto* const code = static_cast<std::uint8_t*>(
            VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        assert(code != nullptr);
        (void)encode_detour_footik_thunk(code, reinterpret_cast<std::uintptr_t>(&footik_recorder),
            reinterpret_cast<std::uintptr_t>(ret));
        FlushInstructionCache(GetCurrentProcess(), code, 4096);
        using quad_fn = void (*)(void*, void*, void*, std::uint32_t);
        void* const driver = reinterpret_cast<void*>(0x11110000);
        void* const ctx = reinterpret_cast<void*>(0x22220000);
        void* const out = reinterpret_cast<void*>(0x33330000);
        reinterpret_cast<quad_fn>(code)(driver, ctx, out, 7);
        assert(g_thunk[0] == driver);
        assert(g_thunk[1] == ctx);
        assert(g_thunk[2] == out);
        assert(g_thunk[3] == reinterpret_cast<void*>(7));
        (void)VirtualFree(code, 0, MEM_RELEASE);
        (void)VirtualFree(ret, 0, MEM_RELEASE);
    }
#endif
}

int main()
{
    ledger_stores_one_return_per_thread_and_matches_behavior();
    tally_splits_phases_and_keeps_samples();
    tally_flags_non_finite();
    context_behavior_reads_plus_8();
#ifdef _WIN32
    footik_thunk_forwards_four_arguments();
#endif
    std::puts("5 probe-liveness b3b4 cases passed (engine-free)");
}
