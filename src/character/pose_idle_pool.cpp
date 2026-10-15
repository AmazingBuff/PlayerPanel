//
// Created by AmazingBuff on 2026/10/15.
//

#include "character/pose_idle_pool.h"

#include <algorithm>

PLUGIN_NAMESPACE_BEGIN

bool pose_is_standing_idle(PoseInputs const& a_inputs)
{
    return !a_inputs.sneaking && !a_inputs.swimming && !a_inputs.sitting && !a_inputs.moving &&
        !a_inputs.attacking && !a_inputs.staggering && !a_inputs.knocked;
}

PoseClass pose_class_from_hands(HandKind a_right, HandKind a_left, bool a_weapon_drawn)
{
    if (!a_weapon_drawn)
        return PoseClass::kNeutral;

    // The two-handed kind means the WEAPON occupies both hands (two-handed melee, bow or
    // crossbow): whatever the other entry reports is that weapon's other hand, not a second item.
    // A staff is its own kind and its own idle.
    if (a_right == HandKind::kStaff || a_left == HandKind::kStaff)
        return PoseClass::kStaff;
    if (a_right == HandKind::kTwoHandedWeapon || a_left == HandKind::kTwoHandedWeapon)
        return PoseClass::kTwoHanded;
    // Both hands carry one-handed weapons: the dual-wield stance.
    if (a_right == HandKind::kOneHandedWeapon && a_left == HandKind::kOneHandedWeapon)
        return PoseClass::kDualWield;
    // A spell in a hand: the casting stance, unless a weapon shares the stance - a single
    // one-handed weapon with the other hand empty, shielded or spelling is the one-handed stance.
    if (a_right == HandKind::kOneHandedWeapon || a_left == HandKind::kOneHandedWeapon)
        return PoseClass::kOneHanded;
    if (a_right == HandKind::kSpell || a_left == HandKind::kSpell)
        return PoseClass::kMagic;
    if (a_right == HandKind::kTorch || a_left == HandKind::kTorch)
        return PoseClass::kTorch;
    if (a_right == HandKind::kShield || a_left == HandKind::kShield)
        return PoseClass::kShield;
    // Drawn with nothing legible in either hand (bound-weapon edge mid-draw): the engine files
    // this under the plain combat idle; keep it distinct from the sheathed neutral stance.
    return PoseClass::kOneHanded;
}

PoseClass pose_class_for(PoseInputs const& a_inputs)
{
    return pose_class_from_hands(a_inputs.right_kind, a_inputs.left_kind, a_inputs.weapon_drawn);
}

bool PoseIdlePool::file(PoseIdleEntry a_entry)
{
    if (a_entry.pose >= PoseClass::kCount || a_entry.clip_name.empty() || a_entry.generator == 0)
        return false;
    const size_t index = static_cast<size_t>(a_entry.pose);
    PoseIdleEntry& slot = m_entries[index];
    if (slot.generator == a_entry.generator && slot.clip_name == a_entry.clip_name)
        return false;
    slot = std::move(a_entry);
    return true;
}

PoseIdleEntry const* PoseIdlePool::find(PoseClass a_class) const
{
    if (a_class >= PoseClass::kCount)
        return nullptr;
    PoseIdleEntry const& slot = m_entries[static_cast<size_t>(a_class)];
    return slot.clip_name.empty() ? nullptr : &slot;
}

void PoseIdlePool::clear()
{
    std::fill(m_entries.begin(), m_entries.end(), PoseIdleEntry{});
}

std::uint32_t PoseIdlePool::size() const
{
    std::uint32_t count = 0;
    for (PoseIdleEntry const& entry : m_entries)
        if (!entry.clip_name.empty())
            ++count;
    return count;
}

PLUGIN_NAMESPACE_END
