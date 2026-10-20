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
}
PLUGIN_NAMESPACE_END
