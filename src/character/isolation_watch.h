//
// Created by AmazingBuff on 2026/10/11.
//

#pragma once

#include <atomic>
#include <memory>

PLUGIN_NAMESPACE_BEGIN

// H05 instrument (FR-03/FR-05 isolation red line). Read-only censuses around every panel operation
// prove, with numbers, that the panel's work leaves the world it borrowed from untouched: the
// player's third-person graph, the engine pose behind it, the player reference itself and the
// nearby actors are re-read before and after each operation while the game is paused, and any
// change between two censuses names the window it happened in.
//
// The watch never writes what it reads and holds no pointers between censuses - actors are
// re-resolved from process handles and the graph re-read from the player's manager each time - so
// a freed or moved object can only be counted, never dereferenced. Session shape: the first census
// of a paused panel session is the baseline; every census reports its own window against the
// previous one and the whole session against the baseline; the session ends on the first frame
// that no longer qualifies (menu closed or unpaused) or on a load boundary, where one summary line
// folds the whole session into a verdict. The tolerance is the one the capture's SOURCE-DIFF
// settled on: the paused world still recomputes world transforms with a few ULPs of jitter, so
// drift below the loose bar is observation, and only drift past it or an object-set change fails.
class IsolationWatch
{
public:
    IsolationWatch();
    ~IsolationWatch();

    // Taken before the named operation runs. The first census of a session is the baseline.
    void census_pre(char const* operation);
    // Taken after the named operation ran.
    void census_post(char const* operation);
    // Ends the session with a summary. No census is taken at close: the menu close and the unpause
    // arrive together, and a read after the unpause would race the engine's first animation frame,
    // so the last operation's post-census is the last comparable state.
    void end(char const* reason);
    // Asked when a load boundary fires. That happens on the main thread while the render bracket
    // may be mid-census, so the request is queued and honored on the render thread's next frame.
    void request_end();
    // Consumes the queued end request; false when none was queued since the last consume.
    bool take_end_request();
    [[nodiscard]] bool armed() const;

    // Implementation types, named here because the free helpers in the implementation file fill
    // and compare them; they stay incomplete, and their definitions live in the .cpp.
    struct Census;
    struct State;

private:
    void take(Census& out);
    void record(char const* operation, char const* stage);

    std::atomic_bool m_armed{ false };
    std::atomic_bool m_end_requested{ false };
    std::unique_ptr<State> m_state;
};

PLUGIN_NAMESPACE_END
