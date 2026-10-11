//
// Created by AmazingBuff on 2026/10/11.
//

#pragma once

#include "character/animation_source.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

PLUGIN_NAMESPACE_BEGIN

// HKX38: the scheduler's data source. A puppet - a player-copy actor the engine assembles at
// full LOD and the panel then owns - stands in for "what would an actor in this state do": its
// behaviour graph runs under the engine while the world plays, and under OUR clock while the
// panel pauses the game (HKX37 proved both halves). The display keeps drawing the detached
// scene-graph clone; the driver only reads the puppet's running clips and hands each frame's
// pose to the copy.
//
// Lifecycle: one puppet per save session. Born at the player on the load boundary (v6.70 recipe:
// full-LOD assembly needs the culler, fade pinned near-invisible), parked deep once assembled
// (v6.25: an actor parked there is invisible and untouchable; whether the engine still ticks it
// at that depth does not matter - the manual tick owns panel time), released at the next load
// boundary. Runtime form IDs never enter a save, so an unclean exit leaves nothing behind.
//
// Panel-side contract: while F2 arms the driver, SceneGraphCopy::animate_at_base_pose asks this
// driver first; an empty response means fall back to the capture-time clip, then to the
// procedural idle. The H05 census counts the puppet as panel-owned, not as a world change.
class PuppetDriver
{
public:
    static PuppetDriver& instance();

    // Load-boundary plumbing (main thread): the save load arms the spawn, the next load arms the
    // release; the render bracket consumes both.
    void request_spawn();
    void request_release();
    // Session boundary seen on the render bracket itself (main menu, generation change).
    void release(char const* reason);

    // Per rendered frame, inside the render bracket: consume the requests, run the grace and the
    // park, and while the game is paused drive the puppet's behaviour graph with our own clock.
    void tick(bool paused);

    // One frame of the copy's animation from the puppet's live graph. Returns the driver identity
    // for the log ("puppet:<clip>#<index>"), empty when the puppet cannot serve this frame.
    std::string drive(RE::NiAVObject& copy_root);

    [[nodiscard]] bool alive() const { return m_puppet != nullptr; }
    // H05 census: this form id is the panel's own, not a world change.
    [[nodiscard]] static bool is_owned_form(std::uint32_t form_id);

private:
    PuppetDriver() = default;

    void spawn();
    void park(char const* why);
    // HKX39: mirrors the player's hand equipment onto the puppet and pushes the draw/sheathe
    // graph state in the same paused tick.
    void poll_equipment(RE::Actor& puppet);

    std::atomic_bool m_spawn_requested{ false };
    std::atomic_bool m_release_requested{ false };
    RE::NiPointer<RE::TESObjectREFR> m_puppet;
    std::uint32_t m_puppet_form_id = 0;
    std::uint32_t m_grace_frames = 0;
    std::uint32_t m_repark_frames = 0;
    std::uint32_t m_manual_ticks = 0;
    std::uint32_t m_stall_frames = 0;
    bool m_stall_logged = false;
    // HKX39: the tier-2 A/B fires on each switch, delayed past the blend's settle window. With
    // HKX42 the solve runs first when the binding table is empty - the A/B then judges the solved
    // mapping, not the identity it replaced.
    bool m_ab_pending = false;
    std::uint32_t m_ab_deadline = 0;
    // HKX42: the empirical mapping solver, armed on every identity-unverified switch.
    bool m_solving = false;
    // HKX39: the equip lever. The player's hands are polled while the panel is open; a change is
    // mirrored onto the puppet and the draw/sheathe graph state is pushed in the same tick, so
    // the panel's equip reaction does not wait for an engine update the pause never delivers.
    std::uint32_t m_right_form = 0;
    std::uint32_t m_left_form = 0;
    std::uint32_t m_equip_reactions = 0;
    std::chrono::steady_clock::time_point m_last_tick{};
    // The clip currently written into the copy: the full resolution is repeated only when the
    // graph's strongest control moves to another clip.
    RE::hkbClipGenerator const* m_current_generator = nullptr;
    ClipPlayback m_playback;
    std::string m_mapping;
    std::string m_driver_note;
};

PLUGIN_NAMESPACE_END
