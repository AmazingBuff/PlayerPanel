//
// Created by AmazingBuff on 2026/10/20.
//
#pragma once
#include <span>
#include <cstdint>
#include <vector>
#include "RE/N/NiTransform.h"
namespace RE { class Actor; class NiAVObject; class NiNode; }
PLUGIN_NAMESPACE_BEGIN
// A mapped equipment model with both parents it may live under. The model is mounted on the
// hand at capture; the transition mount parks it on its captured sheath parent until the equip
// clip reaches its weaponDraw event (and does the reverse on unequip), matching the world's
// sheath -> hand handoff that the copied graph has no behavior graph to perform.
struct HeldCopyMount
{
    RE::NiAVObject* model = nullptr;
    RE::NiNode* hand = nullptr;
    RE::NiNode* sheath_parent = nullptr;
    RE::NiTransform local{};
};
struct HeldDiagnosticIds
{
    std::uint64_t request;
    std::uint64_t accepted_request;
    std::uint32_t capture;
};
void hold_copy_equipment(RE::Actor& source, std::span<RE::NiAVObject* const> originals,
    std::span<RE::NiAVObject* const> copies, std::vector<HeldCopyMount>* mounts = nullptr, HeldDiagnosticIds ids = {});
PLUGIN_NAMESPACE_END
