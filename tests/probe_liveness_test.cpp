//
// C-G5-E1 engine-free cases: mode parsing, caller classification, graph recovery from 63856's
// table argument, the thread/reuse pairing ledger, the one-window state machine, the SPSC ring,
// and - the case that matters most - the detour thunk's argument slots.
//
// The thunk case exists because the previous thunk generation (source_event_capture /
// source_write_capture before this probe) read the saved entry arguments one slot too low, so
// every recorder received the caller's rax as `this`. It builds the production bytes from
// probe_liveness_policy.h, executes them against a recorder and checks what arrives.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/probe_liveness_policy.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>

using test_plugin::animation::behavior_graph::classify_probe_caller;
using test_plugin::animation::behavior_graph::encode_detour_thunk;
using test_plugin::animation::behavior_graph::parse_probe_liveness;
using test_plugin::animation::behavior_graph::probe_graph_from_table_argument;
using test_plugin::animation::behavior_graph::ProbeCaller;
using test_plugin::animation::behavior_graph::ProbeLivenessMode;
using test_plugin::animation::behavior_graph::ProbePairing;
using test_plugin::animation::behavior_graph::ProbeRecord;
using test_plugin::animation::behavior_graph::ProbeRing;
using test_plugin::animation::behavior_graph::ProbeWindow;
using test_plugin::animation::behavior_graph::ProbeWindowClose;

namespace
{
    void mode_text_maps_to_three_states()
    {
        assert(parse_probe_liveness("off") == ProbeLivenessMode::e_off);
        assert(parse_probe_liveness("false") == ProbeLivenessMode::e_off);
        assert(parse_probe_liveness("0") == ProbeLivenessMode::e_off);
        assert(parse_probe_liveness("") == ProbeLivenessMode::e_off);
        assert(parse_probe_liveness("on") == ProbeLivenessMode::e_on);
        assert(parse_probe_liveness("TRUE") == ProbeLivenessMode::e_on);
        assert(parse_probe_liveness("1") == ProbeLivenessMode::e_on);
        assert(parse_probe_liveness("concurrent") == ProbeLivenessMode::e_concurrent);
        assert(parse_probe_liveness("Concurrent") == ProbeLivenessMode::e_concurrent);
        // A typo must never arm a detour.
        assert(!parse_probe_liveness("concurrentt").has_value());
        assert(!parse_probe_liveness("yes").has_value());
    }

    void caller_classification_prefers_engine_site_then_module()
    {
        constexpr std::uintptr_t game = 0x140000000;
        constexpr std::uintptr_t plugin = 0x7ff000000000;
        constexpr std::uintptr_t plugin_end = plugin + 0x200000;
        assert(classify_probe_caller(game + 0xba3f6e, game, plugin, plugin_end)
            == ProbeCaller::e_engine_local_loop);
        assert(classify_probe_caller(plugin + 0x1234, game, plugin, plugin_end) == ProbeCaller::e_plugin);
        assert(classify_probe_caller(game + 0xbb2aca, game, plugin, plugin_end) == ProbeCaller::e_other);
        assert(classify_probe_caller(0, game, plugin, plugin_end) == ProbeCaller::e_other);
    }

