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
#include <cmath>
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
    // NOT alignas(16): declaring the alignment made MSVC assume the source was aligned and emit
    // `movaps [rsp]`, which faults when the detour frame is not 16-aligned - the 0xC0000005 at
    // CharacterPanel.dll+0x53f9c seen three times on 2026-11-08. The copies are memcpy and the
    // struct keeps its natural 8-byte alignment, so the compiler emits movups.
    struct ProbeRecord
    {
        std::uintptr_t graph;
        std::uint32_t caller_or_count;
        std::uint32_t caller_rva;
        std::uint16_t num240;
        std::uint16_t clamped;
        std::uint16_t capacity;
        std::uint16_t slot_num_data;
        std::uint8_t kind;
        std::uint8_t flag246;
        std::uint8_t path;
        std::uint32_t tid;
        // C-G5-E2-A: the per-frame identity result carried by a mix-frame record.
        float weight;
        float max_delta;
        std::uint32_t mix_failed;
        // C-G5-E2-A diagnostics: the slot track counts actually read, so an n==0 frame is
        // explainable from the log instead of inferred (the v72 run reported zero comparisons
        // with no way to see why).
        std::uint16_t slot0_count;
        std::uint16_t slot1_count;
        // v77: the four 58423 gate bytes recorded per frame.
        std::uint16_t ik_count;
        // E2-A′ per-frame bone attribution, so a frame is readable even if the window is
        // interrupted before the close row (two sessions ended ~5 s after the transition).
        std::uint16_t mix_fail_foot;
        std::uint16_t mix_fail_other;
        std::uint8_t ik_mode;
        std::uint8_t ik_global;
        std::uint8_t ik_root;
    };

    // Guard the invariant that made ProbeRing::push compile to movaps: the record must fill whole
    // xmmwords and the element type must be 16-aligned, or the copy faults.
    // Copy helper: volatile byte-wise on purpose. Any whole-struct assignment or memcpy lets MSVC
    // vectorise the 64-byte copy into movaps [rsp], which faults when the detour frame is not
    // 16-aligned (the 0xC0000005 at CharacterPanel.dll+0x53f9c, three times on 2026-11-08).
    // 64 bytes per record and a few thousand records per session make the loop cost irrelevant.
    inline void copy_probe_record(ProbeRecord& a_dst, ProbeRecord const& a_src) noexcept
    {
        auto* const dst = reinterpret_cast<volatile std::uint8_t*>(&a_dst);
        auto const* const src = reinterpret_cast<std::uint8_t const*>(&a_src);
        for (std::size_t i = 0; i < sizeof(ProbeRecord); ++i)
            dst[i] = src[i];
    }

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
            // memcpy rather than a whole-struct assignment: MSVC vectorises the 64-byte copy into
            // movaps [rsp], which faults when the caller's frame is not 16-aligned. That is
            // exactly the 0xC0000005 at CharacterPanel.dll+0x53f9c seen twice on 2026-11-08, and
            // it survived an alignas(16) on the record because the faulting load is the source on
            // the stack, not the ring slot.
            copy_probe_record(m_slots[head % Probe_Liveness_Ring_Depth], a_record);
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
            copy_probe_record(a_out, m_slots[tail % Probe_Liveness_Ring_Depth]);
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

    // The five direct call sites image-wide are `0xbb2b01 / 0xbaebe2 / 0xbae395 / 0xbaf50f /
    // 0xbae731` in *call-instruction* terms (audit32 table). What the callee observes on the
    // stack is the *return address* — the instruction after the 5-byte rel32 call — so the
    // classifier must match call+5. Using the call address itself classified all 81,034
    // application entries as unclassified on the first v70 run (2026-11-05); the same +5 form
    // is what makes `0xba3f6e` the sampled-entry caller in audit29.
    inline constexpr std::array<std::uint32_t, 5> Probe_Apply_Return_Rvas{
        0xbb2b06,  // 63589 tail (call at 0xbb2b01)
        0xbaebe7,  // 63568 tail, generation path (call at 0xbaebe2)
        0xbae39a,  // 63563, GraphPhysicsStepListener route (call at 0xbae395)
        0xbaf514,  // 63572 (call at 0xbaf50f)
        0xbae736   // 0xbae706, no .pdata (call at 0xbae731)
    };

    [[nodiscard]] inline ProbeApplyPath classify_apply_path(std::uintptr_t a_caller,
        std::uintptr_t a_game_base) noexcept
    {
        if (a_caller == 0 || a_game_base == 0 || a_caller < a_game_base)
            return ProbeApplyPath::e_unclassified;
        switch (a_caller - a_game_base)
        {
        case 0xbb2b06: return ProbeApplyPath::e_main_63589;
        case 0xbaebe7: return ProbeApplyPath::e_main_63568;
        case 0xbae39a: return ProbeApplyPath::e_p63563;
        case 0xbaf514: return ProbeApplyPath::e_p63572;
        case 0xbae736: return ProbeApplyPath::e_p_bae731;
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

        // True when this thread has ever owned a tracked (pinned) entry. A return without a
        // pending entry is an orphan only for such threads: untracked threads belong to other
        // graphs, whose returns were never registered and must not be counted as orphans (the
        // first v70 run reported 80,651 of them and drowned the real signal).
        [[nodiscard]] bool ever_tracked(std::uint32_t a_tid) const noexcept
        {
            for (Slot const& slot : m_slots)
                if (slot.tid == a_tid && slot.tid != 0)
                    return true;
            return false;
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
    // ---- C-G5-E2-A: synthesised-pose identity (trans/rot of 64112) -------------------------

    // 64112 mixes in single precision and normalises with `rsqrtss` plus one Newton step, so a
    // component-wise absolute tolerance of 2e-6 covers the rou/trans rounding for unit-scale
    // quaternions and character-scale translations; components above 100 units fall back to a
    // relative 1e-5 so a large translation is not rejected by absolute error alone.
    inline constexpr float Probe_Mix_Abs_Tolerance = 2.0e-6f;
    inline constexpr float Probe_Mix_Rel_Tolerance = 1.0e-5f;
    inline constexpr float Probe_Mix_Large_Component = 100.0f;

    struct ProbeVec4
    {
        float x;
        float y;
        float z;
        float w;
    };

    [[nodiscard]] inline float probe_dot4(ProbeVec4 const& a_left, ProbeVec4 const& a_right) noexcept
    {
        return a_left.x * a_right.x + a_left.y * a_right.y + a_left.z * a_right.z + a_left.w * a_right.w;
    }

    // 64112's transform mix: `(1-w)*A + w*B` per component (the same association order the engine
    // uses, so the comparison is not biased by a different summation order).
    [[nodiscard]] inline ProbeVec4 probe_mix_trans(ProbeVec4 const& a_a, ProbeVec4 const& a_b,
        float a_weight) noexcept
    {
        float const rest = 1.0f - a_weight;
        return { rest * a_a.x + a_weight * a_b.x, rest * a_a.y + a_weight * a_b.y,
            rest * a_a.z + a_weight * a_b.z, rest * a_a.w + a_weight * a_b.w };
    }

    // 64112's rotation mix: sign-corrected nlerp, then `rsqrtss` + one Newton refinement, with the
    // zero-length result written as the zero vector (`cmpeqss`/`andnps` at 0xbd545d..0xbd5487).
    [[nodiscard]] inline ProbeVec4 probe_mix_rot(ProbeVec4 a_a, ProbeVec4 a_b, float a_weight) noexcept
    {
        if (probe_dot4(a_a, a_b) < 0.0f)
        {
            a_b.x = -a_b.x;
            a_b.y = -a_b.y;
            a_b.z = -a_b.z;
            a_b.w = -a_b.w;
        }
        ProbeVec4 const mixed = probe_mix_trans(a_a, a_b, a_weight);
        float const length_squared = probe_dot4(mixed, mixed);
        if (length_squared == 0.0f)
            return { 0.0f, 0.0f, 0.0f, 0.0f };
        float const inverse = 1.0f / std::sqrt(length_squared);
        return { mixed.x * inverse, mixed.y * inverse, mixed.z * inverse, mixed.w * inverse };
    }

    [[nodiscard]] inline float probe_mix_delta(float a_expected, float a_actual) noexcept
    {
        float const delta = std::fabs(a_expected - a_actual);
        float const magnitude = std::fmax(std::fabs(a_expected), std::fabs(a_actual));
        // Above 100 units the tolerance scales with magnitude (relative 1e-5); below it the fixed
        // absolute 2e-6 applies.
        if (magnitude > Probe_Mix_Large_Component)
            return delta > Probe_Mix_Rel_Tolerance * magnitude ? delta : 0.0f;
        return delta > Probe_Mix_Abs_Tolerance ? delta : 0.0f;
    }

    // One window's E2-A tally: how many frames/tracks were compared, how many components failed,
    // the worst delta, and the first few failures. The detour fills this; the close row prints it.
    // C-G5-E2-A′: which bone a residual lands on. The 58423 post-process is foot IK, so a residual
    // that only ever falls on the foot chain supports "identity holds except foot-IK correction",
    // while a residual on an arm/head/spine bone refutes the identity outright. Names come from
    // `characterInstance.setup->animationSkeleton->bones[i].name` and are matched case-insensitively
    // as substrings, which is how Havok names look ("NPC L Foot [LFoot]", "NPC L Calf [LClf]").
    [[nodiscard]] inline bool probe_bone_is_foot_related(char const* a_name) noexcept
    {
        if (a_name == nullptr || *a_name == '\0')
            return false;
        constexpr char const* kKeys[] = { "foot", "toe", "ankle", "calf", "thigh", "knee", "leg" };
        for (char const* key : kKeys)
        {
            std::size_t const key_length = std::strlen(key);
            for (char const* start = a_name; *start != '\0'; ++start)
            {
                std::size_t i = 0;
                for (; i < key_length; ++i)
                {
                    char const c = start[i];
                    if (c == '\0')
                        break;
                    char const lower = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
                    if (lower != key[i])
                        break;
                }
                if (i == key_length)
                    return true;
            }
        }
        return false;
    }

    inline constexpr std::size_t Probe_Bone_Name_Length = 40;

    struct ProbeBoneSample
    {
        std::array<char, Probe_Bone_Name_Length> name{};
        std::uint32_t count;
    };

    struct ProbeMixTally
    {
        static constexpr std::size_t Max_Samples = 4;

        struct Sample
        {
            std::uint32_t track;
            std::uint8_t channel;
            float expected;
            float actual;
            float delta;
        };

        std::uint64_t frames{ 0 };
        // Frames split by branch: the fallback branch never blends two slots, so a session may
        // contain zero main-path frames and that must be visible rather than inferred.
        std::uint64_t main_frames{ 0 };
        std::uint64_t fallback_frames{ 0 };
        std::uint64_t tracks{ 0 };
        std::uint64_t failed{ 0 };
        float max_delta{ 0.0f };
        std::array<Sample, Max_Samples> samples{};
        std::uint32_t sample_count{ 0 };
        bool weight_out_of_range{ false };
        bool non_finite{ false };
        bool read_failed{ false };
        // Failure attribution (v75): which slot a failing component is closer to, and how many
        // failing frames had the IK/post-process gate prepared. Together with the sample rows these
        // distinguish "the copy source is not slot0" from "a later writer touched poseLocal".
        std::uint64_t near_slot0{ 0 };
        std::uint64_t near_slot1{ 0 };
        std::uint64_t near_neither{ 0 };
        std::uint64_t fail_frames{ 0 };
        std::uint64_t fail_frames_ik{ 0 };
        // The number frames had their IK gate prepared (all three pointers present), regardless of
        // whether the frame failed.
        std::uint64_t ik_ready_frames{ 0 };
        // C-G5-E2-A′: residuals split by the bone they landed on. fail_other is the refuting
        // count — a non-zero value means the identity broke on a bone the foot-IK chain cannot
        // reach, and the first few names are kept as evidence.
        std::uint64_t fail_foot{ 0 };
        std::uint64_t fail_other{ 0 };
        std::array<ProbeBoneSample, 4> other_bones{};
        std::uint32_t other_bone_count{ 0 };
        // v76: failures split by the IK/post-process gate. The identity can only be judged strictly
        // on frames where that gate is closed; an all-IK session is undecidable for E2-A and
        // its residuals become E2-B input.
        std::uint64_t ik_frames{ 0 };
        std::uint64_t failed_ik{ 0 };
        std::uint64_t failed_no_ik{ 0 };
        std::uint64_t fail_frames_no_ik{ 0 };

        void add_frame(std::uint32_t a_tracks) noexcept
        {
            ++frames;
            tracks += a_tracks;
        }

        // Returns true when the component passed. A delta of zero means "within tolerance" (see
        // probe_mix_delta), so a non-zero delta is a failure and is also recorded as a sample.
        bool add_component(std::uint32_t a_track, std::uint8_t a_channel, float a_expected,
            float a_actual) noexcept
        {
            if (!std::isfinite(a_expected) || !std::isfinite(a_actual))
            {
                non_finite = true;
                return false;
            }
            float const delta = probe_mix_delta(a_expected, a_actual);
            if (delta == 0.0f)
                return true;
            ++failed;
            if (delta > max_delta)
                max_delta = delta;
            if (sample_count < Max_Samples)
            {
                samples[sample_count] = Sample{ a_track, a_channel, a_expected, a_actual, delta };
                ++sample_count;
            }
            return false;
        }

        // Attributes one failing component to the slot it actually matches, if any. Called by the
        // comparator for every component that add_component rejected.
        void classify_failure(float a_actual, float a_slot0, float a_slot1) noexcept
        {
            float const d0 = std::fabs(a_actual - a_slot0);
            float const d1 = std::fabs(a_actual - a_slot1);
            if (d0 <= d1 && probe_mix_delta(a_slot0, a_actual) == 0.0f)
                ++near_slot0;
            else if (d1 < d0 && probe_mix_delta(a_slot1, a_actual) == 0.0f)
                ++near_slot1;
            else
                ++near_neither;
        }

        // Attributes one failing component to the bone it landed on (E2-A′).
        void classify_bone(std::uint32_t a_track, char const* const* a_names,
            std::uint32_t a_name_count) noexcept
        {
            char const* const name = a_track < a_name_count ? a_names[a_track] : nullptr;
            if (probe_bone_is_foot_related(name))
            {
                ++fail_foot;
                return;
            }
            ++fail_other;
            if (name != nullptr && other_bone_count < other_bones.size())
            {
                ProbeBoneSample& sample = other_bones[other_bone_count];
                std::size_t i = 0;
                for (; i + 1 < Probe_Bone_Name_Length && name[i] != '\0'; ++i)
                    sample.name[i] = name[i];
                sample.name[i] = '\0';
                sample.count = 1;
                ++other_bone_count;
            }
        }

        void reset() noexcept
        {
            frames = 0;
            main_frames = 0;
            fallback_frames = 0;
            tracks = 0;
            failed = 0;
            max_delta = 0.0f;
            samples = {};
            sample_count = 0;
            weight_out_of_range = false;
            non_finite = false;
            read_failed = false;
            near_slot0 = 0;
            near_slot1 = 0;
            near_neither = 0;
            fail_frames = 0;
            fail_frames_ik = 0;
            ik_ready_frames = 0;
            fail_foot = 0;
            fail_other = 0;
            other_bones = {};
            other_bone_count = 0;
            ik_frames = 0;
            failed_ik = 0;
            failed_no_ik = 0;
            fail_frames_no_ik = 0;
        }

        [[nodiscard]] bool clean() const noexcept
        {
            return failed == 0 && !weight_out_of_range && !non_finite && !read_failed;
        }
    };

    // Slot accessors for the identity. A slot container holds a pointer to the slot structure at
    // +0; the structure's word +0x34 is the track-array offset, word +0x32 the track count (only
    // valid when dword +4 > 2, the same gate 63582 uses) and word +0x14 the payload offset.
    struct ProbeSlotView
    {
        void const* tracks;
        std::uint32_t count;
        bool valid;
    };

    [[nodiscard]] inline ProbeSlotView probe_slot_view(void const* a_graph, std::size_t a_container_offset) noexcept
    {
        ProbeSlotView view{ nullptr, 0, false };
        if (a_graph == nullptr)
            return view;
        auto const container = *reinterpret_cast<void const* const*>(
            static_cast<std::byte const*>(a_graph) + a_container_offset);
        if (!plausible_object_pointer(reinterpret_cast<std::uintptr_t>(container)))
            return view;
        auto const slot = *reinterpret_cast<void const* const*>(container);
        if (!plausible_object_pointer(reinterpret_cast<std::uintptr_t>(slot)))
            return view;
        auto const count_gate = *reinterpret_cast<std::int32_t const*>(static_cast<std::byte const*>(slot) + 4);
        auto const count = *reinterpret_cast<std::uint16_t const*>(static_cast<std::byte const*>(slot) + 0x32);
        auto const track_offset = *reinterpret_cast<std::int16_t const*>(static_cast<std::byte const*>(slot) + 0x34);
        view.tracks = static_cast<std::byte const*>(slot) + track_offset;
        view.count = count_gate > 2 ? count : 0;
        view.valid = true;
        return view;
    }

    inline constexpr std::size_t Probe_Mix_Weight_Offset = 0x230;
    inline constexpr std::size_t Probe_Mix_Slot0_Container_Offset = 0x220;
    inline constexpr std::size_t Probe_Mix_Slot1_Container_Offset = 0x228;
    inline constexpr std::size_t Probe_Mix_Track_Stride = 0x30;
    // Channel ids used by the failure samples: 0..2 are translation xyz, 4..7 rotation xyzw.
    inline constexpr std::uint8_t Probe_Mix_Channel_Trans_X = 0;
    inline constexpr std::uint8_t Probe_Mix_Channel_Rot_X = 4;

    [[nodiscard]] inline ProbeVec4 probe_track_vec(std::byte const* a_tracks, std::uint32_t a_index,
        std::size_t a_segment) noexcept
    {
        auto const* const base = a_tracks + static_cast<std::size_t>(a_index) * Probe_Mix_Track_Stride
            + a_segment * sizeof(ProbeVec4);
        ProbeVec4 value{};
        std::memcpy(&value, base, sizeof(value));
        return value;
    }

    // The number of tracks the identity can be compared over. 63582 caps its count by both slot
    // counts because it always blends both, but the fallback branch never reaches 63582: it sets
    // the weight to 1.0 and copies slot0 alone. A slot therefore only caps the count when it
    // actually participates at this weight — otherwise an empty inactive slot would force n to
    // zero (the v73 run read slot0=116, slot1=0, w=1.0 on all 276 frames and compared nothing).
    [[nodiscard]] inline std::uint32_t probe_mix_track_count(std::uint32_t a_clamped, float a_weight,
        std::uint32_t a_slot0_count, std::uint32_t a_slot1_count) noexcept
    {
        std::uint32_t n = a_clamped;
        if (a_weight > 0.0f && a_slot0_count < n)
            n = a_slot0_count;
        if (a_weight < 1.0f && a_slot1_count < n)
            n = a_slot1_count;
        return n;
    }

    // Which branch the frame took, for the per-mode counts: the fallback branch has weight 1.0 by
    // construction (63581 writes `[g+0x230] = 1.0f`), so a whole session can legitimately contain
    // no two-slot blend at all.
    enum class ProbeMixMode : std::uint8_t
    {
        e_fallback,
        e_main
    };

    [[nodiscard]] inline ProbeMixMode probe_mix_mode(float a_weight) noexcept
    {
        return a_weight >= 1.0f || a_weight <= 0.0f ? ProbeMixMode::e_fallback : ProbeMixMode::e_main;
    }
    // C-G5-E2-A v77: the real 58423 IK/post-process gate, verified at 63581
    // 0xbb0040..0xbb0076. Four conditions, one of them engine-wide:
    //   word[g+0x240] > 0 && byte[g+0x249] != 0 && byte[0x2022530] != 0 && [g+0x218] != nullptr
    // This is NOT the payload-overwrite gate at 0xbaffc3, which v75/v76 mistakenly used: that one
    // copies [g+0x148] into the out payload and is unrelated to 58423. byte[g+0x249] is rewritten
    // every frame by 63587 from ctx+0x2e (0xbb23b7) or the frame state (0xbb28da), so this gate
    // genuinely varies; the previous one was open on every frame of the v75 run and had no power
    // to separate anything.
    [[nodiscard]] inline bool probe_ik_gate_open(std::uint16_t a_count, std::uint8_t a_mode,
        std::uint8_t a_global, bool a_root) noexcept
    {
        return a_count > 0 && a_mode != 0 && a_global != 0 && a_root;
    }


    // Compares one track of poseLocal against the identity for the same index. Returns the number
    // of failed components and feeds the tally.
    inline std::uint32_t probe_compare_track(std::byte const* a_pose, std::byte const* a_slot1,
        std::byte const* a_slot0, float a_weight, std::uint32_t a_index, ProbeMixTally& a_tally,
        char const* const* a_bone_names = nullptr, std::uint32_t a_bone_count = 0) noexcept
    {
        std::uint32_t failed = 0;
        ProbeVec4 const pose_trans = probe_track_vec(a_pose, a_index, 0);
        ProbeVec4 const slot1_trans = probe_track_vec(a_slot1, a_index, 0);
        ProbeVec4 const slot0_trans = probe_track_vec(a_slot0, a_index, 0);
        ProbeVec4 const expect_trans = probe_mix_trans(slot1_trans, slot0_trans, a_weight);
        float const expect_xyz[3] = { expect_trans.x, expect_trans.y, expect_trans.z };
        float const pose_xyz[3] = { pose_trans.x, pose_trans.y, pose_trans.z };
        float const slot0_xyz[3] = { slot0_trans.x, slot0_trans.y, slot0_trans.z };
        float const slot1_xyz[3] = { slot1_trans.x, slot1_trans.y, slot1_trans.z };
        for (std::uint8_t i = 0; i < 3; ++i)
        {
            if (!a_tally.add_component(a_index, static_cast<std::uint8_t>(Probe_Mix_Channel_Trans_X + i),
                    expect_xyz[i], pose_xyz[i]))
            {
                a_tally.classify_failure(pose_xyz[i], slot0_xyz[i], slot1_xyz[i]);
                a_tally.classify_bone(a_index, a_bone_names, a_bone_count);
                ++failed;
            }
        }
        ProbeVec4 const pose_rot = probe_track_vec(a_pose, a_index, 1);
        ProbeVec4 const slot1_rot = probe_track_vec(a_slot1, a_index, 1);
        ProbeVec4 const slot0_rot = probe_track_vec(a_slot0, a_index, 1);
        ProbeVec4 const expect_rot = probe_mix_rot(slot1_rot, slot0_rot, a_weight);
        float const expect_quat[4] = { expect_rot.x, expect_rot.y, expect_rot.z, expect_rot.w };
        float const pose_quat[4] = { pose_rot.x, pose_rot.y, pose_rot.z, pose_rot.w };
        float const slot0_quat[4] = { slot0_rot.x, slot0_rot.y, slot0_rot.z, slot0_rot.w };
        float const slot1_quat[4] = { slot1_rot.x, slot1_rot.y, slot1_rot.z, slot1_rot.w };
        for (std::uint8_t i = 0; i < 4; ++i)
        {
            if (!a_tally.add_component(a_index, static_cast<std::uint8_t>(Probe_Mix_Channel_Rot_X + i),
                    expect_quat[i], pose_quat[i]))
            {
                a_tally.classify_failure(pose_quat[i], slot0_quat[i], slot1_quat[i]);
                a_tally.classify_bone(a_index, a_bone_names, a_bone_count);
                ++failed;
            }
        }
        return failed;
    }
}
PLUGIN_NAMESPACE_END
