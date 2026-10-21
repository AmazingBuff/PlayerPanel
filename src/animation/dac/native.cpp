//
// Created by AmazingBuff on 2026/10/20.
//
#include "animation/dac/native.h"
#include "animation/dac/native_profile.h"
#include "RE/B/BSAnimationGraphEvent.h"
#include "RE/E/Effect.h"
#include "RE/E/EffectSetting.h"
#include "RE/S/SpellItem.h"
#include <Windows.h>
#include <cstring>

PLUGIN_NAMESPACE_BEGIN
namespace dac
{
namespace
{
    using ProcessEvent = RE::BSEventNotifyControl (*)(RE::BSTEventSink<RE::BSAnimationGraphEvent>*, RE::BSAnimationGraphEvent&,
        RE::BSTEventSource<RE::BSAnimationGraphEvent>*);
    struct NativeBridge
    {
        HMODULE module = nullptr;
        ProcessEvent process = nullptr;
        bool identity_checked = false;
        bool copied = false;
        std::vector<RuleScope> rules;
    };
    NativeBridge g_bridge; // first inventory/UI-task initialization, after all DataLoaded callbacks

    bool readable(void const* address, std::size_t size)
    {
        if (!address) return size == 0;
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(address, &info, sizeof(info)) || info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
        const auto first = reinterpret_cast<std::uintptr_t>(address);
        const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        return first <= end && size <= end - first;
    }
    template<class T> T read(void const* object, std::size_t offset)
    {
        T value;
        std::memcpy(&value, static_cast<std::byte const*>(object) + offset, sizeof(T));
        return value;
    }
    bool verified(HMODULE module)
    {
        auto const* base = reinterpret_cast<std::uint8_t const*>(module);
        auto const* dos = reinterpret_cast<IMAGE_DOS_HEADER const*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || dos->e_lfanew > 0x1000) return false;
        auto const* nt = reinterpret_cast<IMAGE_NT_HEADERS64 const*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.TimeDateStamp != profile::Timestamp ||
            nt->OptionalHeader.SizeOfImage != profile::Image_Size) return false;
        if (std::memcmp(base + profile::ProcessEvent_Rva, profile::ProcessEvent_Prefix.data(), 16) ||
            std::memcmp(base + profile::LoadTomls_Rva, profile::LoadTomls_Prefix.data(), 16)) return false;
        auto const& debug = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
        if (debug.VirtualAddress > profile::Image_Size || debug.Size > profile::Image_Size - debug.VirtualAddress) return false;
        auto const* entries = reinterpret_cast<IMAGE_DEBUG_DIRECTORY const*>(base + debug.VirtualAddress);
        for (std::size_t i = 0; i < debug.Size / sizeof(IMAGE_DEBUG_DIRECTORY); ++i)
        {
            auto const& entry = entries[i];
            if (entry.Type != IMAGE_DEBUG_TYPE_CODEVIEW || entry.SizeOfData < 24 ||
                entry.AddressOfRawData > profile::Image_Size || entry.SizeOfData > profile::Image_Size - entry.AddressOfRawData) continue;
            auto const* codeview = base + entry.AddressOfRawData;
            if (std::memcmp(codeview, "RSDS", 4) == 0 && std::memcmp(codeview + 4, profile::Codeview_Guid.data(), 16) == 0 &&
                read<std::uint32_t>(codeview, 20) == profile::Codeview_Age) return true;
        }
        return false;
    }
    void log_rejected_spell(RE::MagicItem const& magic, std::string const& reason)
    {
        RE::SpellItem const* const spell = magic.As<RE::SpellItem>();
        if (!spell)
        {
            logger::warn("DAC native spell {:08x} is not a SpellItem (reason: {})", magic.GetFormID(), reason);
            return;
        }
        char const* const name = spell->GetFullName();
        logger::warn("DAC native spell {:08x} '{}' rejected: {} (delivery={} casting={} cost={} perk={} effects={} type={})",
            spell->GetFormID(), name ? name : "", reason,
            static_cast<int>(spell->data.delivery), static_cast<int>(spell->data.castingType), spell->data.costOverride,
            static_cast<void const*>(spell->data.castingPerk), spell->effects.size(),
            static_cast<int>(spell->data.spellType));
    }
    RuleScope copy_rule(void const* trigger)
    {
        static_assert(sizeof(RE::BSFixedString) == 8);
        static_assert(sizeof(RE::BSTSmallArray<RE::MagicItem*, 1>) == profile::CastTrigger_customSpells - profile::CastTrigger_spells);
        auto const& tag = *reinterpret_cast<RE::BSFixedString const*>(static_cast<std::byte const*>(trigger) + profile::CastTrigger_tag);
        RuleScope result;
        result.event = tag.c_str() ? tag.c_str() : "";
        if (read<float>(trigger, profile::CastTrigger_chance) != 1.0f || read<float>(trigger, profile::CastTrigger_cooldown) != 0.0f ||
            read<float>(trigger, profile::CastTrigger_healthCost) != 0.0f || read<float>(trigger, profile::CastTrigger_staminaCost) != 0.0f ||
            read<float>(trigger, profile::CastTrigger_magickaCost) != 0.0f || !read<bool>(trigger, profile::CastTrigger_targetCaster) ||
            read<bool>(trigger, profile::CastTrigger_useWeaponCast) || read<bool>(trigger, profile::CastTrigger_replaceCastingSpell) ||
            read<bool>(trigger, profile::CastTrigger_castForeHandSpell) || read<bool>(trigger, profile::CastTrigger_castOffHandSpell) ||
            read<bool>(trigger, profile::CastTrigger_castEquipedPower) || read<bool>(trigger, profile::CastTrigger_castFaviouriteMagic))
            result.rejection = "native rule has non-restorable costs/chance/cooldown/target/dynamic casting";
        bool constrained = false, weapon_cast = false;
        for (std::size_t hand = 0; hand < 2; ++hand)
        {
            auto const* weapon = static_cast<std::byte const*>(trigger) + profile::CastTrigger_weapons + hand * profile::WeaponTrigger_Size;
            const auto form = read<std::uint32_t>(weapon, profile::WeaponTrigger_formid);
            const auto enchant = read<std::uint32_t>(weapon, profile::WeaponTrigger_enchant);
            constrained = constrained || (form != 0 && form != UINT32_MAX) || read<std::int8_t>(weapon, profile::WeaponTrigger_type) >= 0 ||
                read<void const*>(weapon, profile::WeaponTrigger_keyword) || (enchant != 0 && enchant != UINT32_MAX);
            weapon_cast = weapon_cast || read<bool>(weapon, profile::WeaponTrigger_cast);
        }
        if (constrained && weapon_cast) result.rejection = "native weapon casting can mutate hand nodes/caster state";
        auto const& spells = *reinterpret_cast<RE::BSTSmallArray<RE::MagicItem*, 1> const*>(static_cast<std::byte const*>(trigger) + profile::CastTrigger_spells);
        auto const& custom = *reinterpret_cast<RE::BSTSmallArray<RE::BSFixedString, 1> const*>(static_cast<std::byte const*>(trigger) + profile::CastTrigger_customSpells);
        if (!custom.empty() || spells.empty() || spells.size() > 1024 || !readable(spells.data(), spells.size() * sizeof(RE::MagicItem*)))
        { result.rejection = "native literal spell array unavailable or custom spells configured"; return result; }
        for (RE::MagicItem const* spell : spells)
        {
            std::string reason;
            if (!spell || !restorable_spell(*spell, reason))
            {
                if (spell) log_rejected_spell(*spell, reason);
                result.rejection = spell ? std::move(reason) : "native spell pointer unavailable";
                continue;
            }
            result.spells.push_back(spell->GetFormID());
            for (RE::Effect const* effect : spell->effects)
                if (effect->baseEffect->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kDispelWithKeywords))
                    for (std::uint32_t i = 0; i < effect->baseEffect->numKeywords; ++i)
                        if (RE::BGSKeyword const* keyword = effect->baseEffect->keywords[i]) result.dispel_keywords.push_back(keyword->GetFormID());
        }
        return result;
    }
}

