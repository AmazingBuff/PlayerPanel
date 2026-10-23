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
    if (!parsed) logger::warn("ANIMATION invalid Animation.UnarmedIdle='{}'; using non_combat_loop", value);
    if (!parsed_experiment) logger::warn("EXPERIMENT invalid Experiment.IndependentGraph='{}'; using off", experiment);
    logger::info("ANIMATION setting UnarmedIdle={}", g_unarmed_idle == UnarmedIdle::e_mt_idle ? "mt_idle" : "non_combat_loop");
    logger::info("DIAGNOSTICS setting DetailedAnimationTrace={}", g_detailed_animation_trace);
    logger::info("DIAGNOSTICS setting BehaviorGraphProbe={}", g_behavior_graph_probe);
    logger::info("EXPERIMENT setting IndependentGraph={}", behavior_graph::independent_graph_name(g_independent_graph));
}
UnarmedIdle unarmed_idle_setting() { return g_unarmed_idle; }
bool detailed_animation_trace() { return g_detailed_animation_trace; }
bool behavior_graph_probe() { return g_behavior_graph_probe; }
behavior_graph::IndependentGraphMode independent_graph_setting() { return g_independent_graph; }
}
PLUGIN_NAMESPACE_END
