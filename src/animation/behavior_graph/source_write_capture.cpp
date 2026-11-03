//
// Created by AmazingBuff on 2026/10/31.
//

#include "animation/behavior_graph/source_write_capture.h"

#include "animation/behavior_graph/source_write_capture_policy.h"

#include "animation/behavior_graph/source_timeline_policy.h"

#include "animation/settings.h"

#include <RE/A/Actor.h>
#include <RE/B/BSAnimationGraphManager.h>
#include <RE/B/BShkbAnimationGraph.h>
#include <RE/H/hkbBehaviorGraph.h>
#include <RE/H/hkbBehaviorGraphData.h>
#include <RE/H/hkbBehaviorGraphStringData.h>
#include <RE/H/hkbVariableValueSet.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>
#include <SKSE/Trampoline.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    namespace
    {
        // The phase stream mirrors the G3-A settled rules so the write rows can be anchored to
        // the transition frames; the phase graph is the player's own graph picked with the same
        // rule as the G3-A timeline (last non-first-person graph of the actor).
        constexpr std::size_t kMaxWords = 1024;
        constexpr std::size_t kWordDataOffset = 0x10;
        constexpr std::size_t kWordCountOffset = 0x18;
        constexpr std::size_t kNodeCountOffset = 0x8;
        constexpr std::uint8_t kSettleSamples = 3;
        constexpr std::size_t kPhaseCount = 5;
        char const* const kPhaseNames[kPhaseCount] = {
            "currentDefaultState", "iState", "IsEquipping", "IsUnequipping", "bEquipUnequip"
        };

        std::atomic<bool> g_installed{ false };
        SourceWriteGate& write_gate()
        {
            static SourceWriteGate s_gate;
            return s_gate;
        }
        SourceWriteRing& write_ring()
        {
            static SourceWriteRing s_ring;
            return s_ring;
        }
        SourceTimelineRequests& write_requests()
        {
            static SourceTimelineRequests s_requests;
            return s_requests;
        }

        struct WriteState
        {
            std::size_t index[kPhaseCount] = {};
            bool tracked[kPhaseCount] = {};
            std::int32_t logged[kPhaseCount] = {};
            std::int32_t candidate[kPhaseCount] = {};
            std::uint8_t settled[kPhaseCount] = {};
            std::int32_t node_logged = -1;
            std::int32_t node_candidate = -1;
            std::uint8_t node_settled = 0;
            std::uint64_t seq = 0;
            std::uint64_t dropped_seen = 0;
            std::uint32_t actor_id = 0;
            SourceTimelineIdentity observed{};
            bool has_observed{ false };
            SourceWriteWindow window;
        };

        WriteState& write_state()
        {
            static WriteState s_state;
            return s_state;
        }

        // ---- hook side ----------------------------------------------------------

        // Called for every typed setter call in the process, from any thread. One gate match
        // (up to four pointer compares) misses immediately outside a window; inside, the name
        // is copied HERE because the caller's BSFixedString is a stack temporary that dies
        // before the render-side drain runs. No allocation, no lock, no engine call.
        void record_setter(bool a_is_bool, void const* a_graph, void const* a_name, std::uintptr_t a_value) noexcept
        {
            if (!write_gate().match(a_graph))
                return;
            char const* text = nullptr;
            if (a_name)
                std::memcpy(&text, a_name, sizeof(text));
            (void)write_ring().push(reinterpret_cast<std::uintptr_t>(a_graph),
                static_cast<std::int32_t>(a_value), a_is_bool, text);
        }

        void record_write_bool(void const* a_graph, void const* a_name, std::uintptr_t a_value) noexcept
        {
            record_setter(true, a_graph, a_name, a_value);
        }

        void record_write_int(void const* a_graph, void const* a_name, std::uintptr_t a_value) noexcept
        {
            record_setter(false, a_graph, a_name, a_value);
        }

        // ---- sampler helpers ----------------------------------------------------

        void collect_player_graphs(RE::Actor& actor, std::vector<RE::BShkbAnimationGraph*>& out)
        {
            out.clear();
            RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
            if (!actor.GetAnimationGraphManager(manager) || !manager)
                return;
            for (RE::BSTSmartPointer<RE::BShkbAnimationGraph> const& graph : manager->graphs)
            {
                if (!graph || !graph->behaviorGraph || graph->holder != &actor)
                    continue;
                char const* const name = graph->projectName.c_str();
                if (name && std::strstr(name, "1stperson"))
                    continue;
                if (out.size() < SourceWriteGate::kMaxGraphs)
                    out.push_back(graph.get());
            }
        }

        template <class T>
        T field_at(void const* base, std::size_t offset)
        {
            T value;
            std::memcpy(&value, static_cast<std::byte const*>(base) + offset, sizeof(value));
            return value;
        }

        bool variable_index(RE::hkbBehaviorGraphData const* a_data, char const* a_name, std::size_t& a_out)
        {
            if (!a_data || !a_data->stringData)
                return false;
            auto const& names = a_data->stringData->variableNames;
            std::int32_t const count = names.size();
            if (count <= 0 || static_cast<std::size_t>(count) > kMaxWords)
                return false;
            for (std::int32_t i = 0; i < count; ++i)
            {
                char const* const name = names.data()[i].c_str();
                if (name && std::strcmp(name, a_name) == 0)
                {
                    a_out = static_cast<std::size_t>(i);
                    return true;
                }
            }
            return false;
        }

        std::int32_t read_word(std::int32_t const* a_words, std::size_t a_count, std::size_t a_index)
        {
            return a_index < a_count ? a_words[a_index] : 0;
        }

        void close_window(char const* a_reason)
        {
            write_gate().clear();
            WriteState& s = write_state();
            s.window.cancel();
            logger::info("SOURCE WRITECAPTURE window-closed reason={} actor={:08x} writes={} phases={} ring-dropped={}",
                a_reason, s.actor_id, s.window.write_lines(), s.window.phase_lines(), write_ring().dropped());
        }

        void baseline_capture(WriteState& a_state, RE::hkbBehaviorGraph& a_behavior,
            std::vector<RE::BShkbAnimationGraph*> const& a_graphs, std::int32_t const* a_words,
            std::size_t a_count, std::uint32_t a_actor_id)
        {
            RE::hkbBehaviorGraphData const* const data = a_behavior.data.get();
            for (std::size_t i = 0; i < kPhaseCount; ++i)
            {
                a_state.tracked[i] = variable_index(data, kPhaseNames[i], a_state.index[i]);
                if (a_state.tracked[i])
                {
                    a_state.logged[i] = read_word(a_words, a_count, a_state.index[i]);
                    a_state.candidate[i] = a_state.logged[i];
                }
                a_state.settled[i] = 0;
            }
            void* const nodes = a_behavior.activeNodes;
            a_state.node_logged = nodes ? field_at<std::int32_t>(nodes, kNodeCountOffset) : 0;
            a_state.node_candidate = a_state.node_logged;
            a_state.node_settled = 0;
            a_state.seq = 0;
            a_state.dropped_seen = 0;
            a_state.actor_id = a_actor_id;
            write_ring().drain_discard();
            write_ring().reset_dropped();
            // Gate last: from this point every setter on any of the player's observed graphs
            // lands in the ring.
            std::uintptr_t graphs[SourceWriteGate::kMaxGraphs];
            for (std::size_t i = 0; i < a_graphs.size(); ++i)
                graphs[i] = reinterpret_cast<std::uintptr_t>(a_graphs[i]);
            write_gate().arm(graphs, a_graphs.size());
            std::string tracked;
            for (std::size_t i = 0; i < kPhaseCount; ++i)
                tracked += fmt::format("{}{}={}", i ? " " : "", kPhaseNames[i],
                    a_state.tracked[i] ? std::to_string(a_state.index[i]) : std::string{ "missing" });
            logger::info("SOURCE WRITECAPTURE window-open actor={:08x} graphs={} words={} nodes={} writes-budget={} phases-budget={} seconds={} tracked {}",
                a_actor_id, a_graphs.size(), a_count, a_state.node_logged, Source_Write_Line_Budget,
                Source_Write_Phase_Line_Budget, Source_Write_Window.count(), tracked);
        }

        void drain_writes(WriteState& a_state)
        {
            SourceWriteRecord record{};
            while (write_ring().pop(record))
            {
                if (!a_state.window.take_write_line())
                {
                    logger::info("SOURCE WRITECAPTURE budget-exhausted kind=write actor={:08x} lines={}",
                        a_state.actor_id, Source_Write_Line_Budget);
                    close_window("budget-exhausted");
                    return;
                }
                ++a_state.seq;
                logger::info("SOURCE WRITECAPTURE write actor={:08x} seq={} graph={:#x} kind={} name='{}' value={}",
                    a_state.actor_id, a_state.seq, record.graph, record.is_bool ? "bool" : "int",
                    std::string_view{ record.name, record.name_len }, record.value);
            }
            std::uint64_t const dropped = write_ring().dropped();
            if (dropped != a_state.dropped_seen)
            {
                logger::info("SOURCE WRITECAPTURE ring-dropped actor={:08x} total={} new={}",
                    a_state.actor_id, dropped, dropped - a_state.dropped_seen);
                a_state.dropped_seen = dropped;
            }
        }

        void sample_phase(WriteState& a_state, RE::hkbBehaviorGraph& a_behavior,
            std::int32_t const* a_words, std::size_t a_count)
        {
            for (std::size_t i = 0; i < kPhaseCount; ++i)
            {
                if (!a_state.tracked[i])
                    continue;
                std::int32_t const current = read_word(a_words, a_count, a_state.index[i]);
                if (current != a_state.candidate[i])
                {
                    a_state.candidate[i] = current;
                    a_state.settled[i] = 0;
                    continue;
                }
                if (a_state.settled[i] < kSettleSamples)
                    ++a_state.settled[i];
                if (a_state.settled[i] < kSettleSamples || current == a_state.logged[i])
                    continue;
                if (!a_state.window.take_phase_line())
                {
                    logger::info("SOURCE WRITECAPTURE budget-exhausted kind=phase actor={:08x} lines={}",
                        a_state.actor_id, Source_Write_Phase_Line_Budget);
                    close_window("budget-exhausted");
                    return;
                }
                std::int32_t const previous = a_state.logged[i];
                a_state.logged[i] = current;
                logger::info("SOURCE WRITECAPTURE phase actor={:08x} name='{}' {}->{}",
                    a_state.actor_id, kPhaseNames[i], previous, current);
            }
            void* const nodes = a_behavior.activeNodes;
            std::int32_t const count = nodes ? field_at<std::int32_t>(nodes, kNodeCountOffset) : 0;
            if (count != a_state.node_candidate)
            {
                a_state.node_candidate = count;
                a_state.node_settled = 0;
                return;
            }
            if (a_state.node_settled < kSettleSamples)
                ++a_state.node_settled;
            if (a_state.node_settled < kSettleSamples || count == a_state.node_logged)
                return;
            if (!a_state.window.take_phase_line())
            {
                logger::info("SOURCE WRITECAPTURE budget-exhausted kind=phase actor={:08x} lines={}",
                    a_state.actor_id, Source_Write_Phase_Line_Budget);
                close_window("budget-exhausted");
                return;
            }
            std::int32_t const previous = a_state.node_logged;
            a_state.node_logged = count;
            logger::info("SOURCE WRITECAPTURE phase actor={:08x} name='nodes' {}->{}",
                a_state.actor_id, previous, count);
        }

        void abort_capture(char const* a_reason)
        {
            WriteState& s = write_state();
            s.window.cancel();
            logger::info("SOURCE WRITECAPTURE window-aborted reason={} actor={:08x} writes={} phases={}",
                a_reason, s.actor_id, s.window.write_lines(), s.window.phase_lines());
        }

        // ---- detour plumbing ----------------------------------------------------

        // The shared thunk shape: preserve every register and the flags, hand the three live
        // arguments to the recorder, then jump to the relocated original head.
        constexpr std::size_t kThunkSize = 74;
        constexpr std::size_t kOrigBlockSize = 5 + 6 + 8;  // relocated head + far jmp + address
        constexpr std::size_t kBlockStride = 96;           // 16-aligned thunk, block behind it

        std::size_t encode_thunk(std::uint8_t* a_dst, std::uintptr_t a_recorder, std::uintptr_t a_original)
        {
            // Eight pushes plus pushfq keep rsp 16-byte aligned for the recorder call, and the
            // saved entry rcx/rdx/r8 slot at +0x50/+0x48/+0x40. The previous 0x38/0x30/0x28 slots
            // named rax/rcx/rdx, so the recorder received the caller's rax as the graph pointer and
            // the graph as the name - see the C-G5-E1 thunk-layout case.
            std::size_t o = 0;
            auto const emit = [&o, a_dst](std::initializer_list<std::uint8_t> const& a_bytes) {
                for (std::uint8_t b : a_bytes)
                    a_dst[o++] = b;
            };
            emit({ 0x50, 0x53, 0x51, 0x52 });                          // push rax/rbx/rcx/rdx
            emit({ 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53 });  // push r8..r11
            emit({ 0x9C });                                            // pushfq
            emit({ 0x48, 0x83, 0xEC, 0x20 });                          // sub rsp,20h (shadow space first)
            emit({ 0x48, 0x8B, 0x4C, 0x24, 0x50 });                    // mov rcx,[rsp+50h] (entry rcx)
            emit({ 0x48, 0x8B, 0x54, 0x24, 0x48 });                    // mov rdx,[rsp+48h] (entry rdx)
            emit({ 0x4C, 0x8B, 0x44, 0x24, 0x40 });                    // mov r8,[rsp+40h]  (entry r8)
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

        // Installs one detour: verifies the head, builds thunk plus relocated original, patches
        // the 5-byte jmp. Returns false on any mismatch (logged by the caller's context row).
        bool install_one_detour(REL::ID a_id, char const* a_name, void const* a_recorder)
        {
            REL::Relocation<std::uintptr_t> target{ a_id };
            auto const* const head = reinterpret_cast<std::uint8_t const*>(target.address());
            if (std::memcmp(head, Source_Write_Head_Bytes.data(), Source_Write_Head_Bytes.size()) != 0)
            {
                std::string hex;
                for (std::size_t i = 0; i < 16; ++i)
                    hex += fmt::format("{:02x}", head[i]);
                logger::warn("SOURCE WRITECAPTURE install-failed reason=head-mismatch target='{}' address={:#x} head={}",
                    a_name, target.address(), hex);
                return false;
            }
            auto& trampoline = SKSE::GetTrampoline();
            // Two strides: the thunk lives in the first, the relocated head plus far jump in the
            // second. The earlier single-stride allocation wrote 19 bytes past its block.
            auto* const mem = static_cast<std::uint8_t*>(trampoline.allocate(2 * kBlockStride));
            if (!mem)
            {
                logger::warn("SOURCE WRITECAPTURE install-failed reason=trampoline-exhausted target='{}'", a_name);
                return false;
            }
            std::uint8_t* const thunk = mem;
            std::uint8_t* const original = mem + kBlockStride;
            std::memcpy(original, Source_Write_Head_Bytes.data(), Source_Write_Head_Bytes.size());
            std::uint8_t const far_jmp[6] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
            std::memcpy(original + Source_Write_Head_Bytes.size(), far_jmp, sizeof(far_jmp));
            std::uintptr_t const back = target.address() + Source_Write_Head_Bytes.size();
            std::memcpy(original + Source_Write_Head_Bytes.size() + sizeof(far_jmp), &back, sizeof(back));
            encode_thunk(thunk, reinterpret_cast<std::uintptr_t>(a_recorder),
                reinterpret_cast<std::uintptr_t>(original));
            std::uint8_t patch[5] = { 0xE9, 0x00, 0x00, 0x00, 0x00 };
            std::int32_t const rel = static_cast<std::int32_t>(
                reinterpret_cast<std::uintptr_t>(thunk) - (target.address() + 5));
            std::memcpy(patch + 1, &rel, sizeof(rel));
            REL::safe_write(target.address(), patch, sizeof(patch));
            logger::info("SOURCE WRITECAPTURE installed target='{}' address={:#x} thunk={:#x} original={:#x} head=48 89 5c 24 10",
                a_name, target.address(), reinterpret_cast<std::uintptr_t>(thunk),
                reinterpret_cast<std::uintptr_t>(original));
            return true;
        }
    }

    void install_source_write_capture()
    {
        bool expected = false;
        if (!g_installed.compare_exchange_strong(expected, true))
            return;
        if (!source_write_capture())
        {
            logger::info("SOURCE WRITECAPTURE install-skip reason=off");
            return;
        }
        if (source_timeline() || source_event_capture())
        {
            logger::warn("SOURCE WRITECAPTURE install-skip reason=other-source-instrument-priority");
            return;
        }
        if (independent_graph_setting() != IndependentGraphMode::off)
        {
            logger::warn("SOURCE WRITECAPTURE install-skip reason=independent-graph-not-off");
            return;
        }
        auto& trampoline = SKSE::GetTrampoline();
        trampoline.create(256);
        bool const ok = install_one_detour(REL::ID(63607), "SetGraphVariableBool", &record_write_bool) &&
            install_one_detour(REL::ID(63609), "SetGraphVariableInt", &record_write_int);
        if (!ok)
            logger::warn("SOURCE WRITECAPTURE install-incomplete reason=partial-failure");
    }

    void arm_source_write_capture()
    {
        if (!source_write_capture() || !g_installed.load(std::memory_order_acquire))
            return;
        write_requests().arm();
        logger::info("SOURCE WRITECAPTURE arm-requested");
    }

    void invalidate_source_write_capture()
    {
        if (!source_write_capture() || !g_installed.load(std::memory_order_acquire))
            return;
        write_requests().invalidate();
    }

    void sample_source_write_capture(RE::Actor& actor)
    {
        if (!source_write_capture() || !g_installed.load(std::memory_order_acquire))
            return;
        WriteState& s = write_state();
        SourceTimelineIntent const intent = write_requests().take();
        if (intent == SourceTimelineIntent::e_invalidate)
        {
            s = WriteState{};
            write_gate().clear();
            logger::info("SOURCE WRITECAPTURE invalidated reason=world-reset");
            return;
        }
        if (intent == SourceTimelineIntent::e_arm)
        {
            s = WriteState{};
            s.window.arm(std::chrono::steady_clock::now());
        }
        if (!s.window.active())
            return;
        if (s.window.expire(std::chrono::steady_clock::now()))
        {
            close_window("expired");
            return;
        }
        std::vector<RE::BShkbAnimationGraph*> graphs;
        collect_player_graphs(actor, graphs);
        if (graphs.empty())
        {
            if (s.actor_id != 0)
                abort_capture("source-lost-or-replaced");
            return;
        }
        // The phase anchor graph is the same one the G3-A timeline picked.
        RE::BShkbAnimationGraph* const phase_graph = graphs.back();
        RE::hkbBehaviorGraph* const behavior = phase_graph->behaviorGraph;
        RE::hkbBehaviorGraphData* const data = behavior ? behavior->data.get() : nullptr;
        RE::hkbVariableValueSet* const set = behavior ? behavior->variableValueSet.get() : nullptr;
        if (!behavior || !data || !set)
        {
            if (s.actor_id != 0)
                abort_capture("source-lost-or-replaced");
            return;
        }
        std::int32_t const count = field_at<std::int32_t>(set, kWordCountOffset);
        std::int32_t const* const words = field_at<std::int32_t const*>(set, kWordDataOffset);
        if (count <= 0 || static_cast<std::size_t>(count) > kMaxWords || !words)
        {
            if (s.actor_id != 0)
                abort_capture("source-lost-or-replaced");
            return;
        }
        SourceTimelineIdentity const source{
            reinterpret_cast<std::uintptr_t>(phase_graph), reinterpret_cast<std::uintptr_t>(behavior),
            reinterpret_cast<std::uintptr_t>(behavior), reinterpret_cast<std::uintptr_t>(data),
            reinterpret_cast<std::uintptr_t>(set), reinterpret_cast<std::uintptr_t>(words),
            static_cast<std::size_t>(count)
        };
        if (!s.has_observed)
        {
            s.observed = source;
            s.has_observed = true;
            baseline_capture(s, *behavior, graphs, words, source.count, actor.GetFormID());
            drain_writes(s);
            if (s.window.active())
                sample_phase(s, *behavior, words, source.count);
            return;
        }
        SourceTimelineIdentity const& previous = s.observed;
        if (previous.actor != source.actor || previous.graph != source.graph ||
            previous.behavior != source.behavior || previous.data != source.data ||
            previous.variable_set != source.variable_set || previous.words != source.words ||
            previous.count != source.count)
        {
            abort_capture("source-lost-or-replaced");
            return;
        }
        drain_writes(s);
        if (!s.window.active())
            return;
        sample_phase(s, *behavior, words, source.count);
    }
}
PLUGIN_NAMESPACE_END
