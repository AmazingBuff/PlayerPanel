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
// A4 adds: the raw numeric-accessor type histogram. A numeric site hit only proves the
// accessor was called; type 3 is the graph-variable accessor. A3 read the type only on the
// rows it emitted, and those were capped at 32 total evaluations (7 numeric rows out of 72
// hits), so its "all type 0" claim could not cover the window. The histogram counts every
// armed numeric hit independently of that budget and must reconcile with the numeric site
// count. Type-3 attribution rows have their own bounded budget because they can arrive after
// the detail rows are exhausted.
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

    // Named buckets 0..3 plus one overflow bucket. The engine's numeric accessor type is an
    // int32; type 0 is another value source and type 3 is the graph-variable accessor, other
    // values are unnamed and stay separated instead of being folded into type 0.
    inline constexpr std::size_t Numeric_Type_Buckets = 4;

    // Bounded per-window budget for type-3 attribution rows (name/REFR/clip/owner). The
    // detail-row cap cannot carry them: they may arrive after row 32.
    inline constexpr std::uint32_t Numeric_Type3_Row_Cap = 8;

    struct CandidateWindowTally
    {
        std::uint32_t depth = 0;
        std::uint32_t evaluations = 0;
        std::uint32_t overlaps = 0;
        std::uint32_t cand_active = 0;
        std::uint32_t src_active = 0;
        std::uint32_t neither = 0;
        std::uint32_t sites[static_cast<std::size_t>(OarHitSite::e_site_count)] = {};
        // C-G4-A4: every armed numeric hit by raw type. A negative type (null or unreadable
        // accessor) is unclassified rather than dropped, so the sum always reconciles.
        std::uint32_t numeric_types[Numeric_Type_Buckets] = {};
        std::uint32_t numeric_type_other = 0;
        std::uint32_t numeric_unclassified = 0;
        std::uint32_t numeric_type3_rows = 0;
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
    struct NodeListHeader
    {
        std::uintptr_t entries;
        std::int32_t count;
    };

    [[nodiscard]] inline NodeListHeader read_node_list_header(std::uintptr_t node_list)
    {
        if (!node_list) return { 0, -1 };
        return { *reinterpret_cast<std::uintptr_t const*>(node_list),
            *reinterpret_cast<std::int32_t const*>(node_list + 0x8) };
    }

    // Live candidate active count at hit time — the scan-sanity anchor for the neither
    // bucket (the same +8 read the DriveState rows log as active-nodes=…/N).
    [[nodiscard]] inline std::int32_t candidate_node_list_count()
    {
        return g_candidate_window.cand_node_list
             ? *reinterpret_cast<std::int32_t const*>(g_candidate_window.cand_node_list + 0x8)
             : -1;
    }

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

    // Counts one armed numeric hit by raw accessor type. Called for every hit before the
    // detail-row budget, so a capped row still contributes to the window total.
    inline void note_candidate_numeric_type(int numeric_type)
    {
        if (g_candidate_window.depth == 0) return;
        if (numeric_type < 0)
        {
            ++g_candidate_window.numeric_unclassified;
            return;
        }
        if (static_cast<std::size_t>(numeric_type) < Numeric_Type_Buckets)
            ++g_candidate_window.numeric_types[static_cast<std::size_t>(numeric_type)];
        else
            ++g_candidate_window.numeric_type_other;
    }

    // The reconciliation figure: must equal sites[e_numeric] on every summary row.
    [[nodiscard]] inline std::uint32_t candidate_window_numeric_sum(CandidateWindowTally const& tally)
    {
        std::uint32_t sum = tally.numeric_type_other + tally.numeric_unclassified;
        for (std::size_t index = 0; index < Numeric_Type_Buckets; ++index)
            sum += tally.numeric_types[index];
        return sum;
    }

    // Reserves one type-3 attribution row for this window; false once the budget is spent.
    [[nodiscard]] inline bool take_numeric_type3_row()
    {
        if (g_candidate_window.numeric_type3_rows >= Numeric_Type3_Row_Cap) return false;
        ++g_candidate_window.numeric_type3_rows;
        return true;
    }

    // Zeroes every counter and keeps the window metadata. Arm, take and the foreign-actor
    // take path share it, so a new counter cannot be missed in one of them.
    inline void clear_candidate_window_counts(CandidateWindowTally& tally)
    {
        tally.evaluations = 0;
        tally.overlaps = 0;
        tally.cand_active = 0;
        tally.src_active = 0;
        tally.neither = 0;
        for (std::size_t index = 0; index < static_cast<std::size_t>(OarHitSite::e_site_count); ++index)
            tally.sites[index] = 0;
        for (std::size_t index = 0; index < Numeric_Type_Buckets; ++index)
            tally.numeric_types[index] = 0;
        tally.numeric_type_other = 0;
        tally.numeric_unclassified = 0;
        tally.numeric_type3_rows = 0;
    }

    // Arms a window (resets the counts, remembers the source actor for overlap matching).
    inline void arm_candidate_window(std::uint32_t source_actor_id)
    {
        clear_candidate_window_counts(g_candidate_window);
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
        CandidateWindowTally taken = g_candidate_window;
        clear_candidate_window_counts(g_candidate_window);
        return taken;
    }

    // Takes the counts only when the armed window belongs to the asking actor: interleaved
    // non-candidate sessions (one DisplaySession per registered actor) must neither reset
    // nor consume another window's tally. On mismatch the tally is left untouched and the
    // returned row carries zero counts with the foreign armed id.
    [[nodiscard]] inline CandidateWindowTally take_candidate_window_tally_for(std::uint32_t actor_id)
    {
        if (g_candidate_window.source_actor_id != actor_id)
        {
            CandidateWindowTally foreign = g_candidate_window;
            clear_candidate_window_counts(foreign);
            return foreign;
        }
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
