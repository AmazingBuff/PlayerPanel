//
// Created by AmazingBuff on 2026/10/19.
//
#pragma once
#include "animation/action.h"
#include "animation/variant_state.h"
#include "animation/oar/hooks.h"
#include <unordered_map>

PLUGIN_NAMESPACE_BEGIN
namespace animation
{
    struct Stage
    {
        ActionPrototype action;
        oar::AnimationSelection clip;
    };

    [[nodiscard]] inline oar::AnimationSelection direct_selection(oar::AnimationSelection const& base,
        std::uint16_t index, std::string path)
    {
        oar::AnimationSelection result = base;
        result.index = index;
        result.name = std::move(path);
        result.replaced = index != result.base_index;
        result.replacement_key = 0;
        result.interruptible = false;
        result.interrupt_blend = 0.0f;
        result.loop_blend = 0.0f;
        result.reevaluate_on_loop = true;
        result.variant = {};
        result.variants.reset();
        return result;
    }

    enum class BoneMask : std::uint8_t { e_full_body, e_left_hand, e_right_hand };
    [[nodiscard]] inline bool bone_allowed(BoneMask mask, std::string_view name)
    {
        if (mask == BoneMask::e_full_body) return true;
        const std::string_view side = mask == BoneMask::e_left_hand ? "NPC L " : "NPC R ";
        return name.starts_with(side) && (name.find("Finger") != std::string_view::npos ||
            name.find("Hand") != std::string_view::npos);
    }

    template<class T>
    void compose_contributor(std::unordered_map<std::string, T>& pose,
        std::span<std::pair<std::string, T> const> sample, BoneMask mask)
    {
        for (auto const& [name, value] : sample)
            if (bone_allowed(mask, name)) pose.insert_or_assign(name, value);
    }
}
PLUGIN_NAMESPACE_END
