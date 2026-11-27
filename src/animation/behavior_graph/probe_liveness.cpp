//
// Created by AmazingBuff on 2026/11/03.
//

#include "animation/behavior_graph/probe_liveness.h"

#include "animation/behavior_graph/b3b4_policy.h"
#include "animation/behavior_graph/k3_snapshot_policy.h"
#include "animation/behavior_graph/k5_snapshot_policy.h"
#include "animation/behavior_graph/probe_liveness_policy.h"
#include "animation/behavior_graph/source_timeline_policy.h"
#include "animation/behavior_graph/source_timeline.h"
#include "animation/behavior_graph/transition_probe.h"

#include "animation/settings.h"

#include <RE/P/PlayerCharacter.h>
#include <RE/B/BSAnimationGraphManager.h>
#include <RE/B/BShkbAnimationGraph.h>
#include <RE/H/hkaBone.h>
#include <RE/H/hkaSkeleton.h>
#include <RE/H/hkbBehaviorGraph.h>
#include <RE/H/hkbCharacterSetup.h>
#include <RE/H/hkbClipGenerator.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>
#include <SKSE/Trampoline.h>

#include <Windows.h>
#include <intrin.h>

#include <atomic>
#include <array>
#include <bit>
#include <chrono>
#include <ctime>
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

    // Sampling and scene-application entry sites; neither observes native return.
    constexpr std::uint64_t Sampling_Entry_Id = 63589;
    constexpr std::uint64_t Pose_Apply_Entry_Id = 63856;
    constexpr std::uint64_t Foot_Ik_Entry_Id = 58423;

    enum RecordKind : std::uint8_t
    {
        k_entry = 0,
        k_apply_entry = 1,
        k_apply_return = 2,
        k_mix_frame = 3,
        k_footik_entry = 4,
        k_footik_return = 5
    };

    std::atomic<bool> g_installed{ false };
    std::atomic<bool> g_install_attempted{ false };
    std::atomic<bool> g_arm_requested{ false };
    std::atomic<bool> g_invalidate_requested{ false };
    std::atomic<void*> g_pinned{ nullptr };
    // C-G5-E2-B B3/B4: `&pinned->characterInstance`, i.e. pinned + 0xC0. Audit 220/222: 58315
    // builds the 58423 hkbContext with `character = &graph->characterInstance` and zeroes
    // `ctx->behavior`, so the only usable same-graph identity is this character pointer. Both the
