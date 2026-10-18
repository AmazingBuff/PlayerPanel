//
// Created by AmazingBuff on 2026/10/16.
//

#pragma once

#include "oar/policy.h"
#include "oar/query_policy.h"

#include "RE/N/NiQuaternion.h"
#include "RE/N/NiTransform.h"

#include <algorithm>
#include <cmath>
#include <string>
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

// Whether a recorded world clip belongs to the stance's own idle family. The world-side recorder
// stores the clip generator's animationName; OAR replacements keep the BASE animation name (only
// the binding index changes), so comparing the canonical paths against panel_idle_bases() is a
// reliable "this idle was authored for the stance the panel is about to show" test. It keeps a
// frozen pre-swap world clip from overriding the family the panel just equipped (round 16).
[[nodiscard]] inline bool panel_clip_is_family(std::string_view clip, oar::WeaponForm form)
{
    std::string canonical = oar::canonical_animation_path(clip);
    // Same gender-directory normalisation find_base_animation applies: a project may store the
    // vanilla idle under Animations\Female\ / Animations\Male\.
    for (std::string_view gender : { "animations\\female\\", "animations\\male\\" })
    {
        const std::size_t at = canonical.find(gender);
        if (at != std::string::npos)
            canonical.replace(at, gender.size(), "animations\\");
    }
    for (std::string_view base : panel_idle_bases(form))
    {
        const std::string wanted = oar::canonical_animation_path(base);
        if (canonical == wanted || canonical.ends_with("\\" + wanted))
            return true;
    }
    return false;
}

// The transition bases use the names the world-side sequence recorder captured from the
// player's own demonstration (sixth round: MT_Idle → 1HM_Equip → MLh_Idle → 1HM_Unequip →
// MT_Idle). Vanilla ships these as 1HM_/1HM_ (capital HM); the recorder saw exactly that.
// The lowercase spellings stay as fallbacks in the same query — find_base_animation matches
// case-insensitively, so both resolve; the real names go first.
[[nodiscard]] inline std::vector<std::string_view> panel_transition_bases(oar::WeaponForm form, bool unequip)
{
    using Form = oar::WeaponForm;
    switch (form)
    {
    case Form::kTwoHanded:
        return unequip ? std::vector<std::string_view>{ "Animations\\2HM_Unequip.hkx", "Animations\\2hm_unequip.hkx" } :
            std::vector<std::string_view>{ "Animations\\2HM_Equip.hkx", "Animations\\2hm_equip.hkx" };
    case Form::kTwoHandedAxe:
        return unequip ? std::vector<std::string_view>{ "Animations\\2HW_Unequip.hkx", "Animations\\2hw_unequip.hkx" } :
            std::vector<std::string_view>{ "Animations\\2HW_Equip.hkx", "Animations\\2hw_equip.hkx" };
    case Form::kBow:
        return unequip ? std::vector<std::string_view>{ "Animations\\Bow_Unequip.hkx", "Animations\\bow_unequip.hkx" } :
            std::vector<std::string_view>{ "Animations\\Bow_Equip.hkx", "Animations\\bow_equip.hkx" };
    case Form::kCrossbow:
        return unequip ? std::vector<std::string_view>{ "Animations\\CrossBow_Unequip.hkx", "Animations\\crossbow_unequip.hkx" } :
            std::vector<std::string_view>{ "Animations\\CrossBow_Equip.hkx", "Animations\\crossbow_equip.hkx" };
    case Form::kMagic:
        return unequip ? std::vector<std::string_view>{ "Animations\\MRh_Unequip.hkx", "Animations\\MLh_Unequip.hkx" } :
            std::vector<std::string_view>{ "Animations\\MRh_Equip.hkx", "Animations\\MLh_Equip.hkx" };
    case Form::kStaff:
        return unequip ? std::vector<std::string_view>{ "Animations\\StaffRight_Unequip.hkx", "Animations\\1hm_unequip.hkx" } :
            std::vector<std::string_view>{ "Animations\\StaffRight_Equip.hkx", "Animations\\1hm_equip.hkx" };
    case Form::kNone: return {};
    default:
        return unequip ? std::vector<std::string_view>{ "Animations\\1HM_Unequip.hkx", "Animations\\1hm_unequip.hkx" } :
            std::vector<std::string_view>{ "Animations\\1HM_Equip.hkx", "Animations\\1hm_equip.hkx" };
    }
}

// The NIF-layer hand-attachment node the stance's Weapon animation bone drives. The animation
// skeleton carries the GENERIC "Weapon" bone; the copy's NIF tree carries one variant per weapon
// family. The in-game evidence: trimming alone resolved the tracks, but the longest-prefix pick
// landed on the wrong variant (WeaponSwordLeft / Weapon L Calf) and the visible weapon never
// moved. Vanilla skeleton names, verified against the in-game node census.
[[nodiscard]] inline std::string_view panel_weapon_node(oar::WeaponForm form)
{
    using Form = oar::WeaponForm;
    switch (form)
    {
    case Form::kTwoHandedAxe: return "WeaponAxe";
    case Form::kBow: return "WeaponBow";
    case Form::kCrossbow: return "WeaponCrossbow";
    case Form::kStaff: return "WeaponStaff";
    case Form::kTwoHanded: return "WeaponSword";
    case Form::kOneHanded: return "WeaponSword";
    case Form::kDualWield: return "WeaponSword";
    case Form::kMagic: return "WeaponSword";
    default: return "WeaponSword";
    }
}

PLUGIN_NAMESPACE_END
