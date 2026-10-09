//
// Created by AmazingBuff on 2026/10/9.
//

#include "character/idle_driver.h"

#include "character/snapshot_transform.h"

#include <algorithm>
#include <string_view>

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // A frame that took longer than this is a stall (menu transition, alt-tab, shader compile), not
    // animation time: letting it through would jump the figure forward by a visible amount.
    constexpr double Max_Idle_Step_Seconds = 0.1;
    constexpr double Motion_Report_Interval_Seconds = 2.0;

    constexpr float Degrees_To_Radians = 0.017453292f;
}

void IdleDriver::reset()
{
    m_bound.clear();
    m_bound_once = false;
    m_missing.clear();
    // Restart the clock: the phase the figure was frozen at is not worth resuming from, and a stale
    // tick would otherwise produce one oversized step.
    m_time = 0.0;
    m_last_tick = std::chrono::steady_clock::now();
}

void IdleDriver::set_enabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    m_last_tick = std::chrono::steady_clock::now();
}

bool IdleDriver::bind(RE::NiAVObject& root)
{
    std::vector<RE::NiAVObject*> nodes;
    RE::BSVisit::TraverseScenegraphObjects(&root, [&](RE::NiAVObject* object)
    {
        nodes.push_back(object);
        return RE::BSVisit::BSVisitControl::kContinue;
    });

    m_bound.clear();
    m_missing.clear();
    for (const IdleChannel& channel : Idle_Channels)
    {
        RE::NiAVObject* joint = nullptr;
        for (RE::NiAVObject* node : nodes)
        {
            // Exact names: the copy carries several skeletons plus modded hair and cloth chains whose
            // nodes contain words like "Head", and a substring match would drive those instead.
            if (node->name.c_str() && std::string_view(node->name.c_str()) == channel.joint)
            {
                joint = node;
                break;
            }
        }
        if (!joint)
        {
            if (!m_missing.empty())
                m_missing += ", ";
            m_missing += channel.joint;
            continue;
        }
        // The reference is the captured pose: CharacterClone::pose() has just restored it, so this is
        // where the joint sits before anything of ours touches it.
        m_bound.push_back({ joint, &channel, joint->world.translate });
    }

    m_bound_once = true;
    logger::info("SCOPY IDLE bound {}/{} channels{}",
        m_bound.size(), Idle_Channel_Count,
        m_missing.empty() ? std::string() : fmt::format("; missing: {}", m_missing));
    return !m_bound.empty();
}

void IdleDriver::advance(RE::NiAVObject& root)
{
    if (!m_enabled)
        return;

    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    const double step = std::min(std::chrono::duration<double>(now - m_last_tick).count(), Max_Idle_Step_Seconds);
    m_last_tick = now;
    m_time += std::max(step, 0.0);

    if (!m_bound_once && !bind(root))
        return;

    for (const Bound& bound : m_bound)
    {
        const float value = idle_angle_degrees(bound.channel->amplitude, bound.channel->period_seconds, bound.channel->phase_seconds, m_time);
        if (bound.channel->motion == IdleMotion::e_rotate)
            rotate_about_own_origin(bound.joint, bound.channel->axis, value * Degrees_To_Radians);
        else
            translate_subtree(bound.joint, bound.channel->axis, value);
    }

    if (m_time - m_last_report >= Motion_Report_Interval_Seconds)
    {
        report_motion();
        m_last_report = m_time;
    }
}

void IdleDriver::report_motion()
{
    std::string line;
    std::vector<RE::NiAVObject*> reported;
    for (const Bound& bound : m_bound)
    {
        if (std::find(reported.begin(), reported.end(), bound.joint) != reported.end())
            continue;
        reported.push_back(bound.joint);

        // How far this joint has actually travelled since the captured pose, plus the net rotation
        // this frame's channels ask of it. A joint whose channels are rotations only stays put, which
        // is exactly the case a viewer reads as "the waist does not move".
        const RE::NiPoint3 moved = bound.joint->world.translate - bound.reference_translate;
        float rotation = 0.0f;
        for (const Bound& other : m_bound)
            if (other.joint == bound.joint && other.channel->motion == IdleMotion::e_rotate)
                rotation += idle_angle_degrees(other.channel->amplitude, other.channel->period_seconds, other.channel->phase_seconds, m_time);

        if (!line.empty())
            line += " | ";
        line += fmt::format("'{}' moved=({:.2f},{:.2f},{:.2f}) rot={:.2f}deg",
            bound.joint->name.c_str() ? bound.joint->name.c_str() : "?",
            moved.x, moved.y, moved.z, rotation);
    }
    logger::info("SCOPY IDLE t={:.1f}s {}", m_time, line);
}

PLUGIN_NAMESPACE_END
