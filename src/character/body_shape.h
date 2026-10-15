//
// Created by AmazingBuff on 2026/10/14.
//

#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace RE
{
    class Actor;
}

PLUGIN_NAMESPACE_BEGIN

using BodyMorphValues = std::unordered_map<std::string, float>;

[[nodiscard]] inline std::uint32_t body_shape_mismatches(BodyMorphValues const& source, BodyMorphValues const& target)
{
    constexpr float Match_Epsilon = 0.0001f;
    std::uint32_t mismatches = 0;
    for (auto const& [name, value] : source)
    {
        const auto found = target.find(name);
        if (found == target.end() || std::abs(found->second - value) > Match_Epsilon)
            ++mismatches;
    }
    for (auto const& [name, value] : target)
    {
        (void)value;
        if (!source.contains(name))
            ++mismatches;
    }
    return mismatches;
}

// AFP10-C: the puppet's BODY SHAPE, taken from the player.
//
// Why this is not a node-scale copy: in this setup a BodySlide preset is realized as mesh morphs
// (`.tri` vertex deltas) applied by RaceMenu's skee64 to the body trishape - measured 2026-10-14, all
// 639 `.osp` slider sets carry zero bone/scale definitions, and the puppet's shape comes from OBody
// NG assigning it a RANDOM preset per session ("No preset defined for this actor, getting it
// randomly" -> "Preset Mia UBE will be applied to CharacterPanel Puppet" in OBody.log). Copying
// `NiAVObject::local.scale` would therefore copy nothing that shapes this figure.
//
// The route is skee64's own `IBodyMorphInterface`, which is how OBody writes the shape in the first
// place. The declaration below is a locally written ABI mirror of that interface (provenance in
// body_shape.cpp); it is deliberately minimal in behaviour and exact in vtable order, and a runtime
// version check refuses an interface older than the mirrored layout.
struct BodyShapeSync
{
    bool available = false;   // skee64 answered and the mirrored layout was accepted
    bool applied = false;     // the target's morphs were rewritten (i.e. the check found a difference)
    std::uint32_t compared = 0;    // body morphs the source carries (the mirrored key only)
    std::uint32_t mismatched = 0;  // missing, different or target-only values under the mirrored key
    std::uint32_t copied = 0;      // values actually written onto the target
    std::uint32_t skipped = 0;     // morphs under OTHER keys, deliberately not copied
    std::string sample;            // a few `name=value` pairs, for the log

    [[nodiscard]] bool changed() const { return applied; }
};

// True once the interface has been acquired (it is acquired lazily on first use).
[[nodiscard]] bool body_shape_available();

// Makes `a_target`'s body morphs agree with `a_source`'s, and only pays for the write when they
// disagree: both OBody sets are compared, including target-only values and an empty source. When
// they do not match, the target's OBody keys are cleared first to remove its random preset, and the
// source's values are copied over it before the vertex buffers are re-applied.
//
// ONLY the OBody key is copied, and that is load-bearing rather than an optimisation. A morph store
// is shared by every system that uses one, and the 2026-10-14 session measured what copying all of it
// does: the puppet came up wearing an axe, a bow and a dagger it had never been given, because the
// copy had also carried XPMSE's own "which weapon type is equipped" state
// (`XPMSEAABase_axeeqp/_boweqp/_dageqp/_1hmeqp/_2hmeqp`) onto an actor whose attachment nodes then
// believed those weapons were there. The body shape this route is for lives under OBody's key - that
// is the key OBody writes its preset to - so everything else is counted and left alone.
[[nodiscard]] BodyShapeSync body_shape_sync(RE::Actor& a_target, RE::Actor& a_source);

PLUGIN_NAMESPACE_END
