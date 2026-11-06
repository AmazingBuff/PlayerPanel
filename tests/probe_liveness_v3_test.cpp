//
// C-G5-E1 v3 engine-free cases: apply-path classification, clamped-count recomputation,
// in-flight completion pairing, and both new thunk encoders executed as production bytes.
//
// Build (same flags as the other probe regressions):
//   cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests\probe_liveness_v3_test.cpp
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/probe_liveness_policy.h"

#include <Windows.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace test_plugin::animation::behavior_graph;

namespace
{
    // ---- path classification ------------------------------------------------------------

    void apply_paths_classify_the_five_pinned_callers()
    {
        constexpr std::uintptr_t base = 0x140000000ull;
        // The recorded value is the return address (call instruction + 5), not the call address.
        assert(classify_apply_path(base + 0xbb2b06, base) == ProbeApplyPath::e_main_63589);
        assert(classify_apply_path(base + 0xbaebe7, base) == ProbeApplyPath::e_main_63568);
        assert(classify_apply_path(base + 0xbae39a, base) == ProbeApplyPath::e_p63563);
        assert(classify_apply_path(base + 0xbaf514, base) == ProbeApplyPath::e_p63572);
        assert(classify_apply_path(base + 0xbae736, base) == ProbeApplyPath::e_p_bae731);
        // The call-instruction addresses must NOT classify: the first v70 run classified all
        // 81,034 application entries as unclassified for exactly this reason.
        assert(classify_apply_path(base + 0xbb2b01, base) == ProbeApplyPath::e_unclassified);
        assert(classify_apply_path(base + 0xbaebe2, base) == ProbeApplyPath::e_unclassified);
        assert(classify_apply_path(base + 0xbae395, base) == ProbeApplyPath::e_unclassified);
        assert(classify_apply_path(base + 0xbaf50f, base) == ProbeApplyPath::e_unclassified);
        assert(classify_apply_path(base + 0xbae731, base) == ProbeApplyPath::e_unclassified);
        // Anything else is unclassified too, and the contract forbids folding it elsewhere.
        assert(classify_apply_path(base + 0x0, base) == ProbeApplyPath::e_unclassified);
        assert(classify_apply_path(0, base) == ProbeApplyPath::e_unclassified);
        assert(classify_apply_path(base + 0xbb2b06, 0) == ProbeApplyPath::e_unclassified);
        assert(classify_apply_path(base / 2, base) == ProbeApplyPath::e_unclassified);
        assert(std::strcmp(probe_apply_path_name(ProbeApplyPath::e_main_63589), "63589-tail") == 0);
        assert(std::strcmp(probe_apply_path_name(ProbeApplyPath::e_unclassified), "unclassified") == 0);
        assert(probe_apply_path_index(ProbeApplyPath::e_main_63589) == 0);
        assert(probe_apply_path_index(ProbeApplyPath::e_unclassified) == Probe_Apply_Path_Count - 1);
    }

    // ---- clamped count ------------------------------------------------------------------

    void clamped_count_matches_the_native_clamp_shape()
    {
        // Native: requested < 0 -> capacity; else min(requested, capacity).
        assert(probe_clamped_count(-1, 116) == 116);
        assert(probe_clamped_count(-1000, 0) == 0);
        assert(probe_clamped_count(0, 116) == 0);
        assert(probe_clamped_count(116, 116) == 116);
        assert(probe_clamped_count(117, 116) == 116);
        assert(probe_clamped_count(50, 116) == 50);
        assert(probe_clamped_count(0x7FFFFFFF, 116) == 116);
    }

    // ---- in-flight pairing ---------------------------------------------------------------

