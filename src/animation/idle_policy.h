//
// Created by AmazingBuff on 2026/10/20.
//
#pragma once
#include "animation/action.h"
#include <optional>
#include <utility>
PLUGIN_NAMESPACE_BEGIN
namespace animation
{
    enum class UnarmedIdle : std::uint8_t { e_non_combat_loop, e_mt_idle };
    inline constexpr UnarmedIdle Default_Unarmed_Idle = UnarmedIdle::e_non_combat_loop;
    [[nodiscard]] inline std::optional<UnarmedIdle> parse_unarmed_idle(std::string_view value)
    {
        if (value == "non_combat_loop") return UnarmedIdle::e_non_combat_loop;
        if (value == "mt_idle") return UnarmedIdle::e_mt_idle;
        return std::nullopt;
    }
    struct IdleTarget
    {
        WeaponForm form;
        bool left_primary;
        bool direct;
        bool operator==(IdleTarget const&) const = default;
        [[nodiscard]] bool empty() const { return form == WeaponForm::kNone; }
        [[nodiscard]] bool drawn() const { return !direct; }
        [[nodiscard]] std::vector<std::string_view> bases() const
        {
            if (direct) return { "Animations\\MT_Idle.hkx" };
            std::vector<std::string_view> result = panel_idle_bases(form);
            if (left_primary && result.size() > 1) std::swap(result[0], result[1]);
            return result;
        }
    };
    [[nodiscard]] inline IdleTarget idle_target(WeaponForm form, bool left_primary, UnarmedIdle setting)
    { return { form, left_primary, form == WeaponForm::kNone && setting == UnarmedIdle::e_mt_idle }; }
}
PLUGIN_NAMESPACE_END
