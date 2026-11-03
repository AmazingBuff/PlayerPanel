//
// Created by AmazingBuff on 2026/11/03.
//

#include "animation/behavior_graph/probe_liveness.h"

#include "animation/behavior_graph/probe_liveness_policy.h"

#include "animation/settings.h"

#include <RE/B/BSAnimationGraphManager.h>
#include <RE/B/BShkbAnimationGraph.h>
#include <RE/H/hkbBehaviorGraph.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>
#include <SKSE/Trampoline.h>

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

extern "C" IMAGE_DOS_HEADER __ImageBase;

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
namespace
{
    // The deferred-update queue global (audit29): `q = *(void**)0x31be0d0`, gated on
    // `byte[q+0x430]`. This is an image RVA, not a REL ID, so it is only read on the exact
    // runtime the audit pinned.
    constexpr std::uint32_t Queue_Global_Rva = 0x31be0d0;
    constexpr std::size_t Queue_Mode_Offset = 0x430;

    // 63589 (sampling entry) and 63856 (poseLocal last writer). Both are address-library IDs.
    constexpr std::uint64_t Sampling_Entry_Id = 63589;
    constexpr std::uint64_t Pose_Verdict_Id = 63856;
    constexpr std::size_t Block_Stride = 128;

    enum RecordKind : std::uint8_t
    {
        k_entry = 0,
        k_completion = 1
    };

    std::atomic<bool> g_installed{ false };
    std::atomic<bool> g_arm_requested{ false };
    std::atomic<bool> g_invalidate_requested{ false };
    std::atomic<void*> g_pinned{ nullptr };
    std::atomic<std::uint32_t> g_epoch{ 0 };
    std::atomic<std::uint32_t> g_actor{ 0 };

    // Counters are written from the detour thread and read at close; the window itself is only
    // touched by the main/render thread, like the existing probes.
    struct Counters
    {
        std::atomic<std::uint64_t> entries_total{ 0 };
        std::atomic<std::uint64_t> entries_engine{ 0 };
        std::atomic<std::uint64_t> entries_plugin{ 0 };
        std::atomic<std::uint64_t> entries_other{ 0 };
        std::atomic<std::uint64_t> pinned_engine{ 0 };
        std::atomic<std::uint64_t> completions_total{ 0 };
        std::atomic<std::uint64_t> completions_pinned{ 0 };
        std::atomic<std::uint64_t> unattributed_completions{ 0 };
        std::atomic<std::uint64_t> frames{ 0 };
        std::atomic<std::uint64_t> queue_flag_frames{ 0 };
        std::atomic<std::uint64_t> detail_dropped{ 0 };

        void reset()
        {
            entries_total.store(0, std::memory_order_relaxed);
            entries_engine.store(0, std::memory_order_relaxed);
            entries_plugin.store(0, std::memory_order_relaxed);
            entries_other.store(0, std::memory_order_relaxed);
            pinned_engine.store(0, std::memory_order_relaxed);
            completions_total.store(0, std::memory_order_relaxed);
            completions_pinned.store(0, std::memory_order_relaxed);
            unattributed_completions.store(0, std::memory_order_relaxed);
            frames.store(0, std::memory_order_relaxed);
            queue_flag_frames.store(0, std::memory_order_relaxed);
            detail_dropped.store(0, std::memory_order_relaxed);
        }
    };

    struct ProbeState
    {
        ProbeWindow window;
        ProbeRing ring;
        ProbePairing pairing;
        Counters counters;
        std::uint64_t detail_lines{ 0 };
        std::uint64_t detail_seq{ 0 };
        std::uint64_t tracked_entries{ 0 };
        std::uint64_t paired_completions{ 0 };
        std::uint64_t pairing_mismatch{ 0 };
        std::uint64_t pairing_no_slot{ 0 };
        bool queue_seen{ false };
        std::uintptr_t queue_pointer{ 0 };
        std::uint8_t queue_flag{ 0 };
        std::uint32_t slots_used{ 0 };

        // Atomics are not assignable, so a new window resets members in place.
        void reset()
        {
            window = ProbeWindow{};
            ring.reset();
            pairing.reset();
            counters.reset();
            detail_lines = 0;
            detail_seq = 0;
            tracked_entries = 0;
            paired_completions = 0;
            pairing_mismatch = 0;
            pairing_no_slot = 0;
            queue_seen = false;
            queue_pointer = 0;
            queue_flag = 0;
            slots_used = 0;
        }
    };