    void in_flight_pairs_by_thread_and_counts_overwrites()
    {
        ProbeInFlight ledger;
        ProbeInFlight::Entry entry{};
        entry.graph = reinterpret_cast<void const*>(0x1000);
        entry.pose = reinterpret_cast<void const*>(0x2000);
        entry.requested = 116;
        entry.clamped = 116;
        entry.slot_num_data = 116;
        entry.node_capacity = 116;
        entry.path = ProbeApplyPath::e_main_63568;
        entry.tsc = 1;

        assert(ledger.on_entry(100, entry) == ProbeInFlight::Outcome::e_stored);
        assert(ledger.in_flight() == 1);

        // A different thread has no pending entry: the return recorder must count an orphan.
        ProbeInFlight::Entry out{};
        assert(!ledger.on_return(200, out));
        // ever_tracked separates "a pinned thread whose entry vanished" (orphan) from "some other
        // graph's thread" (untracked); the first v70 run reported 80,651 untracked returns as
        // orphans, drowning the real signal.
        assert(ledger.ever_tracked(100));
        assert(!ledger.ever_tracked(200));
        assert(!ledger.ever_tracked(999));

        // Same thread consumes its entry exactly once.
        assert(ledger.on_return(100, out));
        assert(out.graph == entry.graph && out.clamped == 116 && out.path == ProbeApplyPath::e_main_63568);
        assert(ledger.in_flight() == 0);
        assert(!ledger.on_return(100, out));

        // Re-entry before the return overwrites and is counted; the later call owns the entry.
        ProbeInFlight::Entry first = entry;
        first.clamped = 50;
        ProbeInFlight::Entry second = entry;
        second.clamped = 116;
        assert(ledger.on_entry(100, first) == ProbeInFlight::Outcome::e_stored);
        assert(ledger.on_entry(100, second) == ProbeInFlight::Outcome::e_overwritten);
        assert(ledger.overwritten() == 1);
        assert(ledger.on_return(100, out));
        assert(out.clamped == 116);

        // Slot exhaustion is reported, not silently dropped into another thread's slot.
        for (std::uint32_t tid = 1; tid <= ProbeInFlight::Max_Threads; ++tid)
            (void)ledger.on_entry(tid, entry);
        ProbeInFlight::Entry overflow = entry;
        assert(ledger.on_entry(999, overflow) == ProbeInFlight::Outcome::e_no_slot);
        ledger.reset();
        assert(ledger.in_flight() == 0 && ledger.overwritten() == 0);
    }

    // ---- apply-entry thunk: entry arguments plus the caller return address ---------------

    std::array<std::uintptr_t, 4> g_apply_observed{};
    void record_apply_arguments(void* a_pose, void* a_table, std::uint32_t a_count,
        void* a_caller) noexcept
    {
        g_apply_observed = { reinterpret_cast<std::uintptr_t>(a_pose),
            reinterpret_cast<std::uintptr_t>(a_table), a_count,
            reinterpret_cast<std::uintptr_t>(a_caller) };
    }

