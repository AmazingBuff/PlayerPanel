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
    behavior_graph::SourceTimelineMode g_source_timeline_mode = behavior_graph::SourceTimelineMode::e_off;
    std::string g_source_timeline_graph;
    bool g_source_pose_capture = false;
    bool g_source_transient_watch = false;
    behavior_graph::SourceEventCaptureMode g_source_event_capture_mode = behavior_graph::SourceEventCaptureMode::e_off;
    bool g_source_write_capture = false;
    bool g_candidate_variable_census = false;
    bool g_candidate_input_probe = false;
    std::string g_candidate_input_variable = "IsEquipping";
    std::int32_t g_candidate_input_value = 1;
    bool g_candidate_equip_prime = false;
    bool g_candidate_generation_entry = false;
    bool g_candidate_unequip_probe = false;
    std::string g_candidate_unequip_variable;
    std::string g_candidate_unequip_prime;
    std::string g_candidate_unequip_event;
    bool g_oar_context_watch = false;
    behavior_graph::ProbeLivenessMode g_sampling_liveness_mode = behavior_graph::ProbeLivenessMode::e_off;
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
    char const* const timeline = ini.GetValue("Experiment", "SourceTimeline", "off");
    std::optional<behavior_graph::SourceTimelineMode> const parsed_timeline =
        behavior_graph::parse_source_timeline(timeline ? std::string_view{ timeline } : std::string_view{});
    g_source_timeline_mode = parsed_timeline.value_or(behavior_graph::SourceTimelineMode::e_off);
    char const* const timeline_graph = ini.GetValue("Experiment", "SourceTimelineGraph", "");
    g_source_timeline_graph = timeline_graph ? timeline_graph : "";
    g_source_pose_capture = ini.GetBoolValue("Experiment", "SourcePoseCapture", false);
    g_source_transient_watch = ini.GetBoolValue("Experiment", "SourceTransientWatch", false);
    char const* const event_capture = ini.GetValue("Experiment", "SourceEventCapture", "off");
    std::optional<behavior_graph::SourceEventCaptureMode> const parsed_event_capture =
        behavior_graph::parse_source_event_capture(event_capture ? std::string_view{ event_capture } : std::string_view{});
    g_source_event_capture_mode = parsed_event_capture.value_or(behavior_graph::SourceEventCaptureMode::e_off);
    g_source_write_capture = ini.GetBoolValue("Experiment", "SourceWriteCapture", false);
    g_candidate_variable_census = ini.GetBoolValue("Experiment", "CandidateVariableCensus", false);
    g_candidate_input_probe = ini.GetBoolValue("Experiment", "CandidateInputProbe", false);
    char const* const input_variable = ini.GetValue("Experiment", "CandidateInputVariable", "IsEquipping");
    // A present but empty value means observation only; a missing key keeps the default target.
    g_candidate_input_variable = input_variable ? input_variable : "IsEquipping";
    g_candidate_input_value = static_cast<std::int32_t>(ini.GetLongValue("Experiment", "CandidateInputValue", 1));
    g_candidate_equip_prime = ini.GetBoolValue("Experiment", "CandidateEquipPrime", false);
    g_candidate_generation_entry = ini.GetBoolValue("Experiment", "CandidateGenerationEntry", false);
    g_candidate_unequip_probe = ini.GetBoolValue("Experiment", "CandidateUnequipProbe", false);
    char const* const unequip_variable = ini.GetValue("Experiment", "CandidateUnequipVariable", "");
    g_candidate_unequip_variable = unequip_variable ? unequip_variable : "";
    char const* const unequip_prime = ini.GetValue("Experiment", "CandidateUnequipPrime", "");
    g_candidate_unequip_prime = unequip_prime ? unequip_prime : "";
    char const* const unequip_event = ini.GetValue("Experiment", "CandidateUnequipEvent", "");
    g_candidate_unequip_event = unequip_event ? unequip_event : "";
    g_oar_context_watch = ini.GetBoolValue("Experiment", "OarContextWatch", false);
    char const* const sampling_liveness = ini.GetValue("Experiment", "SamplingLiveness", "off");
    std::optional<behavior_graph::ProbeLivenessMode> const parsed_sampling_liveness =
        behavior_graph::parse_probe_liveness(sampling_liveness ? std::string_view{ sampling_liveness } : std::string_view{});
    g_sampling_liveness_mode = parsed_sampling_liveness.value_or(behavior_graph::ProbeLivenessMode::e_off);
    if (!parsed) logger::warn("ANIMATION invalid Animation.UnarmedIdle='{}'; using non_combat_loop", value);
    if (!parsed_experiment) logger::warn("EXPERIMENT invalid Experiment.IndependentGraph='{}'; using off", experiment);
    if (!parsed_event_capture)
        logger::warn("EXPERIMENT invalid Experiment.SourceEventCapture='{}'; using off (expected off|on|concurrent)",
            event_capture ? event_capture : "");
    if (!parsed_timeline)
        logger::warn("EXPERIMENT invalid Experiment.SourceTimeline='{}'; using off (expected off|on|concurrent)",
            timeline ? timeline : "");
    if (!parsed_sampling_liveness)
        logger::warn("EXPERIMENT invalid Experiment.SamplingLiveness='{}'; using off (expected off|on|concurrent)",
            sampling_liveness ? sampling_liveness : "");
    logger::info("ANIMATION setting UnarmedIdle={}", g_unarmed_idle == UnarmedIdle::e_mt_idle ? "mt_idle" : "non_combat_loop");
    logger::info("DIAGNOSTICS setting DetailedAnimationTrace={}", g_detailed_animation_trace);
    logger::info("DIAGNOSTICS setting BehaviorGraphProbe={}", g_behavior_graph_probe);
    logger::info("EXPERIMENT setting IndependentGraph={}", behavior_graph::independent_graph_name(g_independent_graph));
    logger::info("EXPERIMENT setting DisplayPose={}", g_display_pose);
    logger::info("EXPERIMENT setting DisplayStep={}", g_display_step);
    logger::info("EXPERIMENT setting SourceTimeline={} transient={} graph='{}' pose-capture={}", behavior_graph::source_timeline_mode_name(g_source_timeline_mode),
        g_source_transient_watch, g_source_timeline_graph, g_source_pose_capture);
    logger::info("EXPERIMENT setting SourceEventCapture={}", behavior_graph::source_event_capture_mode_name(g_source_event_capture_mode));
    logger::info("EXPERIMENT setting SourceWriteCapture={}", g_source_write_capture);
    logger::info("EXPERIMENT setting CandidateVariableCensus={}", g_candidate_variable_census);
    logger::info("EXPERIMENT setting CandidateInputProbe={} variable='{}' value={}", g_candidate_input_probe,
        g_candidate_input_variable, g_candidate_input_value);
    logger::info("EXPERIMENT setting CandidateEquipPrime={}", g_candidate_equip_prime);
    logger::info("EXPERIMENT setting CandidateGenerationEntry={}", g_candidate_generation_entry);
    logger::info("EXPERIMENT setting CandidateUnequipProbe={} variable='{}' prime='{}' event='{}'", g_candidate_unequip_probe,
        g_candidate_unequip_variable, g_candidate_unequip_prime, g_candidate_unequip_event);
    logger::info("EXPERIMENT setting OarContextWatch={}", g_oar_context_watch);
    logger::info("EXPERIMENT setting SamplingLiveness={}", behavior_graph::probe_liveness_mode_name(g_sampling_liveness_mode));
}
UnarmedIdle unarmed_idle_setting() { return g_unarmed_idle; }
bool detailed_animation_trace() { return g_detailed_animation_trace; }
bool behavior_graph_probe() { return g_behavior_graph_probe; }
behavior_graph::IndependentGraphMode independent_graph_setting() { return g_independent_graph; }
bool display_pose() { return g_display_pose; }
bool display_step() { return g_display_step; }
bool source_timeline() { return g_source_timeline_mode != behavior_graph::SourceTimelineMode::e_off; }
behavior_graph::SourceTimelineMode source_timeline_mode() { return g_source_timeline_mode; }
bool source_timeline_concurrent()
{
    return g_source_timeline_mode == behavior_graph::SourceTimelineMode::e_concurrent;
}
std::string const& source_timeline_graph() { return g_source_timeline_graph; }
bool source_pose_capture() { return g_source_pose_capture; }
bool source_transient_watch() { return g_source_transient_watch; }
bool source_event_capture() { return g_source_event_capture_mode != behavior_graph::SourceEventCaptureMode::e_off; }
behavior_graph::SourceEventCaptureMode source_event_capture_mode() { return g_source_event_capture_mode; }
bool source_event_capture_concurrent()
{
    return g_source_event_capture_mode == behavior_graph::SourceEventCaptureMode::e_concurrent;
}
bool source_write_capture() { return g_source_write_capture; }
bool candidate_variable_census() { return g_candidate_variable_census; }
bool candidate_input_probe() { return g_candidate_input_probe; }
std::string const& candidate_input_variable() { return g_candidate_input_variable; }
std::int32_t candidate_input_value() { return g_candidate_input_value; }
bool candidate_equip_prime() { return g_candidate_equip_prime; }
bool candidate_generation_entry() { return g_candidate_generation_entry; }
bool candidate_unequip_probe() { return g_candidate_unequip_probe; }
std::string const& candidate_unequip_variable() { return g_candidate_unequip_variable; }
std::string const& candidate_unequip_prime() { return g_candidate_unequip_prime; }
std::string const& candidate_unequip_event() { return g_candidate_unequip_event; }
bool oar_context_watch() { return g_oar_context_watch; }
bool sampling_liveness() { return g_sampling_liveness_mode != behavior_graph::ProbeLivenessMode::e_off; }
behavior_graph::ProbeLivenessMode sampling_liveness_mode() { return g_sampling_liveness_mode; }
}
PLUGIN_NAMESPACE_END
