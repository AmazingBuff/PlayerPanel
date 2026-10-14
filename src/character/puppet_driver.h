//
// Created by AmazingBuff on 2026/10/11.
//

#pragma once

#include "character/animation_source.h"
#include "character/figure_audit.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

PLUGIN_NAMESPACE_BEGIN

// One session-owned Actor supplies graph decisions; the renderer still draws a detached copy.
// Birth requires the running world. Paused stepping uses panel time and never advances other Actors.
class PuppetDriver
{
public:
    static PuppetDriver& instance();

    void request_spawn();
    void request_release();

    void release(char const* reason);

    void tick(bool paused);

    std::string drive(RE::NiAVObject& copy_root);


    [[nodiscard]] static bool is_owned_form(std::uint32_t form_id);

    [[nodiscard]] RE::Actor* actor() const;
    [[nodiscard]] bool tick_enabled() const { return m_tick_enabled.load(std::memory_order_acquire); }

    void set_tick_enabled(bool a_enabled);
    void request_step();

    void set_force_generate(bool a_enabled);
    [[nodiscard]] bool force_generate() const { return m_force_generate.load(std::memory_order_acquire); }

    // Auto mode (`FinalPoseApply::Drive::e_auto`): the generate is forced on PAUSED frames only, so a
    // paused panel has a pose to write while an unpaused one stays unforced for the mapping proof.
    void set_force_when_paused(bool a_enabled);
    [[nodiscard]] bool force_when_paused() const { return m_force_when_paused.load(std::memory_order_acquire); }
    [[nodiscard]] std::uint32_t manual_ticks() const { return m_manual_ticks; }

private:
    PuppetDriver() = default;

    void spawn();
    void park(char const* why);

    // AFP10-A: has the shell finished assembling? The census has to be non-empty, fully bound and
    // unchanged for Assembly_Stable_Frames before the park may move the reference away from the
    // player. Keeps the last census so the park line can print what it decided on.
    [[nodiscard]] bool assembly_settled(RE::NiAVObject* a_root);

    // AFP10-B: the player's worn body items onto the puppet. Returns how many were equipped.
    std::uint32_t dress(RE::Actor& a_puppet);

    // AFP10-C: the player's body morphs onto the puppet (skee64 BodyMorph, the same route OBody uses
    // to shape it). Compares before it writes, so it is cheap when the two already agree - which is
    // what lets the panel re-assert it on a slow cadence and survive OBody re-rolling the preset.
    void apply_body_shape(RE::Actor& a_puppet, char const* a_why);

    // `a_every_frame` is the paused panel: the player is looking at the figure while changing gear.
    // While the world runs the worn set is re-read on the Worn_Poll_Period cadence instead.
    void poll_equipment(RE::Actor& a_puppet, bool a_every_frame);

    std::atomic_bool m_spawn_requested{ false };
    std::atomic_bool m_release_requested{ false };

    std::atomic_bool m_tick_enabled{ true };

    std::atomic_bool m_step_requested{ false };

    std::atomic_bool m_force_generate{ false };
    std::atomic_bool m_force_when_paused{ true };
    RE::NiPointer<RE::TESObjectREFR> m_puppet;
    std::uint32_t m_puppet_form_id = 0;
    // Where the reference was last parked WITH the character controller (see park()): the warp is
    // only requested when the reference actually moves, so the per-frame re-assertion stays cheap.
    RE::NiPoint3 m_park_target{};
    std::uint32_t m_grace_frames = 0;
    std::uint32_t m_repark_frames = 0;
    std::uint32_t m_manual_ticks = 0;

    // The assembly window's own state: the census identity it last saw, how long that identity has
    // held, whether the figure has been dressed yet, and how long the dressing has had to settle.
    FigureAssembly m_assembly_census;
    std::uint32_t m_assembly_signature = 0;
    std::uint32_t m_assembly_stable = 0;
    bool m_dressed = false;
    std::uint32_t m_dress_frames = 0;

    bool m_data_logged = false;
    std::uint32_t m_stall_frames = 0;
    bool m_stall_logged = false;

    std::uint32_t m_right_form = 0;
    std::uint32_t m_left_form = 0;
    std::uint32_t m_equip_reactions = 0;
    // Two independent cadence counters for the world-running branch of poll_equipment: the worn
    // reconcile and the prune must not share one, or the counter-resetting call starves the other
    // (the shared counter let the worn sync's reset keep the prune's threshold permanently unreachable
    // while the world ran - the prune only ever fired on paused frames).
    std::uint32_t m_worn_poll_frames = 0;
    std::uint32_t m_prune_poll_frames = 0;
    std::uint32_t m_shape_ticks = 0;
    std::chrono::steady_clock::time_point m_last_tick{};

    RE::hkbClipGenerator const* m_current_generator = nullptr;
    ClipPlayback m_playback;
    std::string m_mapping;
};

PLUGIN_NAMESPACE_END