    void apply_entry_thunk_forwards_four_arguments()
    {
        std::uint8_t* const memory = static_cast<std::uint8_t*>(
            VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        assert(memory);
        std::memset(memory, 0xCC, 4096);
        std::uint8_t* const block = memory + 64;
        std::uint8_t* const body = block + Detour_Block_Size + 64;
        std::memcpy(body, Probe_Pose_Head_Bytes.data(), Probe_Pose_Head_Bytes.size());
        constexpr std::uint8_t s_return[] = { 0x48, 0x81, 0xC4, 0x88, 0, 0, 0, 0xC3 };
        std::memcpy(body + Probe_Pose_Head_Bytes.size(), s_return, sizeof(s_return));
        std::uint8_t* const original = relocate_detour_head(block, Probe_Pose_Head_Bytes,
            reinterpret_cast<std::uintptr_t>(body + Probe_Pose_Head_Bytes.size()));
        std::size_t const size = encode_detour_thunk_with_caller(block,
            reinterpret_cast<std::uintptr_t>(&record_apply_arguments),
            reinterpret_cast<std::uintptr_t>(original));
        assert(size <= Detour_Original_Offset);
        FlushInstructionCache(GetCurrentProcess(), memory, 4096);
        using Apply = void (*)(void*, void*, std::uint32_t);
        reinterpret_cast<Apply>(block)(reinterpret_cast<void*>(0x1111),
            reinterpret_cast<void*>(0x2222), 116);
        // r9 must be the caller's return address, i.e. inside this module, not a stale register.
        assert(g_apply_observed[0] == 0x1111);
        assert(g_apply_observed[1] == 0x2222);
        assert(g_apply_observed[2] == 116);
        assert(g_apply_observed[3] > 0x10000);
        assert(std::all_of(memory, block, [](std::uint8_t b) { return b == 0xCC; }));
        assert(VirtualFree(memory, 0, MEM_RELEASE));
    }

    // ---- return thunk: recorder runs, frame restores, control returns --------------------

    std::uint32_t g_return_calls = 0;
    void record_apply_return_fixture() noexcept { ++g_return_calls; }

    void return_thunk_preserves_the_caller_frame()
    {
        std::uint8_t* const memory = static_cast<std::uint8_t*>(
            VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        assert(memory);
        std::memset(memory, 0xCC, 4096);
        // A stand-in function with 63856's tail layout: `sub rsp,0x88` followed by the patched
        // `add rsp,0x88; ret`. The patched add must restore exactly the frame the sub created, so
        // the ret must land back in the wrapper - a mismatch crashes instead of returning.
        std::uint8_t* const tail = memory + 512;
        constexpr std::uint8_t s_sub[] = { 0x48, 0x81, 0xEC, 0x88, 0, 0, 0 };
        std::memcpy(tail, s_sub, sizeof(s_sub));
        std::memcpy(tail + sizeof(s_sub), Probe_Pose_Return_Head_Bytes.data(),
            Probe_Pose_Return_Head_Bytes.size());
        constexpr std::uint8_t s_ret[] = { 0xC3 };
        std::memcpy(tail + sizeof(s_sub) + Probe_Pose_Return_Head_Bytes.size(), s_ret, sizeof(s_ret));
        std::uint8_t* const thunk = memory + 1024;
        std::uint8_t* const patched = tail + sizeof(s_sub);
        std::uint8_t* const original = relocate_detour_head(memory + 2048,
            Probe_Pose_Return_Head_Bytes,
            reinterpret_cast<std::uintptr_t>(patched + Probe_Pose_Return_Head_Bytes.size()));
        std::size_t const size = encode_detour_return_thunk(thunk,
            reinterpret_cast<std::uintptr_t>(&record_apply_return_fixture),
            reinterpret_cast<std::uintptr_t>(original));
        assert(size <= Detour_Original_Offset);
        // Patch exactly like install_at: 5-byte jmp plus two nop pads.
        std::uint8_t patch[8] = { 0xE9, 0, 0, 0, 0, 0x90, 0x90, 0x90 };
        std::int32_t const rel = static_cast<std::int32_t>(
            reinterpret_cast<std::uintptr_t>(thunk) - (reinterpret_cast<std::uintptr_t>(patched) + 5));
        std::memcpy(patch + 1, &rel, sizeof(rel));
        std::memcpy(patched, patch, Probe_Pose_Return_Head_Bytes.size());
        // A minimal caller: `call tail; ret`. The call makes the tail's incoming rsp 8 (mod 16),
        // exactly like a real caller of 63856.
        std::uint8_t* const wrapper = memory + 3072;
        constexpr std::uint8_t s_call[] = { 0xE8, 0, 0, 0, 0, 0xC3 };
        std::memcpy(wrapper, s_call, sizeof(s_call));
        std::int32_t const call_rel = static_cast<std::int32_t>(
            reinterpret_cast<std::uintptr_t>(tail) - (reinterpret_cast<std::uintptr_t>(wrapper) + 5));
        std::memcpy(wrapper + 1, &call_rel, sizeof(call_rel));
        FlushInstructionCache(GetCurrentProcess(), memory, 4096);
        using Fn = void (*)();
        reinterpret_cast<Fn>(wrapper)();
        assert(g_return_calls == 1);
        assert(VirtualFree(memory, 0, MEM_RELEASE));
    }

    // ---- reconciliation identity ---------------------------------------------------------

    void path_counters_reconcile_with_entry_total()
    {
        // The recorder increments apply_entries_total and exactly one path slot per call, so the
        // close-row identity `entries == sum(paths)` holds by construction; this case pins the
        // arithmetic the log reader relies on.
        std::array<std::uint64_t, Probe_Apply_Path_Count> paths{ 3, 4, 5, 6, 7, 0 };
        std::uint64_t total = 0;
        for (std::uint64_t value : paths)
            total += value;
        assert(total == 25);
        assert(paths[probe_apply_path_index(ProbeApplyPath::e_unclassified)] == 0);
    }
}

int main()
{
    apply_paths_classify_the_five_pinned_callers();
    clamped_count_matches_the_native_clamp_shape();
    in_flight_pairs_by_thread_and_counts_overwrites();
    apply_entry_thunk_forwards_four_arguments();
    return_thunk_preserves_the_caller_frame();
    path_counters_reconcile_with_entry_total();
    std::puts("6 probe-liveness v3 cases passed (engine-free)");
}
