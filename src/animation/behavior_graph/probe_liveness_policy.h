//
// Created by AmazingBuff on 2026/11/03.
//
#pragma once

#include "animation/behavior_graph/observation_chain_policy.h"
#include "animation/behavior_graph/detour_block_policy.h"

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
    // Observes sampling/application entry liveness and observed thread identities only.
    // Neither entry is an output-completion boundary or a coherent-pose snapshot.

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
        std::uint16_t clamped;
        std::uint16_t capacity;
        std::uint16_t slot_num_data;
        std::uint8_t kind;
        std::uint8_t flag246;
        std::uint8_t path;
        std::uint32_t tid;
    };

    class ProbePushGate
    {
    public:
        bool try_enter() noexcept
        {
            if (!m_busy.test_and_set(std::memory_order_acquire))
                return true;
            m_contended.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        void leave() noexcept { m_busy.clear(std::memory_order_release); }
        std::uint64_t contended() const noexcept { return m_contended.load(std::memory_order_relaxed); }
        void reset() noexcept { m_contended.store(0, std::memory_order_relaxed); }

    private:
        std::atomic_flag m_busy{};
        std::atomic<std::uint64_t> m_contended{ 0 };
    };

    // One atomic admission state linearizes close against entry. Rejected callbacks touch only
    // this state. After close, acquire observation of zero accepted callbacks permits final drain.
    class ProbeAdmission
    {
    public:
        void open() noexcept { m_state.store(0, std::memory_order_release); }
        bool try_enter() noexcept
        {
            if ((m_state.load(std::memory_order_acquire) & Closed_Bit) != 0)
                return false;
            std::uint64_t const before = m_state.fetch_add(1, std::memory_order_acquire);
            if ((before & Closed_Bit) == 0)
                return true;
            leave();
            return false;
        }
        void leave() noexcept { m_state.fetch_sub(1, std::memory_order_release); }
        void close() noexcept { m_state.fetch_or(Closed_Bit, std::memory_order_acq_rel); }
        bool quiescent() const noexcept
        {
            return m_state.load(std::memory_order_acquire) == Closed_Bit;
        }

    private:
        static constexpr std::uint64_t Closed_Bit = std::uint64_t{ 1 } << 63;
        std::atomic<std::uint64_t> m_state{ Closed_Bit };
    };

    class ProbeRecorderLease
    {
    public:
        explicit ProbeRecorderLease(ProbeAdmission& a_admission) noexcept : m_ref_admission(a_admission),
            m_admitted(a_admission.try_enter()) {}
        ~ProbeRecorderLease() { if (m_admitted) m_ref_admission.leave(); }
        ProbeRecorderLease(ProbeRecorderLease const&) = delete;
        ProbeRecorderLease& operator=(ProbeRecorderLease const&) = delete;
        explicit operator bool() const noexcept { return m_admitted; }

    private:
        ProbeAdmission& m_ref_admission;
        bool m_admitted;
    };

    // Multiple producers make one try-only admission to push; contention/full both drop newest.
    // The sole consumer reads only release-published slots and releases tail after its copy.
    class ProbeRing
    {
    public:
        bool push(ProbeRecord const& a_record)
        {
            if (!m_producer.try_enter())
            {
                m_dropped.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            std::size_t const head = m_head.load(std::memory_order_relaxed);
            std::size_t const tail = m_tail.load(std::memory_order_acquire);
            if (head - tail >= Probe_Liveness_Ring_Depth)
            {
                m_dropped.fetch_add(1, std::memory_order_relaxed);
                m_producer.leave();
                return false;
            }
            m_slots[head % Probe_Liveness_Ring_Depth] = a_record;
            m_head.store(head + 1, std::memory_order_release);
            m_producer.leave();
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
            m_producer.reset();
        }

        [[nodiscard]] std::uint64_t contended() const { return m_producer.contended(); }
        [[nodiscard]] std::uint64_t dropped() const { return m_dropped.load(std::memory_order_relaxed); }
        void reset_dropped() { m_dropped.store(0, std::memory_order_relaxed); }

    private:
        ProbePushGate m_producer;
        std::array<ProbeRecord, Probe_Liveness_Ring_Depth> m_slots{};
        std::atomic<std::size_t> m_head{ 0 };
        std::atomic<std::size_t> m_tail{ 0 };
        std::atomic<std::uint64_t> m_dropped{ 0 };
    };

    // This ledger pairs two entry observations; it does not observe native return or completion.
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
            if (slot->pending)
                ++m_unpaired;
            slot->graph = a_graph;
            slot->pending = true;
            return Outcome::e_paired;
        }

        Outcome on_apply_entry(std::uint32_t a_tid, void const* a_graph)
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

        std::uint64_t unpaired_samples() const
        {
            std::uint64_t count = m_unpaired;
            for (Slot const& slot : m_slots)
                if (slot.pending) ++count;
            return count;
        }

        void reset() { m_slots = {}; m_unpaired = 0; }

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
        std::uint64_t m_unpaired{ 0 };
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

    // One window per process with two phases (C-G5-E1 v2). The `concurrent` window is armed by
    // the experiment, which necessarily happens inside the paused panel where the engine does not
    // run its sample loop; the first display close therefore *continues* the window into a resumed
    // phase instead of ending it, so the pinned source graph can be observed while the game runs
    // again. Re-arming after a close is refused so a run cannot stitch two windows together.
    enum class ProbePhase : std::uint8_t
    {
        e_idle,
        e_display,
        e_resumed
    };

    [[nodiscard]] inline char const* probe_phase_name(ProbePhase a_phase) noexcept
    {
        switch (a_phase)
        {
        case ProbePhase::e_display: return "display";
        case ProbePhase::e_resumed: return "resumed";
        default: return "idle";
        }
    }

    inline constexpr std::chrono::seconds Probe_Liveness_Resumed{ 10 };

    class ProbeWindow
    {
    public:
        void arm(std::chrono::steady_clock::time_point a_now)
        {
            m_phase = ProbePhase::e_display;
            m_armed = true;
            m_deadline = a_now + Probe_Liveness_Window;
        }

        // Display close -> resumed phase. Only the first call in a window succeeds, and only from
        // the display phase; the deadline restarts from `a_now`.
        [[nodiscard]] bool enter_resumed(std::chrono::steady_clock::time_point a_now)
        {
            if (m_phase != ProbePhase::e_display)
                return false;
            m_phase = ProbePhase::e_resumed;
            m_deadline = a_now + Probe_Liveness_Resumed;
            return true;
        }

        void cancel(ProbeWindowClose a_reason)
        {
            m_phase = ProbePhase::e_idle;
            m_close = a_reason;
        }

        [[nodiscard]] bool expire(std::chrono::steady_clock::time_point a_now)
        {
            if (m_phase == ProbePhase::e_idle || a_now < m_deadline)
                return false;
            cancel(ProbeWindowClose::e_expired);
            return true;
        }

        // Deadline check without closing, so the caller can emit one close row with the counters
        // it just read.
        [[nodiscard]] bool due(std::chrono::steady_clock::time_point a_now) const
        {
            return m_phase != ProbePhase::e_idle && a_now >= m_deadline;
        }

        [[nodiscard]] bool active() const { return m_phase != ProbePhase::e_idle; }
        [[nodiscard]] bool ever_armed() const { return m_armed; }
        [[nodiscard]] ProbePhase phase() const { return m_phase; }
        [[nodiscard]] ProbeWindowClose close_reason() const { return m_close; }

    private:
        ProbePhase m_phase{ ProbePhase::e_idle };
        bool m_armed{ false };
        ProbeWindowClose m_close{ ProbeWindowClose::e_none };
        std::chrono::steady_clock::time_point m_deadline{};
    };

    // ---- C-G5-E1 v3: return-side completion, apply-path classification, range facts ------

    // This probe owns three detour blocks (sampling entry, apply entry, apply return). The
    // shared c-g5-e1 pool constant stays at two hooks because the typed-setter probe uses it.
    inline constexpr std::size_t Probe_Liveness_Hook_Count = 3;
    inline constexpr std::size_t Probe_Liveness_Pool_Size = Probe_Liveness_Hook_Count * Detour_Block_Size;

    // 63856 has a single return site: its own `add rsp,0x88` at +0x24f from the entry
    // (0xbc322f). The only other exit-looking instruction (0xbc3086) is an intra-body jmp, and
    // the clamped-count<=0 early exit reaches the same add/ret pair. Patching this 7-byte window
    // is safe because no branch targets its interior (the only jump into the window is its first
    // byte, from 0xbc2ffc).
    inline constexpr std::uint32_t Probe_Pose_Return_Offset = 0x24f;
    inline constexpr std::array<std::uint8_t, 7> Probe_Pose_Return_Head_Bytes{ 0x48, 0x81, 0xC4, 0x88, 0x00, 0x00, 0x00 };

    // The five direct callers of 63856 image-wide (audit32). Only the first two are the per-frame
    // apply paths R1 covers; the other three must be attributed separately when they fire rather
    // than folded into a generic "other" bucket.
    enum class ProbeApplyPath : std::uint8_t
    {
        e_main_63589,
        e_main_63568,
        e_p63563,
        e_p63572,
        e_p_bae731,
        e_unclassified
    };

    inline constexpr std::size_t Probe_Apply_Path_Count = 6;

    [[nodiscard]] inline std::size_t probe_apply_path_index(ProbeApplyPath a_path) noexcept
    {
        return static_cast<std::size_t>(a_path);
    }

    [[nodiscard]] inline char const* probe_apply_path_name(ProbeApplyPath a_path) noexcept
    {
        switch (a_path)
        {
        case ProbeApplyPath::e_main_63589: return "63589-tail";
        case ProbeApplyPath::e_main_63568: return "63568-tail";
        case ProbeApplyPath::e_p63563: return "p63563";
        case ProbeApplyPath::e_p63572: return "p63572";
        case ProbeApplyPath::e_p_bae731: return "p_bae731";
        default: return "unclassified";
        }
    }

    [[nodiscard]] inline ProbeApplyPath classify_apply_path(std::uintptr_t a_caller,
        std::uintptr_t a_game_base) noexcept
    {
        if (a_caller == 0 || a_game_base == 0 || a_caller < a_game_base)
            return ProbeApplyPath::e_unclassified;
        switch (a_caller - a_game_base)
        {
        case 0xbb2b01: return ProbeApplyPath::e_main_63589;
        case 0xbaebe2: return ProbeApplyPath::e_main_63568;
        case 0xbae395: return ProbeApplyPath::e_p63563;
        case 0xbaf50f: return ProbeApplyPath::e_p63572;
        case 0xbae731: return ProbeApplyPath::e_p_bae731;
        default: return ProbeApplyPath::e_unclassified;
        }
    }

    // 63856 clamps the requested count against the node-table capacity before writing:
    // `requested < 0 ? capacity : min(requested, capacity)` (audit31, 0xbc2ff0..0xbc2ff7).
    // Recomputed at the entry so a completion record separates "applied" from "empty apply"
    // without reading volatile registers at the return site.
    [[nodiscard]] inline std::uint32_t probe_clamped_count(std::int32_t a_requested,
        std::uint32_t a_capacity) noexcept
    {
        if (a_requested < 0)
            return a_capacity;
        auto const requested = static_cast<std::uint32_t>(a_requested);
        return requested > a_capacity ? a_capacity : requested;
    }

    // Per-thread in-flight entry consumed by the return recorder. The audit does not prove 63856
    // cannot re-enter, so an overwritten pending entry is counted, never silently re-attributed to
    // the later call.
    class ProbeInFlight
    {
    public:
        static constexpr std::size_t Max_Threads = 8;

        struct Entry
        {
            void const* graph;
            void const* pose;
            std::uint32_t requested;
            std::uint32_t clamped;
            std::uint32_t slot_num_data;
            std::uint32_t node_capacity;
            ProbeApplyPath path;
            std::uint64_t tsc;
        };

        enum class Outcome : std::uint8_t
        {
            e_stored,
            e_overwritten,
            e_no_slot
        };

        Outcome on_entry(std::uint32_t a_tid, Entry const& a_entry) noexcept
        {
            Slot* const slot = find_or_insert(a_tid);
            if (!slot)
                return Outcome::e_no_slot;
            Outcome const outcome = slot->pending ? Outcome::e_overwritten : Outcome::e_stored;
            if (slot->pending)
                ++m_overwritten;
            slot->entry = a_entry;
            slot->pending = true;
            return outcome;
        }

        // False when the thread has no pending entry: the return recorder then counts an orphan
        // instead of inventing a completion.
        bool on_return(std::uint32_t a_tid, Entry& a_out) noexcept
        {
            Slot* const slot = find(a_tid);
            if (!slot || !slot->pending)
                return false;
            a_out = slot->entry;
            slot->pending = false;
            return true;
        }

        [[nodiscard]] std::size_t in_flight() const noexcept
        {
            std::size_t pending = 0;
            for (Slot const& slot : m_slots)
                if (slot.pending)
                    ++pending;
            return pending;
        }

        [[nodiscard]] std::uint64_t overwritten() const noexcept { return m_overwritten; }

        void reset() noexcept
        {
            m_slots = {};
            m_overwritten = 0;
        }

    private:
        struct Slot
        {
            std::uint32_t tid;
            bool pending;
            Entry entry;
        };

        Slot* find(std::uint32_t a_tid) noexcept
        {
            for (Slot& slot : m_slots)
                if (slot.tid == a_tid && slot.tid != 0)
                    return &slot;
            return nullptr;
        }

        Slot* find_or_insert(std::uint32_t a_tid) noexcept
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
        std::uint64_t m_overwritten{ 0 };
    };

    // Apply-entry thunk variant that also forwards the caller's return address as the fourth
    // argument (r9). The three entry arguments keep the audited +0x50/+0x48/+0x40 slots and the
    // caller's return address sits at +0x68, exactly like the sampled-entry thunk's second
    // argument; eight pushes plus pushfq plus the 0x20 shadow space keep rsp 16-byte aligned.
    inline std::size_t encode_detour_thunk_with_caller(std::uint8_t* a_dst, std::uintptr_t a_recorder,
        std::uintptr_t a_original)
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
        emit({ 0x48, 0x8B, 0x54, 0x24, 0x48 });                    // mov rdx,[rsp+48h] (entry rdx)
        emit({ 0x44, 0x8B, 0x44, 0x24, 0x40 });                    // mov r8d,[rsp+40h] (entry r8)
        emit({ 0x4C, 0x8B, 0x4C, 0x24, 0x68 });                    // mov r9,[rsp+68h] (return addr)
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

    // Return-side thunk. The function's own frame is still intact at the patched `add rsp,0x88`,
    // so rsp is 8 (mod 16) there; `sub rsp,28h` makes it 16-byte aligned for the recorder call,
    // and the relocated `add rsp,0x88` runs afterwards so the caller's frame is restored before
    // the jump to the original `ret`. Volatile registers and flags are not preserved on purpose:
    // nothing reads them after the return site.
    inline std::size_t encode_detour_return_thunk(std::uint8_t* a_dst, std::uintptr_t a_recorder,
        std::uintptr_t a_original)
    {
        std::size_t o = 0;
        auto const emit = [&o, a_dst](std::initializer_list<std::uint8_t> const& a_bytes) {
            for (std::uint8_t b : a_bytes)
                a_dst[o++] = b;
        };
        emit({ 0x48, 0x83, 0xEC, 0x28 });                          // sub rsp,28h
        emit({ 0x48, 0xB8 });                                      // movabs rax, recorder
        std::memcpy(a_dst + o, &a_recorder, sizeof(a_recorder));
        o += sizeof(a_recorder);
        emit({ 0xFF, 0xD0 });                                      // call rax
        emit({ 0x48, 0x83, 0xC4, 0x28 });                          // add rsp,28h
        emit({ 0xE9 });                                            // jmp original (rel32)
        auto const next = reinterpret_cast<std::uintptr_t>(a_dst) + o + sizeof(std::int32_t);
        std::int32_t const rel = static_cast<std::int32_t>(a_original - next);
        std::memcpy(a_dst + o, &rel, sizeof(rel));
        o += sizeof(rel);
        return o;
    }
}
PLUGIN_NAMESPACE_END
