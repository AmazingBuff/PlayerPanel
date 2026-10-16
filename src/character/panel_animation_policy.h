//
// Created by AmazingBuff on 2026/10/16.
//

#pragma once

#include "oar/policy.h"

#include "RE/N/NiQuaternion.h"
#include "RE/N/NiTransform.h"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <span>
#include <vector>

PLUGIN_NAMESPACE_BEGIN

enum class PanelAnimationPhase : std::uint8_t
{
    e_loading, e_transition, e_idle
};

class LatestAnimationRequest
{
public:
    bool observe(std::uint64_t revision)
    {
        if (m_seen && revision == m_revision)
            return false;
        m_seen = true;
        m_revision = revision;
        ++m_generation;
        return true;
    }
    [[nodiscard]] std::uint64_t generation() const { return m_generation; }
    [[nodiscard]] bool accepts(std::uint64_t generation) const { return generation == m_generation; }
private:
    std::uint64_t m_revision = 0;
    std::uint64_t m_generation = 0;
    bool m_seen = false;
};

class IdleCycleGate
{
public:
    bool crossed(std::uint64_t cycle, bool eligible)
    {
        const bool changed = cycle != m_cycle;
        m_cycle = cycle;
        return changed && eligible;
    }
    void reset() { m_cycle = 0; }
private:
    std::uint64_t m_cycle = 0;
};

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

[[nodiscard]] inline PanelAnimationPhase panel_phase(bool loaded, bool transition_ready, bool transition_finished)
{
    if (!loaded)
        return PanelAnimationPhase::e_loading;
    return transition_ready && !transition_finished ? PanelAnimationPhase::e_transition : PanelAnimationPhase::e_idle;
}

[[nodiscard]] inline std::vector<std::string_view> panel_idle_bases(oar::WeaponForm form)
{
    using Form = oar::WeaponForm;
    switch (form)
    {
    case Form::kOneHanded: return { "Animations\\1hm_idle.hkx" };
    case Form::kTwoHanded: return { "Animations\\2hm_idle.hkx" };
    case Form::kTwoHandedAxe: return { "Animations\\2hw_idle.hkx", "Animations\\2hm_idle.hkx" };
    case Form::kBow: return { "Animations\\bow_idle.hkx" };
    case Form::kCrossbow: return { "Animations\\crossbow_idle.hkx", "Animations\\bow_idle.hkx" };
    case Form::kDualWield: return { "Animations\\dw1hm_idle.hkx", "Animations\\1hm_idle.hkx" };
    case Form::kMagic: return { "Animations\\MRh_Idle.hkx", "Animations\\MLh_Idle.hkx" };
    case Form::kStaff: return { "Animations\\StaffRight_Idle.hkx", "Animations\\StaffLeft_Idle.hkx" };
    case Form::kTorch: return { "Animations\\MT_IdleTorch.hkx", "Animations\\1hm_idle.hkx" };
    case Form::kShield: return { "Animations\\1hm_idle.hkx" };
    default: return { "Animations\\h2h_idle.hkx", "Animations\\MT_Idle.hkx" };
    }
}

[[nodiscard]] inline std::vector<std::string_view> panel_transition_bases(oar::WeaponForm form, bool unequip)
{
    using Form = oar::WeaponForm;
    switch (form)
    {
    case Form::kTwoHanded:
        return unequip ? std::vector<std::string_view>{ "Animations\\2hm_unequip.hkx" } :
            std::vector<std::string_view>{ "Animations\\2hm_equip.hkx" };
    case Form::kTwoHandedAxe:
        return unequip ? std::vector<std::string_view>{ "Animations\\2hw_unequip.hkx" } :
            std::vector<std::string_view>{ "Animations\\2hw_equip.hkx" };
    case Form::kBow:
        return unequip ? std::vector<std::string_view>{ "Animations\\bow_unequip.hkx" } :
            std::vector<std::string_view>{ "Animations\\bow_equip.hkx" };
    case Form::kCrossbow:
        return unequip ? std::vector<std::string_view>{ "Animations\\crossbow_unequip.hkx" } :
            std::vector<std::string_view>{ "Animations\\crossbow_equip.hkx" };
    case Form::kMagic:
        return unequip ? std::vector<std::string_view>{ "Animations\\MRh_Unequip.hkx", "Animations\\MLh_Unequip.hkx" } :
            std::vector<std::string_view>{ "Animations\\MRh_Equip.hkx", "Animations\\MLh_Equip.hkx" };
    case Form::kStaff:
        return unequip ? std::vector<std::string_view>{ "Animations\\StaffRight_Unequip.hkx", "Animations\\1hm_unequip.hkx" } :
            std::vector<std::string_view>{ "Animations\\StaffRight_Equip.hkx", "Animations\\1hm_equip.hkx" };
    case Form::kNone: return {};
    default:
        return unequip ? std::vector<std::string_view>{ "Animations\\1hm_unequip.hkx" } :
            std::vector<std::string_view>{ "Animations\\1hm_equip.hkx" };
    }
}

PLUGIN_NAMESPACE_END
