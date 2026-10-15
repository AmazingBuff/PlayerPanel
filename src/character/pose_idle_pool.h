//
// Created by AmazingBuff on 2026/10/15.
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

PLUGIN_NAMESPACE_BEGIN

// The pose-idle pool (state-idle decision v2, 2026-10-15, user): the panel figure ALWAYS shows a
// STANDING idle. Sneaking, swimming, jumping or moving at panel-open must not put the copy into
// the sneak/swim/locomotion animation - the figure stands. The only pose variation is the combat
// stance: with the weapon drawn, the idle of the weapon form the player is holding is shown
// (one-handed, two-handed, bow, dual-wielded, spell-casting, staff, torch), because that idle is
// part of the combat stance the player is in, not of the sneak/swim pose.
//
// The pool is filled while the world RUNS (the panel is closed): every sampled frame where the
// player is STANDING STILL in a known pose, the engine's own idle clip - OAR's actual decision for
// that pose - is filed under that pose's class. At panel open the desired pose class is computed
// from the player's CURRENT stance (weapon drawn + forms), and the pool serves the clip the
// engine itself played for it. A clip the engine never ran for that pose cannot be in the pool,
// and nothing is invented: the fallback chain (current running idle, strongest clip, procedural
// idle) serves and says so.

// The pose a sampled (or desired) idle belongs to. Kept coarse on purpose: the pool serves clips
// the engine actually played, so a finer key would only split pools without adding fidelity.
enum class PoseClass : std::uint8_t
{
    kNeutral = 0,  // standing, weapon sheathed
    kOneHanded,
    kTwoHanded,
    kBow,
    kDualWield,
    kMagic,   // a hand holds a spell
    kStaff,
    kTorch,
    kShield,  // shield or other non-weapon left-hand item while drawn

    kCount
};

[[nodiscard]] constexpr std::string_view pose_class_name(PoseClass a_class)
{
    switch (a_class)
    {
    case PoseClass::kNeutral: return "neutral";
    case PoseClass::kOneHanded: return "1h";
    case PoseClass::kTwoHanded: return "2h";
    case PoseClass::kBow: return "bow";
    case PoseClass::kDualWield: return "dual";
    case PoseClass::kMagic: return "magic";
    case PoseClass::kStaff: return "staff";
    case PoseClass::kTorch: return "torch";
    case PoseClass::kShield: return "shield";
    default: return "?";
    }
}

// How the two equipped hands carry their items, as the engine's own idle selection reads them.
// WEAPON-TYPE refinement lives with the caller (it reads TESObjectWEAP::GetWeaponType and the
// spell/misc kinds off the live objects); the pure rules here only combine the per-hand kinds.
enum class HandKind : std::uint8_t
{
    kEmpty = 0,
    kOneHandedWeapon,
    kTwoHandedWeapon,  // two-handed melee, bow or crossbow: occupies both hands
    kSpell,
    kStaff,
    kTorch,
    kShield,
};

// The pose class for a stance: the drawn/drawn split by the two hands' kinds. Weapon sheathed is
// ALWAYS the neutral standing idle - sneaking/swimming must not change it. Both hands one-handed
// weapons is the dual-wield stance; a single one-handed weapon (other hand empty, shield or
// spell) is the one-handed stance; a spell alone (either hand) is the casting stance.
[[nodiscard]] PoseClass pose_class_from_hands(HandKind a_right, HandKind a_left, bool a_weapon_drawn);

// Whether the player is standing still in a pose the panel can mirror: no sneak/swim/sit/motion,
// no attack, stagger or knock-down. While this is false the sample is not "the engine playing the
// pose's idle", and nothing is filed.
struct PoseInputs
{
    bool weapon_drawn = false;
    bool sneaking = false;
    bool swimming = false;
    bool sitting = false;
    bool moving = false;
    bool attacking = false;
    bool staggering = false;
    bool knocked = false;
    HandKind right_kind = HandKind::kEmpty;
    HandKind left_kind = HandKind::kEmpty;
};

[[nodiscard]] bool pose_is_standing_idle(PoseInputs const& a_inputs);

[[nodiscard]] PoseClass pose_class_for(PoseInputs const& a_inputs);

// One pool entry: the pose class and the engine clip chosen for it. The generator value is a
// HANDLE, never dereferenced from the pool itself: the picker re-validates it through the full
// resolve chain (vtable, readability, binding, animation) before any use, and falls back when the
// clip is gone - the same discipline every borrowed pointer in this codebase follows.
struct PoseIdleEntry
{
    PoseClass pose = PoseClass::kNeutral;
    std::string clip_name;
    std::uintptr_t generator = 0;  // the running generator's address, for dedupe and re-resolution
};

// Small newest-wins pool: one entry per pose class, replaced on every new sample. Not a history:
// the engine's CURRENT idle for a pose is the answer, an older one is not wanted.
class PoseIdlePool
{
public:
    // Files one sample. A classless or non-idle sample must not be passed in (the caller filters);
    // an entry replaces the class's previous one and reports whether the pool changed.
    bool file(PoseIdleEntry a_entry);

    // The engine clip filed for the desired pose, or null when the pool never saw it. The
    // generator value is a handle the caller re-validates; the name travels for logs and for a
    // live-set match.
    [[nodiscard]] PoseIdleEntry const* find(PoseClass a_class) const;

    // The pool forgets everything on a load boundary.
    void clear();

    [[nodiscard]] std::uint32_t size() const;

private:
    // Indexed by static_cast<size_t>(PoseClass); an empty name means "no entry".
    std::vector<PoseIdleEntry> m_entries{ static_cast<std::size_t>(PoseClass::kCount) };
};

PLUGIN_NAMESPACE_END
