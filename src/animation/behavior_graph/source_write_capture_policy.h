//
// Created by AmazingBuff on 2026/10/31.
//
#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    inline constexpr std::uint32_t Source_Write_Line_Budget = 300;
    inline constexpr std::uint32_t Source_Write_Phase_Line_Budget = 200;
    inline constexpr std::chrono::seconds Source_Write_Window{ 20 };
    inline constexpr std::size_t Source_Write_Ring_Depth = 512;
    // C-G3B-E2: the seven head bytes both typed setters share at their entry (dumped from the
    // Steamless-unpacked 1.6.1170 image at the parsed library addresses): mov [rsp+10h],rbx.
    // Five bytes, RIP-independent, with the instruction boundary exactly at +5, so the 5-byte
    // jmp needs no padding and relocations exactly one instruction.
    inline constexpr std::array<std::uint8_t, 5> Source_Write_Head_Bytes{
        0x48, 0x89, 0x5C, 0x24, 0x10 };

    struct SourceWriteRecord
    {
        std::uintptr_t graph;
        std::int32_t value;
        std::uint8_t is_bool;
        std::uint8_t name_len;
        char name[64];
    };

    // The detour's gate: the player's non-first-person graphs discovered by the sampler's
    // baseline. Matching is up to four relaxed pointer compares behind one armed flag; the
    // window-open path drains the ring afterwards, so a record racing a re-arm at most lands
    // in the discarded prefix.
    class SourceWriteGate
    {
    public:
        static constexpr std::size_t kMaxGraphs = 4;

        void arm(std::uintptr_t const* a_graphs, std::size_t a_count)
        {
            std::size_t const stored = a_count < kMaxGraphs ? a_count : kMaxGraphs;
            for (std::size_t i = 0; i < stored; ++i)
                m_graphs[i].store(a_graphs[i], std::memory_order_release);
            m_count.store(stored, std::memory_order_release);
            m_armed.store(true, std::memory_order_release);
        }

        void clear() { m_armed.store(false, std::memory_order_release); }

        [[nodiscard]] bool match(void const* a_graph) const
        {
            if (!m_armed.load(std::memory_order_relaxed))
                return false;
            std::size_t const count = m_count.load(std::memory_order_acquire);
            std::uintptr_t const candidate = reinterpret_cast<std::uintptr_t>(a_graph);
            for (std::size_t i = 0; i < count; ++i)
                if (m_graphs[i].load(std::memory_order_relaxed) == candidate)
                    return true;
            return false;
        }

        [[nodiscard]] std::size_t count() const { return m_count.load(std::memory_order_acquire); }

    private:
        std::array<std::atomic<std::uintptr_t>, kMaxGraphs> m_graphs{};
        std::atomic<std::size_t> m_count{ 0 };
        std::atomic<bool> m_armed{ false };
    };

    // SPSC ring. The name is copied at push time because the setter caller's BSFixedString is
    // a stack temporary the render-side drain must not touch later; copies cap at 63 bytes and
    // stay NUL-terminated.
    class SourceWriteRing
    {
    public:
        bool push(std::uintptr_t a_graph, std::int32_t a_value, bool a_is_bool, char const* a_name)
        {
            std::size_t const head = m_head.load(std::memory_order_relaxed);
            std::size_t const tail = m_tail.load(std::memory_order_acquire);
            if (head - tail >= Source_Write_Ring_Depth)
            {
                m_dropped.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            SourceWriteRecord& slot = m_slots[head % Source_Write_Ring_Depth];
            slot.graph = a_graph;
            slot.value = a_value;
            slot.is_bool = a_is_bool ? std::uint8_t{ 1 } : std::uint8_t{ 0 };
            std::size_t len = 0;
            if (a_name)
            {
                while (len + 1 < sizeof(slot.name) && a_name[len] != '\0')
                    ++len;
                for (std::size_t i = 0; i < len; ++i)
                    slot.name[i] = a_name[i];
            }
            slot.name[len] = '\0';
            slot.name_len = static_cast<std::uint8_t>(len);
            m_head.store(head + 1, std::memory_order_release);
            return true;
        }

        bool pop(SourceWriteRecord& a_out)
        {
            std::size_t const tail = m_tail.load(std::memory_order_relaxed);
            std::size_t const head = m_head.load(std::memory_order_acquire);
            if (tail == head)
                return false;
            a_out = m_slots[tail % Source_Write_Ring_Depth];
            m_tail.store(tail + 1, std::memory_order_release);
            return true;
        }

        void drain_discard()
        {
            m_tail.store(m_head.load(std::memory_order_acquire), std::memory_order_release);
        }

        [[nodiscard]] std::uint64_t dropped() const { return m_dropped.load(std::memory_order_relaxed); }
        void reset_dropped() { m_dropped.store(0, std::memory_order_relaxed); }

    private:
        std::array<SourceWriteRecord, Source_Write_Ring_Depth> m_slots{};
        std::atomic<std::size_t> m_head{ 0 };
        std::atomic<std::size_t> m_tail{ 0 };
        std::atomic<std::uint64_t> m_dropped{ 0 };
    };

    // Same two-budget window shape as the event capture, with E2's own budgets.
    class SourceWriteWindow
    {
    public:
        void arm(std::chrono::steady_clock::time_point a_now)
        {
            m_active = true;
            m_deadline = a_now + Source_Write_Window;
            m_write_lines = 0;
            m_phase_lines = 0;
            m_exhausted_phase = false;
        }

        void cancel() { m_active = false; }

        [[nodiscard]] bool expire(std::chrono::steady_clock::time_point a_now)
        {
            if (!m_active || a_now < m_deadline)
                return false;
            cancel();
            return true;
        }

        [[nodiscard]] bool take_write_line()
        {
            if (!m_active)
                return false;
            if (m_write_lines < Source_Write_Line_Budget)
            {
                ++m_write_lines;
                return true;
            }
            m_active = false;
            return false;
        }

        [[nodiscard]] bool take_phase_line()
        {
            if (!m_active)
                return false;
            if (m_phase_lines < Source_Write_Phase_Line_Budget)
            {
                ++m_phase_lines;
                return true;
            }
            m_exhausted_phase = true;
            m_active = false;
            return false;
        }

        [[nodiscard]] bool active() const { return m_active; }
        [[nodiscard]] bool exhausted_phase() const { return m_exhausted_phase; }
        [[nodiscard]] std::uint32_t write_lines() const { return m_write_lines; }
        [[nodiscard]] std::uint32_t phase_lines() const { return m_phase_lines; }

    private:
        bool m_active{ false };
        bool m_exhausted_phase{ false };
        std::uint32_t m_write_lines{ 0 };
        std::uint32_t m_phase_lines{ 0 };
        std::chrono::steady_clock::time_point m_deadline{};
    };
}
PLUGIN_NAMESPACE_END