    void table_argument_recovers_graph_and_rejects_bad_pointers()
    {
        auto* const graph = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1'2345'6780));
        auto* const table = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(graph) + 0x160);
        assert(probe_graph_from_table_argument(table) == graph);
        assert(probe_graph_from_table_argument(nullptr) == nullptr);
        // Unaligned and tiny values are rejected instead of producing a dereferenceable pointer.
        assert(probe_graph_from_table_argument(reinterpret_cast<void*>(0x1'2345'6781)) == nullptr);
        assert(probe_graph_from_table_argument(reinterpret_cast<void*>(0x20)) == nullptr);
        assert(probe_graph_from_table_argument(reinterpret_cast<void*>(0x1000)) == nullptr);
    }

    void pairing_tracks_threads_and_reports_mismatch()
    {
        ProbePairing pairing;
        void* const a = reinterpret_cast<void*>(0x1000);
        void* const b = reinterpret_cast<void*>(0x2000);
        assert(pairing.on_entry(7, a) == ProbePairing::Outcome::e_paired);
        assert(pairing.on_completion(7, a) == ProbePairing::Outcome::e_paired);
        // A completion with no pending entry for that thread is unattributed, not a success.
        assert(pairing.on_completion(7, a) == ProbePairing::Outcome::e_unattributed);
        assert(pairing.on_entry(7, a) == ProbePairing::Outcome::e_paired);
        assert(pairing.on_completion(7, b) == ProbePairing::Outcome::e_graph_mismatch);
        // The pending entry is not consumed by a mismatch, so the correct completion still pairs.
        assert(pairing.on_completion(7, a) == ProbePairing::Outcome::e_paired);
        // Threads beyond the fixed table are reported instead of silently shared.
        for (std::uint32_t tid = 1; tid <= ProbePairing::Max_Threads; ++tid)
            (void)pairing.on_entry(100 + tid, a);
        assert(pairing.on_entry(999, a) == ProbePairing::Outcome::e_no_slot);
        pairing.reset();
        assert(pairing.slots_used() == 0);
    }

    void window_arms_once_and_expires_without_second_close()
    {
        ProbeWindow window;
        auto const now = std::chrono::steady_clock::now();
        assert(!window.active());
        assert(!window.ever_armed());
        window.arm(now);
        assert(window.active());
        assert(window.ever_armed());
        assert(!window.due(now + std::chrono::seconds(19)));
        assert(window.due(now + std::chrono::seconds(20)));
        assert(window.expire(now + std::chrono::seconds(20)));
        assert(!window.active());
        assert(window.close_reason() == ProbeWindowClose::e_expired);
        // A second expiry observation is not a second close.
        assert(!window.expire(now + std::chrono::seconds(40)));
        window.arm(now);
        window.cancel(ProbeWindowClose::e_display_close);
        assert(!window.active());
        assert(window.close_reason() == ProbeWindowClose::e_display_close);
    }

    void ring_preserves_order_and_reports_overflow()
    {
        ProbeRing ring;
        for (std::uint32_t i = 0; i < 5000; ++i)
        {
            ProbeRecord record{};
            record.kind = static_cast<std::uint8_t>(i & 1);
            record.caller_or_count = i;
            (void)ring.push(record);
        }
        // Depth 4096 with drop-newest: the producer never writes the consumer's tail.
        assert(ring.dropped() == 904);
        ProbeRecord record{};
        for (std::uint32_t i = 0; i < 4096; ++i)
        {
            assert(ring.pop(record));
            assert(record.caller_or_count == i);
        }
        assert(!ring.pop(record));
        ring.drain_discard();
        (void)ring.push(record);
        ring.drain_discard();
        assert(!ring.pop(record));
        ring.reset();
        assert(ring.dropped() == 0);
        assert(!ring.pop(record));
    }

#ifdef _WIN32
    void* g_a = nullptr;
    void* g_b = nullptr;
    void* g_c = nullptr;

    void thunk_recorder(void* a, void* b, void* c) noexcept
    {
        g_a = a;
        g_b = b;
        g_c = c;
    }

    void* executable_block(std::size_t a_size)
    {
        return VirtualAlloc(nullptr, a_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    }

    // Both variants end with the production far jump; the target here is a one-byte `ret` block,
    // so the thunk returns to the caller exactly like the patched function would after its head.
    void thunk_passes_entry_arguments_and_returns()
    {
        auto* const ret = static_cast<std::uint8_t*>(executable_block(4096));
        assert(ret != nullptr);
        *ret = 0xC3;
        FlushInstructionCache(GetCurrentProcess(), ret, 1);

        auto* const code = static_cast<std::uint8_t*>(executable_block(4096));
        assert(code != nullptr);

        // Sample variant: (entry rcx, return address).
        encode_detour_thunk(code, reinterpret_cast<std::uintptr_t>(&thunk_recorder),
            reinterpret_cast<std::uintptr_t>(ret), false);
        FlushInstructionCache(GetCurrentProcess(), code, 4096);
        using triple_fn = void (*)(void*, void*, void*);
        using pair_fn = void (*)(void*, void*);
        void* const graph = reinterpret_cast<void*>(0x1234);
        reinterpret_cast<pair_fn>(code)(graph, reinterpret_cast<void*>(0x9999));
        assert(g_a == graph);
        // The second argument must be this call's return address: inside this module, non-null and
        // not any of the values we passed in.
        assert(g_b != nullptr);
        assert(g_b != graph);
        assert(reinterpret_cast<std::uintptr_t>(g_b) > 0x10000);

        // Pose variant: (entry rcx, entry rdx, entry r8d).
        g_a = g_b = g_c = nullptr;
        encode_detour_thunk(code, reinterpret_cast<std::uintptr_t>(&thunk_recorder),
            reinterpret_cast<std::uintptr_t>(ret), true);
        FlushInstructionCache(GetCurrentProcess(), code, 4096);
        void* const pose = reinterpret_cast<void*>(0x1111);
        void* const table = reinterpret_cast<void*>(0x2222);
        void* const count = reinterpret_cast<void*>(0x3333);
        reinterpret_cast<triple_fn>(code)(pose, table, count);
        assert(g_a == pose);
        assert(g_b == table);
        assert(g_c == count);

        (void)VirtualFree(code, 0, MEM_RELEASE);
        (void)VirtualFree(ret, 0, MEM_RELEASE);
    }
#endif
}

int main()
{
    mode_text_maps_to_three_states();
    caller_classification_prefers_engine_site_then_module();
    table_argument_recovers_graph_and_rejects_bad_pointers();
    pairing_tracks_threads_and_reports_mismatch();
    window_arms_once_and_expires_without_second_close();
    ring_preserves_order_and_reports_overflow();
#ifdef _WIN32
    thunk_passes_entry_arguments_and_returns();
#endif
    std::printf("7 probe-liveness cases passed (engine-free)\n");
    return 0;
}
