//
// Created by AmazingBuff on 2026/11/04.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/probe_liveness_policy.h"

#include <Windows.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <latch>
#include <thread>
#include <vector>

using namespace test_plugin::animation::behavior_graph;

namespace
{
    std::array<std::uintptr_t, 3> g_observed{};
    void record_arguments(void* a, void* b, std::uint32_t c) noexcept
    {
        g_observed = { reinterpret_cast<std::uintptr_t>(a), reinterpret_cast<std::uintptr_t>(b), c };
    }

    void two_owned_blocks_preserve_guards_and_forward_arguments()
    {
        std::uint8_t* const memory = static_cast<std::uint8_t*>(
            VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        assert(memory);
        std::memset(memory, 0xCC, 4096);
        std::uint8_t* const first = memory + 64;
        std::uint8_t* const second = first + Detour_Block_Size;
        std::uint8_t* const sample_body = first + Detour_Pool_Size + 64;
        std::uint8_t* const apply_body = sample_body + 32;
        std::memcpy(sample_body, Probe_Sample_Head_Bytes.data(), Probe_Sample_Head_Bytes.size());
        constexpr std::uint8_t s_sample_return[] = { 0x41, 0x5E, 0x5F, 0x5E, 0xC3 };
        std::memcpy(sample_body + Probe_Sample_Head_Bytes.size(), s_sample_return, sizeof(s_sample_return));
        std::memcpy(apply_body, Probe_Pose_Head_Bytes.data(), Probe_Pose_Head_Bytes.size());
        constexpr std::uint8_t s_apply_return[] = { 0x48, 0x81, 0xC4, 0x88, 0, 0, 0, 0xC3 };
        std::memcpy(apply_body + Probe_Pose_Head_Bytes.size(), s_apply_return, sizeof(s_apply_return));
        std::uint8_t* const sample_original = relocate_detour_head(first, Probe_Sample_Head_Bytes,
            reinterpret_cast<std::uintptr_t>(sample_body + Probe_Sample_Head_Bytes.size()));
        std::uint8_t* const apply_original = relocate_detour_head(second, Probe_Pose_Head_Bytes,
            reinterpret_cast<std::uintptr_t>(apply_body + Probe_Pose_Head_Bytes.size()));
        assert(sample_original >= first && sample_original + Probe_Sample_Head_Bytes.size() + 14 <= second);
        assert(apply_original >= second && apply_original + Probe_Pose_Head_Bytes.size() + 14 <= first + Detour_Pool_Size);
        std::size_t const sample_size = encode_detour_thunk(first,
            reinterpret_cast<std::uintptr_t>(&record_arguments), reinterpret_cast<std::uintptr_t>(sample_original), false);
        std::size_t const apply_size = encode_detour_thunk(second,
            reinterpret_cast<std::uintptr_t>(&record_arguments), reinterpret_cast<std::uintptr_t>(apply_original), true);
        assert(sample_size <= Detour_Original_Offset && apply_size <= Detour_Original_Offset);
        FlushInstructionCache(GetCurrentProcess(), memory, 4096);
        using Sample = void (*)(void*, void*);
        reinterpret_cast<Sample>(first)(reinterpret_cast<void*>(0x1234), nullptr);
        assert(g_observed[0] == 0x1234 && g_observed[1] > 0x10000);
        using Apply = void (*)(void*, void*, std::uint32_t);
        reinterpret_cast<Apply>(second)(reinterpret_cast<void*>(0x1111), reinterpret_cast<void*>(0x2222), 116);
        assert((g_observed == std::array<std::uintptr_t, 3>{ 0x1111, 0x2222, 116 }));
        assert(std::all_of(memory, first, [](std::uint8_t b) { return b == 0xCC; }));
        assert(std::all_of(first + Detour_Pool_Size, sample_body, [](std::uint8_t b) { return b == 0xCC; }));
        assert(VirtualFree(memory, 0, MEM_RELEASE));
    }

    void concurrent_transport_closes_after_the_last_admitted_record()
    {
        ProbePushGate push_gate;
        assert(push_gate.try_enter());
        assert(!push_gate.try_enter());
        assert(push_gate.contended() == 1);
        push_gate.leave();
        ProbeAdmission admission;
        ProbeRing ring;
        admission.open();
        std::latch start{ 6 };
        std::array<std::thread, 6> producers;
        for (std::uint32_t i = 0; i < producers.size(); ++i)
        {
            producers[i] = std::thread([&, i] {
                start.arrive_and_wait();
                for (std::uint32_t n = 0; n < 100; ++n)
                {
                    ProbeRecorderLease const lease(admission);
                    assert(lease);
                    ProbeRecord record{};
                    record.tid = i + 1;
                    record.graph = 0x10000 + i;
                    record.caller_or_count = i * 100 + n;
                    (void)ring.push(record);
                }
            });
        }
        for (std::thread& producer : producers) producer.join();
        std::latch admitted{ 1 };
        std::latch finish{ 1 };
        std::thread tail([&] {
            ProbeRecorderLease const lease(admission);
            assert(lease);
            admitted.count_down();
            finish.wait();
            ProbeRecord record{};
            record.tid = 7;
            record.graph = 0x20000;
            record.caller_or_count = 600;
            assert(ring.push(record));
        });
        admitted.wait();
        admission.close();
        assert(!admission.quiescent());
        assert(!admission.try_enter());
        finish.count_down();
        tail.join();
        assert(admission.quiescent());
        std::array<bool, 601> seen{};
        std::uint64_t drained = 0;
        ProbeRecord record{};
        while (ring.pop(record))
        {
            assert(record.caller_or_count < seen.size());
            assert(!seen[record.caller_or_count]);
            seen[record.caller_or_count] = true;
            ++drained;
        }
        assert(seen[600]);
        assert(drained + ring.dropped() == 601);
        assert(ring.contended() <= ring.dropped());
        assert(!admission.try_enter());
    }
}

int main()
{
    two_owned_blocks_preserve_guards_and_forward_arguments();
    concurrent_transport_closes_after_the_last_admitted_record();
    std::puts("2 probe-liveness repair cases passed (engine-free)");
}
