//
// Created by AmazingBuff on 2026/11/03.
//
#pragma once

#include "animation/behavior_graph/observation_chain_policy.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <optional>
#include <string_view>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // C-G5-E1: minimal liveness probe for the source sampling producer. It answers exactly three
    // questions audit29 left open - which thread runs the frame path, whether the pinned source
    // graph is really sampled (not only the candidate), and whether a queue consumer re-enters
    // sampling on another thread. It never compares poses and never produces a G5 verdict.

    inline constexpr std::size_t Probe_Liveness_Ring_Depth = 4096;
    inline constexpr std::uint32_t Probe_Liveness_Detail_Budget = 2048;
    inline constexpr std::chrono::seconds Probe_Liveness_Window{ 20 };

    // Pre-registered head bytes, verified byte for byte against the fixed AE 1.6.1170 image
    // (SHA-256 843823c1...): 63589 begins with three pushes (5-byte boundary, no padding needed),
    // 63856 begins with `sub rsp,0x88` (7-byte boundary, so the 5-byte jmp carries two 0x90 pads).
    inline constexpr std::array<std::uint8_t, 5> Probe_Sample_Head_Bytes{ 0x40, 0x56, 0x57, 0x41, 0x56 };
    inline constexpr std::array<std::uint8_t, 7> Probe_Pose_Head_Bytes{ 0x48, 0x81, 0xEC, 0x88, 0x00, 0x00, 0x00 };

    // 63358 calls 63589 from 0xba3f6e (its local per-graph loop). Any other return address inside
    // the game means a path the audit has not pinned; a return address inside this module is the
    // plugin's own GenerationEntryID/SamplingBranchID call and must never count as source liveness.
    inline constexpr std::uint32_t Probe_Engine_Local_Loop_Caller_Rva = 0xba3f6e;

    // 63589 and 63568 both pass `&graph+0x160` to 63856 (0xbb2af7 and 0xbaebd8), so the second
    // argument names the graph exactly; no pointer arithmetic guess is involved.
    inline constexpr std::uintptr_t Probe_Table_Argument_Delta = 0x160;

    enum class ProbeLivenessMode : std::uint8_t
    {
        e_off,
        e_on,
        e_concurrent
    };

    // Unknown text yields nullopt so a typo cannot arm a detour; the caller logs the observed
    // string and falls back to off. Boolean spellings keep the historical meaning.
    [[nodiscard]] inline std::optional<ProbeLivenessMode> parse_probe_liveness(std::string_view a_text) noexcept
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
        if (equal_ci(a_text, "concurrent")) return ProbeLivenessMode::e_concurrent;
        if (equal_ci(a_text, "on") || equal_ci(a_text, "true") || equal_ci(a_text, "1"))
            return ProbeLivenessMode::e_on;
        if (equal_ci(a_text, "off") || equal_ci(a_text, "false") || equal_ci(a_text, "0") || a_text.empty())
            return ProbeLivenessMode::e_off;
        return std::nullopt;
    }

    [[nodiscard]] inline char const* probe_liveness_mode_name(ProbeLivenessMode a_mode) noexcept
    {
        switch (a_mode)
        {
        case ProbeLivenessMode::e_on: return "on";
        case ProbeLivenessMode::e_concurrent: return "concurrent";
        default: return "off";
        }
    }

    // Builds the detour thunk: save every volatile register and the flags, reserve the shadow
    // space, pass the entry arguments to the recorder, run the relocated head and jump back into
    // the patched function.
    //
    // The layout is pinned by tests/probe_liveness_test.cpp because the previous generation of
    // thunks (source_event_capture / source_write_capture before C-G5-E1) read the saved entry
    // arguments one slot too low - they were placed before the `sub rsp,0x20` while their offsets
    // assumed it had already happened - so every recorder received the caller's rax as `this`.
    // After the pushes, `pushfq` and the 0x20 shadow space, the saved entry rcx/rdx/r8 sit at
    // +0x50/+0x48/+0x40 and the return address at +0x68; eight pushes plus pushfq leave rsp
    // 16-byte aligned for the call.
    //
    // a_pose_arguments == false passes (entry rcx, return address); == true passes
    // (entry rcx, entry rdx, entry r8).
    inline std::size_t encode_detour_thunk(std::uint8_t* a_dst, std::uintptr_t a_recorder,
        std::uintptr_t a_original, bool a_pose_arguments)
    {
        std::size_t o = 0;
        auto const emit = [&o, a_dst](std::initializer_list<std::uint8_t> const& a_bytes) {
            for (std::uint8_t b : a_bytes)
                a_dst[o++] = b;
        };
        emit({ 0x50, 0x53, 0x51, 0x52 });                          // push rax/rbx/rcx/rdx
        emit({ 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53 });  // push r8..r11
        emit({ 0x9C });                                            // pushfq
        emit({ 0x48, 0x83, 0xEC, 0x20 });                          // sub rsp,20h
        emit({ 0x48, 0x8B, 0x4C, 0x24, 0x50 });                    // mov rcx,[rsp+50h] (entry rcx)
        if (a_pose_arguments)
        {
            emit({ 0x48, 0x8B, 0x54, 0x24, 0x48 });                // mov rdx,[rsp+48h] (entry rdx)
            emit({ 0x44, 0x8B, 0x44, 0x24, 0x40 });                // mov r8d,[rsp+40h] (entry r8)
        }
        else
        {
            emit({ 0x48, 0x8B, 0x54, 0x24, 0x68 });                // mov rdx,[rsp+68h] (return addr)
        }
        emit({ 0x48, 0xB8 });                                      // movabs rax, recorder
        std::memcpy(a_dst + o, &a_recorder, sizeof(a_recorder));
        o += sizeof(a_recorder);
        emit({ 0xFF, 0xD0 });                                      // call rax
        emit({ 0x48, 0x83, 0xC4, 0x20 });                          // add rsp,20h
        emit({ 0x9D });                                            // popfq
        emit({ 0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58 });  // pop r11..r8
        emit({ 0x5A, 0x59, 0x5B, 0x58 });                          // pop rdx/rcx/rbx/rax
        emit({ 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 });              // jmp qword [rip+0]
        std::memcpy(a_dst + o, &a_original, sizeof(a_original));
        o += sizeof(a_original);
        return o;
    }

    enum class ProbeCaller : std::uint8_t
    {
        e_engine_local_loop,
        e_plugin,
        e_other
    };

    // Classifies a return address. `a_game_base` is the runtime module base, so the engine site is
    // matched as an RVA rather than an absolute address.
    [[nodiscard]] inline ProbeCaller classify_probe_caller(std::uintptr_t a_caller, std::uintptr_t a_game_base,
        std::uintptr_t a_plugin_begin, std::uintptr_t a_plugin_end) noexcept
    {
        if (a_caller == 0)
            return ProbeCaller::e_other;
        if (a_game_base != 0 && a_caller >= a_game_base
            && a_caller - a_game_base == Probe_Engine_Local_Loop_Caller_Rva)
            return ProbeCaller::e_engine_local_loop;
        if (a_plugin_begin != 0 && a_caller >= a_plugin_begin && a_caller < a_plugin_end)
            return ProbeCaller::e_plugin;
        return ProbeCaller::e_other;
    }

    // 63856's second argument is `&graph+0x160`. Null, tiny and unaligned values are rejected with
    // the same plausibility rule the observation hardening uses, so a bogus call cannot produce a
    // graph pointer that later gets dereferenced.
    [[nodiscard]] inline void* probe_graph_from_table_argument(void* a_table) noexcept
    {
        auto const value = reinterpret_cast<std::uintptr_t>(a_table);
        if (!plausible_object_pointer(value))
            return nullptr;
        if (value < Probe_Table_Argument_Delta + 0x10000)
            return nullptr;
        return reinterpret_cast<void*>(value - Probe_Table_Argument_Delta);
    }

    struct ProbeRecord
    {
        std::uintptr_t graph;
        std::uint32_t caller_or_count;
        std::uint16_t num240;
        std::uint8_t kind;
        std::uint8_t flag246;
        std::uint32_t tid;
    };

    // Single producer (the engine's sampling thread), single consumer (the render drain's drain).
    // Overflow drops the newest record and counts it; nothing here blocks or allocates.
    class ProbeRing
    {
    public:
        bool push(ProbeRecord const& a_record)
        {
            std::size_t const head = m_head.load(std::memory_order_relaxed);
            std::size_t const tail = m_tail.load(std::memory_order_acquire);
            if (head - tail >= Probe_Liveness_Ring_Depth)
            {
                m_dropped.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            m_slots[head % Probe_Liveness_Ring_Depth] = a_record;
            m_head.store(head + 1, std::memory_order_release);
            return true;
        }

        bool pop(ProbeRecord& a_out)
        {
            std::size_t const tail = m_tail.load(std::memory_order_relaxed);
            std::size_t const head = m_head.load(std::memory_order_acquire);
            if (tail == head)
                return false;
            a_out = m_slots[tail % Probe_Liveness_Ring_Depth];
            m_tail.store(tail + 1, std::memory_order_release);
            return true;
        }

        void drain_discard()
        {
            m_tail.store(m_head.load(std::memory_order_acquire), std::memory_order_release);
        }

        // Only valid while no window is armed (the producer's gate is the window), which is the
        // state every call site resets from.
        void reset()
        {
            m_head.store(0, std::memory_order_relaxed);
            m_tail.store(0, std::memory_order_relaxed);
            m_dropped.store(0, std::memory_order_relaxed);
        }

        [[nodiscard]] std::uint64_t dropped() const { return m_dropped.load(std::memory_order_relaxed); }
        void reset_dropped() { m_dropped.store(0, std::memory_order_relaxed); }

    private:
        std::array<ProbeRecord, Probe_Liveness_Ring_Depth> m_slots{};
        std::atomic<std::size_t> m_head{ 0 };
        std::atomic<std::size_t> m_tail{ 0 };
        std::atomic<std::uint64_t> m_dropped{ 0 };
    };

    // Pairs each 63589 entry with the 63856 completion that follows it on the same thread. The
    // engine's per-graph loop is sequential, so "last entry for this thread" is the correct
    // attribution; a completion that does not match the pending entry is reported, never silently
    // dropped. This is pure bookkeeping and carries no engine dependency.
    class ProbePairing
    {
    public:
        static constexpr std::size_t Max_Threads = 8;

        enum class Outcome : std::uint8_t
        {
            e_paired,
            e_unattributed,
            e_graph_mismatch,
            e_no_slot
        };

        Outcome on_entry(std::uint32_t a_tid, void const* a_graph)
        {
            Slot* const slot = find_or_insert(a_tid);
            if (!slot)
                return Outcome::e_no_slot;
            slot->graph = a_graph;
            slot->pending = true;
            return Outcome::e_paired;
        }

        Outcome on_completion(std::uint32_t a_tid, void const* a_graph)
        {
            Slot* const slot = find(a_tid);
            if (!slot || !slot->pending)
                return Outcome::e_unattributed;
            if (slot->graph != a_graph)
                return Outcome::e_graph_mismatch;
            slot->pending = false;
            return Outcome::e_paired;
        }

        [[nodiscard]] std::size_t slots_used() const
        {
            std::size_t used = 0;
            for (Slot const& slot : m_slots)
                if (slot.tid != 0)
                    ++used;
            return used;
        }

        void reset() { m_slots = {}; }

    private:
        struct Slot
        {
            std::uint32_t tid;
            void const* graph;
            bool pending;
        };

        Slot* find(std::uint32_t a_tid)
        {
            for (Slot& slot : m_slots)
                if (slot.tid == a_tid)
                    return &slot;
            return nullptr;
        }

        Slot* find_or_insert(std::uint32_t a_tid)
        {
            if (Slot* const existing = find(a_tid))
                return existing;
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

        std::array<Slot, Max_Threads> m_slots{};
    };

    enum class ProbeWindowClose : std::uint8_t
    {
        e_none,
        e_display_close,
        e_expired,
        e_world_reset,
        e_abort,
        e_rearm
    };

    [[nodiscard]] inline char const* probe_window_close_name(ProbeWindowClose a_reason) noexcept
    {
        switch (a_reason)
        {
        case ProbeWindowClose::e_display_close: return "display-close";
        case ProbeWindowClose::e_expired: return "expired";
        case ProbeWindowClose::e_world_reset: return "world-reset";
        case ProbeWindowClose::e_abort: return "abort";
        case ProbeWindowClose::e_rearm: return "rearm";
        default: return "none";
        }
    }

    // One window per process: arm once, then expire/cancel. Re-arming after a close is refused so a
    // run cannot stitch two windows into one conclusion.
    class ProbeWindow
    {
    public:
        void arm(std::chrono::steady_clock::time_point a_now)
        {
            m_active = true;
            m_armed = true;
            m_deadline = a_now + Probe_Liveness_Window;
        }

        void cancel(ProbeWindowClose a_reason)
        {
            m_active = false;
            m_close = a_reason;
        }

        [[nodiscard]] bool expire(std::chrono::steady_clock::time_point a_now)
        {
            if (!m_active || a_now < m_deadline)
                return false;
            cancel(ProbeWindowClose::e_expired);
            return true;
        }

        // Deadline check without closing, so the caller can emit one close row with the counters
        // it just read.
        [[nodiscard]] bool due(std::chrono::steady_clock::time_point a_now) const
        {
            return m_active && a_now >= m_deadline;
        }

        [[nodiscard]] bool active() const { return m_active; }
        [[nodiscard]] bool ever_armed() const { return m_armed; }
        [[nodiscard]] ProbeWindowClose close_reason() const { return m_close; }

    private:
        bool m_active{ false };
        bool m_armed{ false };
        ProbeWindowClose m_close{ ProbeWindowClose::e_none };
        std::chrono::steady_clock::time_point m_deadline{};
    };
}
PLUGIN_NAMESPACE_END
