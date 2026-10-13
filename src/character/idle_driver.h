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

enum class IdleMotion : uint8_t
{
    e_rotate,
    e_translate
};

// World axes: X screen-horizontal, Y view direction, Z up. Apply parent channels first.
struct IdleChannel
{
    const char* joint;      // exact node name in the copy's skeleton
    IdleMotion motion;
    int axis;               // 0 = world X (screen horizontal), 1 = world Y (camera axis), 2 = world Z (up)
    float amplitude;        // degrees for a rotation, studio units for a translation
    double period_seconds;
    double phase_seconds;
};

inline constexpr IdleChannel Idle_Channels[] = {

    { "NPC Pelvis [Pelv]", IdleMotion::e_translate, 0, 0.9f, 6.0, 0.0 },
    { "NPC Pelvis [Pelv]", IdleMotion::e_rotate, 1, 2.5f, 6.0, 0.0 },
    { "NPC Pelvis [Pelv]", IdleMotion::e_rotate, 2, 1.5f, 9.0, 1.2 },

    { "NPC Spine1 [Spn1]", IdleMotion::e_rotate, 1, -1.8f, 6.0, 0.0 },
    { "NPC Spine1 [Spn1]", IdleMotion::e_rotate, 0, 1.2f, 3.2, 0.0 },
    { "NPC Spine2 [Spn2]", IdleMotion::e_rotate, 0, 1.0f, 3.2, 0.25 },

    { "NPC Head [Head]", IdleMotion::e_rotate, 0, 1.5f, 5.0, 0.6 },
    { "NPC Head [Head]", IdleMotion::e_rotate, 2, 3.0f, 7.5, 0.0 },
};

inline constexpr size_t Idle_Channel_Count = std::size(Idle_Channels);

inline float idle_angle_degrees(float amplitude_degrees, double period_seconds, double phase_seconds, double seconds)
{
    if (period_seconds <= 0.0)
        return 0.0f;
    const double phase = 6.283185307179586 * (seconds + phase_seconds) / period_seconds;
    return amplitude_degrees * static_cast<float>(std::sin(phase));
}

// Base placement restores the captured pose before each frame, preventing accumulated motion.
class IdleDriver
{
public:
    void reset();

    void restart();

    void advance(RE::NiAVObject& root);

private:
    struct Bound
    {
        RE::NiAVObject* joint;
        IdleChannel const* channel;

    };

    bool bind(RE::NiAVObject& root);

    bool m_bound_once = false;
    double m_time = 0.0;

    std::chrono::steady_clock::time_point m_last_tick{};
    std::vector<Bound> m_bound;
    std::string m_missing;
};

PLUGIN_NAMESPACE_END