    ProbeState& state()
    {
        static ProbeState s_state;
        return s_state;
    }

    std::uintptr_t game_base()
    {
        return REL::Module::get().base();
    }

    struct ModuleRange
    {
        std::uintptr_t begin;
        std::uintptr_t end;
    };

    ModuleRange plugin_range()
    {
        auto const* const base = reinterpret_cast<std::uint8_t const*>(&__ImageBase);
        auto const* const dos = reinterpret_cast<IMAGE_DOS_HEADER const*>(base);
        auto const* const nt = reinterpret_cast<IMAGE_NT_HEADERS const*>(base + dos->e_lfanew);
        auto const begin = reinterpret_cast<std::uintptr_t>(base);
        return { begin, begin + nt->OptionalHeader.SizeOfImage };
    }

    bool exact_runtime()
    {
        return REL::Module::get().version() == SKSE::RUNTIME_SSE_1_6_1170;
    }

    template <class T>
    T read_offset(void const* a_base, std::size_t a_offset) noexcept
    {
        T value{};
        std::memcpy(&value, static_cast<std::byte const*>(a_base) + a_offset, sizeof(value));
        return value;
    }

    // ---- detour recorders (no allocation, no locks, no engine calls) -----------

    void record_sample_entry(void* a_graph, void* a_caller) noexcept
    {
        ProbeState& s = state();
        if (!s.window.active())
            return;
        Counters& c = s.counters;
        c.entries_total.fetch_add(1, std::memory_order_relaxed);
        void const* const pinned = g_pinned.load(std::memory_order_relaxed);
        ModuleRange const module = plugin_range();
        ProbeCaller const caller = classify_probe_caller(reinterpret_cast<std::uintptr_t>(a_caller),
            game_base(), module.begin, module.end);
        switch (caller)
        {
        case ProbeCaller::e_engine_local_loop: c.entries_engine.fetch_add(1, std::memory_order_relaxed); break;
        case ProbeCaller::e_plugin: c.entries_plugin.fetch_add(1, std::memory_order_relaxed); break;
        default: c.entries_other.fetch_add(1, std::memory_order_relaxed); break;
        }
        bool const is_pinned = pinned != nullptr && a_graph == pinned;
        if (is_pinned && caller == ProbeCaller::e_engine_local_loop)
            c.pinned_engine.fetch_add(1, std::memory_order_relaxed);
        if (!is_pinned && caller != ProbeCaller::e_plugin)
            return;
        ProbeRecord record{};
        record.graph = reinterpret_cast<std::uintptr_t>(a_graph);
        {
            auto const caller_value = reinterpret_cast<std::uintptr_t>(a_caller);
            auto const base = game_base();
            record.caller_or_count = (base != 0 && caller_value >= base)
                ? static_cast<std::uint32_t>(caller_value - base)
                : 0xFFFFFFFFu;
        }
        record.tid = static_cast<std::uint32_t>(__readgsqword(0x48));
        record.kind = k_entry;
        if (is_pinned && plausible_object_pointer(record.graph))
        {
            record.num240 = read_offset<std::uint16_t>(a_graph, 0x240);
            record.flag246 = read_offset<std::uint8_t>(a_graph, 0x246);
        }
        if (!s.ring.push(record))
            c.detail_dropped.fetch_add(1, std::memory_order_relaxed);
    }

    void record_pose_write(void* a_pose, void* a_table, std::uint32_t a_count) noexcept
    {
        ProbeState& s = state();
        if (!s.window.active())
            return;
        Counters& c = s.counters;
        c.completions_total.fetch_add(1, std::memory_order_relaxed);
        void const* const pinned = g_pinned.load(std::memory_order_relaxed);
        void* const graph = probe_graph_from_table_argument(a_table);
        bool const is_pinned = pinned != nullptr && graph == pinned;
        if (is_pinned)
            c.completions_pinned.fetch_add(1, std::memory_order_relaxed);
        else
            c.unattributed_completions.fetch_add(1, std::memory_order_relaxed);
        if (!is_pinned)
            return;
        ProbeRecord record{};
        record.graph = reinterpret_cast<std::uintptr_t>(graph);
        record.caller_or_count = a_count;
        record.num240 = a_count > 0xFFFFu ? 0xFFFFu : static_cast<std::uint16_t>(a_count);
        record.tid = static_cast<std::uint32_t>(__readgsqword(0x48));
        record.kind = k_completion;
        (void)a_pose;
        if (!s.ring.push(record))
            c.detail_dropped.fetch_add(1, std::memory_order_relaxed);
    }

