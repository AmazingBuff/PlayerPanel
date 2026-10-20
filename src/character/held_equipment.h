//
// Created by AmazingBuff on 2026/10/20.
//
#pragma once
#include <span>
namespace RE { class Actor; class NiAVObject; }
PLUGIN_NAMESPACE_BEGIN
void hold_copy_equipment(RE::Actor& source, std::span<RE::NiAVObject* const> originals,
    std::span<RE::NiAVObject* const> copies);
PLUGIN_NAMESPACE_END