// v97 (`g_pinned`) and v97a (`g_pinned_behavior`) runs used identities that can never match.
    std::atomic<void*> g_pinned_character{ nullptr };
    std::atomic<void*> g_plugin_graph{ nullptr };
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
        std::atomic<std::uint64_t> apply_entries_total{ 0 };
        std::atomic<std::uint64_t> apply_entries_pinned{ 0 };
        std::atomic<std::uint64_t> other_apply_entries{ 0 };
        std::atomic<std::uint64_t> frames{ 0 };
        std::atomic<std::uint64_t> queue_flag_frames{ 0 };
        std::atomic<std::uint64_t> detail_dropped{ 0 };
        std::atomic<std::uint64_t> eligible_records{ 0 };
        // C-G5-E1 v3: per-path entry/completion split and the range facts R2/R3/R4. Path indexes
        // follow ProbeApplyPath; unclassified is its own slot and must stay zero for a pass.
        std::array<std::atomic<std::uint64_t>, Probe_Apply_Path_Count> path_entries{};
        std::array<std::atomic<std::uint64_t>, Probe_Apply_Path_Count> path_completions{};
        std::atomic<std::uint64_t> apply_returns_total{ 0 };
        std::atomic<std::uint64_t> completions_pinned{ 0 };
        std::atomic<std::uint64_t> paired_completions{ 0 };
        std::atomic<std::uint64_t> orphan_returns{ 0 };
        std::atomic<std::uint64_t> returns_untracked{ 0 };
        std::atomic<std::uint64_t> unclassified_applies{ 0 };
        std::atomic<std::uint64_t> count_over_capacity{ 0 };
        std::atomic<std::uint64_t> clamped_zero{ 0 };
        std::atomic<std::uint64_t> overwritten_entries{ 0 };
        std::atomic<std::uint64_t> footik_overwritten{ 0 };
        std::atomic<std::uint32_t> max_in_flight{ 0 };
        std::atomic<bool> pose_unstable{ false };

        void reset()
        {
            entries_total.store(0, std::memory_order_relaxed);
            entries_engine.store(0, std::memory_order_relaxed);
            entries_plugin.store(0, std::memory_order_relaxed);
            entries_other.store(0, std::memory_order_relaxed);
            pinned_engine.store(0, std::memory_order_relaxed);
            apply_entries_total.store(0, std::memory_order_relaxed);
            apply_entries_pinned.store(0, std::memory_order_relaxed);
            other_apply_entries.store(0, std::memory_order_relaxed);
            frames.store(0, std::memory_order_relaxed);
            queue_flag_frames.store(0, std::memory_order_relaxed);
            detail_dropped.store(0, std::memory_order_relaxed);
            eligible_records.store(0, std::memory_order_relaxed);
            for (std::size_t i = 0; i < Probe_Apply_Path_Count; ++i)
            {
                path_entries[i].store(0, std::memory_order_relaxed);
                path_completions[i].store(0, std::memory_order_relaxed);
            }
            apply_returns_total.store(0, std::memory_order_relaxed);
            completions_pinned.store(0, std::memory_order_relaxed);
            paired_completions.store(0, std::memory_order_relaxed);
            orphan_returns.store(0, std::memory_order_relaxed);
            returns_untracked.store(0, std::memory_order_relaxed);
            unclassified_applies.store(0, std::memory_order_relaxed);
            count_over_capacity.store(0, std::memory_order_relaxed);
            clamped_zero.store(0, std::memory_order_relaxed);
            overwritten_entries.store(0, std::memory_order_relaxed);
            footik_overwritten.store(0, std::memory_order_relaxed);
            max_in_flight.store(0, std::memory_order_relaxed);
            pose_unstable.store(false, std::memory_order_relaxed);
        }
    };

    struct ProbeState
    {
        ProbeWindow window;
        ProbeAdmission admission;
        bool close_pending{ false };        ProbeRing ring;
        ProbePairing pairing;
        ProbeInFlight in_flight;
        ProbeFootikInFlight footik;
        ProbeFootikTally footik_tally;
        ProbeB3B4Ledger b3b4;
        ProbeB3B4Tally b3b4_tally;
        std::uint64_t b3_overwritten{ 0 };
        std::uint64_t b3_no_out{ 0 };
        std::uint64_t b3_stale{ 0 };
        std::uint64_t b3_multi{ 0 };
        std::uint64_t b3_behavior_mismatch{ 0 };
        bool b3b4_first_logged{ false };
        bool k3s_closed{ false };
        ProbeMixTally mix;
        Counters counters;
        std::uint64_t detail_lines{ 0 };
        std::uint64_t drained_records{ 0 };
        std::uint64_t drained_samples{ 0 };
        std::uint64_t drained_applies{ 0 };
        std::uint64_t drained_returns{ 0 };
        std::uint64_t drained_mix_frames{ 0 };
        std::uint64_t drained_footik_entries{ 0 };
        std::uint64_t drained_footik_returns{ 0 };
        std::uint64_t orphan_applies{ 0 };
        std::uint64_t detail_seq{ 0 };
        std::uint64_t tracked_entries{ 0 };
        std::uint64_t paired_apply_entries{ 0 };
        std::uint64_t pairing_mismatch{ 0 };
        std::uint64_t pairing_no_slot{ 0 };
        bool queue_seen{ false };
        std::uintptr_t queue_pointer{ 0 };
        std::uint8_t queue_flag{ 0 };
        std::uint32_t slots_used{ 0 };
        std::uintptr_t expected_pose{ 0 };
        std::uint8_t fill_cap_state{ 0 };  // 0 unread, 1 ok, 2 violation
        std::uint32_t skeleton_capacity{ 0 };
        std::uint32_t skeleton_fill{ 0 };

        // C-G5-E3 (M2): the candidate's step timeline (one TSC per pose-producing step) and the
        // window manifest identity. The manifest is written at arm and completed at close.
        std::array<std::uint64_t, Probe_Step_Timeline> step_tsc{};
        std::uint32_t step_count{ 0 };
        std::uint64_t request{ 0 };
        std::uint64_t arm_tsc{ 0 };
        std::uint64_t arm_steady_ns{ 0 };
        std::uint64_t close_tsc{ 0 };
        std::uint64_t close_steady_ns{ 0 };

        // Appends one step timestamp and returns its index. A saturated timeline returns the
        // overflow marker (== Probe_Step_Timeline) instead of overwriting an earlier step, so a
        // full timeline is visible in the manifest rather than silently truncated.
        [[nodiscard]] std::uint32_t note_step(std::uint64_t a_tsc) noexcept
        {
            if (step_count >= Probe_Step_Timeline)
                return Probe_Step_Timeline;
            step_tsc[step_count] = a_tsc;
            return step_count++;
        }

        // Atomics are not assignable, so a new window resets members in place.
        void reset()
        {
            window = ProbeWindow{};
            ring.reset();
            pairing.reset();
            in_flight.reset();
            footik.reset();
            footik_tally.reset();
            b3b4.reset();
            b3b4_tally.reset();
            b3_overwritten = 0;
            b3_no_out = 0;
            b3_stale = 0;
            b3_multi = 0;
            b3_behavior_mismatch = 0;
            b3b4_first_logged = false;
            k3s_closed = false;
            mix.reset();
            counters.reset();
            detail_lines = 0;
            drained_records = 0;
            drained_samples = 0;
            drained_applies = 0;
            drained_returns = 0;
            drained_mix_frames = 0;
            drained_footik_entries = 0;
            drained_footik_returns = 0;
            orphan_applies = 0;
            close_pending = false;
            detail_seq = 0;
            tracked_entries = 0;
            paired_apply_entries = 0;
            pairing_mismatch = 0;
            pairing_no_slot = 0;
            queue_seen = false;
            queue_pointer = 0;
            queue_flag = 0;
            slots_used = 0;
            expected_pose = 0;
            fill_cap_state = 0;
            skeleton_capacity = 0;
            skeleton_fill = 0;
            step_count = 0;
            request = 0;
            arm_tsc = 0;
            arm_steady_ns = 0;
            close_tsc = 0;
            close_steady_ns = 0;
        }
    };

    ProbeState& state()
    {
        static ProbeState s_state;
        return s_state;
    }

    // C-G5-E2-A′: the pinned skeleton's bone names, cached once per window on the render thread.
    inline constexpr std::size_t Max_Cached_Bones = 256;
    char g_bone_names[Max_Cached_Bones][Probe_Bone_Name_Length];
    char const* g_bone_name_ptrs[Max_Cached_Bones];
    // E2-B: per-call snapshots of the 58423 out block (entry side and return side).
    ProbeVec4 g_footik_entry_tracks[116 * 2];
    ProbeVec4 g_footik_return_tracks[116 * 2];
    ProbeVec4* entry_tracks() noexcept { return g_footik_entry_tracks; }
    ProbeVec4* return_tracks() noexcept { return g_footik_return_tracks; }
    // C-G5-E2-B B3/B4: return-side snapshot and apply-side re-read buffers. Both are static and
    // bounded by the pre-registered 116-track capacity; the re-read buffer is detour-local.
    ProbeVec4 g_b3b4_return_snapshot[Probe_B3B4_Track_Capacity * 2];
    ProbeVec4 g_b3b4_reread[Probe_B3B4_Track_Capacity * 2];
    std::atomic<std::uint32_t> g_bone_count{ 0 };

    // ---- C-G5-CAL K3-S: per-apply source snapshot ring and writer ---------------------------
    // The detour only copies a fixed record into this ring (no allocation, no locks, no file
    // I/O); the render drain writes it to disk. 1,024 records x 5,268 B is about 5.4 MiB, under
    // the pre-registered 8 MiB budget.
    class K3SourceRing
    {
    public:
        bool push(K3SourceRecord const& a_record) noexcept
        {
            if (!m_gate.try_enter())
            {
                m_dropped.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            std::size_t const head = m_head.load(std::memory_order_relaxed);
            std::size_t const tail = m_tail.load(std::memory_order_acquire);
            if (head - tail >= K3S_Record_Capacity)
            {
                m_dropped.fetch_add(1, std::memory_order_relaxed);
                m_gate.leave();
                return false;
            }
            std::memcpy(&m_slots[head % K3S_Record_Capacity], &a_record, sizeof(a_record));
            m_head.store(head + 1, std::memory_order_release);
            m_gate.leave();
            return true;
        }

        bool pop(K3SourceRecord& a_out) noexcept
        {
            std::size_t const tail = m_tail.load(std::memory_order_relaxed);
            std::size_t const head = m_head.load(std::memory_order_acquire);
            if (tail == head)
                return false;
            std::memcpy(&a_out, &m_slots[tail % K3S_Record_Capacity], sizeof(a_out));
            m_tail.store(tail + 1, std::memory_order_release);
            return true;
        }

        void drain_discard() noexcept
        {
            m_tail.store(m_head.load(std::memory_order_acquire), std::memory_order_release);
        }

        void reset() noexcept
        {
            m_head.store(0, std::memory_order_relaxed);
            m_tail.store(0, std::memory_order_relaxed);
            m_dropped.store(0, std::memory_order_relaxed);
            m_gate.reset();
        }

        [[nodiscard]] std::uint64_t dropped() const noexcept
        {
            return m_dropped.load(std::memory_order_relaxed);
        }

    private:
        std::array<K3SourceRecord, K3S_Record_Capacity> m_slots{};
        std::atomic<std::size_t> m_head{ 0 };
        std::atomic<std::size_t> m_tail{ 0 };
        std::atomic<std::uint64_t> m_dropped{ 0 };
        ProbePushGate m_gate;
    };

    K3SourceRing g_k3s_ring;
    K3SourceWriter g_k3s_writer;
    std::atomic<std::uint32_t> g_k3s_seq{ 0 };
    std::atomic<std::uint64_t> g_k3s_records{ 0 };
    std::atomic<std::uint64_t> g_k3s_two_read_bad{ 0 };
    std::atomic<std::uint64_t> g_k3s_clip_overflow{ 0 };
    std::atomic<std::uint64_t> g_k3s_no_pose{ 0 };
    std::atomic<std::uint64_t> g_k3s_write_failed{ 0 };
    std::atomic<bool> g_k3s_open_failed{ false };

    // ---- C-G5-CAL K5: per-apply source snapshot, CPK5 format v1 ------------------------------
    // Same ring/drain split as K3-S with the extended record (per-clip identity columns,
    // K6 plan §10.3). The identity cache is detour-owned and validated against three live
    // header fields on every hit, so a same-address ABA mostly recomputes instead of
    // aliasing a stale identity.
    class K5SourceRing
    {
    public:
        bool push(K5SourceRecord const& a_record) noexcept
        {
            if (!m_gate.try_enter())
            {
                m_dropped.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            std::size_t const head = m_head.load(std::memory_order_relaxed);
            std::size_t const tail = m_tail.load(std::memory_order_acquire);
            if (head - tail >= K5S_Record_Capacity)
            {
                m_dropped.fetch_add(1, std::memory_order_relaxed);
                m_gate.leave();
                return false;
            }
            std::memcpy(&m_slots[head % K5S_Record_Capacity], &a_record, sizeof(a_record));
            m_head.store(head + 1, std::memory_order_release);
            m_gate.leave();
            return true;
        }

        bool pop(K5SourceRecord& a_out) noexcept
        {
            std::size_t const tail = m_tail.load(std::memory_order_relaxed);
            std::size_t const head = m_head.load(std::memory_order_acquire);
            if (tail == head)
                return false;
            std::memcpy(&a_out, &m_slots[tail % K5S_Record_Capacity], sizeof(a_out));
            m_tail.store(tail + 1, std::memory_order_release);
            return true;
        }

        void drain_discard() noexcept
        {
            m_tail.store(m_head.load(std::memory_order_acquire), std::memory_order_release);
        }

        void reset() noexcept
        {
            m_head.store(0, std::memory_order_relaxed);
            m_tail.store(0, std::memory_order_relaxed);
            m_dropped.store(0, std::memory_order_relaxed);
            m_gate.reset();
        }

        [[nodiscard]] std::uint64_t dropped() const noexcept
        {
            return m_dropped.load(std::memory_order_relaxed);
        }

    private:
        std::array<K5SourceRecord, K5S_Record_Capacity> m_slots{};
        std::atomic<std::size_t> m_head{ 0 };
        std::atomic<std::size_t> m_tail{ 0 };
        std::atomic<std::uint64_t> m_dropped{ 0 };
        ProbePushGate m_gate;
    };

    // C-G5-TRANS v1: a separate source-side ring keeps the frozen CPK5/CPK5S
    // record layouts unchanged. The detour copies only this fixed record; the
    // render drain formats it.
    class TransitionProbeRing
    {
    public:
        bool push(TransitionProbeRecord const& a_record) noexcept
        {
            if (!m_gate.try_enter())
            {
                m_dropped.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            std::size_t const head = m_head.load(std::memory_order_relaxed);
            std::size_t const tail = m_tail.load(std::memory_order_acquire);
            if (head - tail >= Transition_Probe_Source_Capacity)
            {
                m_dropped.fetch_add(1, std::memory_order_relaxed);
                m_gate.leave();
                return false;
            }
            std::memcpy(&m_slots[head % Transition_Probe_Source_Capacity], &a_record,
                sizeof(a_record));
            m_head.store(head + 1, std::memory_order_release);
            m_gate.leave();
            return true;
        }

        bool pop(TransitionProbeRecord& a_out) noexcept
        {
            std::size_t const tail = m_tail.load(std::memory_order_relaxed);
            std::size_t const head = m_head.load(std::memory_order_acquire);
            if (tail == head)
                return false;
            std::memcpy(&a_out, &m_slots[tail % Transition_Probe_Source_Capacity], sizeof(a_out));
            m_tail.store(tail + 1, std::memory_order_release);
            return true;
        }

        void drain_discard() noexcept
        {
            m_tail.store(m_head.load(std::memory_order_acquire), std::memory_order_release);
        }

        void reset() noexcept
        {
            m_head.store(0, std::memory_order_relaxed);
            m_tail.store(0, std::memory_order_relaxed);
            m_dropped.store(0, std::memory_order_relaxed);
            m_gate.reset();
        }

        [[nodiscard]] std::uint64_t dropped() const noexcept
        {
            return m_dropped.load(std::memory_order_relaxed);
        }

    private:
        std::array<TransitionProbeRecord, Transition_Probe_Source_Capacity> m_slots{};
        std::atomic<std::size_t> m_head{ 0 };
        std::atomic<std::size_t> m_tail{ 0 };
        std::atomic<std::uint64_t> m_dropped{ 0 };
        ProbePushGate m_gate;
    };

    K5SourceRing g_k5s_ring;
    TransitionProbeRing g_transition_ring;
    K5SourceWriter g_k5s_writer;
    K5IdentityCache g_k5s_cache;
    K5ReconCounters g_k5s_counters;
    std::atomic<std::uint32_t> g_k5s_seq{ 0 };
    std::atomic<bool> g_k5s_open_failed{ false };
    std::atomic<std::uint64_t> g_transition_records{ 0 };
    std::atomic<std::uint64_t> g_transition_overflow{ 0 };
    std::atomic<std::uint64_t> g_transition_unavailable{ 0 };
    std::atomic<std::uint32_t> g_transition_probe_seq{ 0 };

    std::uintptr_t game_base()
    {
        return REL::Module::get().base();
    }

    // C-G5-E2-B B3/B4: the `on` (F7) lifecycle must also pin the player's third-person source
    // graph, otherwise `g_pinned` stays null and every B3/B4 comparison is skipped. This resolves
    // the same graph the concurrent lifecycle pins, from the render thread at arm time (objects
    // alive, no dereference of stale pointers). Read-only: no graph write, no new detour.
    RE::BShkbAnimationGraph* player_source_graph()
    {
        RE::PlayerCharacter* const player = RE::PlayerCharacter::GetSingleton();
        if (player == nullptr)
            return nullptr;
        RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
        if (!player->GetAnimationGraphManager(manager) || !manager)
            return nullptr;
        RE::BShkbAnimationGraph* chosen = nullptr;
        for (RE::BSTSmartPointer<RE::BShkbAnimationGraph> const& graph : manager->graphs)
        {
            if (!graph || !graph->behaviorGraph || graph->holder != player)
                continue;
            char const* const name = graph->projectName.c_str();
            if (name != nullptr && source_graph_is_first_person(std::string_view{ name }))
                continue;
            chosen = graph.get();
        }
        return chosen;
    }

    // C-G5-E3 (M2): the one clock basis shared with the source timeline.
    std::uint64_t probe_tsc_now() noexcept
    {
#if defined(_MSC_VER)
        return static_cast<std::uint64_t>(__rdtsc());
#else
        return static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
#endif
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

    // 58423's IK/post-process gate (audit29/63581 at 0xbb0040..0xbb0076): four conditions, all
    // read from the graph and one engine-wide byte. This is NOT the payload-overwrite gate at
    // 0xbaffc3 (`[g+0x110] -> [+0x40] -> [+0x88]` with `[g+0x100]`), which v75/v76 mistakenly
    // used: that one copies `[g+0x148]` into the out payload and is unrelated to 58423.
    // `byte[g+0x249]` is rewritten every frame by 63587 from ctx+0x2e (0xbb23b7) or the frame
    // state (0xbb28da), so this gate genuinely varies from frame to frame.
    // Reads the four 58423 gate inputs (see probe_ik_gate_open in the policy header).
    bool ik_gate_open(void const* a_graph) noexcept
    {
        if (a_graph == nullptr)
            return false;
        auto const* const g = static_cast<std::byte const*>(a_graph);
        auto const count = read_offset<std::uint16_t>(g, 0x240);
        auto const mode = read_offset<std::uint8_t>(g, 0x249);
        auto const root = read_offset<void const*>(g, 0x218) != nullptr;
        std::uint8_t global = 0;
        // Engine-wide IK enable byte; an image RVA, read only on the pinned runtime.
        constexpr std::uint32_t Ik_Global_Rva = 0x2022530;
        if (exact_runtime())
            global = *reinterpret_cast<std::uint8_t const*>(game_base() + Ik_Global_Rva);
        return probe_ik_gate_open(count, mode, global, root);
    }

    std::uint32_t caller_rva_of(void* a_caller) noexcept
    {
        auto const value = reinterpret_cast<std::uintptr_t>(a_caller);
        auto const base = game_base();
        return (base != 0 && value >= base) ? static_cast<std::uint32_t>(value - base) : 0xFFFFFFFFu;
    }

    // ---- detour recorders (no allocation, no locks, no engine calls) -----------

    void record_sample_entry(void* a_graph, void* a_caller) noexcept
    {
        ProbeState& s = state();
        ProbeRecorderLease const lease(s.admission);
        if (!lease)
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
        case ProbeCaller::e_plugin:
            c.entries_plugin.fetch_add(1, std::memory_order_relaxed);
            // Identify application entries for the same plugin graph.
            g_plugin_graph.store(a_graph, std::memory_order_relaxed);
            break;
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
        c.eligible_records.fetch_add(1, std::memory_order_relaxed);
        if (!s.ring.push(record))
            c.detail_dropped.fetch_add(1, std::memory_order_relaxed);
    }

    void record_apply_entry(void* a_pose, void* a_table, std::uint32_t a_count, void* a_caller) noexcept
    {
        ProbeState& s = state();
        ProbeRecorderLease const lease(s.admission);
        if (!lease)
            return;
        Counters& c = s.counters;
        c.apply_entries_total.fetch_add(1, std::memory_order_relaxed);
        void const* const pinned = g_pinned.load(std::memory_order_relaxed);
        void* const graph = probe_graph_from_table_argument(a_table);
        ProbeApplyPath const path = classify_apply_path(reinterpret_cast<std::uintptr_t>(a_caller), game_base());
        std::size_t const path_index = probe_apply_path_index(path);
        c.path_entries[path_index].fetch_add(1, std::memory_order_relaxed);
        if (path == ProbeApplyPath::e_unclassified)
            c.unclassified_applies.fetch_add(1, std::memory_order_relaxed);
        bool const is_pinned = pinned != nullptr && graph == pinned;
        if (is_pinned)
            c.apply_entries_pinned.fetch_add(1, std::memory_order_relaxed);
        else
            c.other_apply_entries.fetch_add(1, std::memory_order_relaxed);
        // Range facts (R3/R4) exist only for the pinned source graph: the clone/other graphs in
        // this manager do not carry the same table contents (the first v70 run read capacity 0 and
        // garbage slot words for them, which then produced 436 false count-over-capacity hits).
        std::uint32_t capacity = 0;
        std::uint32_t slot_num_data = 0;
        std::uint32_t const caller_rva = caller_rva_of(a_caller);
        if (is_pinned && plausible_object_pointer(reinterpret_cast<std::uintptr_t>(graph)))
        {
            capacity = read_offset<std::uint32_t>(graph, 0x170);
            slot_num_data = probe_slot_view(graph, Probe_Mix_Slot0_Container_Offset).count;
            if (a_count > capacity)
                c.count_over_capacity.fetch_add(1, std::memory_order_relaxed);
            // R4: the audit's single-lazy-allocation claim means one pose buffer per graph. A
            // different pointer inside one window refutes it, so it is recorded, never smoothed.
            auto const pose = reinterpret_cast<std::uintptr_t>(a_pose);
            if (s.expected_pose == 0)
                s.expected_pose = pose;
            else if (s.expected_pose != pose)
                c.pose_unstable.store(true, std::memory_order_relaxed);
        }
        std::uint32_t const clamped = is_pinned
            ? probe_clamped_count(static_cast<std::int32_t>(a_count), capacity)
            : a_count;
        if (is_pinned && clamped == 0)
            c.clamped_zero.fetch_add(1, std::memory_order_relaxed);
        // The in-flight entry the return recorder will consume; same-tid pairing is structural.
        if (is_pinned)
        {
            ProbeInFlight::Entry entry{};
            entry.graph = graph;
            entry.pose = a_pose;
            entry.requested = a_count;
            entry.clamped = clamped;
            entry.slot_num_data = slot_num_data;
            entry.node_capacity = capacity;
            entry.path = path;
            entry.tsc = __rdtsc();
            if (transition_state_probe() && k5_full_sample())
            {
                std::uint32_t const probe_seq =
                    g_transition_probe_seq.fetch_add(1, std::memory_order_relaxed) + 1;
                entry.probe_seq = probe_seq;
                TransitionProbeRecord probe{};
                auto const* const source_graph = static_cast<RE::BShkbAnimationGraph const*>(graph);
                if (source_graph != nullptr)
                {
                    capture_transition_probe(*source_graph, TransitionProbeSide::source,
                        TransitionProbeSegment::window, TransitionProbePhase::pre_sample, 0, 0,
                        probe_seq, entry.tsc, probe);
                }
                else
                {
                    probe.magic = Transition_Probe_Magic;
                    probe.version = Transition_Probe_Version;
                    probe.side = static_cast<std::uint8_t>(TransitionProbeSide::source);
                    probe.segment = static_cast<std::uint8_t>(TransitionProbeSegment::window);
                    probe.phase = static_cast<std::uint8_t>(TransitionProbePhase::pre_sample);
                    probe.probe_seq = probe_seq;
                    probe.tsc = entry.tsc;
                    probe.flags |= Transition_Probe_Flag_Unavailable;
                }
                if (transition_probe_overflowed(probe))
                    g_transition_overflow.fetch_add(1, std::memory_order_relaxed);
                if ((probe.flags & Transition_Probe_Flag_Unavailable) != 0)
                    g_transition_unavailable.fetch_add(1, std::memory_order_relaxed);
                (void)g_transition_ring.push(probe);
            }
            auto const tid = static_cast<std::uint32_t>(__readgsqword(0x48));
            if (s.in_flight.on_entry(tid, entry) == ProbeInFlight::Outcome::e_overwritten)
                c.overwritten_entries.fetch_add(1, std::memory_order_relaxed);
            // High-water mark of concurrently pending entries, sampled while one is held.
            std::uint32_t const pending = static_cast<std::uint32_t>(s.in_flight.in_flight());
            std::uint32_t current = c.max_in_flight.load(std::memory_order_relaxed);
            while (pending > current
                && !c.max_in_flight.compare_exchange_weak(current, pending, std::memory_order_relaxed))
            {
            }
        }
        // ---- C-G5-E2-B B3/B4: the out-block boundary at the memcpy source ----------------
        // Runs before any of the 63856 scene writes. B3 re-reads the 58423-return `out` block
        // and compares it against the stored return snapshot; B4 compares poseLocal (a_pose)
        // against the same snapshot. No slot: counts as stale, never as success.
        if (is_pinned)
        {
            auto const tid = static_cast<std::uint32_t>(__readgsqword(0x48));
            // The ledger is keyed on &pinned->characterInstance (ctx->character), not the outer
            // BShkbAnimationGraph pointer used to identify the 63856 apply entry.
            void const* const pinned_character = g_pinned_character.load(std::memory_order_relaxed);
            ProbeB3B4Slot const* const slot = s.b3b4.consume(tid, pinned_character);
            if (slot == nullptr)
            {
                ++s.b3_stale;
            }
            else
            {
                std::uint16_t reread_count = 0;
                bool const reread_ok = probe_read_footik_out(slot->out, g_b3b4_reread,
                    Probe_B3B4_Track_Capacity, reread_count);
                if (!reread_ok)
                {
                    s.b3b4_tally.read_failed = true;
                    ++s.b3_no_out;
                }
                else if (reread_count != slot->tracks)
                {
                    // The out block's count changed between 58423's return and the apply: the
                    // stored snapshot no longer describes the block. Count, never resolve.
                    ++s.b3_stale;
                }
                else
                {
                    // v97c one-shot layout diagnostic: the v97b run failed B4 on all 1,167
                    // applies with a systematic 0/1 sample pattern, which is more consistent
                    // with an out-block/poseLocal layout mismatch than with a real rewrite.
                    // Print the raw header and the first tracks once so the next run can settle
                    // the layout instead of guessing again.
                    if (!s.b3b4_first_logged)
                    {
                        s.b3b4_first_logged = true;
                        auto const* const head = static_cast<std::byte const*>(
                            *reinterpret_cast<void const* const*>(slot->out));
                        auto const* const pose = static_cast<ProbeVec4 const*>(
                            static_cast<void const*>(a_pose));
                        logger::info("PROBE LIVENESS b3b4-first out-ptr={:#x} block={:#x} tracks={} pose={:#x} clamped={} hdr32={} hdr34={} out0=({:.4f},{:.4f},{:.4f},{:.4f}) out1=({:.4f},{:.4f},{:.4f},{:.4f}) out2=({:.4f},{:.4f},{:.4f},{:.4f}) pose0=({:.4f},{:.4f},{:.4f},{:.4f}) pose1=({:.4f},{:.4f},{:.4f},{:.4f}) pose2=({:.4f},{:.4f},{:.4f},{:.4f})",
                            reinterpret_cast<std::uintptr_t>(slot->out),
                            reinterpret_cast<std::uintptr_t>(head), slot->tracks,
                            reinterpret_cast<std::uintptr_t>(pose), clamped,
                            read_offset<std::uint16_t>(head, Probe_Footik_Out_Count_Offset),
                            read_offset<std::int16_t>(head, Probe_Footik_Out_Tracks_Offset),
                            slot->snapshot[0].x, slot->snapshot[0].y, slot->snapshot[0].z,
                            slot->snapshot[0].w,
                            slot->snapshot[1].x, slot->snapshot[1].y, slot->snapshot[1].z,
                            slot->snapshot[1].w,
                            slot->snapshot[2].x, slot->snapshot[2].y, slot->snapshot[2].z,
                            slot->snapshot[2].w,
                            pose[0].x, pose[0].y, pose[0].z, pose[0].w,
                            pose[1].x, pose[1].y, pose[1].z, pose[1].w,
                            pose[2].x, pose[2].y, pose[2].z, pose[2].w);
                    }
                    (void)s.b3b4_tally.compare(0, slot->tracks, slot->snapshot.data(), 2,
                        g_b3b4_reread, 2);
                    if (clamped > 0)
                    {
                        std::uint32_t const n = clamped < slot->tracks ? clamped : slot->tracks;
                        // poseLocal is hkQsTransform: translation, rotation, scale per track.
                        (void)s.b3b4_tally.compare(1, n, slot->snapshot.data(), 2,
                            static_cast<ProbeVec4 const*>(static_cast<void const*>(a_pose)), 3);
                    }
                }
            }
        }
        // ---- C-G5-E2-A: synthesised-pose identity (trans + rot of 64112) ----------------
        // poseLocal, both slot track arrays and the weight are still the ones this call uses:
        // 63587 swaps slots only on the next frame. The comparison runs here and is reported by
        // the render drain as a mix-frame record; the detour itself never formats or prints.
        // A frame is recorded even when nothing was compared, so the slot counts are visible and
        // an n==0 outcome is explainable from the log instead of inferred.
        if (is_pinned && clamped > 0)
        {
            float weight = 0.0f;
            if (plausible_object_pointer(reinterpret_cast<std::uintptr_t>(graph)))
                std::memcpy(&weight, static_cast<std::byte const*>(graph) + Probe_Mix_Weight_Offset,
                    sizeof(weight));
            ProbeSlotView const slot0 = probe_slot_view(graph, Probe_Mix_Slot0_Container_Offset);
            ProbeSlotView const slot1 = probe_slot_view(graph, Probe_Mix_Slot1_Container_Offset);
            bool const weight_ok = weight >= 0.0f && weight <= 1.0f;
            // A slot only needs to be readable when it participates at this weight: the fallback
            // branch is single-slot by construction (63581 writes 1.0), so requiring slot1 here
            // would reject every fallback frame (the v73 run read slot1=0 on all 276 frames).
            bool const need0 = weight_ok && weight > 0.0f;
            bool const need1 = weight_ok && weight < 1.0f;
            bool const slots_ok = (!need0 || slot0.valid) && (!need1 || slot1.valid);
            if (!slots_ok)
                s.mix.read_failed = true;
            if (!weight_ok)
                s.mix.weight_out_of_range = true;
            std::uint32_t n = slots_ok
                ? probe_mix_track_count(clamped, weight, slot0.count, slot1.count)
                : 0;
            std::uint32_t failed = 0;
            float worst = 0.0f;
            std::uint32_t frame_fail_foot = 0;
            std::uint32_t frame_fail_other = 0;
            if (slots_ok && weight_ok && n > 0)
            {
                ProbeMixTally frame{};
                frame.frames = 1;
                frame.tracks = n;
                std::uint32_t const bone_count = g_bone_count.load(std::memory_order_relaxed);
                for (std::uint32_t i = 0; i < n; ++i)
                {
                    failed += probe_compare_track(static_cast<std::byte const*>(a_pose),
                        static_cast<std::byte const*>(slot1.tracks),
                        static_cast<std::byte const*>(slot0.tracks), weight, i, frame,
                        g_bone_name_ptrs, bone_count);
                }
                worst = frame.max_delta;
                frame_fail_foot = static_cast<std::uint32_t>(frame.fail_foot);
                frame_fail_other = static_cast<std::uint32_t>(frame.fail_cloth + frame.fail_unexplained);
                // v77 attribution: the real 58423 gate (four conditions, see ik_gate_open above),
                // not the payload-overwrite gate v75/v76 used. A frame is only a strict test of the
                // plain fallback copy when that gate is closed; gate-open residuals are E2-B input.
                bool const ik_ready = ik_gate_open(graph);
                if (ik_ready)
                {
                    ++s.mix.ik_ready_frames;
                    ++s.mix.ik_frames;
                    s.mix.failed_ik += failed;
                    if (failed > 0)
                        ++s.mix.fail_frames_ik;
                }
                else
                {
                    s.mix.failed_no_ik += failed;
                    if (failed > 0)
                        ++s.mix.fail_frames_no_ik;
                }
                if (failed > 0)
                    ++s.mix.fail_frames;
                // v76: the per-component attribution was only ever accumulated into the local frame
                // tally, so the window counters stayed zero while e2a-failed was non-zero.
                s.mix.fail_foot += frame.fail_foot;
                s.mix.fail_cloth += frame.fail_cloth;
                s.mix.fail_unexplained += frame.fail_unexplained;
                for (std::uint32_t k = 0; k < frame.unexplained_bone_count
                    && s.mix.unexplained_bone_count < s.mix.unexplained_bones.size(); ++k)
                {
                    ProbeBoneSample const& src = frame.unexplained_bones[k];
                    ProbeBoneSample* dst = nullptr;
                    for (std::uint32_t j = 0; j < s.mix.unexplained_bone_count; ++j)
                    {
                        if (std::strncmp(s.mix.unexplained_bones[j].name.data(), src.name.data(),
                                Probe_Bone_Name_Length - 1) == 0)
                        {
                            dst = &s.mix.unexplained_bones[j];
                            break;
                        }
                    }
                    if (dst != nullptr)
                        dst->count += src.count;
                    else
                        s.mix.unexplained_bones[s.mix.unexplained_bone_count++] = src;
                }
                s.mix.near_slot0 += frame.near_slot0;
                s.mix.near_slot1 += frame.near_slot1;
                s.mix.near_neither += frame.near_neither;
                s.mix.frames += frame.frames;
                if (probe_mix_mode(weight) == ProbeMixMode::e_main)
                    ++s.mix.main_frames;
                else
                    ++s.mix.fallback_frames;
                s.mix.tracks += frame.tracks;
                s.mix.failed += frame.failed;
                if (frame.max_delta > s.mix.max_delta)
                    s.mix.max_delta = frame.max_delta;
                if (frame.non_finite)
                    s.mix.non_finite = true;
                // The first four failing components are kept for the close row; the sample slots are
                // diagnostic only and never feed a criterion.
                if (frame.sample_count > 0)
                {
                    for (std::uint32_t k = 0; k < frame.sample_count
                        && s.mix.sample_count < ProbeMixTally::Max_Samples; ++k)
                        s.mix.samples[s.mix.sample_count++] = frame.samples[k];
                }
            }
            ProbeRecord mix{};
            mix.graph = reinterpret_cast<std::uintptr_t>(graph);
            mix.weight = weight;
            mix.max_delta = worst;
            mix.mix_failed = failed;
            mix.num240 = n > 0xFFFFu ? 0xFFFFu : static_cast<std::uint16_t>(n);
            mix.clamped = clamped > 0xFFFFu ? 0xFFFFu : static_cast<std::uint16_t>(clamped);
            mix.mix_fail_foot = frame_fail_foot > 0xFFFFu ? 0xFFFFu : static_cast<std::uint16_t>(frame_fail_foot);
            mix.mix_fail_other = frame_fail_other > 0xFFFFu ? 0xFFFFu : static_cast<std::uint16_t>(frame_fail_other);
            mix.slot0_count = slot0.count > 0xFFFFu ? 0xFFFFu : static_cast<std::uint16_t>(slot0.count);
            mix.slot1_count = slot1.count > 0xFFFFu ? 0xFFFFu : static_cast<std::uint16_t>(slot1.count);
            // The four 58423 gate bytes, so a gate-closed frame is directly readable.
            if (is_pinned && plausible_object_pointer(reinterpret_cast<std::uintptr_t>(graph)))
            {
                auto const* const g = static_cast<std::byte const*>(graph);
                mix.ik_mode = read_offset<std::uint8_t>(g, 0x249);
                mix.ik_count = read_offset<std::uint16_t>(g, 0x240);
                mix.ik_global = static_cast<std::uint8_t>(exact_runtime()
                    ? *reinterpret_cast<std::uint8_t const*>(game_base() + 0x2022530) : 0);
                mix.ik_root = read_offset<void const*>(g, 0x218) != nullptr ? 1 : 0;
            }
            mix.path = static_cast<std::uint8_t>(path_index);
            mix.tid = static_cast<std::uint32_t>(__readgsqword(0x48));
            mix.kind = k_mix_frame;
            // C-G5-E3 (M2): one TSC per pose-producing step for the pinned graph. The index rides
            // in `caller_or_count`, which mix frames otherwise leave unused.
            if (is_pinned)
                mix.caller_or_count = s.note_step(probe_tsc_now());
            c.eligible_records.fetch_add(1, std::memory_order_relaxed);
            if (!s.ring.push(mix))
                c.detail_dropped.fetch_add(1, std::memory_order_relaxed);
        }
        // The argument count precedes the native table clamp and any scene writes.
        bool const is_plugin = g_plugin_graph.load(std::memory_order_relaxed) == graph;
        if (!is_pinned && !is_plugin)
            return;
        ProbeRecord record{};
        record.graph = reinterpret_cast<std::uintptr_t>(graph);
        record.caller_or_count = a_count;
        record.caller_rva = caller_rva;
        record.num240 = clamped > 0xFFFFu ? 0xFFFFu : static_cast<std::uint16_t>(clamped);
        record.clamped = record.num240;
        record.capacity = capacity > 0xFFFFu ? 0xFFFFu : static_cast<std::uint16_t>(capacity);
        record.slot_num_data = slot_num_data > 0xFFFFu ? 0xFFFFu : static_cast<std::uint16_t>(slot_num_data);
        record.path = static_cast<std::uint8_t>(path_index);
        record.tid = static_cast<std::uint32_t>(__readgsqword(0x48));
        record.kind = k_apply_entry;
        c.eligible_records.fetch_add(1, std::memory_order_relaxed);
        if (!s.ring.push(record))
            c.detail_dropped.fetch_add(1, std::memory_order_relaxed);
    }

    // C-G5-CAL K3-S: source-side per-apply snapshot. Runs at the pinned graph's 63856 apply
    // return, after the scene writes and before the next update. The detour only copies a fixed
    // record into the ring; the render drain writes it to disk.
    void record_k3s_return(ProbeInFlight::Entry const& a_entry) noexcept
    {
        if (sampling_liveness_mode() == ProbeLivenessMode::e_off)
            return;
        if (a_entry.pose == nullptr)
        {
            if (k5_full_sample())
                g_k5s_counters.no_pose.fetch_add(1, std::memory_order_relaxed);
            else
                g_k3s_no_pose.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        // C-G5-CAL K5: CPK5 format v1 replaces the K3-S record when K5FullSample is on.
        // Same pose copy/two-read rule and clip walk, plus per-clip identity columns from
        // the detour-owned cache. K5 counters replace the K3S counters; the two paths never
        // run in the same window.
        if (k5_full_sample())
        {
            auto const* const pose = static_cast<RE::hkQsTransform const*>(a_entry.pose);
            std::uint32_t const count = a_entry.clamped < K5_Track_Capacity
                ? a_entry.clamped : K5_Track_Capacity;
            if (count == 0)
                return;
            alignas(16) RE::hkQsTransform first[K5_Track_Capacity]{};
            alignas(16) RE::hkQsTransform second[K5_Track_Capacity]{};
            std::memcpy(first, pose, count * sizeof(RE::hkQsTransform));
            std::memcpy(second, pose, count * sizeof(RE::hkQsTransform));
            bool const same = std::memcmp(first, second, count * sizeof(RE::hkQsTransform)) == 0;
            if (!same)
                g_k5s_counters.two_read_bad.fetch_add(1, std::memory_order_relaxed);
            K5SourceRecord record{};
            record.seq = g_k5s_seq.fetch_add(1, std::memory_order_relaxed) + 1;
            record.tsc = __rdtsc();
            record.path = static_cast<std::uint8_t>(probe_apply_path_index(a_entry.path));
            record.two_read_equal = same ? 1u : 0u;
            record.track_count = static_cast<std::uint16_t>(count);
            for (std::uint32_t i = 0; i < count; ++i)
            {
                float* const v = record.tracks[i].value;
                std::memcpy(v, &first[i].translation, 3 * sizeof(float));
                std::memcpy(v + 3, &first[i].rotation, 4 * sizeof(float));
                std::memcpy(v + 7, &first[i].scale, 3 * sizeof(float));
            }
            auto const* const graph = static_cast<RE::BShkbAnimationGraph const*>(a_entry.graph);
            RE::hkbBehaviorGraph* const behavior = graph ? graph->characterInstance.behaviorGraph.get() : nullptr;
            if (behavior && behavior->activeNodes)
            {
                RE::NodeList const& nodes = *behavior->activeNodes;
                std::size_t const limit = std::min<std::size_t>(nodes.size(), 256);
                bool identity_failed_record = false;
                for (std::size_t i = 0; i < limit; ++i)
                {
                    RE::hkbNode* const node = nodes.data()[i].nodeClone;
                    if (node == nullptr)
                        continue;
                    if (*reinterpret_cast<std::uintptr_t const*>(node) != RE::VTABLE_hkbClipGenerator[0].address())
                        continue;
                    if (record.clip_count >= K5_Clip_Capacity)
                    {
                        record.flags |= K5_Flag_Clip_Overflow;
                        g_k5s_counters.clip_overflow.fetch_add(1, std::memory_order_relaxed);
                        break;
                    }
                    auto const* const clip = static_cast<RE::hkbClipGenerator const*>(node);
                    K5ClipRecord& out = record.clips[record.clip_count];
                    out.node_ptr = reinterpret_cast<std::uintptr_t>(clip);
                    out.binding_ptr = plausible_object_pointer(reinterpret_cast<std::uintptr_t>(clip->binding))
                        ? reinterpret_cast<std::uintptr_t>(clip->binding)
                        : 0;
                    out.binding_index = clip->animationBindingIndex;
                    out.local_time_bits = std::bit_cast<std::uint32_t>(clip->localTime);
                    out.mode = static_cast<std::uint8_t>(clip->mode.get());
                    char const* const name = clip->animationName.c_str();
                    if (name != nullptr)
                    {
                        std::size_t k = 0;
                        for (; k + 1 < K5_Clip_Name_Length && name[k] != '\0'; ++k)
                            out.name[k] = name[k];
                        out.name[k] = '\0';
                    }
                    K5IdentityCache::Entry const entry = out.binding_ptr != 0
                        ? g_k5s_cache.identity_for(reinterpret_cast<
                            RE::hkaAnimationBinding const*>(out.binding_ptr))
                        : K5IdentityCache::Entry{ false, AnimationResourceIdentity{} };
                    if (!entry.ok)
                    {
                        identity_failed_record = true;
                        g_k5s_counters.identity_failed_clips.fetch_add(1, std::memory_order_relaxed);
                    }
                    out.duration_bits = entry.identity.duration_bits;
                    out.data_fnv = entry.identity.data_hash;
                    out.blockoff_fnv = entry.identity.block_offsets_hash;
                    out.map_fnv = entry.identity.map_hash;
                    out.map_count = entry.identity.map_count;
                    ++record.clip_count;
                }
                if (identity_failed_record)
                    g_k5s_counters.drift_records.fetch_add(1, std::memory_order_relaxed);
            }
            g_k5s_counters.attempted.fetch_add(1, std::memory_order_relaxed);
            if (transition_state_probe())
            {
                TransitionProbeRecord probe{};
                if (graph != nullptr)
                {
                    capture_transition_probe(*graph, TransitionProbeSide::source,
                        TransitionProbeSegment::window, TransitionProbePhase::post_sample,
                        record.seq, 0, a_entry.probe_seq, record.tsc, probe);
                }
                else
                {
                    probe.magic = Transition_Probe_Magic;
                    probe.version = Transition_Probe_Version;
                    probe.side = static_cast<std::uint8_t>(TransitionProbeSide::source);
                    probe.segment = static_cast<std::uint8_t>(TransitionProbeSegment::window);
                    probe.phase = static_cast<std::uint8_t>(TransitionProbePhase::post_sample);
                    probe.sequence = record.seq;
                    probe.probe_seq = a_entry.probe_seq;
                    probe.tsc = record.tsc;
                    probe.flags |= Transition_Probe_Flag_Unavailable;
                }
                if (transition_probe_overflowed(probe))
                    g_transition_overflow.fetch_add(1, std::memory_order_relaxed);
                if ((probe.flags & Transition_Probe_Flag_Unavailable) != 0)
                    g_transition_unavailable.fetch_add(1, std::memory_order_relaxed);
                (void)g_transition_ring.push(probe);
            }
            (void)g_k5s_ring.push(record);
            return;
        }
        auto const* const pose = static_cast<RE::hkQsTransform const*>(a_entry.pose);
        std::uint32_t const count = a_entry.clamped < K3S_Track_Capacity
            ? a_entry.clamped : K3S_Track_Capacity;
        if (count == 0)
            return;
        alignas(16) RE::hkQsTransform first[K3S_Track_Capacity]{};
        alignas(16) RE::hkQsTransform second[K3S_Track_Capacity]{};
        std::memcpy(first, pose, count * sizeof(RE::hkQsTransform));
        std::memcpy(second, pose, count * sizeof(RE::hkQsTransform));
        bool const same = std::memcmp(first, second, count * sizeof(RE::hkQsTransform)) == 0;
        if (!same)
            g_k3s_two_read_bad.fetch_add(1, std::memory_order_relaxed);
        K3SourceRecord record{};
        record.seq = g_k3s_seq.fetch_add(1, std::memory_order_relaxed) + 1;
        record.tsc = __rdtsc();
        record.path = static_cast<std::uint8_t>(probe_apply_path_index(a_entry.path));
        record.two_read_equal = same ? 1u : 0u;
        record.track_count = static_cast<std::uint16_t>(count);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            float* const v = record.tracks[i].value;
            std::memcpy(v, &first[i].translation, 3 * sizeof(float));
            std::memcpy(v + 3, &first[i].rotation, 4 * sizeof(float));
            std::memcpy(v + 7, &first[i].scale, 3 * sizeof(float));
        }
        auto const* const graph = static_cast<RE::BShkbAnimationGraph const*>(a_entry.graph);
        RE::hkbBehaviorGraph* const behavior = graph ? graph->characterInstance.behaviorGraph.get() : nullptr;
        if (behavior && behavior->activeNodes)
        {
            RE::NodeList const& nodes = *behavior->activeNodes;
            std::size_t const limit = std::min<std::size_t>(nodes.size(), 256);
            for (std::size_t i = 0; i < limit; ++i)
            {
                RE::hkbNode* const node = nodes.data()[i].nodeClone;
                if (node == nullptr)
                    continue;
                if (*reinterpret_cast<std::uintptr_t const*>(node) != RE::VTABLE_hkbClipGenerator[0].address())
                    continue;
                if (record.clip_count >= K3S_Clip_Capacity)
                {
                    record.flags |= K3S_Flag_Clip_Overflow;
                    g_k3s_clip_overflow.fetch_add(1, std::memory_order_relaxed);
                    break;
                }
                auto const* const clip = static_cast<RE::hkbClipGenerator const*>(node);
                K3ClipRecord& out = record.clips[record.clip_count];
                out.binding_index = clip->animationBindingIndex;
                out.local_time_bits = std::bit_cast<std::uint32_t>(clip->localTime);
                out.mode = static_cast<std::uint8_t>(clip->mode.get());
                char const* const name = clip->animationName.c_str();
                if (name != nullptr)
                {
                    std::size_t k = 0;
                    for (; k + 1 < K3_Clip_Name_Length && name[k] != '\0'; ++k)
                        out.name[k] = name[k];
                    out.name[k] = '\0';
                }
                ++record.clip_count;
            }
        }
        (void)g_k3s_ring.push(record);
    }

    // Return-side recorder (C-G5-E1 v3): consumes the thread's in-flight entry, so completion is
    // attributed to the same thread that entered 63856 without reading registers at the return
    // site. It runs after the scene writes, before the relocated `add rsp,0x88`. A return with no
    // pending entry is an orphan only for threads that have owned a tracked entry; every other
    // thread belongs to a graph this window never pinned, and those returns are counted separately
    // so they cannot drown the orphan signal.
    void record_apply_return() noexcept
    {
        ProbeState& s = state();
        ProbeRecorderLease const lease(s.admission);
        if (!lease)
            return;
        Counters& c = s.counters;
        c.apply_returns_total.fetch_add(1, std::memory_order_relaxed);
        auto const tid = static_cast<std::uint32_t>(__readgsqword(0x48));
        ProbeInFlight::Entry entry{};
        if (!s.in_flight.on_return(tid, entry))
        {
            if (s.in_flight.ever_tracked(tid))
                c.orphan_returns.fetch_add(1, std::memory_order_relaxed);
            else
                c.returns_untracked.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        std::uint32_t const pending = static_cast<std::uint32_t>(s.in_flight.in_flight());
        std::uint32_t current = c.max_in_flight.load(std::memory_order_relaxed);
        while (pending > current
            && !c.max_in_flight.compare_exchange_weak(current, pending, std::memory_order_relaxed))
        {
        }
        c.path_completions[probe_apply_path_index(entry.path)].fetch_add(1, std::memory_order_relaxed);
        // Completions are matched by path as well, so the close row can show entry/completion per
        // path; the pinned flag is re-derived from the entry's graph rather than this thread.
        void const* const pinned = g_pinned.load(std::memory_order_relaxed);
        bool const is_pinned = pinned != nullptr && entry.graph == pinned;
        if (is_pinned)
        {
            c.completions_pinned.fetch_add(1, std::memory_order_relaxed);
            if (entry.clamped != 0)
                c.paired_completions.fetch_add(1, std::memory_order_relaxed);
            record_k3s_return(entry);
        }
        bool const is_plugin = g_plugin_graph.load(std::memory_order_relaxed) == entry.graph;
        if (!is_pinned && !is_plugin)
            return;
        ProbeRecord record{};
        record.graph = reinterpret_cast<std::uintptr_t>(entry.graph);
        record.caller_or_count = entry.requested;
        record.num240 = entry.clamped > 0xFFFFu ? 0xFFFFu : static_cast<std::uint16_t>(entry.clamped);
        record.clamped = record.num240;
        record.capacity = entry.node_capacity > 0xFFFFu ? 0xFFFFu : static_cast<std::uint16_t>(entry.node_capacity);
        record.slot_num_data = entry.slot_num_data > 0xFFFFu ? 0xFFFFu : static_cast<std::uint16_t>(entry.slot_num_data);
        record.path = static_cast<std::uint8_t>(probe_apply_path_index(entry.path));
        record.tid = tid;
        record.kind = k_apply_return;
        c.eligible_records.fetch_add(1, std::memory_order_relaxed);
        if (!s.ring.push(record))
            c.detail_dropped.fetch_add(1, std::memory_order_relaxed);
    }
    // 58423 entry: record the out block and snapshot its tracks before foot IK runs.
    void record_footik_entry(void* a_driver, void* a_ctx, void* a_out, std::uint32_t a_flag) noexcept
    {
        (void)a_driver;
        (void)a_flag;
        ProbeState& s = state();
        ProbeRecorderLease const lease(s.admission);
        if (!lease)
            return;
        ProbeFootikTally& tally = s.footik_tally;
        ++tally.calls;
        auto const tid = static_cast<std::uint32_t>(__readgsqword(0x48));
        ProbeFootikInFlight::Entry entry{};
        entry.out = a_out;
        entry.character = probe_b3b4_context_character(a_ctx);
        entry.tsc = __rdtsc();
        std::uint16_t snapshot_count = 0;
        bool const entry_ok = probe_read_footik_out(a_out, entry_tracks(), 116, snapshot_count);
        if (!entry_ok)
            return;
        entry.tracks = snapshot_count;
        if (s.footik.on_entry(tid, entry) == ProbeFootikInFlight::Outcome::e_overwritten)
            s.counters.footik_overwritten.fetch_add(1, std::memory_order_relaxed);
        ProbeRecord record{};
        record.graph = reinterpret_cast<std::uintptr_t>(a_out);
        record.tid = tid;
        record.kind = k_footik_entry;
        record.num240 = entry.tracks > 0xFFFFu ? 0xFFFFu : static_cast<std::uint16_t>(entry.tracks);
        s.counters.eligible_records.fetch_add(1, std::memory_order_relaxed);
        if (!s.ring.push(record))
            s.counters.detail_dropped.fetch_add(1, std::memory_order_relaxed);
    }

    // 58423 return: snapshot the out tracks again, compare against the entry snapshot, and push
    // one b-row. The out block is what 63581 memcpy-s into poseLocal on the same frame.
    void record_footik_return() noexcept
    {
        ProbeState& s = state();
        ProbeRecorderLease const lease(s.admission);
        if (!lease)
            return;
        auto const tid = static_cast<std::uint32_t>(__readgsqword(0x48));
        ProbeFootikInFlight::Entry entry{};
        if (!s.footik.on_return(tid, entry))
            return;
        ProbeFootikTally& tally = s.footik_tally;
        std::uint16_t count = 0;
        bool const read_ok = probe_read_footik_out(entry.out, return_tracks(), 116, count);
        std::uint32_t rewritten = 0;
        if (read_ok && entry.tracks > 0 && entry.tracks == count)
        {
            ++tally.compared;
            std::uint32_t const n = count > entry.tracks ? entry.tracks : count;
            for (std::uint32_t i = 0; i < n; ++i)
            {
                float const e_t[3] = { entry_tracks()[i*2].x, entry_tracks()[i*2].y, entry_tracks()[i*2].z };
                float const r_t[3] = { return_tracks()[i*2].x, return_tracks()[i*2].y, return_tracks()[i*2].z };
                for (std::uint8_t c = 0; c < 3; ++c)
                {
                    float const delta = probe_mix_delta(e_t[c], r_t[c]);
                    if (delta != 0.0f)
                    {
                        ++rewritten;
                        if (delta > tally.max_delta)
                            tally.max_delta = delta;
                    }
                }
                float const e_r[4] = { entry_tracks()[i*2+1].x, entry_tracks()[i*2+1].y, entry_tracks()[i*2+1].z, entry_tracks()[i*2+1].w };
                float const r_r[4] = { return_tracks()[i*2+1].x, return_tracks()[i*2+1].y, return_tracks()[i*2+1].z, return_tracks()[i*2+1].w };
                for (std::uint8_t c = 0; c < 4; ++c)
                {
                    float const delta = probe_mix_delta(e_r[c], r_r[c]);
                    if (delta != 0.0f)
                    {
                        ++rewritten;
                        if (delta > tally.max_delta)
                            tally.max_delta = delta;
                    }
                }
            }
            tally.rewritten_components += rewritten;
            tally.last_frame_rewritten = rewritten;
        }
        // C-G5-E2-B B3/B4: store the return snapshot only for the pinned graph's calls. The
        // character identity comes from 58423's hkbContext (second argument, +0x00). A pending
        // slot from an earlier return on the same thread that never reached apply is overwritten
        // and counted; the pre-registration forbids picking one of several returns.
        void const* const pinned = g_pinned_character.load(std::memory_order_relaxed);
        void const* const character = entry.character;
        if (pinned != nullptr && character == pinned)
        {
            if (read_ok && count > 0)
            {
                if (s.b3b4.match(tid, pinned) != nullptr)
                    ++s.b3_multi;
                switch (s.b3b4.on_return(tid, entry.out, character, count, return_tracks()))
                {
                case ProbeB3B4Ledger::Outcome::e_overwritten:
                    ++s.b3_overwritten;
                    break;
                case ProbeB3B4Ledger::Outcome::e_no_slot:
                    ++s.b3_no_out;
                    break;
                default:
                    break;
                }
            }
            else
            {
                ++s.b3_no_out;
            }
        }
        else if (pinned != nullptr)
        {
            ++s.b3_behavior_mismatch;
        }
        ProbeRecord record{};
        record.graph = reinterpret_cast<std::uintptr_t>(entry.out);
        record.tid = tid;
        record.kind = k_footik_return;
        record.num240 = entry.tracks > 0xFFFFu ? 0xFFFFu : static_cast<std::uint16_t>(entry.tracks);
        record.clamped = count > 0xFFFFu ? 0xFFFFu : static_cast<std::uint16_t>(count);
        record.mix_failed = rewritten;
        s.counters.eligible_records.fetch_add(1, std::memory_order_relaxed);
        if (!s.ring.push(record))
            s.counters.detail_dropped.fetch_add(1, std::memory_order_relaxed);
    }

    // C-G5-CAL K3-S: render-thread drain. Opens the artifact lazily and writes one fixed record
    // per drained slot; any failure stops the drain and marks the window invalid.
    void drain_k3s_records() noexcept
    {
        if (sampling_liveness_mode() == ProbeLivenessMode::e_off)
            return;
        K3SourceRecord record{};
        while (g_k3s_ring.pop(record))
        {
            if (!g_k3s_writer.is_open())
            {
                auto const log_dir = SKSE::log::log_directory();
                if (!log_dir)
                {
                    g_k3s_open_failed.store(true, std::memory_order_relaxed);
                    g_k3s_ring.drain_discard();
                    return;
                }
                std::time_t const now = std::time(nullptr);
                std::tm utc{};
                (void)gmtime_s(&utc, &now);
                char stamp[32]{};
                (void)std::strftime(stamp, sizeof(stamp), "%Y%m%dT%H%M%SZ", &utc);
                std::string const path = fmt::format("{}/CharacterPanel-g5k3s-{}-{}.bin",
                    log_dir->string(), Plugin::Plugin_Build_Identity, stamp);
                if (!g_k3s_writer.open(path, Plugin::Plugin_Build_Identity))
                {
                    g_k3s_open_failed.store(true, std::memory_order_relaxed);
                    g_k3s_ring.drain_discard();
                    return;
                }
                logger::info("K3S SNAPSHOT open path='{}' identity='{}'", path,
                    Plugin::Plugin_Build_Identity);
            }
            if (!g_k3s_writer.append(record))
            {
                // Capacity rejection is reported by overflow=true, not as an I/O failure.
                if (!g_k3s_writer.overflow())
                    g_k3s_write_failed.fetch_add(1, std::memory_order_relaxed);
                g_k3s_ring.drain_discard();
                return;
            }
            g_k3s_records.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // C-G5-CAL K5: render-thread drain for the CPK5 source artifact (mirror of the K3-S
    // drain; the record type and capacity differ, nothing else).
    void drain_k5s_records() noexcept
    {
        if (sampling_liveness_mode() == ProbeLivenessMode::e_off || !k5_full_sample())
            return;
        K5SourceRecord record{};
        while (g_k5s_ring.pop(record))
        {
            if (!g_k5s_writer.is_open())
            {
                auto const log_dir = SKSE::log::log_directory();
                if (!log_dir)
                {
                    g_k5s_open_failed.store(true, std::memory_order_relaxed);
                    g_k5s_ring.drain_discard();
                    return;
                }
                std::time_t const now = std::time(nullptr);
                std::tm utc{};
                (void)gmtime_s(&utc, &now);
                char stamp[32]{};
                (void)std::strftime(stamp, sizeof(stamp), "%Y%m%dT%H%M%SZ", &utc);
                std::string const path = fmt::format("{}/CharacterPanel-g5k5s-{}-{}.bin",
                    log_dir->string(), Plugin::Plugin_Build_Identity, stamp);
                if (!g_k5s_writer.open(path, Plugin::Plugin_Build_Identity))
                {
                    g_k5s_open_failed.store(true, std::memory_order_relaxed);
                    g_k5s_ring.drain_discard();
                    return;
                }
                logger::info("K5 SOURCE open path='{}' identity='{}'", path,
                    Plugin::Plugin_Build_Identity);
            }
            if (!g_k5s_writer.append(record))
            {
                // Capacity rejection is reported by overflow=true, not as an I/O failure.
                if (!g_k5s_writer.overflow())
                    g_k5s_counters.write_failed.fetch_add(1, std::memory_order_relaxed);
                g_k5s_ring.drain_discard();
                return;
            }
            g_k5s_counters.recorded.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // C-G5-TRANS v1: render-thread drain for the separate transition/blender probe ring.
    void drain_transition_probe_records() noexcept
    {
        if (!transition_state_probe() || sampling_liveness_mode() == ProbeLivenessMode::e_off ||
            !k5_full_sample())
            return;
        TransitionProbeRecord record{};
        while (g_transition_ring.pop(record))
        {
            log_transition_probe(record);
            g_transition_records.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void reset_k3s() noexcept
    {
        g_k3s_ring.reset();
        g_k3s_seq.store(0, std::memory_order_relaxed);
        g_k3s_records.store(0, std::memory_order_relaxed);
        g_k3s_two_read_bad.store(0, std::memory_order_relaxed);
        g_k3s_clip_overflow.store(0, std::memory_order_relaxed);
        g_k3s_no_pose.store(0, std::memory_order_relaxed);
        g_k3s_write_failed.store(0, std::memory_order_relaxed);
        g_k3s_open_failed.store(false, std::memory_order_relaxed);
        (void)g_k3s_writer.close();
        g_k5s_ring.reset();
        g_k5s_seq.store(0, std::memory_order_relaxed);
        g_k5s_open_failed.store(false, std::memory_order_relaxed);
        g_k5s_counters.attempted.store(0, std::memory_order_relaxed);
        g_k5s_counters.recorded.store(0, std::memory_order_relaxed);
        g_k5s_counters.two_read_bad.store(0, std::memory_order_relaxed);
        g_k5s_counters.clip_overflow.store(0, std::memory_order_relaxed);
        g_k5s_counters.no_pose.store(0, std::memory_order_relaxed);
        g_k5s_counters.write_failed.store(0, std::memory_order_relaxed);
        g_k5s_counters.identity_failed_clips.store(0, std::memory_order_relaxed);
        g_k5s_counters.drift_records.store(0, std::memory_order_relaxed);
        (void)g_k5s_writer.close();
        g_transition_ring.reset();
        g_transition_records.store(0, std::memory_order_relaxed);
        g_transition_overflow.store(0, std::memory_order_relaxed);
        g_transition_unavailable.store(0, std::memory_order_relaxed);
        g_transition_probe_seq.store(0, std::memory_order_relaxed);
    }

    void drain_probe_records(ProbeState& s)
    {
        ProbeRecord record{};
        while (s.ring.pop(record))
        {
            ++s.drained_records;
            switch (record.kind)
            {
            case k_entry:
                ++s.drained_samples;
                break;
            case k_apply_entry:
                ++s.drained_applies;
                break;
            case k_mix_frame:
                ++s.drained_mix_frames;
                break;
            case k_footik_entry:
                ++s.drained_footik_entries;
                break;
            case k_footik_return:
                ++s.drained_footik_returns;
                break;
            default:
                ++s.drained_returns;
                break;
            }
            if (record.kind == k_entry)
            {
                ProbePairing::Outcome const outcome = s.pairing.on_entry(record.tid, reinterpret_cast<void*>(record.graph));
                if (outcome == ProbePairing::Outcome::e_paired)
                    ++s.tracked_entries;
                else if (outcome == ProbePairing::Outcome::e_no_slot)
                    ++s.pairing_no_slot;
            }
            else if (record.kind == k_apply_entry)
            {
                ProbePairing::Outcome const outcome = s.pairing.on_apply_entry(record.tid, reinterpret_cast<void*>(record.graph));
                if (outcome == ProbePairing::Outcome::e_paired)
                    ++s.paired_apply_entries;
                else if (outcome == ProbePairing::Outcome::e_graph_mismatch)
                    ++s.pairing_mismatch;
                else if (outcome == ProbePairing::Outcome::e_unattributed)
                    ++s.orphan_applies;
            }
            // k_apply_return pairs through ProbeInFlight at record time, not through this ledger.
            if (s.detail_lines >= Probe_Liveness_Detail_Budget)
                continue;
            ++s.detail_lines;
            ++s.detail_seq;
            if (record.kind == k_entry)
            {
                logger::info("PROBE LIVENESS sample-entry drain-seq={} tid={} graph={:#x} caller={:#x} num240={} flag246={}",
                    s.detail_seq, record.tid, record.graph, record.caller_or_count, record.num240, record.flag246);
            }
            else if (record.kind == k_apply_entry)
            {
                logger::info("PROBE LIVENESS apply-entry drain-seq={} tid={} graph={:#x} path={} caller={:#x} requested={} clamped={} capacity={} slot-num-data={}",
                    s.detail_seq, record.tid, record.graph,
                    probe_apply_path_name(static_cast<ProbeApplyPath>(record.path)),
                    record.caller_rva, record.caller_or_count, record.clamped, record.capacity,
                    record.slot_num_data);
            }
            else if (record.kind == k_mix_frame)
            {
                logger::info("PROBE LIVENESS e2a tid={} graph={:#x} path={} step={} n={} clamped={} slot0={} slot1={} w={:.6f} ik=[mode:{} count:{} global:{} root:{}] mix-failed={} foot={} other={} max-delta={:.3e}",
                    record.tid, record.graph, probe_apply_path_name(static_cast<ProbeApplyPath>(record.path)),
                    record.caller_or_count,
                    record.num240, record.clamped, record.slot0_count, record.slot1_count,
                    record.weight, static_cast<unsigned>(record.ik_mode), record.ik_count,
                    static_cast<unsigned>(record.ik_global), static_cast<unsigned>(record.ik_root),
                    record.mix_failed, record.mix_fail_foot, record.mix_fail_other, record.max_delta);
            }
            else if (record.kind == k_footik_entry)
            {
                logger::info("PROBE LIVENESS b-entry tid={} out={:#x} tracks={}",
                    record.tid, record.graph, record.num240);
            }
            else if (record.kind == k_footik_return)
            {
                logger::info("PROBE LIVENESS b-return tid={} out={:#x} tracks-entry={} tracks-return={} rewritten={}",
                    record.tid, record.graph, record.num240, record.clamped, record.mix_failed);
            }
            else
            {
                logger::info("PROBE LIVENESS apply-return drain-seq={} tid={} graph={:#x} path={} clamped={} capacity={} slot-num-data={}",
                    s.detail_seq, record.tid, record.graph,
                    probe_apply_path_name(static_cast<ProbeApplyPath>(record.path)),
                    record.clamped, record.capacity, record.slot_num_data);
            }
        }
    }

    void finalize_probe_close(ProbeState& s)
    {
        if (!s.close_pending || !s.admission.quiescent())
            return;
        drain_probe_records(s);
        drain_k3s_records();
        drain_k5s_records();
        drain_transition_probe_records();
        if (transition_state_probe() && sampling_liveness_mode() != ProbeLivenessMode::e_off &&
            k5_full_sample())
        {
            logger::info(
                "TRANSITION PROBE close side=source records={} dropped={} overflow={} unavailable={}",
                g_transition_records.load(std::memory_order_relaxed), g_transition_ring.dropped(),
                g_transition_overflow.load(std::memory_order_relaxed),
                g_transition_unavailable.load(std::memory_order_relaxed));
        }
        if (sampling_liveness_mode() != ProbeLivenessMode::e_off && !s.k3s_closed &&
            !k5_full_sample())
        {
            s.k3s_closed = true;
            bool const had_file = g_k3s_writer.is_open();
            (void)g_k3s_writer.close();
            logger::info("K3S SNAPSHOT close path='{}' records={} dropped={} overflow={} two-read-bad={} clip-overflow={} no-pose={} write-failed={} open-failed={}",
                had_file ? g_k3s_writer.path() : std::string{},
                g_k3s_records.load(std::memory_order_relaxed),
                g_k3s_ring.dropped(), g_k3s_writer.overflow(),
                g_k3s_two_read_bad.load(std::memory_order_relaxed),
                g_k3s_clip_overflow.load(std::memory_order_relaxed),
                g_k3s_no_pose.load(std::memory_order_relaxed),
                g_k3s_write_failed.load(std::memory_order_relaxed),
                g_k3s_open_failed.load(std::memory_order_relaxed));
        }
        // C-G5-CAL K5: source-window close row, RECON row and JSON ledger. The close row
        // always accompanies a K5 window; the RECON row and ledger follow the
        // K5Reconciliation key (K6 plan §10.1). Artifact hashes are registered offline.
        if (sampling_liveness_mode() != ProbeLivenessMode::e_off && k5_full_sample())
        {
            (void)g_k5s_writer.close();
            std::uint64_t const attempted = g_k5s_counters.attempted.load(std::memory_order_relaxed);
            std::uint64_t const recorded = g_k5s_counters.recorded.load(std::memory_order_relaxed);
            std::uint64_t const dropped = g_k5s_ring.dropped();
            logger::info("K5 SOURCE close path='{}' records={} attempted={} dropped={} overflow={} two-read-bad={} clip-overflow={} no-pose={} write-failed={} identity-failed={} drift-records={} open-failed={}",
                recorded > 0 || attempted > 0 ? g_k5s_writer.path() : std::string{},
                recorded, attempted, dropped, g_k5s_writer.overflow(),
                g_k5s_counters.two_read_bad.load(std::memory_order_relaxed),
                g_k5s_counters.clip_overflow.load(std::memory_order_relaxed),
                g_k5s_counters.no_pose.load(std::memory_order_relaxed),
                g_k5s_counters.write_failed.load(std::memory_order_relaxed),
                g_k5s_counters.identity_failed_clips.load(std::memory_order_relaxed),
                g_k5s_counters.drift_records.load(std::memory_order_relaxed),
                g_k5s_open_failed.load(std::memory_order_relaxed));
            if (k5_reconciliation())
            {
                logger::info("K5 RECON side=source expected={} attempted={} recorded={} missing={} truncated={} "
                    "two-read-bad={} clip-overflow={} identity-failed={} drift-records={} dropped={} no-pose={}",
                    attempted, attempted, recorded,
                    attempted > recorded + dropped ? attempted - recorded - dropped : 0,
                    g_k5s_writer.overflow() ? 1u : 0u,
                    g_k5s_counters.two_read_bad.load(std::memory_order_relaxed),
                    g_k5s_counters.clip_overflow.load(std::memory_order_relaxed),
                    g_k5s_counters.identity_failed_clips.load(std::memory_order_relaxed),
                    g_k5s_counters.drift_records.load(std::memory_order_relaxed),
                    dropped,
                    g_k5s_counters.no_pose.load(std::memory_order_relaxed));
                if (g_k5s_writer.path().empty())
                {
                    logger::info("K5 RECON ledger path='' status=no-artifact");
                }
                else
                {
                    std::string json_path = g_k5s_writer.path();
                    json_path.replace(json_path.size() - 4, 4, ".json");
                    std::ofstream json(json_path.c_str(), std::ios::binary | std::ios::trunc);
                    if (!json)
                    {
                        logger::info("K5 RECON ledger path='{}' status=write-failed", json_path);
                    }
                    else
                    {
                        std::string escaped;
                        escaped.reserve(g_k5s_writer.path().size());
                        for (char const c : g_k5s_writer.path())
                        {
                            if (c == '\\' || c == '"')
                                escaped.push_back('\\');
                            escaped.push_back(c);
                        }
                        json << fmt::format(
                            "{{\"magic\":\"CPK5\",\"side\":\"source\",\"identity\":\"{}\",\"artifact\":\"{}\","
                            "\"record_capacity\":{},\"track_capacity\":{},\"clip_capacity\":{},"
                            "\"expected\":{},\"attempted\":{},\"recorded\":{},\"missing\":{},\"dropped\":{},"
                            "\"overflow\":{},\"write_failed\":{},\"truncated\":{},\"two_read_bad\":{},"
                            "\"clip_overflow\":{},\"no_pose\":{},\"identity_failed_clips\":{},\"drift_records\":{}}}\n",
                            Plugin::Plugin_Build_Identity, escaped,
                            K5S_Record_Capacity, K5_Track_Capacity, K5_Clip_Capacity,
                            attempted, attempted, recorded,
                            attempted > recorded + dropped ? attempted - recorded - dropped : 0,
                            dropped,
                            g_k5s_writer.overflow() ? 1 : 0,
                            g_k5s_counters.write_failed.load(std::memory_order_relaxed),
                            g_k5s_writer.overflow() ? 1 : 0,
                            g_k5s_counters.two_read_bad.load(std::memory_order_relaxed),
                            g_k5s_counters.clip_overflow.load(std::memory_order_relaxed),
                            g_k5s_counters.no_pose.load(std::memory_order_relaxed),
                            g_k5s_counters.identity_failed_clips.load(std::memory_order_relaxed),
                            g_k5s_counters.drift_records.load(std::memory_order_relaxed));
                        logger::info("K5 RECON ledger path='{}' status=written", json_path);
                    }
                }
            }
        }
        std::uint64_t const eligible = s.counters.eligible_records.load(std::memory_order_relaxed);
        std::uint64_t const missing = eligible - s.drained_records - s.ring.dropped();
        s.slots_used = static_cast<std::uint32_t>(s.pairing.slots_used());
        char const* fill_state = s.fill_cap_state == 1 ? "ok" : (s.fill_cap_state == 2 ? "violation" : "unread");
            logger::info("PROBE LIVENESS window=close reason={} actor={:08x} epoch={} frames={} entries={} e:engine={}/plugin={}/other={} pinned-engine={} apply-entries={} pinned-apply-entries={} other-apply-entries={} tracked-sample-entries={} paired-apply-entries={} pair-mismatch={} pair-no-slot={} threads={} queue-seen={} queue={:#x} flag={} queue-flag-frames={} detail={} detail-dropped={} ring-dropped={} contention-dropped={} full-dropped={} eligible-records={} recorded-records={} drained-records={} drained-samples={} drained-applies={} missing-records={} orphan-applies={} unpaired-samples={} suppressed-rows={} transport-complete={} apply-paths=63589:{}/63568:{}/p63563:{}/p63572:{}/p_bae731:{}/un:{} apply-completions=63589:{}/63568:{}/p63563:{}/p63572:{}/p_bae731:{}/un:{} apply-returns={} completions-pinned={} paired-completions={} drained-returns={} in-flight-now={} in-flight-max={} orphan-returns={} untracked-returns={} overwritten-entries={} count-over-capacity={} clamped-zero={} pose-stable={} fill-le-cap={} skeleton=[cap:{} fill:{}] e2a-frames={} e2a-main-frames={} e2a-fallback-frames={} e2a-tracks={} e2a-failed={} e2a-max-delta={:.3e} e2a-fail-frames={} e2a-fail-ik={} e2a-fail-no-ik={} e2a-ik-frames={} e2a-failed-ik={} e2a-failed-no-ik={} e2a-ik-ready={} e2a-near-slot0={} e2a-near-slot1={} e2a-near-neither={} e2a-fail-foot={} e2a-fail-cloth={} e2a-fail-unexplained={} e2a-ok={} e2a-w-domain={} e2a-non-finite={} e2a-read-failed={} e2a-samples={} b-calls={} b-compared={} b-rewritten={} b-max-delta={:.3e} b-clean={} b-overwritten={} b-entries={} b-returns={} b-last-rewritten={} b3-checked={} b3-equal={} b3-failed={} b3-max-delta={:.3e} b3-stale={} b3-multi={} b3-no-out={} b3-overwritten={} b3-behavior-mismatch={} b4-checked={} b4-equal={} b4-failed={} b4-max-delta={:.3e} b3b4-read-failed={} b3b4-non-finite={} b3b4-samples={}",
            probe_window_close_name(s.window.close_reason()), g_actor.load(std::memory_order_relaxed),
            g_epoch.load(std::memory_order_relaxed), s.counters.frames.load(std::memory_order_relaxed),
            s.counters.entries_total.load(std::memory_order_relaxed),
            s.counters.entries_engine.load(std::memory_order_relaxed),
            s.counters.entries_plugin.load(std::memory_order_relaxed),
            s.counters.entries_other.load(std::memory_order_relaxed),
            s.counters.pinned_engine.load(std::memory_order_relaxed),
            s.counters.apply_entries_total.load(std::memory_order_relaxed),
            s.counters.apply_entries_pinned.load(std::memory_order_relaxed),
            s.counters.other_apply_entries.load(std::memory_order_relaxed),
            s.tracked_entries, s.paired_apply_entries, s.pairing_mismatch, s.pairing_no_slot, s.slots_used,
            s.queue_seen ? "yes" : "no", s.queue_pointer, s.queue_flag,
            s.counters.queue_flag_frames.load(std::memory_order_relaxed), s.detail_lines,
            s.counters.detail_dropped.load(std::memory_order_relaxed), s.ring.dropped(),
            s.ring.contended(), s.ring.dropped() - s.ring.contended(), eligible, eligible - s.ring.dropped(), s.drained_records,
            s.drained_samples, s.drained_applies, missing, s.orphan_applies,
            s.pairing.unpaired_samples(), s.drained_records - s.detail_lines,
            (missing == 0 && s.ring.dropped() == 0) ? "yes" : "no",
            s.counters.path_entries[probe_apply_path_index(ProbeApplyPath::e_main_63589)].load(std::memory_order_relaxed),
            s.counters.path_entries[probe_apply_path_index(ProbeApplyPath::e_main_63568)].load(std::memory_order_relaxed),
            s.counters.path_entries[probe_apply_path_index(ProbeApplyPath::e_p63563)].load(std::memory_order_relaxed),
            s.counters.path_entries[probe_apply_path_index(ProbeApplyPath::e_p63572)].load(std::memory_order_relaxed),
            s.counters.path_entries[probe_apply_path_index(ProbeApplyPath::e_p_bae731)].load(std::memory_order_relaxed),
            s.counters.unclassified_applies.load(std::memory_order_relaxed),
            s.counters.path_completions[probe_apply_path_index(ProbeApplyPath::e_main_63589)].load(std::memory_order_relaxed),
            s.counters.path_completions[probe_apply_path_index(ProbeApplyPath::e_main_63568)].load(std::memory_order_relaxed),
            s.counters.path_completions[probe_apply_path_index(ProbeApplyPath::e_p63563)].load(std::memory_order_relaxed),
            s.counters.path_completions[probe_apply_path_index(ProbeApplyPath::e_p63572)].load(std::memory_order_relaxed),
            s.counters.path_completions[probe_apply_path_index(ProbeApplyPath::e_p_bae731)].load(std::memory_order_relaxed),
            s.counters.path_completions[probe_apply_path_index(ProbeApplyPath::e_unclassified)].load(std::memory_order_relaxed),
            s.counters.apply_returns_total.load(std::memory_order_relaxed),
            s.counters.completions_pinned.load(std::memory_order_relaxed),
            s.counters.paired_completions.load(std::memory_order_relaxed),
            s.drained_returns, s.in_flight.in_flight(),
            s.counters.max_in_flight.load(std::memory_order_relaxed),
            s.counters.orphan_returns.load(std::memory_order_relaxed),
            s.counters.returns_untracked.load(std::memory_order_relaxed),
            s.counters.overwritten_entries.load(std::memory_order_relaxed),
            s.counters.count_over_capacity.load(std::memory_order_relaxed),
            s.counters.clamped_zero.load(std::memory_order_relaxed),
            s.counters.pose_unstable.load(std::memory_order_relaxed) ? "no" : "yes",
            fill_state, s.skeleton_capacity, s.skeleton_fill,
            s.mix.frames, s.mix.main_frames, s.mix.fallback_frames, s.mix.tracks, s.mix.failed, s.mix.max_delta,
            s.mix.fail_frames, s.mix.fail_frames_ik, s.mix.fail_frames_no_ik,
            s.mix.ik_frames, s.mix.failed_ik, s.mix.failed_no_ik, s.mix.ik_ready_frames,
            s.mix.near_slot0, s.mix.near_slot1, s.mix.near_neither, s.mix.fail_foot, s.mix.fail_cloth,
            s.mix.fail_unexplained,
            (s.mix.clean() ? "yes" : "no"), (s.mix.weight_out_of_range ? "violation" : "ok"),
            (s.mix.non_finite ? "yes" : "no"), (s.mix.read_failed ? "yes" : "no"),
            s.mix.sample_count,
            s.footik_tally.calls, s.footik_tally.compared, s.footik_tally.rewritten_components,
            s.footik_tally.max_delta, (s.footik_tally.clean() ? "yes" : "no"),
            s.counters.footik_overwritten.load(std::memory_order_relaxed),
            s.drained_footik_entries, s.drained_footik_returns,
            s.footik_tally.last_frame_rewritten,
            s.b3b4_tally.b3_checked, s.b3b4_tally.b3_equal, s.b3b4_tally.b3_failed,
            s.b3b4_tally.b3_max_delta, s.b3_stale, s.b3_multi, s.b3_no_out, s.b3_overwritten,
            s.b3_behavior_mismatch, s.b3b4_tally.b4_checked, s.b3b4_tally.b4_equal,
            s.b3b4_tally.b4_failed, s.b3b4_tally.b4_max_delta,
            s.b3b4_tally.read_failed ? "yes" : "no",
            s.b3b4_tally.non_finite ? "yes" : "no",
            s.b3b4_tally.sample_count);

        // v97c: the pre-registered stop rule for multiple/unattributed returns and stale slots
        // now actually gates the verdict. The v97b run reported multi=1161 because the slot was
        // never consumed; that is fixed in the ledger, but a genuine multi still invalidates.
        bool const b3_clean = s.b3b4_tally.b3_checked > 0 && s.b3b4_tally.b3_failed == 0
            && !s.b3b4_tally.read_failed && !s.b3b4_tally.non_finite
            && s.b3_multi == 0 && s.b3_stale == 0 && s.b3_no_out == 0 && s.b3_overwritten == 0;
        char const* const b3_verdict =
            b3_clean
            ? (s.b3b4_tally.b4_checked > 0 && s.b3b4_tally.b4_failed == 0
                ? "B3+B4-PASS" : "B3-PASS-B4-OPEN")
            : "B3-INVALID";
        logger::info("PROBE LIVENESS b3b4-window b3-checked={} b3-stable={} b3-changed={} b4-checked={} b4-equal={} b4-failed={} b4-max-delta={:.3e} verdict={}",
            s.b3b4_tally.b3_checked,
            b3_clean ? "yes" : "no",
            s.b3b4_tally.b3_failed,
            s.b3b4_tally.b4_checked, s.b3b4_tally.b4_equal, s.b3b4_tally.b4_failed,
            s.b3b4_tally.b4_max_delta, b3_verdict);

        for (std::uint32_t i = 0; i < s.b3b4_tally.sample_count; ++i)
        {
            ProbeB3B4Sample const& sample = s.b3b4_tally.samples[i];
            logger::info("PROBE LIVENESS b3b4-sample track={} channel={} expected={:.6f} actual={:.6f} delta={:.6g}",
                sample.track, static_cast<unsigned>(sample.channel), sample.expected, sample.actual,
                sample.delta);
        }

        // v75/v77: the failing components themselves, so attribution never depends on a summary.
        for (std::uint32_t i = 0; i < s.mix.sample_count; ++i)
        {
            ProbeMixTally::Sample const& sample = s.mix.samples[i];
            logger::info("PROBE LIVENESS e2a-sample index={} track={} channel={} expected={:.6f} actual={:.6f} delta={:.6g}",
                i, sample.track, static_cast<unsigned>(sample.channel), sample.expected, sample.actual,
                sample.delta);
        }
        // E2-A′: the bones that a residual landed on outside the foot-IK chain. Any name here is
        // refuting evidence for the identity, so it is printed verbatim.
        for (std::uint32_t i = 0; i < s.mix.unexplained_bone_count; ++i)
            logger::info("PROBE LIVENESS e2a-bone index={} name='{}' count={}", i,
                s.mix.unexplained_bones[i].name.data(), s.mix.unexplained_bones[i].count);

        // C-G5-E3 (M2): the pairing manifest. The rule is fixed here and in the policy test, so the
        // ledger cannot be tuned after seeing a pose error: each source settle confirmation pairs
        // with the first candidate step at or after its TSC, and a step is consumed at most once.
        {
            std::array<std::uint64_t, 128> settles{};
            std::size_t const settle_count = take_source_settles(settles.data(), settles.size());
            ProbeSettlePairing const pairing = probe_pair_settles(settles.data(),
                static_cast<std::uint32_t>(settle_count), s.step_tsc.data(), s.step_count);
            double const seconds = s.close_steady_ns > s.arm_steady_ns
                ? static_cast<double>(s.close_steady_ns - s.arm_steady_ns) / 1'000'000'000.0 : 0.0;
            double const tsc_per_second = seconds > 0.0 && s.close_tsc > s.arm_tsc
                ? static_cast<double>(s.close_tsc - s.arm_tsc) / seconds : 0.0;
            logger::info("PROBE LIVENESS manifest actor={:08x} epoch={} request={} steps={} steps-saturated={} step-tsc-first={} step-tsc-last={} arm-tsc={} close-tsc={} arm-ns={} close-ns={} tsc-per-second={:.0f} source-settles={} paired={} unpaired={} source-settle-backlog={} pairing=first-step-at-or-after-settle",
                g_actor.load(std::memory_order_relaxed), g_epoch.load(std::memory_order_relaxed), s.request,
                s.step_count, s.step_count >= Probe_Step_Timeline ? "yes" : "no",
                s.step_count > 0 ? s.step_tsc[0] : 0,
                s.step_count > 0 ? s.step_tsc[s.step_count - 1] : 0,
                s.arm_tsc, s.close_tsc, s.arm_steady_ns, s.close_steady_ns, tsc_per_second,
                pairing.settles, pairing.paired, pairing.unpaired, source_settle_backlog());
        }

        g_plugin_graph.store(nullptr, std::memory_order_relaxed);
        g_pinned.store(nullptr, std::memory_order_release);
        g_pinned_character.store(nullptr, std::memory_order_release);
        s.close_pending = false;
    }

    enum class ProbeThunkKind : std::uint8_t
    {
        e_sample_entry,
        e_apply_entry,
        e_apply_return,
        e_footik_entry
    };

    bool install_at(std::uintptr_t a_address, char const* a_name, std::uint8_t const* a_head,
        std::size_t a_head_size, void const* a_recorder, ProbeThunkKind a_kind)
    {
        auto const* const head = reinterpret_cast<std::uint8_t const*>(a_address);
        if (std::memcmp(head, a_head, a_head_size) != 0)
        {
            std::string hex;
            for (std::size_t i = 0; i < 16; ++i)
                hex += fmt::format("{:02x}", head[i]);
            logger::warn("PROBE LIVENESS install-failed reason=head-mismatch target='{}' address={:#x} head={}",
                a_name, a_address, hex);
            return false;
        }
        auto& trampoline = SKSE::GetTrampoline();
        auto* const mem = static_cast<std::uint8_t*>(trampoline.allocate(Detour_Block_Size));
        if (!mem)
        {
            logger::warn("PROBE LIVENESS install-failed reason=trampoline-exhausted target='{}'", a_name);
            return false;
        }
        std::uint8_t* const thunk = mem;
        std::uint8_t* const original = relocate_detour_head(mem, { a_head, a_head_size },
            a_address + a_head_size);
        switch (a_kind)
        {
        case ProbeThunkKind::e_sample_entry:
            (void)encode_detour_thunk(thunk, reinterpret_cast<std::uintptr_t>(a_recorder),
                reinterpret_cast<std::uintptr_t>(original), false);
            break;
        case ProbeThunkKind::e_apply_entry:
            (void)encode_detour_thunk_with_caller(thunk, reinterpret_cast<std::uintptr_t>(a_recorder),
                reinterpret_cast<std::uintptr_t>(original));
            break;
        case ProbeThunkKind::e_footik_entry:
            (void)encode_detour_footik_thunk(thunk, reinterpret_cast<std::uintptr_t>(a_recorder),
                reinterpret_cast<std::uintptr_t>(original));
            break;
        default:
            (void)encode_detour_return_thunk(thunk, reinterpret_cast<std::uintptr_t>(a_recorder),
                reinterpret_cast<std::uintptr_t>(original));
            break;
        }
        // The patch covers the whole verified head: a 5-byte jmp plus inert pads. 63589's head is
        // exactly 5 bytes (three pushes); 63856's entry is the 7-byte `sub rsp,0x88` and its return
        // site is the 7-byte `add rsp,0x88`.
        std::uint8_t patch[8] = { 0xE9, 0x00, 0x00, 0x00, 0x00, 0x90, 0x90, 0x90 };
        std::int32_t const rel = static_cast<std::int32_t>(
            reinterpret_cast<std::uintptr_t>(thunk) - (a_address + 5));
        std::memcpy(patch + 1, &rel, sizeof(rel));
        REL::safe_write(a_address, patch, a_head_size);
        logger::info("PROBE LIVENESS installed target='{}' address={:#x} thunk={:#x} original={:#x} head-size={}",
            a_name, a_address, reinterpret_cast<std::uintptr_t>(thunk),
            reinterpret_cast<std::uintptr_t>(original), a_head_size);
        return true;
    }
}

    void install_probe_liveness()
    {
        bool expected = false;
        if (!g_install_attempted.compare_exchange_strong(expected, true))
            return;
        if (sampling_liveness_mode() == ProbeLivenessMode::e_off)
        {
            logger::info("PROBE LIVENESS install-skip reason=off");
            return;
        }
        // C-G5-E3 (M1): the timeline is admitted only in its `concurrent` state, where it installs
        // no detour and is driven by the same experiment window. The legacy `on` lifecycle still
        // wins the process, exactly as before.
        if (source_timeline_mode() == SourceTimelineMode::e_on)
        {
            logger::warn("PROBE LIVENESS install-skip reason=source-timeline-priority");
            return;
        }
        if (!exact_runtime())
        {
            logger::warn("PROBE LIVENESS install-skip reason=runtime-version");
            return;
        }
        auto& trampoline = SKSE::GetTrampoline();
        (void)state();
        trampoline.create(Probe_Liveness_Pool_Size);
        REL::Relocation<std::uintptr_t> const sample_site{ REL::ID(Sampling_Entry_Id) };
        REL::Relocation<std::uintptr_t> const apply_site{ REL::ID(Pose_Apply_Entry_Id) };
        bool const sample = install_at(sample_site.address(), "63589-sampling-entry",
            Probe_Sample_Head_Bytes.data(), Probe_Sample_Head_Bytes.size(), &record_sample_entry,
            ProbeThunkKind::e_sample_entry);
        bool const pose = sample && install_at(apply_site.address(), "63856-apply-entry",
            Probe_Pose_Head_Bytes.data(), Probe_Pose_Head_Bytes.size(), &record_apply_entry,
            ProbeThunkKind::e_apply_entry);
        // C-G5-E1 v3: the return site is the single `add rsp,0x88` at +0x24f. A head mismatch there
        // leaves the entry detour installed but cannot arm a window (completion evidence would be
        // missing), so both are required for `ready`.
        bool const pose_return = pose && install_at(apply_site.address() + Probe_Pose_Return_Offset,
            "63856-apply-return", Probe_Pose_Return_Head_Bytes.data(), Probe_Pose_Return_Head_Bytes.size(),
            &record_apply_return, ProbeThunkKind::e_apply_return);
        // C-G5-E2-B: observe 58423 (foot IK) at its entry and its single return site.
        REL::Relocation<std::uintptr_t> const footik_site{ REL::ID(Foot_Ik_Entry_Id) };
        bool const footik_entry = pose_return && install_at(footik_site.address(), "58423-footik-entry",
            Probe_Footik_Entry_Head_Bytes.data(), Probe_Footik_Entry_Head_Bytes.size(), &record_footik_entry,
            ProbeThunkKind::e_footik_entry);
        bool const footik_return = footik_entry && install_at(
            footik_site.address() + Probe_Footik_Return_Offset, "58423-footik-return",
            Probe_Footik_Return_Head_Bytes.data(), Probe_Footik_Return_Head_Bytes.size(),
            &record_footik_return, ProbeThunkKind::e_apply_return);
        g_installed.store(sample && pose && pose_return && footik_entry && footik_return,
            std::memory_order_release);
        if (!sample || !pose || !pose_return || !footik_entry || !footik_return)
            logger::warn("PROBE LIVENESS install-incomplete sample={} apply={} apply-return={} footik-entry={} footik-return={} ready=no",
                sample, pose, pose_return, footik_entry, footik_return);
    }

    void arm_probe_liveness()
    {
        if (sampling_liveness_mode() != ProbeLivenessMode::e_on || !g_installed.load(std::memory_order_acquire))
            return;
        if (independent_graph_setting() != IndependentGraphMode::off)
            return;
        // C-G5-E2-B B3/B4: pin the source graph before arming. `on` mode used to arm with
        // pinned=no, which made every B3/B4 comparison a no-op. A missing graph is a hard stop:
        // the window must not open without a pinned identity.
        RE::BShkbAnimationGraph* const pinned = player_source_graph();
        if (pinned == nullptr)
        {
            logger::warn("PROBE LIVENESS arm-skip reason=no-pinned-source-graph mode=on");
            return;
        }
        g_pinned.store(pinned, std::memory_order_release);
        // characterInstance is an embedded hkbCharacter at BShkbAnimationGraph+0xC0; its address
        // is exactly what 58315 writes into hkbContext::character for this graph.
        g_pinned_character.store(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(pinned) + 0xC0),
            std::memory_order_release);
        logger::info("PROBE LIVENESS pinned mode=on graph={:#x} character={:#x} behavior={:#x}",
            reinterpret_cast<std::uintptr_t>(pinned),
            reinterpret_cast<std::uintptr_t>(reinterpret_cast<std::byte const*>(pinned) + 0xC0),
            reinterpret_cast<std::uintptr_t>(pinned->behaviorGraph));
        g_arm_requested.store(true, std::memory_order_release);
        logger::info("PROBE LIVENESS arm-requested mode=on");
    }

    bool arm_probe_liveness_concurrent(RE::BShkbAnimationGraph const& a_source_graph, std::uint32_t a_actor_id,
        std::uint64_t a_request)
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
        // C-G5-E2-A′: cache the pinned skeleton's bone names once per window so the detour can
        // attribute a residual to a bone without touching engine strings or allocating. Reading
        // them here (render thread, object alive) is the only safe place.
        {
            std::uint32_t bone_count = 0;
            RE::hkbCharacterSetup const* const setup = pinned->characterInstance.setup.get();
            if (setup != nullptr)
            {
                RE::hkaSkeleton const* const skeleton = setup->animationSkeleton.get();
                if (skeleton != nullptr)
                {
                    auto const bone_size = static_cast<std::uint32_t>(skeleton->bones.size());
                    for (std::uint32_t i = 0; i < bone_size && bone_count < Max_Cached_Bones; ++i)
                    {
                        char const* const name = skeleton->bones[static_cast<std::uint32_t>(i)].name.c_str();
                        if (name == nullptr)
                            continue;
                        std::size_t k = 0;
                        for (; k + 1 < Probe_Bone_Name_Length && name[k] != '\0'; ++k)
                            g_bone_names[bone_count][k] = name[k];
                        g_bone_names[bone_count][k] = '\0';
                        g_bone_name_ptrs[bone_count] = g_bone_names[bone_count];
                        ++bone_count;
                    }
                }
            }
            g_bone_count.store(bone_count, std::memory_order_release);
            logger::info("PROBE LIVENESS bone-names cached={}", bone_count);
        }
        // R5: reference fill vs pose capacity, read once per window. The chain is
        // graph+0xc0 -> character, [character+0x50] -> skeleton holder, [holder+0x20] -> word;
        // capacity is dword +0x30 and reference fill is dword +0x40 (audit29). Every step is
        // null-checked so an unreadable chain reports `unread` instead of a guess; a fill above the
        // capacity is an engine-side over-read risk and stops the run at adjudication.
        if (auto const* const skeleton_holder = read_offset<void*>(
                reinterpret_cast<std::byte const*>(pinned) + 0xc0, 0x50))
        {
            if (auto const* const word = read_offset<void*>(skeleton_holder, 0x20))
            {
                s.skeleton_capacity = read_offset<std::uint32_t>(word, 0x30);
                s.skeleton_fill = read_offset<std::uint32_t>(word, 0x40);
                s.fill_cap_state = s.skeleton_fill <= s.skeleton_capacity ? 1 : 2;
            }
        }
        g_pinned.store(pinned, std::memory_order_release);
        g_pinned_character.store(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(pinned) + 0xC0),
            std::memory_order_release);
        g_actor.store(a_actor_id, std::memory_order_relaxed);
        std::uint32_t const epoch = g_epoch.fetch_add(1, std::memory_order_relaxed) + 1;
        auto const armed_at = std::chrono::steady_clock::now();
        s.request = a_request;
        s.arm_tsc = probe_tsc_now();
        s.arm_steady_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(armed_at.time_since_epoch()).count());
        s.window.arm(armed_at);
        s.admission.open();
        logger::info("PROBE LIVENESS concurrent-armed actor={:08x} epoch={} request={} graph={:#x} behavior={:#x} pinned=yes",
            a_actor_id, epoch, a_request, reinterpret_cast<std::uintptr_t>(pinned),
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
        // C-G5-E1 v2: the concurrent window is armed inside the paused panel, where the engine does
        // not run its sample loop. The first display close therefore continues the same window into
        // a resumed phase instead of ending it; counters stay cumulative and the segment row marks
        // the boundary so the resumed phase can be read on its own.
        if (std::strcmp(a_reason, "display-close") == 0
            && sampling_liveness_mode() == ProbeLivenessMode::e_concurrent
            && s.window.phase() == ProbePhase::e_display
            && s.window.enter_resumed(std::chrono::steady_clock::now()))
        {
            logger::info("PROBE LIVENESS window=segment provisional=yes from=display to=resumed seconds={} frames={} entries={} e:engine={}/plugin={}/other={} pinned-engine={} apply-entries={} pinned-apply-entries={} other-apply-entries={} detail={} detail-dropped={} ring-dropped={}",
                Probe_Liveness_Resumed.count(),
                s.counters.frames.load(std::memory_order_relaxed),
                s.counters.entries_total.load(std::memory_order_relaxed),
                s.counters.entries_engine.load(std::memory_order_relaxed),
                s.counters.entries_plugin.load(std::memory_order_relaxed),
                s.counters.entries_other.load(std::memory_order_relaxed),
                s.counters.pinned_engine.load(std::memory_order_relaxed),
                s.counters.apply_entries_total.load(std::memory_order_relaxed),
                s.counters.apply_entries_pinned.load(std::memory_order_relaxed),
                s.counters.other_apply_entries.load(std::memory_order_relaxed),
                s.detail_lines, s.counters.detail_dropped.load(std::memory_order_relaxed), s.ring.dropped());
            return;
        }
        ProbeWindowClose reason = ProbeWindowClose::e_abort;
        if (std::strcmp(a_reason, "display-close") == 0) reason = ProbeWindowClose::e_display_close;
        else if (std::strcmp(a_reason, "expired") == 0) reason = ProbeWindowClose::e_expired;
        else if (std::strcmp(a_reason, "world-reset") == 0) reason = ProbeWindowClose::e_world_reset;
        else if (std::strcmp(a_reason, "rearm") == 0) reason = ProbeWindowClose::e_rearm;
        s.close_tsc = probe_tsc_now();
        s.close_steady_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        s.window.cancel(reason);
        s.admission.close();
        s.close_pending = true;
        finalize_probe_close(s);
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
            // Request close; final identity clearing waits for admitted callbacks and final drain.
            if (s.window.active())
                close_probe_liveness_window("world-reset");
            finalize_probe_close(s);
            return;
        }
        if (g_arm_requested.exchange(false, std::memory_order_acq_rel) && !s.window.ever_armed())
        {
            // The F7 window observes the player's own graph like the legacy E1 lifecycle; the
            // pinned set is empty, so counters run but no detail records can match.
            s.reset();
            reset_k3s();
            g_epoch.fetch_add(1, std::memory_order_relaxed);
            s.window.arm(std::chrono::steady_clock::now());
            s.admission.open();
            logger::info("PROBE LIVENESS window=open mode=on epoch={} pinned=yes",
                g_epoch.load(std::memory_order_relaxed));
        }
        finalize_probe_close(s);
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
        drain_probe_records(s);
        drain_k3s_records();
    }
}
PLUGIN_NAMESPACE_END
