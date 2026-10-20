//
// Created by AmazingBuff on 2026/10/20.
//
#include "character/held_equipment.h"
#include "RE/B/BipedAnim.h"
#include "RE/B/BSVisit.h"
#include "RE/T/TESObjectWEAP.h"
#include <array>
PLUGIN_NAMESPACE_BEGIN
namespace
{
    bool both_hands(RE::TESObjectWEAP const& weapon)
    {
        const RE::WEAPON_TYPE type = weapon.GetWeaponType();
        return type == RE::WEAPON_TYPE::kTwoHandSword || type == RE::WEAPON_TYPE::kTwoHandAxe ||
            type == RE::WEAPON_TYPE::kBow || type == RE::WEAPON_TYPE::kCrossbow;
    }
}
void hold_copy_equipment(RE::Actor& source, std::span<RE::NiAVObject* const> originals,
    std::span<RE::NiAVObject* const> copies)
{
    const auto& biped = source.GetBiped(false);
    std::array<RE::TESForm*, 2> hands{ source.GetEquippedObject(false), source.GetEquippedObject(true) };
    std::unordered_set<RE::NiAVObject*> held;
    std::size_t attached = 0, missing = 0;
    const auto copy_of = [&](RE::NiAVObject* original) -> RE::NiAVObject*
    {
        const auto at = std::ranges::find(originals, original);
        return at != originals.end() ? copies[static_cast<std::size_t>(at - originals.begin())] : nullptr;
    };
    const auto mount = [&](std::string_view name) -> RE::NiNode*
    {
        for (RE::NiAVObject* object : copies)
            if (object->name.c_str() && name == object->name.c_str()) return object->AsNode();
        return nullptr;
    };
    for (std::size_t hand = 0; hand < hands.size(); ++hand)
    {
        RE::TESForm* form = hands[hand];
        if (!form) continue;
        RE::TESObjectWEAP const* weapon = form->As<RE::TESObjectWEAP>();
        if (!weapon && !form->IsArmor() && !form->As<RE::TESObjectLIGH>()) continue;
        const bool bow = weapon && (weapon->GetWeaponType() == RE::WEAPON_TYPE::kBow || weapon->GetWeaponType() == RE::WEAPON_TYPE::kCrossbow);
        const std::string_view target_name = bow || hand == 1 || form->IsArmor() ? "SHIELD" : "WEAPON";
        RE::NiNode* target = mount(target_name);
        RE::NiAVObject* model = nullptr;
        bool ambiguous = false;
        if (biped)
        {
            const auto consider = [&](RE::BIPOBJECT const& part)
            {
                if (part.item != form || !part.partClone) return;
                RE::NiAVObject* candidate = copy_of(part.partClone.get());
                if (!candidate || held.contains(candidate)) return;
                if (model && model != candidate) ambiguous = true;
                model = candidate;
            };
            if (form->IsArmor())
            {
                if (RE::BIPOBJECT const* shield = biped->GetShieldObject()) consider(*shield);
            }
            else
                for (RE::BIPOBJECT const& part : biped->objects) consider(part);
        }
        if (!model && weapon)
        {
            std::array<char, 1024> node_name{};
            weapon->GetNodeName(node_name.data());
            for (RE::NiAVObject* original : originals)
                if (original->name.c_str() && std::string_view(original->name.c_str()) == node_name.data())
                {
                    RE::NiAVObject* candidate = copy_of(original);
                    if (held.contains(candidate)) continue;
                    if (model && model != candidate) ambiguous = true;
                    model = candidate;
                }
        }
        if (!target || !model || ambiguous || model == target)
        {
            ++missing;
            logger::warn("SCOPY HELD actor={:08x} form={:08x} side={} mount={} missing={}", source.GetFormID(), form->GetFormID(),
                hand == 0 ? "right" : "left", target_name, !target ? "copy-hand-node" : ambiguous ? "unique-equipped-root" : "mapped-equipped-root");
        }
        else
        {
            RE::NiPointer<RE::NiAVObject> retained(model);
            const RE::NiTransform local = model->local;
            if (model->parent != target)
            {
                if (model->parent) model->parent->DetachChild(model);
                target->AttachChild(model, true);
                model->local = local;
            }
            target->SetAppCulled(false);
            model->SetAppCulled(false);
            RE::BSVisit::TraverseScenegraphObjects(model, [](RE::NiAVObject* object)
            {
                const std::string_view name = object->name.c_str() ? object->name.c_str() : "";
                const bool sheath = name == "Scb" || name == "ScbLeft";
                if (sheath) object->SetAppCulled(true);
                return RE::BSVisit::BSVisitControl::kContinue;
            });
            held.insert(model);
            ++attached;
        }
        if (weapon && both_hands(*weapon)) break;
    }
    logger::info("SCOPY HELD actor={:08x} attached={} missing={} source-graph=read-only", source.GetFormID(), attached, missing);
}
PLUGIN_NAMESPACE_END
