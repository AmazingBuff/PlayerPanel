//
// Created by AmazingBuff on 2026/10/11.
//

#pragma once

#if CHARACTER_PANEL_S2_PROBE

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

PLUGIN_NAMESPACE_BEGIN

// HKX37 probe (s2-animation-handoff.md §0 "傀儡 Actor" direction). The idea under test: the panel
// keeps drawing the detached scene-graph clone exactly as today, but the animation DATA comes from
// a real, engine-ticked actor - a puppet copy of the player - so OAR evaluates its conditions for
// the puppet and the panel samples whatever OAR actually chose. Five questions, one F6 press each:
//
//   1. spawn   - can the puppet be born at the player (v6.70 recipe) and assemble a full graph?
//   2. tick    - does IAnimationGraphManagerHolder::UpdateAnimationGraphManager advance the
//                puppet's behaviour graph inside the paused panel, driven by our own clock?
//   3. (rides on 2) - does OAR replace the puppet's clips during a manual tick?
//   4. equip   - does equipping a weapon on the puppet DURING the pause activate an equip clip?
//   5. sample  - does the verified sampling chain (engine-loaded binding -> SamplePartialTracks)
//                reproduce the puppet's own poseLocal (numeric A/B, HKX28 protocol)?
//
// The probe owns the puppet outright: it pins movement/combat flags, keeps the fade suppressed so
// the shell never appears, and deletes the reference on the final press or when a load boundary
// invalidates it. Everything is probed with guarded reads; a failed stage logs a reason and the
// stage machine keeps advancing so one session still answers the remaining questions.
class PuppetProbe
{
public:
    static PuppetProbe& instance();

    // F6, pressed on the input thread; the render bracket consumes the request.
    void request_advance();
    // Per rendered frame, inside the render bracket (next to SceneGraphCopy::process_requests).
    void tick();

private:
    enum class Stage : std::uint8_t
    {
        e_off,        // nothing spawned
        e_alive,      // spawned at the player; the engine ticks it while unpaused
        e_manual,     // manual UpdateAnimationGraphManager while the panel is paused
        e_equipped,   // equipment mirrored during the pause; watching for the equip clip
        e_sampled,    // numeric A/B taken
        e_dead,       // deleted; the next press restarts from e_off
    };

    PuppetProbe() = default;

    void spawn();
    void start_manual_tick();
    void equip_test();
    void sample_ab();
    void kill(char const* reason);

    [[nodiscard]] RE::Actor* actor() const;
    std::string log_clips(char const* phase);

    std::atomic<std::uint8_t> m_requested{ 0 };
    Stage m_stage = Stage::e_off;
    RE::NiPointer<RE::TESObjectREFR> m_puppet;
    std::uint32_t m_puppet_form_id = 0;
    std::uint32_t m_grace_frames = 0;
    std::uint32_t m_ticks = 0;
    std::uint32_t m_digests = 0;
    std::chrono::steady_clock::time_point m_last_tick{};
    std::vector<std::string> m_replacements_seen;  // distinct OAR-style names the digests observed
    std::string m_ab_verdict = "not-taken";
};

PLUGIN_NAMESPACE_END

#endif
