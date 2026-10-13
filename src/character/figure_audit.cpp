//
// Created by AmazingBuff on 2026/10/13.
//

#include "character/figure_audit.h"

#include "RE/B/BSGeometry.h"
#include "RE/B/BSVisit.h"
#include "RE/N/NiNode.h"

#include <cmath>
#include <cstring>

// winnt.h defines macros of these names, which would rewrite the REX constants used below as
// `REX::W32::0x1000`; every Windows header this file needs is already parsed at this point.
#undef MEM_COMMIT
#undef PAGE_NOACCESS

PLUGIN_NAMESPACE_BEGIN

namespace
{
    bool readable(void const* address, std::size_t bytes)
    {
        if (!address || bytes == 0)
            return false;
        REX::W32::MEMORY_BASIC_INFORMATION info{};
        if (REX::W32::VirtualQuery(address, &info, sizeof(info)) == 0)
            return false;
        if (info.state != REX::W32::MEM_COMMIT || info.protect == REX::W32::PAGE_NOACCESS)
            return false;
        const auto* const begin = static_cast<const std::uint8_t*>(address);
        const auto* const end = static_cast<const std::uint8_t*>(info.baseAddress) + info.regionSize;
        return begin + bytes <= end;
    }

}

SkinPalette figure_skin_palette(RE::NiAVObject& a_root)
{
    // The palette array is allocated on demand (a 3BA skin measured 47 slots against 131 declared),
    // so the walk is bounded by what the instance actually points at, never by the declared capacity.
    constexpr std::uint32_t Max_Palette_Entries = 1024;

    SkinPalette palette;
    RE::BSVisit::TraverseScenegraphGeometries(&a_root, [&](RE::BSGeometry* a_geometry)
    {
        if (!a_geometry || !readable(a_geometry, sizeof(RE::BSGeometry)))
            return RE::BSVisit::BSVisitControl::kContinue;

        const RE::NiSkinInstance* const skin = a_geometry->GetGeometryRuntimeData().skinInstance.get();
        if (!skin || !skin->rootParent || !readable(skin, sizeof(RE::NiSkinInstance)))
            return RE::BSVisit::BSVisitControl::kContinue;
        ++palette.skinned;

        const std::uint32_t declared = skin->allocatedSize;
        RE::NiAVObject* const* const bones = skin->bones;
        if (!bones || declared == 0 || declared > Max_Palette_Entries ||
            !readable(bones, sizeof(RE::NiAVObject*) * static_cast<std::size_t>(declared)))
            return RE::BSVisit::BSVisitControl::kContinue;

        // Entries are collected as POINTERS and never dereferenced: the array can hold slots that
        // are not readable nodes (the AFP9 crash read a "name" at 0x1010), and necessity is decided
        // by membership of a node the caller resolved itself.
        bool bound = false;
        for (std::uint32_t i = 0; i < declared; ++i)
        {
            RE::NiAVObject* const node = bones[i];
            if (!node)
                continue;
            bound = true;
            ++palette.entries;
            palette.nodes.insert(node);
        }
        if (bound)
            ++palette.bound;
        return RE::BSVisit::BSVisitControl::kContinue;
    });
    return palette;
}

FigureBound figure_skinned_bound(RE::NiAVObject& a_root)
{
    FigureBound out;
    RE::BSVisit::TraverseScenegraphGeometries(&a_root, [&](RE::BSGeometry* a_geometry)
    {
        if (!a_geometry || !readable(a_geometry, sizeof(RE::BSGeometry)))
            return RE::BSVisit::BSVisitControl::kContinue;
        if (!a_geometry->GetGeometryRuntimeData().skinInstance)
            return RE::BSVisit::BSVisitControl::kContinue;
        // Framing must follow the visible figure - the helper exclusion the clone route measured
        // (3BA/UBE collision helpers drag the bound and the centering off the body).
        if (figure_is_collision_helper(a_geometry->name.c_str()))
            return RE::BSVisit::BSVisitControl::kContinue;
        const RE::NiBound& bound = a_geometry->worldBound;
        if (!out.valid)
        {
            out.center = bound.center;
            out.radius = bound.radius;
            out.valid = true;
            return RE::BSVisit::BSVisitControl::kContinue;
        }
        const RE::NiPoint3 d = bound.center - out.center;
        const float dist = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        if (out.radius >= bound.radius + dist)
            return RE::BSVisit::BSVisitControl::kContinue;  // fully contained
        if (bound.radius >= out.radius + dist)
        {
            out.center = bound.center;
            out.radius = bound.radius;
            return RE::BSVisit::BSVisitControl::kContinue;
        }
        const float merged = (dist + out.radius + bound.radius) * 0.5f;
        const float t = dist > 1e-6f ? (merged - out.radius) / dist : 0.0f;
        out.center = out.center + d * t;
        out.radius = merged;
        return RE::BSVisit::BSVisitControl::kContinue;
    });
    return out;
}

FigureBound figure_skeleton_bound(RE::NiAVObject& a_root)
{
    // The palette array can hold slots that are not readable nodes (the AFP9 crash read a name at
    // 0x1010), so every entry is filtered through the set of nodes this tree actually contains before
    // anything is read from it.
    std::unordered_set<RE::NiAVObject const*> in_tree;
    RE::BSVisit::TraverseScenegraphObjects(&a_root, [&](RE::NiAVObject* a_object)
    {
        if (a_object)
            in_tree.insert(a_object);
        return RE::BSVisit::BSVisitControl::kContinue;
    });

    std::vector<RE::NiPoint3> points;
    std::unordered_set<RE::NiAVObject const*> seen;
    RE::BSVisit::TraverseScenegraphGeometries(&a_root, [&](RE::BSGeometry* a_geometry)
    {
        if (!a_geometry || !readable(a_geometry, sizeof(RE::BSGeometry)))
            return RE::BSVisit::BSVisitControl::kContinue;
        const RE::NiSkinInstance* const skin = a_geometry->GetGeometryRuntimeData().skinInstance.get();
        if (!skin || !skin->rootParent || !readable(skin, sizeof(RE::NiSkinInstance)))
            return RE::BSVisit::BSVisitControl::kContinue;

        constexpr std::uint32_t Max_Palette_Entries = 1024;
        const std::uint32_t declared = skin->allocatedSize;
        RE::NiAVObject* const* const bones = skin->bones;
        if (!bones || declared == 0 || declared > Max_Palette_Entries ||
            !readable(bones, sizeof(RE::NiAVObject*) * static_cast<std::size_t>(declared)))
            return RE::BSVisit::BSVisitControl::kContinue;

        for (std::uint32_t i = 0; i < declared; ++i)
        {
            RE::NiAVObject* const bone = bones[i];
            if (!bone || !in_tree.contains(bone) || !seen.insert(bone).second)
                continue;
            points.push_back(bone->world.translate);
        }
        return RE::BSVisit::BSVisitControl::kContinue;
    });

    FigureBound out;
    if (points.empty())
        return out;

    RE::NiPoint3 sum{};
    for (RE::NiPoint3 const& point : points)
        sum = sum + point;
    const RE::NiPoint3 center = sum / static_cast<float>(points.size());
    float radius = 0.0f;
    for (RE::NiPoint3 const& point : points)
        radius = std::max(radius, (point - center).Length());

    out.center = center;
    out.radius = radius;
    out.valid = true;
    return out;
}

PLUGIN_NAMESPACE_END
