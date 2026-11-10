//
// Created by AmazingBuff on 2026/10/29.
//
#pragma once

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
    inline constexpr std::uint32_t Source_Timeline_Line_Budget = 800;
    inline constexpr std::chrono::seconds Source_Timeline_Window{ 20 };

    // C-G5-E3 (M1): the timeline gains a third state instead of the old boolean. `on` keeps the
    // G3-A lifecycle verbatim (F7 outside the panel, `player_graph` resolution); `concurrent` is
    // armed by the candidate experiment with the pinned source graph and closed at the display
    // boundary, so the source trajectory and the candidate steps share one window. The state
    // change is a value-domain extension: no new INI key.
    enum class SourceTimelineMode : std::uint8_t
    {
        e_off,
        e_on,
        e_concurrent
    };

    // Unknown text yields nullopt so a typo cannot arm a window by accident; the caller logs the
    // observed string and falls back to off. Boolean spellings keep the historical meaning.
    [[nodiscard]] inline std::optional<SourceTimelineMode> parse_source_timeline(std::string_view a_text) noexcept
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
        if (equal_ci(a_text, "concurrent")) return SourceTimelineMode::e_concurrent;
        if (equal_ci(a_text, "on") || equal_ci(a_text, "true") || equal_ci(a_text, "1"))
            return SourceTimelineMode::e_on;
        if (equal_ci(a_text, "off") || equal_ci(a_text, "false") || equal_ci(a_text, "0") || a_text.empty())
            return SourceTimelineMode::e_off;
        return std::nullopt;
    }

    [[nodiscard]] inline char const* source_timeline_mode_name(SourceTimelineMode a_mode) noexcept
    {
        switch (a_mode)
        {
        case SourceTimelineMode::e_on: return "on";
        case SourceTimelineMode::e_concurrent: return "concurrent";
        default: return "off";
        }
    }

    // C-G5-E3 v3: the first-person graph's project name is "FirstPerson", but every historical
    // resolver filtered on the lowercase substring "1stperson" with a case-sensitive strstr. The
    // 2026-11-10 census showed both graphs on the same actor — `DefaultFemale` 652/106 and
    // `FirstPerson` 463/36 — so the filter never fired and the `on` timeline, the event capture and
    // the write capture all watched the first-person graph. Match both spellings case-insensitively.
    [[nodiscard]] inline bool source_graph_is_first_person(std::string_view a_name) noexcept
    {
        auto contains_ci = [](std::string_view a_hay, std::string_view a_needle) {
            if (a_needle.empty() || a_hay.size() < a_needle.size())
                return false;
            for (std::size_t i = 0; i + a_needle.size() <= a_hay.size(); ++i)
            {
                bool match = true;
                for (std::size_t k = 0; k < a_needle.size(); ++k)
                {
                    char left = a_hay[i + k];
                    char right = a_needle[k];
                    if (left >= 'A' && left <= 'Z') left = static_cast<char>(left - 'A' + 'a');
                    if (right >= 'A' && right <= 'Z') right = static_cast<char>(right - 'A' + 'a');
                    if (left != right)
                    {
                        match = false;
                        break;
                    }
                }
                if (match)
                    return true;
            }
            return false;
        };
        return contains_ci(a_name, "firstperson") || contains_ci(a_name, "1stperson");
    }

    // C-G5-E3 v2 §八: one actor can own more than one managed non-1stperson graph (the `on`
    // resolver used to take the last, which was the wrong object for the pairing run: 463/36 vs the
    // candidate source's 652/106). The selector picks by project name; an empty selector returns
    // Source_Timeline_No_Pick, so the caller keeps its legacy last-match rule.
    inline constexpr std::size_t Source_Timeline_No_Pick = static_cast<std::size_t>(-1);

    [[nodiscard]] inline bool source_timeline_name_matches(std::string_view a_name,
        std::string_view a_selector) noexcept
    {
        if (a_selector.empty() || a_name.size() != a_selector.size())
            return false;
        for (std::size_t i = 0; i < a_name.size(); ++i)
        {
            char left = a_name[i];
            char right = a_selector[i];
            if (left >= 'A' && left <= 'Z') left = static_cast<char>(left - 'A' + 'a');
            if (right >= 'A' && right <= 'Z') right = static_cast<char>(right - 'A' + 'a');
            if (left != right)
                return false;
        }
        return true;
    }

    // First case-insensitive match wins. Returns Source_Timeline_No_Pick when the selector is empty
    // or nothing matches, so a typo can never silently select a different graph than intended.
    [[nodiscard]] inline std::size_t source_timeline_pick(std::string_view const* a_names,
        std::size_t a_count, std::string_view a_selector) noexcept
    {
        if (a_selector.empty() || a_names == nullptr)
            return Source_Timeline_No_Pick;
        for (std::size_t i = 0; i < a_count; ++i)
        {
            if (source_timeline_name_matches(a_names[i], a_selector))
                return i;
        }
        return Source_Timeline_No_Pick;
    }

    // C-G5-E3 (M2): every logged source settle publishes its TSC so the candidate probe can judge
    // "first candidate step at or after the settle confirmation" inside the same window. A settle
    // costs one slot; 64 is far above the settle count of a 20 s window, and overflow only drops
    // the ledger entry (counted), never the log line.
    inline constexpr std::size_t Source_Timeline_Settle_Ring = 64;

    class SourceSettleRing
    {
    public:
        [[nodiscard]] bool push(std::uint64_t a_tsc) noexcept
        {
            std::size_t const head = m_head.load(std::memory_order_relaxed);
            if (head - m_tail.load(std::memory_order_acquire) >= Source_Timeline_Settle_Ring)
            {
                m_dropped.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            m_slots[head % Source_Timeline_Settle_Ring] = a_tsc;
            m_head.store(head + 1, std::memory_order_release);
            return true;
        }

        [[nodiscard]] bool pop(std::uint64_t& a_out) noexcept
        {
            std::size_t const tail = m_tail.load(std::memory_order_relaxed);
            if (tail == m_head.load(std::memory_order_acquire))
                return false;
            a_out = m_slots[tail % Source_Timeline_Settle_Ring];
            m_tail.store(tail + 1, std::memory_order_release);
            return true;
        }

        [[nodiscard]] std::size_t size() const noexcept
        {
            return m_head.load(std::memory_order_acquire) - m_tail.load(std::memory_order_relaxed);
        }

        [[nodiscard]] std::uint64_t dropped() const noexcept { return m_dropped.load(std::memory_order_relaxed); }

        void reset() noexcept
        {
            m_head.store(0, std::memory_order_relaxed);
            m_tail.store(0, std::memory_order_relaxed);
            m_dropped.store(0, std::memory_order_relaxed);
        }

    private:
        std::array<std::uint64_t, Source_Timeline_Settle_Ring> m_slots{};
        std::atomic<std::size_t> m_head{ 0 };
        std::atomic<std::size_t> m_tail{ 0 };
        std::atomic<std::uint64_t> m_dropped{ 0 };
    };

    struct SourceTimelineIdentity
    {
        std::uintptr_t actor;
        std::uintptr_t graph;
        std::uintptr_t behavior;
        std::uintptr_t data;
        std::uintptr_t variable_set;
        std::uintptr_t words;
        std::size_t count;
    };

    enum class SourceTimelineIntent : std::uint8_t
    {
        e_none,
        e_arm,
        e_invalidate
    };

    // Callbacks publish intents only; invalidation wins until the sampler consumes it.
    class SourceTimelineRequests
    {
    public:
        SourceTimelineRequests() : m_pending(SourceTimelineIntent::e_none) {}

        void arm()
        {
            SourceTimelineIntent expected = SourceTimelineIntent::e_none;
            (void)m_pending.compare_exchange_strong(expected, SourceTimelineIntent::e_arm);
        }

        void invalidate() { m_pending.store(SourceTimelineIntent::e_invalidate); }
        [[nodiscard]] SourceTimelineIntent take() { return m_pending.exchange(SourceTimelineIntent::e_none); }

    private:
        std::atomic<SourceTimelineIntent> m_pending;
    };

    enum class SourceTimelineSample : std::uint8_t
    {
        e_baseline,
        e_compare,
        e_aborted
    };

    class SourceTimelineWindow
    {
    public:
        SourceTimelineWindow() : m_active(false), m_armed(false), m_exhausted(false), m_lines(0), m_deadline{}, m_source{} {}

        void arm(std::chrono::steady_clock::time_point now)
        {
            m_active = true;
            m_armed = true;
            m_exhausted = false;
            m_lines = 0;
            m_deadline = now + Source_Timeline_Window;
            m_source.reset();
        }

        void cancel() { m_active = false; }

        [[nodiscard]] bool expire(std::chrono::steady_clock::time_point now)
        {
            if (!m_active || now < m_deadline) return false;
            cancel();
            return true;
        }

        [[nodiscard]] SourceTimelineSample observe_source(SourceTimelineIdentity const& source)
        {
            if (!m_active) return SourceTimelineSample::e_aborted;
            if (!m_source)
            {
                m_source = source;
                return SourceTimelineSample::e_baseline;
            }
            SourceTimelineIdentity const& previous = *m_source;
            if (previous.actor != source.actor || previous.graph != source.graph ||
                previous.behavior != source.behavior || previous.data != source.data ||
                previous.variable_set != source.variable_set || previous.words != source.words ||
                previous.count != source.count)
            {
                cancel();
                return SourceTimelineSample::e_aborted;
            }
            return SourceTimelineSample::e_compare;
        }

        [[nodiscard]] bool take_line()
        {
            if (m_lines < Source_Timeline_Line_Budget)
            {
                ++m_lines;
                return true;
            }
            m_exhausted = true;
            return false;
        }

        [[nodiscard]] bool active() const { return m_active; }
        [[nodiscard]] bool ever_armed() const { return m_armed; }
        [[nodiscard]] bool exhausted() const { return m_exhausted; }
        [[nodiscard]] std::uint32_t lines() const { return m_lines; }

    private:
        bool m_active;
        bool m_armed;
        bool m_exhausted;
        std::uint32_t m_lines;
        std::chrono::steady_clock::time_point m_deadline;
        std::optional<SourceTimelineIdentity> m_source;
    };
}
PLUGIN_NAMESPACE_END
