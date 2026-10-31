//
// Created by AmazingBuff on 2026/10/31.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/source_write_capture_policy.h"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>

using test_plugin::animation::behavior_graph::Source_Write_Ring_Depth;
using test_plugin::animation::behavior_graph::SourceWriteGate;
using test_plugin::animation::behavior_graph::SourceWriteRecord;
using test_plugin::animation::behavior_graph::SourceWriteRing;
using test_plugin::animation::behavior_graph::SourceWriteWindow;

namespace
{
    void ring_copies_names_at_push_and_drops_on_overflow()
    {
        SourceWriteRing ring;
        // A temporary buffer models the setter caller's stack string: only the copy inside the
        // ring survives.
        char stack_name[64];
        std::memcpy(stack_name, "bEquipUnequip", 14);
        assert(ring.push(0x1234, 1, true, stack_name));
        std::memset(stack_name, 0, sizeof(stack_name));
        SourceWriteRecord record{};
        assert(ring.pop(record));
        assert(record.graph == 0x1234);
        assert(record.value == 1);
        assert(record.is_bool == 1);
        assert(record.name_len == 13);
        assert(std::string(record.name, record.name_len) == "bEquipUnequip");

        // Long names truncate at the 63-byte bound and stay NUL-terminated.
        char long_name[200];
        std::memset(long_name, 'x', sizeof(long_name));
        assert(ring.push(2, -3, false, long_name));
        assert(ring.pop(record));
        assert(record.name_len == 63);
        assert(record.name[63] == '\0');
        assert(record.value == -3);
        assert(record.is_bool == 0);

        // A null name degrades to an empty row instead of a fault.
        assert(ring.push(3, 7, true, nullptr));
        assert(ring.pop(record));
        assert(record.name_len == 0);
        assert(record.name[0] == '\0');

        // Overflow drops the arriving records and counts them. Three records were already
        // popped, so the ring is empty and a 2000-record flood loses 2000 - depth.
        for (int i = 0; i < 2000; ++i)
            (void)ring.push(4, i, true, "n");
        assert(ring.dropped() == 2000 - static_cast<int>(Source_Write_Ring_Depth));
        ring.drain_discard();
        assert(!ring.pop(record));
        ring.reset_dropped();
        assert(ring.dropped() == 0);
    }

    void gate_matches_only_armed_graphs()
    {
        SourceWriteGate gate;
        std::uintptr_t const graphs[] = { 0xA, 0xB, 0xC };
        assert(!gate.match(reinterpret_cast<void const*>(0xA)));
        gate.arm(graphs, 3);
        assert(gate.count() == 3);
        assert(gate.match(reinterpret_cast<void const*>(0xA)));
        assert(gate.match(reinterpret_cast<void const*>(0xB)));
        assert(gate.match(reinterpret_cast<void const*>(0xC)));
        assert(!gate.match(reinterpret_cast<void const*>(0xD)));
        gate.clear();
        assert(!gate.match(reinterpret_cast<void const*>(0xA)));
        // Counts above the internal cap are clamped, not overflowed.
        std::uintptr_t const too_many[] = { 0x1, 0x2, 0x3, 0x4, 0x5, 0x6 };
        gate.arm(too_many, 6);
        assert(gate.count() == SourceWriteGate::kMaxGraphs);
        assert(gate.match(reinterpret_cast<void const*>(0x4)));
        assert(!gate.match(reinterpret_cast<void const*>(0x5)));
    }

    void budgets_close_the_window()
    {
        SourceWriteWindow window;
        auto const now = std::chrono::steady_clock::now();
        window.arm(now);
        for (std::uint32_t i = 0; i < 300; ++i)
            assert(window.take_write_line());
        assert(!window.take_write_line());
        assert(!window.active());
        assert(!window.take_phase_line());

        window.arm(now);
        for (std::uint32_t i = 0; i < 200; ++i)
            assert(window.take_phase_line());
        assert(!window.take_phase_line());
        assert(!window.active());
        assert(window.exhausted_phase());

        window.arm(now);
        assert(window.expire(now + std::chrono::seconds(20)));
        assert(!window.active());
    }
}

int main()
{
    ring_copies_names_at_push_and_drops_on_overflow();
    gate_matches_only_armed_graphs();
    budgets_close_the_window();
    return 0;
}
