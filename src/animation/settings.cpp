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
    if (!parsed) logger::warn("ANIMATION invalid Animation.UnarmedIdle='{}'; using non_combat_loop", value);
    logger::info("ANIMATION setting UnarmedIdle={}", g_unarmed_idle == UnarmedIdle::e_mt_idle ? "mt_idle" : "non_combat_loop");
    logger::info("DIAGNOSTICS setting DetailedAnimationTrace={}", g_detailed_animation_trace);
}
UnarmedIdle unarmed_idle_setting() { return g_unarmed_idle; }
bool detailed_animation_trace() { return g_detailed_animation_trace; }
}
PLUGIN_NAMESPACE_END
