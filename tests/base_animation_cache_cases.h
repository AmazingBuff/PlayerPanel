//
// Created by AmazingBuff on 2026/10/21.
//

#pragma once

#include "animation/idle_policy.h"
#include "animation/oar/query_policy.h"

PLUGIN_NAMESPACE_BEGIN
namespace tests
{
    inline bool fixed_bases_resolve_once_across_menu_sessions()
    {
        oar::QuerySession::invalidate_projects();
        std::vector<std::string> names(15000, "Animations\\OpenAnimationReplacer\\Pack\\1hm_idle.hkx");
        names[0] = "Animations\\DynamicAnimationReplacer\\Pack\\MT_Idle.hkx";
        names[1] = "Actors\\Character\\Animations\\Female\\2hw_idle.hkx";
        names[2] = "Animations\\Female\\MT_Idle.hkx";
        for (std::string_view name : panel_base_animation_names())
        {
            if (oar::canonical_animation_path(name) != "animations\\2hw_idle.hkx") names.emplace_back(name);
        }
        const std::size_t standing_index = names.size();
        names.emplace_back("ANIMATIONS/MT_IDLE.HKX");
        std::size_t reads = 0;
        const auto read = [&](std::size_t index) -> std::string_view { ++reads; return names[index]; };
        const oar::AnimationTableIdentity identity{ &names, names.data(), names.size() };
        oar::QuerySession first_menu;
        const oar::BaseAnimationCache::Preparation cold = oar::QuerySession::projects().prepare(identity, read);
        const std::array<std::string_view, 1> standing{ "Animations\\MT_Idle.hkx" };
        const std::optional<oar::BaseAnimation> mt = cold.project->find(standing);
        const std::optional<oar::BaseAnimation> axe = cold.project->find(panel_idle_bases(animation::WeaponForm::kTwoHandedAxe));
        const animation::IdleTarget left_magic{ animation::WeaponForm::kMagic, true, false };
        const std::optional<oar::BaseAnimation> magic = cold.project->find(left_magic.bases());
        const bool resolved = cold.initialized && reads == names.size() &&
            cold.project->requested() < 64 && cold.project->resolved() == cold.project->requested() &&
            mt && mt->name == "Animations\\MT_Idle.hkx" && mt->index != 2 && mt->index != standing_index &&
            axe && axe->index == 1 && magic && magic->name == "Animations\\MLh_Idle.hkx";
        first_menu.reset();
        oar::QuerySession next_menu;
        const oar::BaseAnimationCache::Preparation warm = next_menu.projects().prepare(identity, read);
        const bool reused = !warm.initialized && reads == names.size() && warm.project->find(standing)->index == mt->index;
        oar::QuerySession::invalidate_projects();
        return resolved && reused;
    }

    inline bool project_replacement_and_load_reset_refresh_sparse_indices()
    {
        oar::BaseAnimationCache cache;
        int owner = 0;
        std::vector<std::string> old_names{ "Animations\\MT_Idle.hkx", "Animations\\Bow_IdleHeld.hkx" };
        std::size_t reads = 0;
        const auto old_read = [&](std::size_t index) -> std::string_view { ++reads; return old_names[index]; };
        const std::array<std::string_view, 1> standing{ "Animations\\MT_Idle.hkx" };
        const oar::BaseAnimationCache::Preparation old = cache.prepare({ &owner, old_names.data(), old_names.size() }, old_read);
        const std::optional<oar::BaseAnimation> retained = old.project->find(standing);
        std::vector<std::string> names{
            "Animations\\OpenAnimationReplacer\\Deduplicated\\Actual.hkx", "animations/MT_IDLE.hkx" };
        const auto read = [&](std::size_t index) -> std::string_view { ++reads; return names[index]; };
        const oar::AnimationTableIdentity replacement{ &owner, names.data(), names.size() };
        const oar::BaseAnimationCache::Preparation fresh = cache.prepare(replacement, read);
        const std::optional<oar::BaseAnimation> mt = fresh.project->find(standing);
        const bool missing = !fresh.project->find(panel_idle_bases(animation::WeaponForm::kBow));
        const oar::BaseAnimationCache::Preparation warm = cache.prepare(replacement, read);
        const bool reused_missing = !warm.initialized && !warm.project->find(panel_idle_bases(animation::WeaponForm::kBow)) && reads == 4;
        const std::string actual = oar::BaseAnimationCache::binding_name(names.size(), 0, read);
        const std::string invalid = oar::BaseAnimationCache::binding_name(names.size(), 0xFFFF, read);
        const bool replaced = old.initialized && retained && retained->index == 0 && retained->name == old_names[0] &&
            fresh.initialized && mt && mt->index == 1 && missing && reused_missing &&
            actual == names[0] && invalid.empty() && reads == 5;
        names.push_back("Animations\\Bow_IdleHeld.hkx");
        const oar::BaseAnimationCache::Preparation resized = cache.prepare({ &owner, names.data(), names.size() }, read);
        const bool appended = resized.initialized && resized.project->find(panel_idle_bases(animation::WeaponForm::kBow))->index == 2;
        cache.invalidate();
        std::swap(names[0], names[1]);
        const oar::BaseAnimationCache::Preparation loaded = cache.prepare({ &owner, names.data(), names.size() }, read);
        return replaced && appended && loaded.initialized && loaded.project->find(standing)->index == 0 && reads == 11;
    }
}
PLUGIN_NAMESPACE_END
