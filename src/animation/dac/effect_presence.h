//
// Created by AmazingBuff on 2026/11/26.
//
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

PLUGIN_NAMESPACE_BEGIN
namespace dac
{
    struct ObservedEffectPresence
    {
        std::uint32_t effect;
        bool managed;
        bool dispelled;
        bool inactive;
    };

    struct EffectCorrection
    {
        std::uint32_t effect;
        bool all_effects;
        bool active_only;
        bool operator==(EffectCorrection const&) const = default;
    };

    [[nodiscard]] inline std::vector<EffectCorrection> dispelled_effect_corrections(std::span<ObservedEffectPresence> effects)
    {
        if (std::ranges::none_of(effects, [](ObservedEffectPresence const& effect) { return effect.managed && effect.dispelled; }))
            return {};

        std::ranges::sort(effects, {}, &ObservedEffectPresence::effect);
        std::vector<EffectCorrection> corrections;
        while (!effects.empty())
        {
            EffectCorrection correction{ effects.front().effect, false, false };
            bool live = false;
            bool active_live = false;
            std::size_t end = 0;
            for (; end < effects.size() && effects[end].effect == correction.effect; ++end)
            {
                ObservedEffectPresence const& effect = effects[end];
                if (!effect.dispelled)
                {
                    live = true;
                    active_live |= !effect.inactive;
                }
                else if (effect.managed)
                {
                    correction.all_effects = true;
                    correction.active_only |= !effect.inactive;
                }
            }
            correction.all_effects &= !live;
            correction.active_only &= !active_live;
            if (correction.all_effects || correction.active_only)
                corrections.push_back(correction);
            effects = effects.subspan(end);
        }
        return corrections;
    }
}
PLUGIN_NAMESPACE_END
