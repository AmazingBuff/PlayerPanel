//
// Created by AmazingBuff on 2026/10/19.
//
#pragma once
#include "oar/query_policy.h"
#include "oar/policy.h"
PLUGIN_NAMESPACE_BEGIN
namespace animation
{
    // Animation family only. The source actor's draw state is read independently of its equipment.
    enum class WeaponForm : std::uint8_t
    {
        kNone = 0,   // nothing drawn / nothing legible: the plain standing idle
        kOneHanded,
        kTwoHanded,
        kBow,
        kDualWield,
        kMagic,   // a hand holds a spell
        kStaff,
        kTorch,
        kShield,
        kCrossbow,
        kTwoHandedAxe
    };

    [[nodiscard]] inline std::string_view weapon_form_name(WeaponForm a_form)
    {
        switch (a_form)
        {
        case WeaponForm::kNone: return "none";
        case WeaponForm::kOneHanded: return "1h";
        case WeaponForm::kTwoHanded: return "2h";
        case WeaponForm::kBow: return "bow";
        case WeaponForm::kDualWield: return "dual";
        case WeaponForm::kMagic: return "magic";
        case WeaponForm::kStaff: return "staff";
        case WeaponForm::kTorch: return "torch";
        case WeaponForm::kShield: return "shield";
        case WeaponForm::kCrossbow: return "crossbow";
        case WeaponForm::kTwoHandedAxe: return "2hw";
        default: return "?";
        }
    }

    // The panel's standing figure answers with the form the two hands carry: a two-handed weapon or
    // a bow occupies both hands (the engine reports it in whichever entry it last touched, so both
    // are read), two one-handed weapons are the dual stance, and a lone one-handed weapon with a
    // shield, torch or spell in the other hand is still the one-handed stance.
    enum class HandKind : std::uint8_t
    {
        kEmpty = 0,
        kOneHandedWeapon,
        kTwoHandedWeapon,
        kStaff,
        kSpell,
        kTorch,
        kShield,
        kBow,
        kCrossbow,
        kTwoHandedAxe
    };

    [[nodiscard]] inline WeaponForm weapon_form_from_hands(HandKind a_right, HandKind a_left)
    {
        if (a_right == HandKind::kBow || a_left == HandKind::kBow)
            return WeaponForm::kBow;
        if (a_right == HandKind::kCrossbow || a_left == HandKind::kCrossbow)
            return WeaponForm::kCrossbow;
        if (a_right == HandKind::kTwoHandedAxe || a_left == HandKind::kTwoHandedAxe)
            return WeaponForm::kTwoHandedAxe;
        if (a_right == HandKind::kTwoHandedWeapon || a_left == HandKind::kTwoHandedWeapon)
            return WeaponForm::kTwoHanded;
        if (a_right == HandKind::kStaff || a_left == HandKind::kStaff)
            return WeaponForm::kStaff;
        if (a_right == HandKind::kOneHandedWeapon && a_left == HandKind::kOneHandedWeapon)
            return WeaponForm::kDualWield;
        if (a_right == HandKind::kOneHandedWeapon || a_left == HandKind::kOneHandedWeapon)
            return WeaponForm::kOneHanded;
        if (a_right == HandKind::kSpell || a_left == HandKind::kSpell)
            return WeaponForm::kMagic;
        if (a_right == HandKind::kTorch || a_left == HandKind::kTorch)
            return WeaponForm::kTorch;
        if (a_right == HandKind::kShield || a_left == HandKind::kShield)
            return WeaponForm::kShield;
        return WeaponForm::kNone;
    }


    using oar::canonical_animation_path;
    enum class ActionKind : std::uint8_t
    {
        e_standing_idle, e_equip, e_weapon_idle, e_unequip, e_hand_idle
    };
    struct ActionPrototype
    {
        ActionKind kind;
        animation::WeaponForm form;
        bool loop;
    };
    // How a world clip decides the stance the player is ENTERING with (the panel-open edge). The
    // engine clears weaponState before the menu event and freezes it for the whole menu session,
    // so the recent clip history is the honest source: a draw move means drawn, a sheathe move
    // means sheathed, an idle decides by its family, and locomotion/attacks are not decisive on
    // their own (round 17: entering during 1HM_Equip answered sheathed and showed MT_Idle, and
    // entering during 1HM_Unequip answered drawn and showed a weapon idle).
    enum class ClipStance : std::uint8_t
    {
        e_undecided,
        e_drawn,
        e_sheathed
    };

    [[nodiscard]] inline std::string engine_clip_name(std::string_view clip)
    {
        const std::size_t slash = clip.find_last_of("\\/");
        return canonical_animation_path(slash == std::string_view::npos ? clip : clip.substr(slash + 1));
    }

    // "unequip" contains "equip": the sheathe move must be tested first.
    [[nodiscard]] inline bool clip_is_unequip(std::string_view clip)
    {
        return engine_clip_name(clip).find("unequip") != std::string::npos;
    }

    [[nodiscard]] inline bool clip_is_equip(std::string_view clip)
    {
        const std::string path = engine_clip_name(clip);
        return path.find("unequip") == std::string::npos && path.find("equip") != std::string::npos;
    }

    [[nodiscard]] inline ClipStance clip_stance(std::string_view clip)
    {
        const std::string path = engine_clip_name(clip);
        if (path.find("unequip") != std::string::npos)
            return ClipStance::e_sheathed;
        if (path.find("equip") != std::string::npos)
            return ClipStance::e_drawn;
        if (path.find("idle") == std::string::npos)
            return ClipStance::e_undecided;
        // The unarmed base idle family answers sheathed; every other idle-named clip (weapon,
        // hand, torch, sneaking ...) answers drawn, matching the previous rounds' name rule.
        for (std::string_view base : { "mt_idle", "sneakmtidle", "mt_sprint", "mt_walk", "mt_run", "mt_jump", "mag_" })
            if (path.find(base) != std::string::npos)
                return ClipStance::e_sheathed;
        return ClipStance::e_drawn;
    }

    enum class EntryActionKind : std::uint8_t { e_none, e_equip, e_unequip };
    [[nodiscard]] inline EntryActionKind entry_action_kind(std::string_view clip)
    {
        if (clip_is_unequip(clip)) return EntryActionKind::e_unequip;
        if (clip_is_equip(clip)) return EntryActionKind::e_equip;
        return EntryActionKind::e_none;
    }

}
[[nodiscard]] inline std::vector<std::string_view> panel_idle_bases(animation::WeaponForm form)
{
    using Form = animation::WeaponForm;
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
[[nodiscard]] inline bool panel_clip_is_family(std::string_view clip, animation::WeaponForm form)
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
[[nodiscard]] inline std::vector<std::string_view> panel_transition_bases(animation::WeaponForm form, bool unequip)
{
    using Form = animation::WeaponForm;
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
[[nodiscard]] inline std::string_view panel_weapon_node(animation::WeaponForm form)
{
    using Form = animation::WeaponForm;
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
