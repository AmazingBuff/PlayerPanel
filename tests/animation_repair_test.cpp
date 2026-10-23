//
// Created by AmazingBuff on 2026/10/23.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include <cmath>
#include <string_view>
namespace Plugin { inline constexpr std::string_view Plugin_Author = "AmazingBuff"; }
#include "animation/idle_policy.h"
#include "animation/oar/ied_conditions.h"
#include "animation/sequence.h"

#include <array>
#include <cassert>
#include <iostream>

namespace
{
    using namespace test_plugin;
    enum class ComponentType : std::uint8_t { kBool, kComparison, kNumeric };
    struct Component
    {
        ComponentType type;
        std::string name;
        ComponentType GetType() const { return type; }
        std::string GetName() const { return name; }
    };
    struct Version
    {
        std::uint32_t pack() const { return oar::Ied_Condition_Version; }
    };
    struct Placement
    {
        std::array<Component, 3> components;
        std::string GetRequiredPluginName() const { return std::string(oar::Ied_Provider); }
        std::string GetName() const { return "IED_GearNodeEquippedPlacementHint"; }
        Version GetRequiredVersion() const { return {}; }
        std::uint32_t GetNumComponents() const { return 3; }
        Component const* GetComponent(std::uint32_t index) const { return &components[index]; }
    };

    void equipment_removal_preserves_query_hands_and_native_ied_boundary()
    {
        const std::array<std::uint32_t, 2> equipped{ 0x123, 0x456 };
        const std::array<std::uint32_t, 2> without_shield{ 0x123, 0 };
        const auto removal = animation::left_shield_transition(equipped, without_shield, true, false);
        assert(removal && removal->kind == animation::ActionKind::e_unequip);
        oar::PreviewState retained;
        retained.hand_override = animation::transition_hands(removal->kind, equipped);
        assert(retained.hand_override == equipped);
        const Placement hint{ { Component{ ComponentType::kBool, "Left hand" },
            Component{ ComponentType::kComparison, "Comparison" },
            Component{ ComponentType::kNumeric, "Weapon placement ID" } } };
        int native_reads = 0;
        std::string rejection;
        const bool unchanged_right = (*retained.hand_override)[0] == without_shield[0];
        assert(!oar::evaluate_ied_condition(hint, oar::Ied_Provider_Version, true,
            [&] { ++native_reads; return false; },
            [&](std::string_view reason) { rejection = reason; }, unchanged_right));
        assert(native_reads == 1 && rejection.empty());
        const bool unchanged_left = (*retained.hand_override)[1] == without_shield[1];
        assert(!oar::evaluate_ied_condition(hint, oar::Ied_Provider_Version, true,
            [&] { ++native_reads; return true; },
            [&](std::string_view reason) { rejection = reason; }, unchanged_left));
        assert(native_reads == 1 && !rejection.empty());
        retained.hand_override = animation::transition_hands(animation::ActionKind::e_unequip, without_shield);
        assert(retained.hand_override == without_shield);
        assert(!oar::finish_predicate(oar::standing_movement_direction(0.0f), false, true));
        assert(!oar::standing_movement_direction(1.0f));
        assert(panel_transition_bases(animation::WeaponForm::kOneHanded, true).front() == "Animations\\1HM_Unequip.hkx");
    }

    void one_handed_torch_composes_complete_left_arm_and_removes_cleanly()
    {
        const animation::WeaponForm form = animation::weapon_form_from_hands(
            animation::HandKind::kOneHandedWeapon, animation::HandKind::kTorch);
        assert(form == animation::WeaponForm::kOneHanded);
        assert(panel_idle_bases(form).front() == "Animations\\1hm_idle.hkx");
        assert(panel_torch_arm_bases().front() == "Animations\\Tor_1HMPose.hkx");
        assert(std::ranges::find(panel_base_animation_names(), panel_torch_arm_bases().front()) != panel_base_animation_names().end());
        const std::vector<std::pair<std::string, int>> body{
            { "NPC Root [Root]", 1 }, { "NPC Spine1 [Spn1]", 1 }, { "NPC L Thigh [LThg]", 1 },
            { "NPC R Hand [RHnd]", 1 }, { "NPC L Clavicle [LClv]", 1 }, { "NPC L UpperArm [LUar]", 1 },
            { "NPC L Forearm [LLar]", 1 }, { "NPC L Hand [LHnd]", 1 }, { "NPC L Finger11 [LF11]", 1 } };
        std::vector<std::pair<std::string, int>> torch = body;
        for (auto& entry : torch) entry.second = 2;
        std::unordered_map<std::string, int> pose;
        animation::compose_contributor(pose, std::span(body), animation::BoneMask::e_full_body);
        animation::compose_contributor(pose, std::span(std::as_const(torch)), animation::BoneMask::e_left_arm);
        assert(pose.at("NPC Root [Root]") == 1 && pose.at("NPC Spine1 [Spn1]") == 1 &&
            pose.at("NPC L Thigh [LThg]") == 1 && pose.at("NPC R Hand [RHnd]") == 1);
        assert(pose.at("NPC L Clavicle [LClv]") == 2 && pose.at("NPC L UpperArm [LUar]") == 2 &&
            pose.at("NPC L Forearm [LLar]") == 2 && pose.at("NPC L Hand [LHnd]") == 2 && pose.at("NPC L Finger11 [LF11]") == 2);
        pose.clear();
        animation::compose_contributor(pose, std::span(body), animation::BoneMask::e_full_body);
        assert(pose.at("NPC L UpperArm [LUar]") == 1 && pose.at("NPC L Hand [LHnd]") == 1);
    }
}

int main()
{
    equipment_removal_preserves_query_hands_and_native_ied_boundary();
    one_handed_torch_composes_complete_left_arm_and_removes_cleanly();
    std::cout << "2 animation repair scenarios passed (engine-free policies and composition)\n";
}
