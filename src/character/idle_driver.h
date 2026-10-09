//
// Created by AmazingBuff on 2026/10/9.
//

#pragma once

#include "RE/N/NiAVObject.h"

#include <chrono>
#include <cmath>
#include <iterator>
#include <string>
#include <vector>

PLUGIN_NAMESPACE_BEGIN

// Procedural idle for the studio copy (PRD FR-03). The copy carries no animation graph of its own
// and the game is paused while the panel is open, so the figure stands frozen unless something
// drives it: this drives a few of the copy's own joints from a clock we own, making it breathe and
// shift weight without touching the source actor, the world simulation or the engine's behaviour
// graphs.
//
// Every frame recomputes the rotations from the captured pose (CharacterClone::pose() restores it
// first), so nothing accumulates and switching the driver off freezes the figure back to the
// captured pose on the next frame.
//
// What one channel does to its joint. A rotation turns the joint (and its descendants) about the
// joint's own origin, so the joint itself does not travel; a translation moves the joint and its
// descendants without turning anything.
enum class IdleMotion : uint8_t
{
    e_rotate,
    e_translate
};

// One drive applied to one joint. Channels must be listed parents-first: a channel pivots on its
// joint's current world position, which a parent's channel has already moved.
struct IdleChannel
{
    const char* joint;      // exact node name in the copy's skeleton
    IdleMotion motion;
    int axis;               // 0 = world X (screen horizontal), 1 = world Y (camera axis), 2 = world Z (up)
    float amplitude;        // degrees for a rotation, studio units for a translation
    double period_seconds;
    double phase_seconds;
};

// The animation. Amplitudes are small on purpose — an idle has to read as breathing and weight
// shift, not as a pose — and the periods are deliberately unrelated, so the figure never returns to
// the same attitude on a short cycle.
//
// Axes in the studio: Z is vertical (Skyrim's up survives the studio placement, which only adds a
// rotation about Z), Y runs along the camera's view axis, X is the screen-horizontal axis. So X is a
// lean toward or away from the camera, Y is a screen roll, Z is a turn.
//
// This skeleton splits the figure in two — the pelvis hangs under CME LBody while the spine hangs
// under CME UBody, siblings under CME Body — so a rotation of the pelvis cannot carry the torso, and
// a rotation about the pelvis's own origin does not move the pelvis at all. The hips therefore need
// a translation channel to read as a weight shift; the torso is driven separately and counter-rolls
// so the shoulders stay level over the shifted hips.
inline constexpr IdleChannel Idle_Channels[] = {
    // Weight shift: the hips travel sideways, tilt, and turn slowly.
    { "NPC Pelvis [Pelv]", IdleMotion::e_translate, 0, 0.9f, 6.0, 0.0 },
    { "NPC Pelvis [Pelv]", IdleMotion::e_rotate, 1, 2.5f, 6.0, 0.0 },
    { "NPC Pelvis [Pelv]", IdleMotion::e_rotate, 2, 1.5f, 9.0, 1.2 },
    // The torso counter-rolls over the shifted hips and breathes with a lean.
    { "NPC Spine1 [Spn1]", IdleMotion::e_rotate, 1, -1.8f, 6.0, 0.0 },
    { "NPC Spine1 [Spn1]", IdleMotion::e_rotate, 0, 1.2f, 3.2, 0.0 },
    { "NPC Spine2 [Spn2]", IdleMotion::e_rotate, 0, 1.0f, 3.2, 0.25 },
    // The head leads the breathing slightly and looks around on its own slow cycle.
    { "NPC Head [Head]", IdleMotion::e_rotate, 0, 1.5f, 5.0, 0.6 },
    { "NPC Head [Head]", IdleMotion::e_rotate, 2, 3.0f, 7.5, 0.0 },
};

inline constexpr size_t Idle_Channel_Count = std::size(Idle_Channels);

// The angle of one channel at a given idle time, in degrees. Pure math, unit-tested: the amplitudes,
// periods and phases are the whole animation, and a wrong sign or period is invisible in a single
// frame but obvious over one cycle.
inline float idle_angle_degrees(float amplitude_degrees, double period_seconds, double phase_seconds, double seconds)
{
    if (period_seconds <= 0.0)
        return 0.0f;
    const double phase = 6.283185307179586 * (seconds + phase_seconds) / period_seconds;
    return amplitude_degrees * static_cast<float>(std::sin(phase));
}

class IdleDriver
{
public:
    void reset();
    void set_enabled(bool enabled);
    [[nodiscard]] bool enabled() const { return m_enabled; }
    [[nodiscard]] size_t driven_channels() const { return m_bound.size(); }
    [[nodiscard]] double time() const { return m_time; }

    // Advances the clock and applies every channel to the copy's joints.
    void advance(RE::NiAVObject& root);

private:
    struct Bound
    {
        RE::NiAVObject* joint;
        IdleChannel const* channel;
        RE::NiPoint3 reference_translate;  // the captured pose's position of that joint
    };

    // Resolves each channel's joint by exact name; logs once what was found and what was missing.
    bool bind(RE::NiAVObject& root);
    // Every couple of seconds, prints how far each driven joint has actually travelled. "The waist
    // does not look like it moves" is not something a table of amplitudes can settle.
    void report_motion();

    bool m_enabled = true;
    bool m_bound_once = false;
    double m_time = 0.0;
    double m_last_report = 0.0;
    std::chrono::steady_clock::time_point m_last_tick{};
    std::vector<Bound> m_bound;
    std::string m_missing;
};

PLUGIN_NAMESPACE_END
