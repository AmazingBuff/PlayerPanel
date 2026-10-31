//
// Created by AmazingBuff on 2026/10/20.
//
#include "animation/settings.h"
#include <SimpleIni.h>
PLUGIN_NAMESPACE_BEGIN
namespace animation
{
namespace
{
    UnarmedIdle g_unarmed_idle = Default_Unarmed_Idle;
    bool g_detailed_animation_trace = false;
    bool g_behavior_graph_probe = false;
    behavior_graph::IndependentGraphMode g_independent_graph = behavior_graph::IndependentGraphMode::off;
    bool g_display_pose = false;
    bool g_display_step = false;
    bool g_source_timeline = false;
    bool g_source_event_capture = false;
    bool g_source_write_capture = false;
    bool g_candidate_variable_census = false;
    bool g_candidate_input_probe = false;
    std::string g_candidate_input_variable = "IsEquipping";
    std::int32_t g_candidate_input_value = 1;
    bool g_candidate_generation_entry = false;
    bool g_candidate_unequip_probe = false;
    std::string g_candidate_unequip_variable;
    std::string g_candidate_unequip_prime;
}
void load_settings()
{
    CSimpleIniA ini;
    ini.SetUnicode();
    (void)ini.LoadFile("Data/SKSE/Plugins/CharacterPanel.ini");
    char const* value = ini.GetValue("Animation", "UnarmedIdle", "non_combat_loop");
    std::optional<UnarmedIdle> parsed = parse_unarmed_idle(value);
    g_unarmed_idle = parsed.value_or(Default_Unarmed_Idle);
    g_detailed_animation_trace = ini.GetBoolValue("Diagnostics", "DetailedAnimationTrace", false);
    g_behavior_graph_probe = ini.GetBoolValue("Diagnostics", "BehaviorGraphProbe", false);
    char const* experiment = ini.GetValue("Experiment", "IndependentGraph", "off");
    std::optional<behavior_graph::IndependentGraphMode> parsed_experiment = behavior_graph::parse_independent_graph(experiment);
    g_independent_graph = parsed_experiment.value_or(behavior_graph::IndependentGraphMode::off);
    g_display_pose = ini.GetBoolValue("Experiment", "DisplayPose", false);
    g_display_step = ini.GetBoolValue("Experiment", "DisplayStep", false);
    g_source_timeline = ini.GetBoolValue("Experiment", "SourceTimeline", false);
    g_source_event_capture = ini.GetBoolValue("Experiment", "SourceEventCapture", false);
    g_source_write_capture = ini.GetBoolValue("Experiment", "SourceWriteCapture", false);
    g_candidate_variable_census = ini.GetBoolValue("Experiment", "CandidateVariableCensus", false);
    g_candidate_input_probe = ini.GetBoolValue("Experiment", "CandidateInputProbe", false);
    char const* const input_variable = ini.GetValue("Experiment", "CandidateInputVariable", "IsEquipping");
    // A present but empty value means observation only; a missing key keeps the default target.
    g_candidate_input_variable = input_variable ? input_variable : "IsEquipping";
    g_candidate_input_value = static_cast<std::int32_t>(ini.GetLongValue("Experiment", "CandidateInputValue", 1));
    g_candidate_generation_entry = ini.GetBoolValue("Experiment", "CandidateGenerationEntry", false);
    g_candidate_unequip_probe = ini.GetBoolValue("Experiment", "CandidateUnequipProbe", false);
    char const* const unequip_variable = ini.GetValue("Experiment", "CandidateUnequipVariable", "");
    g_candidate_unequip_variable = unequip_variable ? unequip_variable : "";
    char const* const unequip_prime = ini.GetValue("Experiment", "CandidateUnequipPrime", "");
    g_candidate_unequip_prime = unequip_prime ? unequip_prime : "";
    if (!parsed) logger::warn("ANIMATION invalid Animation.UnarmedIdle='{}'; using non_combat_loop", value);
    if (!parsed_experiment) logger::warn("EXPERIMENT invalid Experiment.IndependentGraph='{}'; using off", experiment);
    logger::info("ANIMATION setting UnarmedIdle={}", g_unarmed_idle == UnarmedIdle::e_mt_idle ? "mt_idle" : "non_combat_loop");
    logger::info("DIAGNOSTICS setting DetailedAnimationTrace={}", g_detailed_animation_trace);
    logger::info("DIAGNOSTICS setting BehaviorGraphProbe={}", g_behavior_graph_probe);
    logger::info("EXPERIMENT setting IndependentGraph={}", behavior_graph::independent_graph_name(g_independent_graph));
    logger::info("EXPERIMENT setting DisplayPose={}", g_display_pose);
    logger::info("EXPERIMENT setting DisplayStep={}", g_display_step);
    logger::info("EXPERIMENT setting SourceTimeline={}", g_source_timeline);
    logger::info("EXPERIMENT setting SourceEventCapture={}", g_source_event_capture);
    logger::info("EXPERIMENT setting SourceWriteCapture={}", g_source_write_capture);
    logger::info("EXPERIMENT setting CandidateVariableCensus={}", g_candidate_variable_census);
    logger::info("EXPERIMENT setting CandidateInputProbe={} variable='{}' value={}", g_candidate_input_probe,
        g_candidate_input_variable, g_candidate_input_value);
    logger::info("EXPERIMENT setting CandidateGenerationEntry={}", g_candidate_generation_entry);
    logger::info("EXPERIMENT setting CandidateUnequipProbe={} variable='{}' prime='{}'", g_candidate_unequip_probe,
        g_candidate_unequip_variable, g_candidate_unequip_prime);
}
UnarmedIdle unarmed_idle_setting() { return g_unarmed_idle; }
bool detailed_animation_trace() { return g_detailed_animation_trace; }
bool behavior_graph_probe() { return g_behavior_graph_probe; }
behavior_graph::IndependentGraphMode independent_graph_setting() { return g_independent_graph; }
bool display_pose() { return g_display_pose; }
bool display_step() { return g_display_step; }
bool source_timeline() { return g_source_timeline; }
bool source_event_capture() { return g_source_event_capture; }
bool source_write_capture() { return g_source_write_capture; }
bool candidate_variable_census() { return g_candidate_variable_census; }
bool candidate_input_probe() { return g_candidate_input_probe; }
std::string const& candidate_input_variable() { return g_candidate_input_variable; }
std::int32_t candidate_input_value() { return g_candidate_input_value; }
bool candidate_generation_entry() { return g_candidate_generation_entry; }
bool candidate_unequip_probe() { return g_candidate_unequip_probe; }
std::string const& candidate_unequip_variable() { return g_candidate_unequip_variable; }
std::string const& candidate_unequip_prime() { return g_candidate_unequip_prime; }
}
PLUGIN_NAMESPACE_END
