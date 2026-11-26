//
// Created by AmazingBuff on 2026/10/20.
//
#pragma once
#include "animation/dac/effect_presence.h"
#include "animation/dac/session_core.h"
#include <string>
#include <string_view>
namespace RE { class Actor; class MagicItem; }
PLUGIN_NAMESPACE_BEGIN
namespace dac
{
    struct RuleScope
    {
        std::string event;
        std::string rejection;
        std::vector<std::uint32_t> spells;
        std::vector<std::uint32_t> dispel_keywords;
    };
    struct IdleGoal
    {
        std::vector<std::uint32_t> on_effects;
        std::vector<std::uint32_t> off_keywords;
    };
    struct EffectObservation
    {
        std::size_t signature;
        std::vector<EffectCorrection> corrections;
    };
    [[nodiscard]] bool smooth_idle_goal(std::vector<RuleScope> const& rules, IdleGoal& goal, std::string& reason);
    [[nodiscard]] bool idle_goal_ready(RE::Actor& actor, IdleGoal const& goal);
    // One-line evidence for a failed preparation: which on markers are absent/inactive and which
    // effects hold the goal's off keywords.
    [[nodiscard]] std::string idle_goal_diagnostic(RE::Actor& actor, IdleGoal const& goal);
    // The source's non-dispelled Smooth Moveset markers as sorted file-local ids. A trailing "!"
    // marks an effect whose remaining copies are all inactive.
    [[nodiscard]] std::string smooth_marker_report(RE::Actor& actor);
    [[nodiscard]] bool restorable_spell(RE::MagicItem const& spell, std::string& reason);
    [[nodiscard]] std::vector<std::uint32_t> entry_scope(RE::Actor& actor, std::vector<RuleScope> const& rules);
    [[nodiscard]] std::vector<EffectValue> capture_effects(RE::Actor& actor, std::vector<std::uint32_t> const& spells);
    [[nodiscard]] EffectObservation observe_effects(RE::Actor& actor, std::vector<std::uint32_t> const& spells);
    [[nodiscard]] bool event_scope(RE::Actor& actor, std::string_view event, std::vector<RuleScope> const& rules,
        std::vector<std::uint32_t> const& controlled, std::vector<std::uint32_t>& touched, std::string& reason);
    [[nodiscard]] bool restore_effects(RE::Actor& actor, Restoration const& restoration);
}
PLUGIN_NAMESPACE_END
