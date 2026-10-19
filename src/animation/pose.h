//
// Created by AmazingBuff on 2026/10/16.
//

#pragma once

#include "oar/policy.h"
#include "animation/action.h"

#include "RE/N/NiQuaternion.h"
#include "RE/N/NiTransform.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <span>
#include <vector>

PLUGIN_NAMESPACE_BEGIN

[[nodiscard]] inline RE::NiTransform blend_panel_pose(RE::NiTransform const& before,
    RE::NiTransform const& after, float weight)
{
    const RE::NiQuaternion first(before.rotate);
    RE::NiQuaternion second(after.rotate);
    second.Correct(first);
    RE::NiQuaternion blended{
        first.w + (second.w - first.w) * weight, first.x + (second.x - first.x) * weight,
        first.y + (second.y - first.y) * weight, first.z + (second.z - first.z) * weight };
    const float length = std::sqrt(blended.Dot(blended));
    if (length > 0.000001f)
    {
        blended.w /= length;
        blended.x /= length;
        blended.y /= length;
        blended.z /= length;
    }
    RE::NiTransform result;
    result.rotate = blended.ToRotation();
    result.translate = before.translate + (after.translate - before.translate) * weight;
    result.scale = before.scale + (after.scale - before.scale) * weight;
    return result;
}

class EquipmentRefreshGate
{
public:
    void reset(std::uint64_t revision)
    {
        m_revision = revision;
        m_signature = 0;
        m_quiet = 0;
        m_pending = false;
    }
    void retry() { m_pending = true; m_quiet = 0; }
    [[nodiscard]] bool needs_sample(std::uint64_t revision) const { return m_pending || revision != m_revision; }
    [[nodiscard]] bool sample(std::uint64_t revision, std::uint64_t signature)
    {
        if (revision != m_revision)
        {
            m_revision = revision;
            retry();
        }
        if (!m_pending)
            return false;
        m_quiet = signature == m_signature ? m_quiet + 1 : 1;
        m_signature = signature;
        if (m_quiet < 4)
            return false;
        m_pending = false;
        return true;
    }
private:
    std::uint64_t m_revision = 0;
    std::uint64_t m_signature = 0;
    std::uint32_t m_quiet = 0;
    bool m_pending = false;
};

[[nodiscard]] inline bool covers_standing_body(std::span<std::string_view const> bones)
{
    for (std::string_view required : { "NPC Pelvis [Pelv]", "NPC Spine1 [Spn1]", "NPC Head [Head]",
             "NPC L Thigh [LThg]", "NPC R Thigh [RThg]", "NPC L Calf [LClf]", "NPC R Calf [RClf]" })
        if (std::ranges::find(bones, required) == bones.end())
            return false;
    return true;
}

PLUGIN_NAMESPACE_END
