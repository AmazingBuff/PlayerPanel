//
// C-G4-A instrument (2026-10-31): thread-local marker and hit tally for "native candidate
// activity in progress". drive_graph_step and the pre-registered event dispatches arm the
// depth guard; the OAR evaluate detours count their hits into the tally; the window close
// sites take the tally for their OAR CONTEXT summary row. Data only — each emission site
// owns its log row. The tally is process-global per thread; the panel drives one player
// candidate, so interleaved non-player sessions only ever take zero counts.
//
#pragma once

#include <cstdint>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    struct CandidateWindowTally
    {
        std::uint32_t depth = 0;
        std::uint32_t evaluations = 0;
        std::uint32_t overlaps = 0;
        std::uint32_t source_actor_id = 0;
    };

    inline thread_local CandidateWindowTally g_candidate_window{};

    [[nodiscard]] inline bool candidate_window_active() { return g_candidate_window.depth > 0; }

    // A hit whose REFR form id equals the armed window's source actor id is the leak
    // signature this instrument looks for: a holder-less candidate has no REFR of its own,
    // so the only REFR an OAR fallback (holder -> rootNode) could resolve for it is its
    // source's.
    inline void note_candidate_window_hit(std::uint32_t refr_id)
    {
        if (g_candidate_window.depth == 0) return;
        ++g_candidate_window.evaluations;
        if (refr_id != 0 && refr_id == g_candidate_window.source_actor_id) ++g_candidate_window.overlaps;
    }

    // Arms a window (resets the counts, remembers the source actor for overlap matching).
    inline void arm_candidate_window(std::uint32_t source_actor_id)
    {
        g_candidate_window.evaluations = 0;
        g_candidate_window.overlaps = 0;
        g_candidate_window.source_actor_id = source_actor_id;
    }

    // Takes the accumulated counts without disarming; callers emit their own rows.
    [[nodiscard]] inline CandidateWindowTally take_candidate_window_tally()
    {
        CandidateWindowTally const taken{ g_candidate_window.depth, g_candidate_window.evaluations,
            g_candidate_window.overlaps, g_candidate_window.source_actor_id };
        g_candidate_window.evaluations = 0;
        g_candidate_window.overlaps = 0;
        return taken;
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
