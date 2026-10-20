//
// Created by AmazingBuff on 2026/10/20.
//
#include "dac/effects.h"
#include "base/def.h"
#include "RE/A/ActiveEffect.h"
#include "RE/E/Effect.h"
#include "RE/E/EffectSetting.h"
#include "RE/M/MagicTarget.h"
#include "RE/S/SpellItem.h"
#include <tuple>

PLUGIN_NAMESPACE_BEGIN
namespace dac
{
namespace
{
    bool contains(std::vector<std::uint32_t> const& ids, std::uint32_t id)
    { return std::ranges::find(ids, id) != ids.end(); }
    void include(std::vector<std::uint32_t>& ids, std::uint32_t id)
    { if (!contains(ids, id)) ids.push_back(id); }
    bool matches_keywords(RE::EffectSetting const& effect, std::vector<std::uint32_t> const& keywords)
    {
        for (std::uint32_t i = 0; i < effect.numKeywords; ++i)
            if (effect.keywords[i] && contains(keywords, effect.keywords[i]->GetFormID())) return true;
        return false;
    }
    void sort_effects(std::vector<EffectValue>& values)
    {
        std::ranges::sort(values, [](EffectValue const& a, EffectValue const& b)
        {
            return std::tie(a.spell, a.effect_index, a.caster, a.source, a.casting_source, a.inactive, a.magnitude, a.duration, a.elapsed, a.dual) <
                std::tie(b.spell, b.effect_index, b.caster, b.source, b.casting_source, b.inactive, b.magnitude, b.duration, b.elapsed, b.dual);
        });
    }
    // The pinned SDK declares but does not define the interface destructor. This
    // borrowed MSVC callback implements its documented destructor/Modify slots
    // directly; AddTarget does not own the callback or a saved ActiveEffect.
    class RestoreEffect final
    {
    public:
        explicit RestoreEffect(EffectValue const& value) : m_value(value) {}
        virtual ~RestoreEffect() = default;
        virtual void ModifyActiveEffect(RE::ActiveEffect* effect)
        {
            effect->magnitude = m_value.magnitude;
            effect->duration = m_value.duration;
            effect->elapsedSeconds = m_value.elapsed;
            effect->castingSource = static_cast<RE::MagicSystem::CastingSource>(m_value.casting_source);
            if (m_value.inactive) effect->flags.set(RE::ActiveEffect::Flag::kInactive);
            else effect->flags.reset(RE::ActiveEffect::Flag::kInactive);
            if (m_value.dual) effect->flags.set(RE::ActiveEffect::Flag::kDual);
            else effect->flags.reset(RE::ActiveEffect::Flag::kDual);
            m_modified = true;
        }
        bool modified() const { return m_modified; }
    private:
        EffectValue m_value;
        bool m_modified = false;
    };
}

bool restorable_spell(RE::MagicItem const& magic, std::string& reason)
{
    RE::SpellItem const* spell = magic.As<RE::SpellItem>();
    using Casting = RE::MagicSystem::CastingType;
    if (!spell || spell->data.delivery != RE::MagicSystem::Delivery::kSelf || spell->HasVMAD() ||
        (spell->data.castingType != Casting::kConstantEffect && spell->data.castingType != Casting::kFireAndForget) ||
        spell->data.costOverride != 0 || spell->data.castingPerk || spell->effects.empty() ||
        (spell->data.spellType != RE::MagicSystem::SpellType::kSpell && spell->data.spellType != RE::MagicSystem::SpellType::kAbility))
    { reason = "spell needs target, casting, cost, perk or script simulation"; return false; }
    for (RE::Effect const* item : spell->effects)
    {
        RE::EffectSetting const* base = item ? item->baseEffect : nullptr;
        if (!base) { reason = "unresolved base effect"; return false; }
        auto const& data = base->data;
        const bool marker = data.archetype == RE::EffectArchetype::kScript ||
            ((data.archetype == RE::EffectArchetype::kValueModifier || data.archetype == RE::EffectArchetype::kPeakValueModifier) && item->effectItem.magnitude == 0.0f);
        if (!marker || base->IsHostile() || data.numCounterEffects != 0 || base->HasVMAD() || base->conditions.head || item->conditions.head || item->effectItem.area ||
            data.baseCost != 0.0f || data.associatedForm || data.projectileBase || data.explosion || data.equipAbility ||
            (data.castingType != Casting::kConstantEffect && data.castingType != Casting::kFireAndForget) ||
            data.delivery != RE::MagicSystem::Delivery::kSelf)
        { reason = "effect is outside the inert self-marker restoration subset"; return false; }
    }
    return true;
}

std::vector<std::uint32_t> entry_scope(RE::Actor& actor, std::vector<RuleScope> const& rules)
{
    std::vector<std::uint32_t> result, keywords;
    for (RuleScope const& rule : rules)
        if (rule.rejection.empty())
        {
            for (std::uint32_t spell : rule.spells) include(result, spell);
            for (std::uint32_t keyword : rule.dispel_keywords) include(keywords, keyword);
        }
    actor.AsMagicTarget()->VisitActiveEffects([&](RE::ActiveEffect* effect)
    {
        if (effect && effect->spell && effect->effect && effect->effect->baseEffect &&
            matches_keywords(*effect->effect->baseEffect, keywords))
        {
            std::string reason;
            if (restorable_spell(*effect->spell, reason)) include(result, effect->spell->GetFormID());
        }
        return RE::BSContainer::ForEachResult::kContinue;
    });
    return result;
}

std::vector<EffectValue> capture_effects(RE::Actor& actor, std::vector<std::uint32_t> const& spells)
{
    std::vector<EffectValue> result;
    actor.AsMagicTarget()->VisitActiveEffects([&](RE::ActiveEffect* active)
    {
        if (!active || active->flags.any(RE::ActiveEffect::Flag::kDispelled) || !active->spell ||
            !contains(spells, active->spell->GetFormID()) || !active->effect || !active->effect->baseEffect)
            return RE::BSContainer::ForEachResult::kContinue;
        auto const& entries = active->spell->effects;
        const auto item = std::ranges::find(entries, active->effect);
        if (item != entries.end())
            result.push_back({ active->spell->GetFormID(), active->effect->baseEffect->GetFormID(),
                static_cast<std::uint32_t>(item - entries.begin()), active->caster.native_handle(),
                active->source ? active->source->GetFormID() : 0, static_cast<std::int32_t>(active->castingSource),
                active->flags.any(RE::ActiveEffect::Flag::kInactive), active->magnitude, active->duration, active->elapsedSeconds, active->flags.any(RE::ActiveEffect::Flag::kDual) });
        return RE::BSContainer::ForEachResult::kContinue;
    });
    sort_effects(result);
    return result;
}

std::size_t effect_signature(RE::Actor& actor, std::vector<std::uint32_t> const& spells)
{
    std::size_t signature = Amazing_Hash;
    actor.AsMagicTarget()->VisitActiveEffects([&](RE::ActiveEffect* active)
    {
        if (active && !active->flags.any(RE::ActiveEffect::Flag::kDispelled) && active->spell &&
            contains(spells, active->spell->GetFormID()) && active->effect && active->effect->baseEffect)
        {
            signature = hash_combine(signature, active->spell->GetFormID());
            signature = hash_combine(signature, active->effect->baseEffect->GetFormID());
            signature = hash_combine(signature, active->flags.any(RE::ActiveEffect::Flag::kInactive));
            signature = hash_combine(signature, active->magnitude);
            signature = hash_combine(signature, active->duration);
        }
        return RE::BSContainer::ForEachResult::kContinue;
    });
    return signature;
}

bool event_scope(RE::Actor& actor, std::string_view event, std::vector<RuleScope> const& rules,
    std::vector<std::uint32_t> const& controlled, std::vector<std::uint32_t>& touched, std::string& reason)
{
    std::vector<std::uint32_t> keywords;
    bool matched = false;
    for (RuleScope const& rule : rules)
        if (rule.event == event)
        {
            matched = true;
            if (!rule.rejection.empty()) { reason = rule.rejection; return false; }
            for (std::uint32_t spell : rule.spells)
            {
                RE::MagicItem* magic = RE::TESForm::LookupByID<RE::MagicItem>(spell);
                if (!magic || magic->CalculateMagickaCost(&actor) != 0.0f)
                { reason = "native spell cost is nonzero or form unavailable"; return false; }
                include(touched, spell);
            }
            for (std::uint32_t keyword : rule.dispel_keywords) include(keywords, keyword);
        }
    if (!matched) return false;
    actor.AsMagicTarget()->VisitActiveEffects([&](RE::ActiveEffect* active)
    {
        if (active && !active->flags.any(RE::ActiveEffect::Flag::kDispelled) && active->effect && active->effect->baseEffect &&
            matches_keywords(*active->effect->baseEffect, keywords))
        {
            if (!active->spell || !contains(controlled, active->spell->GetFormID()))
            { reason = "incoming keyword would dispel an unrelated/non-entry spell group"; return RE::BSContainer::ForEachResult::kStop; }
            include(touched, active->spell->GetFormID());
        }
        return RE::BSContainer::ForEachResult::kContinue;
    });
    return reason.empty();
}

bool smooth_idle_goal(std::vector<RuleScope> const& rules, IdleGoal& goal, std::string& reason)
{
    bool matched = false;
    for (RuleScope const& rule : rules)
        if (rule.event == "Smooth_loop")
        {
            matched = true;
            if (!rule.rejection.empty()) { reason = rule.rejection; return false; }
            for (std::uint32_t id : rule.spells)
            {
                RE::SpellItem const* spell = RE::TESForm::LookupByID<RE::SpellItem>(id);
                if (!spell) { reason = "Smooth_loop spell unavailable"; return false; }
                for (RE::Effect const* effect : spell->effects)
                {
                    RE::EffectSetting const& base = *effect->baseEffect;
                    if (spell->data.castingType == RE::MagicSystem::CastingType::kConstantEffect &&
                        base.data.castingType == RE::MagicSystem::CastingType::kConstantEffect)
                        include(goal.on_effects, base.GetFormID());
                    else if (base.data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kDispelWithKeywords))
                        for (std::uint32_t i = 0; i < base.numKeywords; ++i)
                            if (base.keywords[i]) include(goal.off_keywords, base.keywords[i]->GetFormID());
                }
            }
        }
    if (!matched || goal.on_effects.empty()) { reason = "Smooth_loop persistent marker protocol unavailable"; return false; }
    return true;
}

