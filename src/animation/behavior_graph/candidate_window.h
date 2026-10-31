//
// C-G4-A/A2 instrument (2026-10-31): thread-local marker and hit tally for "native candidate
// activity in progress". drive_graph_step and the pre-registered event dispatches arm the
// depth guard; the OAR evaluate detours count their hits into the tally; the window close
// sites take the tally for their OAR CONTEXT summary row. Data only — each emission site
// owns its log row. The tally is process-global per thread; the panel drives one player
// candidate, so interleaved non-player sessions only ever take zero counts.
//
// A2 adds: per-detour-site counts (a numeric hit means a graph variable was actually
// fetched), clip-owner classification against the candidate's and the source's
// hkbBehaviorGraph::activeNodes, and a peek for the segment snapshot rows.
//
#pragma once

#include <cstddef>
#include <cstdint>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    enum class OarHitSite : std::uint8_t
    {
        e_evaluate,
        e_numeric,
        e_evaluate_set,
        e_evaluate_replacement,
        e_site_count
    };

    enum class OarClipOwner : std::uint8_t { e_none, e_candidate, e_source };

    struct CandidateWindowTally
    {
        std::uint32_t depth = 0;
        std::uint32_t evaluations = 0;
        std::uint32_t overlaps = 0;
        std::uint32_t cand_active = 0;
        std::uint32_t src_active = 0;
        std::uint32_t neither = 0;
        std::uint32_t sites[static_cast<std::size_t>(OarHitSite::e_site_count)] = {};
        std::uint32_t source_actor_id = 0;
        // hkbBehaviorGraph::activeNodes NodeList pointers, captured for hit-time clip-owner
        // classification. The candidate's list does not exist until its activation, so
        // callers arm first and set the lists once the graphs are live; hits before that
        // classify against the source's list only.
        std::uintptr_t cand_node_list = 0;
        std::uintptr_t src_node_list = 0;
    };

    inline thread_local CandidateWindowTally g_candidate_window{};

    [[nodiscard]] inline bool candidate_window_active() { return g_candidate_window.depth > 0; }

    // The audited engine NodeList layout (construction audit, 58377's walk): 0x10-byte
    // header {entries@+0, count@+8}, entries are 0x90-byte records with the node pointer at
    // entry+0x58. Activate initializes the count sentinel; a non-positive or oversized count
    // reads as empty.
    [[nodiscard]] inline bool active_node_list_contains(std::uintptr_t node_list, std::uintptr_t node)
    {
        if (!node_list || !node) return false;
        const std::uintptr_t entries = *reinterpret_cast<std::uintptr_t const*>(node_list);
        const std::int32_t count = *reinterpret_cast<std::int32_t const*>(node_list + 0x8);
        if (!entries || count <= 0 || count > 4096) return false;
        for (std::int32_t index = 0; index < count; ++index)
        {
            const std::uintptr_t entry = entries + static_cast<std::uintptr_t>(index) * 0x90;
            if (*reinterpret_cast<std::uintptr_t const*>(entry + 0x58) == node) return true;
        }
        return false;
    }

    [[nodiscard]] inline OarClipOwner classify_clip_owner(std::uintptr_t clip)
    {
        if (!clip) return OarClipOwner::e_none;
        if (active_node_list_contains(g_candidate_window.cand_node_list, clip)) return OarClipOwner::e_candidate;
        if (active_node_list_contains(g_candidate_window.src_node_list, clip)) return OarClipOwner::e_source;
        return OarClipOwner::e_none;
    }

    [[nodiscard]] inline char const* oar_clip_owner_name(OarClipOwner owner)
    {
        switch (owner)
        {
        case OarClipOwner::e_candidate: return "cand";
        case OarClipOwner::e_source: return "src";
        default: return "neither";
        }
    }

    [[nodiscard]] inline char const* oar_hit_site_name(OarHitSite site)
    {
        switch (site)
        {
        case OarHitSite::e_numeric: return "numeric";
        case OarHitSite::e_evaluate_set: return "evaluate-set";
        case OarHitSite::e_evaluate_replacement: return "evaluate-replacement";
        default: return "evaluate";
        }
    }

    // A hit whose REFR form id equals the armed window's source actor id is the leak
    // signature this instrument looks for: a holder-less candidate has no REFR of its own,
    // so the only REFR an OAR fallback (holder -> rootNode) could resolve for it is its
    // source's. The clip owner counters answer which graph's active clips the evaluation
    // machinery was working on.
    inline void note_candidate_window_hit(OarHitSite site, std::uint32_t refr_id, std::uintptr_t clip)
    {
        if (g_candidate_window.depth == 0) return;
        auto& window = g_candidate_window;
        ++window.evaluations;
        ++window.sites[static_cast<std::size_t>(site)];
        if (refr_id != 0 && refr_id == window.source_actor_id) ++window.overlaps;
        const OarClipOwner owner = classify_clip_owner(clip);
        if (owner == OarClipOwner::e_candidate) ++window.cand_active;
        else if (owner == OarClipOwner::e_source) ++window.src_active;
        else ++window.neither;
    }

    // Arms a window (resets the counts, remembers the source actor for overlap matching).
    inline void arm_candidate_window(std::uint32_t source_actor_id)
    {
        g_candidate_window.evaluations = 0;
        g_candidate_window.overlaps = 0;
        g_candidate_window.cand_active = 0;
        g_candidate_window.src_active = 0;
        g_candidate_window.neither = 0;
        for (std::size_t index = 0; index < static_cast<std::size_t>(OarHitSite::e_site_count); ++index)
            g_candidate_window.sites[index] = 0;
        g_candidate_window.source_actor_id = source_actor_id;
    }

    // Captures the two graphs' activeNodes NodeLists for hit-time clip-owner classification.
    inline void arm_candidate_window_node_lists(std::uintptr_t cand_node_list, std::uintptr_t src_node_list)
    {
        g_candidate_window.cand_node_list = cand_node_list;
        g_candidate_window.src_node_list = src_node_list;
    }

    // Snapshot without clearing, for the segment rows inside a window.
    [[nodiscard]] inline CandidateWindowTally peek_candidate_window_tally()
    {
        return g_candidate_window;
    }

    // Takes the accumulated counts without disarming; callers emit their own rows.
    [[nodiscard]] inline CandidateWindowTally take_candidate_window_tally()
    {
        CandidateWindowTally taken = peek_candidate_window_tally();
        g_candidate_window.evaluations = 0;
        g_candidate_window.overlaps = 0;
        g_candidate_window.cand_active = 0;
        g_candidate_window.src_active = 0;
        g_candidate_window.neither = 0;
        for (std::size_t index = 0; index < static_cast<std::size_t>(OarHitSite::e_site_count); ++index)
            g_candidate_window.sites[index] = 0;
        return taken;
    }

    // Takes the counts only when the armed window belongs to the asking actor: interleaved
    // non-candidate sessions (one DisplaySession per registered actor) must neither reset
    // nor consume another window's tally. On mismatch the tally is left untouched and the
    // returned row carries zero counts with the foreign armed id.
    [[nodiscard]] inline CandidateWindowTally take_candidate_window_tally_for(std::uint32_t actor_id)
    {
        if (g_candidate_window.source_actor_id != actor_id)
            return CandidateWindowTally{ g_candidate_window.depth, 0, 0, 0, 0, 0, {}, g_candidate_window.source_actor_id,
                g_candidate_window.cand_node_list, g_candidate_window.src_node_list };
        return take_candidate_window_tally();
    }

    class ScopedCandidateWindow
    {
    public:
        ScopedCandidateWindow() { ++g_candidate_window.depth; }
        ~ScopedCandidateWindow() { --g_candidate_window.depth; }
        ScopedCandidateWindow(ScopedCandidateWindow const&) = delete;
        ScopedCandidateWindow& operator=(ScopedCandidateWindow const&) = delete;
    };
}
PLUGIN_NAMESPACE_END
