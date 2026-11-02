//
// Created by AmazingBuff on 2026/10/31.
//
#pragma once

#include "animation/behavior_graph/source_timeline_policy.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    inline constexpr std::uint32_t Source_Event_Line_Budget = 600;
    inline constexpr std::uint32_t Source_Phase_Line_Budget = 200;
    inline constexpr std::chrono::seconds Source_Event_Window{ 20 };
    inline constexpr std::size_t Source_Event_Ring_Depth = 1024;

    // C-G4-C1: three modes instead of E1's boolean. `on` keeps the E1 semantics verbatim
    // (panel-outside F7 window, requires IndependentGraph=off); `concurrent` is a separate
    // lifecycle that may coexist with the candidate experiment because the window is armed and
    // closed by the experiment/display path instead of by F7. Neither mode removes the other's
    // guards, and the two never share a window.
    enum class SourceEventCaptureMode : std::uint8_t
    {
        e_off,
        e_on,
        e_concurrent
    };

    // Unknown text yields nullopt so a typo can never arm a hook by accident; the caller logs the
    // observed string and falls back to off. Accepts the historical boolean spellings.
    [[nodiscard]] inline std::optional<SourceEventCaptureMode> parse_source_event_capture(
        std::string_view a_text) noexcept
    {
        auto equal_ci = [](std::string_view a_left, std::string_view a_right) {
            if (a_left.size() != a_right.size()) return false;
            for (std::size_t i = 0; i < a_left.size(); ++i)
            {
                char const l = a_left[i] >= 'A' && a_left[i] <= 'Z' ? static_cast<char>(a_left[i] - 'A' + 'a') : a_left[i];
                if (l != a_right[i]) return false;
            }
            return true;
        };
        if (equal_ci(a_text, "concurrent")) return SourceEventCaptureMode::e_concurrent;
        if (equal_ci(a_text, "on") || equal_ci(a_text, "true") || equal_ci(a_text, "1"))
            return SourceEventCaptureMode::e_on;
        if (equal_ci(a_text, "off") || equal_ci(a_text, "false") || equal_ci(a_text, "0") || a_text.empty())
            return SourceEventCaptureMode::e_off;
        return std::nullopt;
    }

    [[nodiscard]] inline char const* source_event_capture_mode_name(SourceEventCaptureMode a_mode) noexcept
    {
        switch (a_mode)
        {
        case SourceEventCaptureMode::e_on: return "on";
        case SourceEventCaptureMode::e_concurrent: return "concurrent";
        default: return "off";
        }
    }
    // C-G3B-E1: the seven head bytes the install step verifies before patching 58377 (dumped
    // from the Steamless-unpacked 1.6.1170 image, hash-checked against the audit record):
    // mov rax,rsp; mov [rax+8],rcx. Both instructions are RIP-independent and the boundary at
    // +7 leaves room for a 5-byte jmp plus two pads.
    inline constexpr std::array<std::uint8_t, 7> Source_Event_Head_Bytes{
        0x48, 0x8B, 0xC4, 0x48, 0x89, 0x48, 0x08 };

    struct SourceEventRecord
    {
        std::uint32_t id;
        std::uint8_t payload_set;
        std::uint8_t ctx_behavior_set;
    };

    // Single producer (the game thread dispatching graph events), single consumer (the render
    // hook's drain). Overflow drops the record and counts it; nothing here blocks or allocates.
    class SourceEventRing
    {
    public:
        bool push(SourceEventRecord const& a_record)
        {
            std::size_t const head = m_head.load(std::memory_order_relaxed);
            std::size_t const tail = m_tail.load(std::memory_order_acquire);
            if (head - tail >= Source_Event_Ring_Depth)
            {
                m_dropped.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            m_slots[head % Source_Event_Ring_Depth] = a_record;
            m_head.store(head + 1, std::memory_order_release);
            return true;
        }

        bool pop(SourceEventRecord& a_out)
        {
            std::size_t const tail = m_tail.load(std::memory_order_relaxed);
            std::size_t const head = m_head.load(std::memory_order_acquire);
            if (tail == head)
                return false;
            a_out = m_slots[tail % Source_Event_Ring_Depth];
            m_tail.store(tail + 1, std::memory_order_release);
            return true;
        }

        // Discards everything currently queued; a window opening calls this so no record from
        // outside the window can be mistaken for in-window evidence.
        void drain_discard()
        {
            m_tail.store(m_head.load(std::memory_order_acquire), std::memory_order_release);
        }

        [[nodiscard]] std::uint64_t dropped() const { return m_dropped.load(std::memory_order_relaxed); }
        void reset_dropped() { m_dropped.store(0, std::memory_order_relaxed); }

    private:
        std::array<SourceEventRecord, Source_Event_Ring_Depth> m_slots{};
        std::atomic<std::size_t> m_head{ 0 };
        std::atomic<std::size_t> m_tail{ 0 };
        std::atomic<std::uint64_t> m_dropped{ 0 };
    };

    enum class SourceEventBudget : std::uint8_t
    {
        e_none,
        e_event,
        e_phase
    };

    // The E1 window: one arm, a fixed deadline, two independent line budgets. Whichever budget
    // runs out first ends the window; cancel always wins.
    class SourceEventWindow
    {
    public:
        void arm(std::chrono::steady_clock::time_point a_now)
        {
            m_active = true;
            m_deadline = a_now + Source_Event_Window;
            m_event_lines = 0;
            m_phase_lines = 0;
            m_budget = SourceEventBudget::e_none;
        }

        void cancel() { m_active = false; }

        [[nodiscard]] bool expire(std::chrono::steady_clock::time_point a_now)
        {
            if (!m_active || a_now < m_deadline)
                return false;
            cancel();
            return true;
        }

        [[nodiscard]] bool take_event_line()
        {
            if (!m_active)
                return false;
            if (m_event_lines < Source_Event_Line_Budget)
            {
                ++m_event_lines;
                return true;
            }
            m_budget = SourceEventBudget::e_event;
            m_active = false;
            return false;
        }

        [[nodiscard]] bool take_phase_line()
        {
            if (!m_active)
                return false;
            if (m_phase_lines < Source_Phase_Line_Budget)
            {
                ++m_phase_lines;
                return true;
            }
            m_budget = SourceEventBudget::e_phase;
            m_active = false;
            return false;
        }

        [[nodiscard]] bool active() const { return m_active; }
        [[nodiscard]] SourceEventBudget exhausted_budget() const { return m_budget; }
        [[nodiscard]] std::uint32_t event_lines() const { return m_event_lines; }
        [[nodiscard]] std::uint32_t phase_lines() const { return m_phase_lines; }

    private:
        bool m_active{ false };
        std::uint32_t m_event_lines{ 0 };
        std::uint32_t m_phase_lines{ 0 };
        SourceEventBudget m_budget{ SourceEventBudget::e_none };
        std::chrono::steady_clock::time_point m_deadline{};
    };
}
PLUGIN_NAMESPACE_END
