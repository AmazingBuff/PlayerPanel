//
// Created by AmazingBuff on 2026/10/13.
//

#include "character/figure_audit.h"

#include "RE/B/BSGeometry.h"
#include "RE/B/BSVisit.h"
#include "RE/N/NiNode.h"

#include <algorithm>
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

FigureAssembly figure_measure_assembly(RE::NiAVObject& a_root)
{
    FigureAssembly out;
    RE::BSVisit::TraverseScenegraphGeometries(&a_root, [&](RE::BSGeometry* a_geometry)
    {
        if (!a_geometry || !readable(a_geometry, sizeof(RE::BSGeometry)))
            return RE::BSVisit::BSVisitControl::kContinue;
        ++out.geometries;

        const RE::NiSkinInstance* const skin = a_geometry->GetGeometryRuntimeData().skinInstance.get();
        if (!skin || !skin->rootParent || !readable(skin, sizeof(RE::NiSkinInstance)))
            return RE::BSVisit::BSVisitControl::kContinue;
        ++out.skinned;
        if (!skin->skinData)
        {
            return RE::BSVisit::BSVisitControl::kContinue;
        }
        const std::uint32_t capacity = skin->skinData->GetBoneCount();
        const std::uint32_t slots = std::min(capacity, skin->allocatedSize);
        out.alloc_slots += slots;

        // Only pointer comparison here: a palette slot is not guaranteed to be a readable node
        // (the AFP9 crash read a name at 0x1010), so nothing below is dereferenced.
        RE::NiAVObject* const* const bones = skin->bones;
        if (!bones)
        {
            return RE::BSVisit::BSVisitControl::kContinue;
        }
        for (std::uint32_t i = 0; i < slots; ++i)
            if (bones[i])
                ++out.bound_nodes;
        return RE::BSVisit::BSVisitControl::kContinue;
    });
    return out;
}

PLUGIN_NAMESPACE_END
