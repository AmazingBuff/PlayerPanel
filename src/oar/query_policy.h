//
// Created by AmazingBuff on 2026/10/16.
//

#pragma once

#include "oar/base_animation_cache.h"
#include "oar/selection_state.h"

PLUGIN_NAMESPACE_BEGIN
namespace oar
{
    class QuerySession
    {
    public:
        explicit QuerySession(std::uint32_t seed = 0x50414E4C) : conditions(seed) {}
        ConditionRandomState conditions;
        void reset() { conditions.reset(); }
        static void invalidate_projects() { projects().invalidate(); }
        [[nodiscard]] static BaseAnimationCache& projects()
        {
            static BaseAnimationCache s_projects;
            return s_projects;
        }
    };
}
PLUGIN_NAMESPACE_END
