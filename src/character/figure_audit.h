//
// Created by AmazingBuff on 2026/10/13.
//

#pragma once

#include "character/snapshot_transform.h"

#include "RE/N/NiAVObject.h"
#include "RE/N/NiPoint3.h"
#include "RE/N/NiTransform.h"

#include <string_view>
#include <unordered_set>

namespace RE
{
    class BSGeometry;
    class NiSkinInstance;
}

PLUGIN_NAMESPACE_BEGIN

// R01's skin audit needs the same collision-helper classification the clone's framing uses, so the
// rule lives once: body and physics mods name their skeleton-driven collision helpers after these
// tokens; they carry a skinInstance but are not part of the visible figure.
[[nodiscard]] inline bool figure_is_collision_helper(std::string_view a_name)
{
    if (a_name.empty())
        return false;
    constexpr std::string_view tokens[] = { "Collision", "VirtualGround", "3BCA_", "Stopper" };
    for (const std::string_view token : tokens)
        if (a_name.find(token) != std::string_view::npos)
            return true;
    return false;
}

// What one skinned figure's palette actually reads: the nodes `NiSkinInstance::bones` points at,
// over every skinned geometry under the root. This is the necessity oracle for the AFP9 coverage
// audit - a pose slot the write could not reach is a *necessary* bone when a target mesh's palette
// references it, and merely a helper (or an attachment the panel does not draw) otherwise.
//
// The set holds pointers ONLY, and nothing in here dereferences an entry: the palette array is the
// engine's own allocation and the 2026-10-13 crash (CharacterPanel.dll+0x2B78A, a name read at
// 0x1010) proved entries cannot be assumed to be readable NiAVObjects. Callers classify by a node
// they resolved themselves through the tree, never by a name read out of this array.
struct SkinPalette
{
    std::unordered_set<RE::NiAVObject const*> nodes;
    std::uint32_t skinned = 0;
    std::uint32_t bound = 0;
    std::uint32_t entries = 0;
};

[[nodiscard]] SkinPalette figure_skin_palette(RE::NiAVObject& a_root);

// The visible figure's skinned bound, excluding collision helpers. R01's framing must follow the
// body the panel draws, not the 3BCA_*/VirtualGround helpers that drag a 3BA/UBE bound off it.
struct FigureBound
{
    RE::NiPoint3 center;
    float radius = 0.0f;
    bool valid = false;
};

[[nodiscard]] FigureBound figure_skinned_bound(RE::NiAVObject& a_root);

// The same bound solved from data THIS frame owns: the union of the skinned nodes' world positions
// (the skeleton's own extent), reached through the tree so no palette slot is ever dereferenced.
//
// `figure_skinned_bound` reads geometry worldBounds, and those are a cache the engine refreshes on
// its own schedule - the 2026-10-13 23:44 session measured one still carrying the pre-park world
// position while the bones had already moved to the studio anchor. Centering a figure against such a
// bound throws it ~8000 units along the axis the cache is stale in: the AFP9g panel image showed the
// figure small, dark and off-centre because it was placed ~2820 units deeper than the anchor (and so
// far from the studio rig). Framing measures from this one; the cached bound stays as a comparison.
[[nodiscard]] FigureBound figure_skeleton_bound(RE::NiAVObject& a_root);

// R01's framing, expressed the only way a LIVE actor's tree accepts it: a world-space delta applied
// downward. Writing the root's `local` and cascading does NOT move the root itself - measured on the
// puppet (AFP9c: `restore world-d=(0,0,0)` while the vertices projected to NDC (236, 290), i.e. the
// figure never left its park depth) - because the downward pass recomputes the CHILDREN's worlds
// from the object's own, which the write never changed. A detached graph has no parent and its local
// is its world, which is why the copy route could pose by writing `local` and this route cannot.
//
// The math is the copy route's own solve (`snapshot_delta`): the placement is composed with the
// current world's inverse and re-centred so the measured bound center lands on the anchor at the
// studio scale. `delta * bound_center` is the assertion a caller can log - it must equal the anchor.
[[nodiscard]] inline RE::NiTransform figure_world_delta(RE::NiTransform const& a_current_world,
    RE::NiPoint3 const& a_center, RE::NiPoint3 const& a_anchor, float a_scale, float a_facing_z_radians)
{
    RE::NiTransform placement;
    placement.rotate.MakeZRotation(a_facing_z_radians);
    placement.scale = a_scale;
    placement.translate = a_anchor;
    return snapshot_delta(a_current_world, placement, a_center, a_anchor);
}

// One audit class of a pose slot the write could not reach. `weapon-attachment` exists because AFP8
// measured exactly the five `Weapon*` nodes as world-space outliers the graph does not drive.
enum class SlotNecessity : std::uint8_t
{
    e_necessary,
    e_helper,
    e_weapon_attachment,
    e_unknown
};

[[nodiscard]] inline char const* slot_necessity_name(SlotNecessity a_class)
{
    switch (a_class)
    {
    case SlotNecessity::e_necessary:
        return "necessary";
    case SlotNecessity::e_helper:
        return "helper";
    case SlotNecessity::e_weapon_attachment:
        return "weapon-attachment";
    case SlotNecessity::e_unknown:
        return "unknown";
    }
    return "?";
}

// Classifies one pose slot against a palette. `a_node` is the node the CALLER resolved for that
// slot by name in the actor's own tree - null when the name did not resolve there, which is exactly
// the unreachable case - so this never reads a pointer out of the palette array.
//
// The order matters: a bone a skinned mesh actually reads cannot be waved off whatever its name says
// (the palette oracle first), then the AFP8 attachment exception - the measured `Weapon*` outliers
// the graph does not drive - then the helper default. `unknown` is reserved for an unbound palette
// (a figure the engine has never rendered), which the audit reports as "classification
// unavailable", never as "not necessary". A slot whose name resolved to no node cannot be necessary:
// no palette in this tree can reference a node that is not there.
[[nodiscard]] inline SlotNecessity figure_slot_necessity(std::string_view a_name,
    RE::NiAVObject const* a_node, SkinPalette const& a_palette)
{
    if (a_node && a_palette.nodes.contains(a_node))
        return SlotNecessity::e_necessary;
    if (a_name.find("Weapon") != std::string_view::npos)
        return SlotNecessity::e_weapon_attachment;
    if (a_palette.bound != 0)
        return SlotNecessity::e_helper;
    return SlotNecessity::e_unknown;
}

PLUGIN_NAMESPACE_END