    // ---- thunk encoding ------------------------------------------------------

    // The shared encoder lives in the policy header so the engine-free layout case compiles the
    // exact bytes this installer writes.
    std::size_t encode_thunk(std::uint8_t* a_dst, std::uintptr_t a_recorder, std::uintptr_t a_original,
        bool a_pose_arguments)
    {
        return encode_detour_thunk(a_dst, a_recorder, a_original, a_pose_arguments);
    }

    bool install_one(REL::ID a_id, char const* a_name, std::uint8_t const* a_head, std::size_t a_head_size,
        void const* a_recorder, bool a_pose_arguments)
    {
        REL::Relocation<std::uintptr_t> target{ a_id };
        auto const* const head = reinterpret_cast<std::uint8_t const*>(target.address());
        if (std::memcmp(head, a_head, a_head_size) != 0)
        {
            std::string hex;
            for (std::size_t i = 0; i < 16; ++i)
                hex += fmt::format("{:02x}", head[i]);
            logger::warn("PROBE LIVENESS install-failed reason=head-mismatch target='{}' address={:#x} head={}",
                a_name, target.address(), hex);
            return false;
        }
        auto& trampoline = SKSE::GetTrampoline();
        auto* const mem = static_cast<std::uint8_t*>(trampoline.allocate(Block_Stride));
        if (!mem)
        {
            logger::warn("PROBE LIVENESS install-failed reason=trampoline-exhausted target='{}'", a_name);
            return false;
        }
        std::uint8_t* const thunk = mem;
        std::uint8_t* const original = mem + 2 * Block_Stride;
        std::memcpy(original, a_head, a_head_size);
        {
            std::uint8_t const far_jmp[6] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
            std::memcpy(original + a_head_size, far_jmp, sizeof(far_jmp));
            std::uintptr_t const back = target.address() + a_head_size;
            std::memcpy(original + a_head_size + sizeof(far_jmp), &back, sizeof(back));
        }
        (void)encode_thunk(thunk, reinterpret_cast<std::uintptr_t>(a_recorder),
            reinterpret_cast<std::uintptr_t>(original), a_pose_arguments);
        // The patch covers the whole verified head: a 5-byte jmp plus inert pads. 63589's head is
        // exactly 5 bytes (three pushes); 63856's is the 7-byte `sub rsp,0x88`.
        std::uint8_t patch[8] = { 0xE9, 0x00, 0x00, 0x00, 0x00, 0x90, 0x90, 0x90 };
        std::int32_t const rel = static_cast<std::int32_t>(
            reinterpret_cast<std::uintptr_t>(thunk) - (target.address() + 5));
        std::memcpy(patch + 1, &rel, sizeof(rel));
        REL::safe_write(target.address(), patch, a_head_size);
        logger::info("PROBE LIVENESS installed target='{}' address={:#x} thunk={:#x} original={:#x} head-size={}",
            a_name, target.address(), reinterpret_cast<std::uintptr_t>(thunk),
            reinterpret_cast<std::uintptr_t>(original), a_head_size);
        return true;
    }
}

    void install_probe_liveness()
    {
        bool expected = false;
        if (!g_installed.compare_exchange_strong(expected, true))
            return;
        if (sampling_liveness_mode() == ProbeLivenessMode::e_off)
        {
            logger::info("PROBE LIVENESS install-skip reason=off");
            return;
        }
        if (source_timeline() || source_event_capture() || source_write_capture())
        {
            logger::warn("PROBE LIVENESS install-skip reason=other-detour-probe-priority");
            return;
        }
        if (!exact_runtime())
        {
            logger::warn("PROBE LIVENESS install-skip reason=runtime-version");
            return;
        }
        auto& trampoline = SKSE::GetTrampoline();
        trampoline.create(256);
        bool const sample = install_one(REL::ID(Sampling_Entry_Id), "63589-sampling-entry",
            Probe_Sample_Head_Bytes.data(), Probe_Sample_Head_Bytes.size(), &record_sample_entry, false);
        bool const pose = sample && install_one(REL::ID(Pose_Verdict_Id), "63856-pose-write",
            Probe_Pose_Head_Bytes.data(), Probe_Pose_Head_Bytes.size(), &record_pose_write, true);
        if (!sample || !pose)
            logger::warn("PROBE LIVENESS install-incomplete sample={} pose={}", sample, pose);
    }

    void arm_probe_liveness()
    {
        if (sampling_liveness_mode() != ProbeLivenessMode::e_on || !g_installed.load(std::memory_order_acquire))
            return;
        if (independent_graph_setting() != IndependentGraphMode::off)
            return;
        g_arm_requested.store(true, std::memory_order_release);
        logger::info("PROBE LIVENESS arm-requested mode=on");
    }

    bool arm_probe_liveness_concurrent(RE::BShkbAnimationGraph const& a_source_graph, std::uint32_t a_actor_id)
    {
        if (sampling_liveness_mode() != ProbeLivenessMode::e_concurrent || !g_installed.load(std::memory_order_acquire))
            return false;
        RE::Actor* const holder = a_source_graph.holder;
        if (!holder)
        {
            logger::warn("PROBE LIVENESS concurrent-arm-skip reason=no-holder actor={:08x}", a_actor_id);
            return false;
        }
        // Pin only a graph the manager still owns; pointers are compared, never dereferenced here.
        RE::BShkbAnimationGraph* pinned = nullptr;
        RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
        if (holder->GetAnimationGraphManager(manager) && manager)
        {
            for (RE::BSTSmartPointer<RE::BShkbAnimationGraph> const& graph : manager->graphs)
            {
                if (graph && graph.get() == &a_source_graph)
                {
                    pinned = graph.get();
                    break;
                }
            }
        }
        if (!pinned)
        {
            logger::warn("PROBE LIVENESS concurrent-arm-skip reason=source-not-in-manager actor={:08x}", a_actor_id);
            return false;
        }
        if (!pinned->behaviorGraph)
        {
            logger::warn("PROBE LIVENESS concurrent-arm-skip reason=no-behavior actor={:08x}", a_actor_id);
            return false;
        }
        ProbeState& s = state();
        if (s.window.ever_armed())
        {
            logger::warn("PROBE LIVENESS concurrent-arm-skip reason=window-already-used actor={:08x}", a_actor_id);
            return false;
        }
        s.reset();
        g_pinned.store(pinned, std::memory_order_release);
        g_actor.store(a_actor_id, std::memory_order_relaxed);
        std::uint32_t const epoch = g_epoch.fetch_add(1, std::memory_order_relaxed) + 1;
        s.window.arm(std::chrono::steady_clock::now());
        logger::info("PROBE LIVENESS concurrent-armed actor={:08x} epoch={} graph={:#x} behavior={:#x} pinned=yes",
            a_actor_id, epoch, reinterpret_cast<std::uintptr_t>(pinned),
            reinterpret_cast<std::uintptr_t>(pinned->behaviorGraph));
        return true;
    }

    bool probe_liveness_window_open()
    {
        return state().window.active();
    }

    void close_probe_liveness_window(char const* a_reason)
    {
        ProbeState& s = state();
        if (!s.window.active())
            return;
        ProbeWindowClose reason = ProbeWindowClose::e_abort;
        if (std::strcmp(a_reason, "display-close") == 0) reason = ProbeWindowClose::e_display_close;
        else if (std::strcmp(a_reason, "expired") == 0) reason = ProbeWindowClose::e_expired;
        else if (std::strcmp(a_reason, "world-reset") == 0) reason = ProbeWindowClose::e_world_reset;
        else if (std::strcmp(a_reason, "rearm") == 0) reason = ProbeWindowClose::e_rearm;
        s.window.cancel(reason);
        s.slots_used = static_cast<std::uint32_t>(s.pairing.slots_used());
        logger::info("PROBE LIVENESS window=close reason={} actor={:08x} epoch={} frames={} entries={} e:engine={}/plugin={}/other={} pinned-engine={} completions={} pinned={} unattributed={} tracked-entries={} paired-completions={} pair-mismatch={} pair-no-slot={} threads={} queue-seen={} queue={:#x} flag={} queue-flag-frames={} detail={} detail-dropped={} ring-dropped={}",
            probe_window_close_name(reason), g_actor.load(std::memory_order_relaxed),
            g_epoch.load(std::memory_order_relaxed), s.counters.frames.load(std::memory_order_relaxed),
            s.counters.entries_total.load(std::memory_order_relaxed),
            s.counters.entries_engine.load(std::memory_order_relaxed),
            s.counters.entries_plugin.load(std::memory_order_relaxed),
            s.counters.entries_other.load(std::memory_order_relaxed),
            s.counters.pinned_engine.load(std::memory_order_relaxed),
            s.counters.completions_total.load(std::memory_order_relaxed),
            s.counters.completions_pinned.load(std::memory_order_relaxed),
            s.counters.unattributed_completions.load(std::memory_order_relaxed),
            s.tracked_entries, s.paired_completions, s.pairing_mismatch, s.pairing_no_slot, s.slots_used,
            s.queue_seen ? "yes" : "no", s.queue_pointer, s.queue_flag,
            s.counters.queue_flag_frames.load(std::memory_order_relaxed), s.detail_lines,
            s.counters.detail_dropped.load(std::memory_order_relaxed), s.ring.dropped());
    }

    void invalidate_probe_liveness()
    {
        if (sampling_liveness_mode() == ProbeLivenessMode::e_off)
            return;
        g_invalidate_requested.store(true, std::memory_order_release);
        logger::info("PROBE LIVENESS invalidate-requested reason=world-reset");
    }

    void sample_probe_liveness()
    {
        if (sampling_liveness_mode() == ProbeLivenessMode::e_off || !g_installed.load(std::memory_order_acquire))
            return;
        ProbeState& s = state();
        if (g_invalidate_requested.exchange(false, std::memory_order_acq_rel))
        {
            // Close first (it cancels and emits the row), then drop the pin so a stale graph can
            // never match a later entry.
            if (s.window.active())
                close_probe_liveness_window("world-reset");
            g_pinned.store(nullptr, std::memory_order_release);
            return;
        }
        if (g_arm_requested.exchange(false, std::memory_order_acq_rel) && !s.window.ever_armed())
        {
            // The F7 window observes the player's own graph like the legacy E1 lifecycle; the
            // pinned set is empty, so counters run but no detail records can match.
            s.reset();
            g_epoch.fetch_add(1, std::memory_order_relaxed);
            s.window.arm(std::chrono::steady_clock::now());
            logger::info("PROBE LIVENESS window=open mode=on epoch={} pinned=no",
                g_epoch.load(std::memory_order_relaxed));
        }
        if (!s.window.active())
            return;
        auto const now = std::chrono::steady_clock::now();
        if (s.window.due(now))
        {
            close_probe_liveness_window("expired");
            return;
        }
        s.counters.frames.fetch_add(1, std::memory_order_relaxed);
        // Deferred-update queue snapshot (read-only): the contract's Q1 evidence.
        if (exact_runtime())
        {
            void* queue = nullptr;
            std::memcpy(&queue, reinterpret_cast<void const*>(game_base() + Queue_Global_Rva), sizeof(queue));
            if (queue)
            {
                std::uint8_t flag = 0;
                std::memcpy(&flag, static_cast<std::byte const*>(queue) + Queue_Mode_Offset, sizeof(flag));
                s.queue_seen = true;
                s.queue_pointer = reinterpret_cast<std::uintptr_t>(queue);
                s.queue_flag = flag;
                if (flag != 0)
                    s.counters.queue_flag_frames.fetch_add(1, std::memory_order_relaxed);
            }
        }
        ProbeRecord record{};
        while (s.ring.pop(record))
        {
            if (record.kind == k_entry)
            {
                ProbePairing::Outcome const outcome = s.pairing.on_entry(record.tid, reinterpret_cast<void*>(record.graph));
                if (outcome == ProbePairing::Outcome::e_paired)
                    ++s.tracked_entries;
                else if (outcome == ProbePairing::Outcome::e_no_slot)
                    ++s.pairing_no_slot;
            }
            else
            {
                ProbePairing::Outcome const outcome = s.pairing.on_completion(record.tid, reinterpret_cast<void*>(record.graph));
                if (outcome == ProbePairing::Outcome::e_paired)
                    ++s.paired_completions;
                else if (outcome == ProbePairing::Outcome::e_graph_mismatch)
                    ++s.pairing_mismatch;
                else if (outcome == ProbePairing::Outcome::e_unattributed)
                    ++s.pairing_mismatch;
            }
            if (s.detail_lines >= Probe_Liveness_Detail_Budget)
                continue;
            ++s.detail_lines;
            ++s.detail_seq;
            if (record.kind == k_entry)
            {
                logger::info("PROBE LIVENESS entry seq={} tid={} graph={:#x} caller={:#x} num240={} flag246={}",
                    s.detail_seq, record.tid, record.graph, record.caller_or_count, record.num240, record.flag246);
            }
            else
            {
                logger::info("PROBE LIVENESS completion seq={} tid={} graph={:#x} count={}",
                    s.detail_seq, record.tid, record.graph, record.caller_or_count);
            }
        }
    }
}
PLUGIN_NAMESPACE_END