std::vector<RuleScope> const* native_rules()
{
    NativeBridge& bridge = g_bridge;
    if (!bridge.identity_checked)
    {
        bridge.identity_checked = true;
        HMODULE module = GetModuleHandleW(profile::Module_Name);
        if (!module || !verified(module))
        { logger::warn("DAC native phase events disabled: original DAC NG Plus DLL/CodeView/entry profile unavailable"); return nullptr; }
        bridge.module = module;
        bridge.process = reinterpret_cast<ProcessEvent>(reinterpret_cast<std::uintptr_t>(module) + profile::ProcessEvent_Rva);
    }
    if (!bridge.module) return nullptr;
    auto const* base = reinterpret_cast<std::byte const*>(bridge.module);
    // LoadTomls sets this during DataLoaded. Never inspect its vector in our own
    // DataLoaded callback: the first later inventory/UI task provides the stable boundary.
    if (!read<bool>(base, profile::TomlLoaded_Rva)) return nullptr;
    if (!bridge.copied)
    {
        auto const* vector = base + profile::CastTriggers_Rva;
        const std::uintptr_t first = read<std::uintptr_t>(vector, 0), last = read<std::uintptr_t>(vector, 8), capacity = read<std::uintptr_t>(vector, 16);
        if (last < first || capacity < last || (last - first) % profile::CastTrigger_Size != 0 ||
            (last - first) / profile::CastTrigger_Size > 1024 || !readable(reinterpret_cast<void const*>(first), last - first))
        { logger::warn("DAC native phase events disabled: loaded CastTriggers layout invalid"); bridge.module = nullptr; return nullptr; }
        for (std::uintptr_t trigger = first; trigger < last; trigger += profile::CastTrigger_Size)
        {
            RuleScope rule = copy_rule(reinterpret_cast<void const*>(trigger));
            if (!rule.rejection.empty()) logger::warn("DAC native rule event='{}' unsupported for preview: {}", rule.event, rule.rejection);
            bridge.rules.push_back(std::move(rule));
        }
        bridge.copied = true;
        logger::info("DAC original NG Plus matched rules={} handler=0x{:x} stride={}", bridge.rules.size(), profile::ProcessEvent_Rva, profile::CastTrigger_Size);
    }
    return &bridge.rules;
}

void native_dispatch(RE::Actor& source, std::string const& event)
{
    RE::BSAnimationGraphEvent animation_event{ RE::BSFixedString(event), &source, RE::BSFixedString() };
    // The evidenced DAC-only body ignores sink/source and returns directly. Its PC/NPC
    // wrappers also call the original Actor sink and are deliberately never invoked here.
    (void)g_bridge.process(nullptr, animation_event, nullptr);
}
}
PLUGIN_NAMESPACE_END
