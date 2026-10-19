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
        e_standing_idle, e_equip, e_weapon_idle_start, e_weapon_idle_loop, e_unequip, e_hand_idle
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

    // "unequip" contains "equip": the sheathe move must be tested first.
    [[nodiscard]] inline bool clip_is_unequip(std::string_view clip)
    {
        return canonical_animation_path(clip).find("unequip") != std::string::npos;
    }

    [[nodiscard]] inline bool clip_is_equip(std::string_view clip)
    {
        const std::string path = canonical_animation_path(clip);
        return path.find("unequip") == std::string::npos && path.find("equip") != std::string::npos;
    }

    [[nodiscard]] inline ClipStance clip_stance(std::string_view clip)
    {
        const std::string path = canonical_animation_path(clip);
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

    // A pack's idle sequence authored as sibling folders ("... idle Start", "... idle Loop").
    // OAR chooses between them with phase conditions (Smooth Moveset gates them on its own magic
    // effects), and those effects are switched by animation events the panel never fires while
    // sampling offline — so the panel advances by NAME, per the user's "the name is the
    // interface" rule (round 17).
    enum class ClipSequenceToken : std::uint8_t
    {
        e_none,
        e_start,
        e_loop
    };

    [[nodiscard]] inline ClipSequenceToken clip_sequence_token(std::string_view clip)
    {
        const std::string path = canonical_animation_path(clip);
        const std::size_t file = path.find_last_of('\\');
        if (file == std::string::npos || file == 0)
            return ClipSequenceToken::e_none;
        const std::size_t parent_begin = path.find_last_of('\\', file - 1);
        if (parent_begin == std::string::npos)
            return ClipSequenceToken::e_none;
        std::string_view parent(path.data() + parent_begin + 1, file - parent_begin - 1);
        while (!parent.empty() && (std::isdigit(static_cast<unsigned char>(parent.back())) || parent.back() == ' '))
            parent.remove_suffix(1);
        if (parent.ends_with("start")) return ClipSequenceToken::e_start;
        if (parent.ends_with("loop")) return ClipSequenceToken::e_loop;
        return ClipSequenceToken::e_none;
    }

    // What the panel should mirror as its entry action when the inventory opens. The source
    // clip is the world's own decision; replaying it from the source's local time is the only
    // faithful continuation while the paused world cannot be re-evaluated (round 18).
    enum class EntryActionKind : std::uint8_t
    {
        e_none,
        e_equip,
        e_unequip,
        e_sequence_start
    };

    [[nodiscard]] inline EntryActionKind entry_action_kind(std::string_view clip)
    {
        const std::string path = canonical_animation_path(clip);
        if (path.find("unequip") != std::string::npos)
            return EntryActionKind::e_unequip;
        if (path.find("equip") != std::string::npos)
            return EntryActionKind::e_equip;
        if (clip_sequence_token(path) == ClipSequenceToken::e_start)
            return EntryActionKind::e_sequence_start;
        return EntryActionKind::e_none;
    }

    // Case-insensitive path helpers. The sibling scan walks the whole animation-name catalogue
    // (~15k entries), so it must not allocate: the earlier canonical_animation_path-per-name
    // version built 15k strings on the draw thread at every transition (round 17).
    [[nodiscard]] inline bool path_equals_icase(std::string_view lhs, std::string_view rhs)
    {
        if (lhs.size() != rhs.size())
            return false;
        for (std::size_t i = 0; i < lhs.size(); ++i)
            if (std::tolower(static_cast<unsigned char>(lhs[i])) != std::tolower(static_cast<unsigned char>(rhs[i])))
                return false;
        return true;
    }

    [[nodiscard]] inline std::size_t path_rfind_icase(std::string_view haystack, std::string_view needle)
    {
        if (needle.empty() || haystack.size() < needle.size())
            return std::string_view::npos;
        for (std::size_t i = haystack.size() - needle.size() + 1; i-- > 0;)
            if (path_equals_icase(haystack.substr(i, needle.size()), needle))
                return i;
        return std::string_view::npos;
    }

    [[nodiscard]] inline std::string_view path_tail(std::string_view path)
    {
        const std::size_t at = path.find_last_of("\\/");
        return at == std::string_view::npos ? path : path.substr(at + 1);
    }

    [[nodiscard]] inline std::string_view path_parent(std::string_view path)
    {
        const std::size_t file = path.find_last_of("\\/");
        if (file == std::string_view::npos || file == 0)
            return {};
        const std::size_t parent = path.find_last_of("\\/", file - 1);
        return parent == std::string_view::npos ? std::string_view{} : path.substr(parent + 1, file - parent - 1);
    }

    [[nodiscard]] inline std::string_view path_root(std::string_view path)
    {
        const std::size_t file = path.find_last_of("\\/");
        if (file == std::string_view::npos || file == 0)
            return {};
        const std::size_t parent = path.find_last_of("\\/", file - 1);
        return parent == std::string_view::npos ? std::string_view{} : path.substr(0, parent);
    }

    // The sequence family of a member folder: the folder name with its trailing start/loop marker
    // and episode digits removed ("1hm Non-Combat idle Loop2" → "1hm non-combat idle"). Siblings
    // must share it: Smooth Moveset also authors "1hm Combat idle Loop" under the SAME project
    // folder, and a root+filename match alone crossed into the combat family (round 18 verdict).
    [[nodiscard]] inline std::string path_sequence_stem(std::string_view parent)
    {
        std::string_view p = parent;
        bool marker_stripped = false;
        bool stripped = true;
        while (stripped)
        {
            stripped = false;
            while (!p.empty() && (std::isdigit(static_cast<unsigned char>(p.back())) || p.back() == ' '))
            {
                p.remove_suffix(1);
                stripped = true;
            }
            for (std::string_view marker : { "start", "loop" })
            {
                if (p.size() >= marker.size() &&
                    path_equals_icase(p.substr(p.size() - marker.size()), marker))
                {
                    p.remove_suffix(marker.size());
                    marker_stripped = true;
                    stripped = true;
                    break;
                }
            }
        }
        while (!p.empty() && (p.back() == ' ' || p.back() == '_' || p.back() == '-'))
            p.remove_suffix(1);
        // A folder without a sequence marker belongs to no family.
        return marker_stripped ? std::string(p) : std::string();
    }

    // One member of the sequence family a clip belongs to.
    struct SequenceMember
    {
        std::uint16_t index = 0xFFFF;
        std::string name;
        ClipSequenceToken token = ClipSequenceToken::e_none;
        int episode = 0;  // digits after the token: Loop → 0, Loop2 → 2
    };

    // Every member of the sequence family `current` belongs to: same project folder, same file
    // name, same folder stem (parent minus its start/loop marker and episode digits), ordered
    // Start → Loop → Loop2. The pack's own folder convention is the only interface — OAR's phase
    // conditions are frozen while the panel pauses the world, so the sequence is planned once
    // from names instead of being re-decided at every boundary (round 18, user model).
    [[nodiscard]] inline std::vector<SequenceMember> collect_sequence_family(
        std::span<std::string const> names, std::string_view current)
    {
        const std::string_view wanted_file = path_tail(current);
        const std::string_view wanted_root = path_root(current);
        const std::string wanted_stem = path_sequence_stem(path_parent(current));
        std::vector<SequenceMember> members;
        if (wanted_file.empty() || wanted_root.empty() || wanted_stem.empty())
            return members;
        for (std::size_t i = 0; i < names.size() && i < 0xFFFF; ++i)
        {
            const std::string_view path = names[i];
            if (!path_equals_icase(path_tail(path), wanted_file) ||
                !path_equals_icase(path_root(path), wanted_root))
                continue;
            const std::string_view parent = path_parent(path);
            const std::string stem = path_sequence_stem(parent);
            if (!path_equals_icase(stem, wanted_stem))
                continue;
            // The member's own token: the folder name ends (after episode digits) with the
            // marker. clip_sequence_token() cannot be used here — it expects a full path and
            // this is a bare folder name (round 18: the Loop/Loop2 members came back e_none).
            SequenceMember member;
            member.index = static_cast<std::uint16_t>(i);
            member.name = std::string(path);
            for (std::string_view marker : { std::string_view("loop"), std::string_view("start") })
            {
                const std::size_t at = path_rfind_icase(parent, marker);
                if (at == std::string_view::npos)
                    continue;
                const std::size_t end = at + marker.size();
                std::size_t after = end;
                while (after < parent.size() &&
                       (std::isdigit(static_cast<unsigned char>(parent[after])) || parent[after] == ' '))
                    ++after;
                if (after != parent.size())
                    continue;
                member.token = marker == "loop" ? ClipSequenceToken::e_loop : ClipSequenceToken::e_start;
                for (std::size_t d = end; d < parent.size() && std::isdigit(static_cast<unsigned char>(parent[d])); ++d)
                    member.episode = member.episode * 10 + (parent[d] - '0');
                break;
            }
            if (member.token == ClipSequenceToken::e_none)
                continue;
            members.push_back(std::move(member));
        }
        std::sort(members.begin(), members.end(), [](SequenceMember const& lhs, SequenceMember const& rhs)
        {
            const int lhs_rank = lhs.token == ClipSequenceToken::e_start ? 0 : 1;
            const int rhs_rank = rhs.token == ClipSequenceToken::e_start ? 0 : 1;
            if (lhs_rank != rhs_rank)
                return lhs_rank < rhs_rank;
            if (lhs.episode != rhs.episode)
                return lhs.episode < rhs.episode;
            return lhs.index < rhs.index;
        });
        return members;
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
