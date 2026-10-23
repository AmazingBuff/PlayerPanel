//
// Created by AmazingBuff on 2026/10/21.
//

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

PLUGIN_NAMESPACE_BEGIN
namespace oar
{
    inline constexpr std::string_view Ied_Provider = "OpenAnimationReplacer-IEDConditionExtensions";
    inline constexpr uint32_t Ied_Provider_Version = 0x01000020;
    inline constexpr uint32_t Ied_Condition_Version = 0x01000000;

    template <class Condition>
    [[nodiscard]] std::string_view ied_condition_rejection(Condition const& condition,
        std::optional<uint32_t> provider_version, bool hand_override,
        std::optional<bool> requested_hand_unchanged = std::nullopt)
    {
        const auto provider = condition.GetRequiredPluginName();
        if (!provider.c_str() || Ied_Provider != provider.c_str())
            return "third-party condition requires an explicit preview adapter";
        if (!provider_version)
            return "IED placement provider unavailable";
        if (*provider_version != Ied_Provider_Version)
            return "IED placement provider version mismatch (requires embedded 1.0.2.0)";
        const auto storage = condition.GetName();
        const std::string_view name = storage.c_str() ? storage.c_str() : "";
        const bool equipped = name == "IED_GearNodeEquippedPlacementHint";
        const bool shield = name == "SDS_IsShieldOnBackEnabled";
        if (!equipped && name != "IED_GearNodePlacementHint" && !shield)
            return hand_override ? "unknown IED condition does not support historical-hand preview queries" : "";
        if (condition.GetRequiredVersion().pack() != Ied_Condition_Version || condition.GetNumComponents() != (shield ? 0u : 3u))
            return "unsupported IED placement condition schema/version";
        if (shield)
            return {};

        for (uint32_t i = 0; i < 3; ++i)
        {
            auto const* const component = condition.GetComponent(i);
            if (!component)
                return "unsupported IED placement condition schema/version";
            using ComponentType = std::remove_cv_t<decltype(component->GetType())>;
            const ComponentType expected = i == 0 && equipped ? ComponentType::kBool :
                i == 1 ? ComponentType::kComparison : ComponentType::kNumeric;
            const std::string_view expected_name = i == 0 ? (equipped ? "Left hand" : "Gear node ID") :
                i == 1 ? "Comparison" : "Weapon placement ID";
            const auto component_name = component->GetName();
            if (component->GetType() != expected || !component_name.c_str() || expected_name != component_name.c_str())
                return "unsupported IED placement condition schema/version";
        }
        if (hand_override && equipped && !requested_hand_unchanged.value_or(false))
            return "IED equipped placement hint does not support a changed historical hand";
        return {};
    }

    template <class Condition, class Native, class Reject>
    [[nodiscard]] bool evaluate_ied_condition(Condition const& condition,
        std::optional<uint32_t> provider_version, bool hand_override, Native&& native, Reject&& reject,
        std::optional<bool> requested_hand_unchanged = std::nullopt)
    {
        const std::string_view reason = ied_condition_rejection(condition, provider_version, hand_override, requested_hand_unchanged);
        if (!reason.empty())
        {
            std::forward<Reject>(reject)(reason);
            return false;
        }
        return std::forward<Native>(native)();
    }
}
PLUGIN_NAMESPACE_END
