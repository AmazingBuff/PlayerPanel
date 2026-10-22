//
// Created by AmazingBuff on 2026/10/20.
//
#pragma once
#include "animation/idle_policy.h"
PLUGIN_NAMESPACE_BEGIN
namespace animation
{
    void load_settings();
    [[nodiscard]] UnarmedIdle unarmed_idle_setting();
    [[nodiscard]] bool detailed_animation_trace();
}
PLUGIN_NAMESPACE_END