bool idle_goal_ready(RE::Actor& actor, IdleGoal const& goal)
{
    std::vector<std::uint32_t> present;
    bool clear = true;
    actor.AsMagicTarget()->VisitActiveEffects([&](RE::ActiveEffect* active)
    {
        if (!active || active->flags.any(RE::ActiveEffect::Flag::kDispelled) ||
            !active->effect || !active->effect->baseEffect) return RE::BSContainer::ForEachResult::kContinue;
        RE::EffectSetting const& base = *active->effect->baseEffect;
        if (contains(goal.on_effects, base.GetFormID()))
        {
            if (!active->flags.any(RE::ActiveEffect::Flag::kInactive)) include(present, base.GetFormID());
        }
        else if (matches_keywords(base, goal.off_keywords)) clear = false;
        return RE::BSContainer::ForEachResult::kContinue;
    });
    return clear && std::ranges::all_of(goal.on_effects, [&](std::uint32_t id) { return contains(present, id); });
}

bool restore_effects(RE::Actor& actor, Restoration const& restoration)
{
    std::vector<EffectValue> current = capture_effects(actor, restoration.spells);
    if (current == restoration.baseline) return true;
    std::vector<std::uint32_t> changed;
    for (std::uint32_t spell : restoration.spells)
    {
        std::vector<EffectValue> before, after;
        for (EffectValue const& effect : restoration.baseline) if (effect.spell == spell) before.push_back(effect);
        for (EffectValue const& effect : current) if (effect.spell == spell) after.push_back(effect);
        if (before != after) changed.push_back(spell);
    }
    std::vector<std::pair<std::uint32_t, std::uint32_t>> groups;
    actor.AsMagicTarget()->VisitActiveEffects([&](RE::ActiveEffect* effect)
    {
        if (effect && !effect->flags.any(RE::ActiveEffect::Flag::kDispelled) && effect->spell && contains(changed, effect->spell->GetFormID()))
        {
            const auto group = std::make_pair(effect->spell->GetFormID(), effect->caster.native_handle());
            if (std::ranges::find(groups, group) == groups.end()) groups.push_back(group);
        }
        return RE::BSContainer::ForEachResult::kContinue;
    });
    bool applied = true;
    for (auto const& [spell_id, caster_id] : groups)
    {
        RE::NiPointer<RE::Actor> original_caster;
        if (caster_id && !RE::LookupReferenceByHandle(caster_id, original_caster)) { applied = false; continue; }
        RE::ActorHandle caster = original_caster ? original_caster->GetHandle() : RE::ActorHandle{};
        applied = actor.AsMagicTarget()->DispelEffect(RE::TESForm::LookupByID<RE::MagicItem>(spell_id), caster) && applied;
    }
    for (EffectValue const& saved : restoration.baseline)
        if (contains(changed, saved.spell))
        {
            RE::MagicItem* spell = RE::TESForm::LookupByID<RE::MagicItem>(saved.spell);
            if (!spell || saved.effect_index >= spell->effects.size() ||
                !spell->effects[saved.effect_index] || !spell->effects[saved.effect_index]->baseEffect ||
                spell->effects[saved.effect_index]->baseEffect->GetFormID() != saved.effect)
            { applied = false; continue; }
            RE::NiPointer<RE::Actor> source_actor;
            if (saved.caster && !RE::LookupReferenceByHandle(saved.caster, source_actor)) { applied = false; continue; }
            RestoreEffect callback(saved);
            RE::MagicTarget::AddTargetData data{};
            data.caster = source_actor.get();
            data.magicItem = spell;
            data.effect = spell->effects[saved.effect_index];
            data.source = saved.source ? RE::TESForm::LookupByID<RE::TESBoundObject>(saved.source) : nullptr;
            if (saved.source && !data.source) { applied = false; continue; }
            data.postCreationCallback = reinterpret_cast<RE::MagicTarget::IPostCreationModification*>(&callback);
            data.magnitude = saved.magnitude;
            data.power = 1.0f;
            data.dualCasted = saved.dual;
            data.castingSource = static_cast<RE::MagicSystem::CastingSource>(saved.casting_source);
            const bool added = actor.AsMagicTarget()->AddTarget(data);
            applied = added && callback.modified() && applied;
        }
    const std::vector<EffectValue> restored = capture_effects(actor, restoration.spells);
    return applied && restored == restoration.baseline;
}
}
PLUGIN_NAMESPACE_END
