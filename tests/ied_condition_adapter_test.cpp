//
// Created by AmazingBuff on 2026/10/21.
//

#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/oar/ied_conditions.h"

#include <array>
#include <cassert>
#include <iostream>
#include <string>

using namespace test_plugin;
namespace
{
    enum class ComponentType : uint8_t { kBool, kComparison, kNumeric };

    class FakeComponent final
    {
    public:
        FakeComponent(ComponentType type, std::string name) : m_type(type), m_name(std::move(name)) {}
        ComponentType GetType() const { return m_type; }
        std::string GetName() const { return m_name; }

    private:
        ComponentType m_type;
        std::string m_name;
    };

    struct FakeVersion
    {
        uint32_t pack() const { return oar::Ied_Condition_Version; }
    };

    class FakePlacementCondition final
    {
    public:
        FakePlacementCondition() : m_left(true), m_negated(false), m_disabled(false), m_native_reads(0),
            m_components{ FakeComponent(ComponentType::kBool, "Left hand"),
                FakeComponent(ComponentType::kComparison, "Comparison"),
                FakeComponent(ComponentType::kNumeric, "Weapon placement ID") } {}

        std::string GetRequiredPluginName() const { return std::string(oar::Ied_Provider); }
        std::string GetName() const { return "IED_GearNodeEquippedPlacementHint"; }
        FakeVersion GetRequiredVersion() const { return {}; }
        uint32_t GetNumComponents() const { return 3; }
        FakeComponent const* GetComponent(uint32_t index) const { return &m_components[index]; }

        bool Evaluate(std::array<int, 2> const& source)
        {
            if (m_disabled)
                return true;
            ++m_native_reads;
            const bool comparison = source[m_left ? 1 : 0] <= 1;
            return m_negated ? !comparison : comparison;
        }
        void set_left(bool value) { m_left = value; }
        void set_negated(bool value) { m_negated = value; }
        void set_disabled(bool value) { m_disabled = value; }
        int native_reads() const { return m_native_reads; }

    private:
        bool m_left;
        bool m_negated;
        bool m_disabled;
        int m_native_reads;
        std::array<FakeComponent, 3> m_components;
    };

    class FakeShieldCondition final
    {
    public:
        FakeShieldCondition() : m_negated(false) {}
        std::string GetRequiredPluginName() const { return std::string(oar::Ied_Provider); }
        std::string GetName() const { return "SDS_IsShieldOnBackEnabled"; }
        FakeVersion GetRequiredVersion() const { return {}; }
        uint32_t GetNumComponents() const { return 0; }
        FakeComponent const* GetComponent(uint32_t) const { return nullptr; }
        bool Evaluate(bool enabled) const { return m_negated ? !enabled : enabled; }
        void set_negated(bool value) { m_negated = value; }

    private:
        bool m_negated;
    };

    void placement_admission_preserves_native_configuration_and_false()
    {
        FakePlacementCondition condition;
        const std::array<int, 2> source{ 0, 2 };
        std::string diagnostic;
        const auto evaluate = [&]
        {
            return oar::evaluate_ied_condition(condition, oar::Ied_Provider_Version, false,
                [&] { return condition.Evaluate(source); },
                [&](std::string_view reason) { diagnostic = reason; });
        };
        assert(!evaluate() && diagnostic.empty());
        condition.set_negated(true);
        assert(evaluate() && diagnostic.empty());
        condition.set_left(false);
        assert(!evaluate() && diagnostic.empty());
        condition.set_disabled(true);
        assert(evaluate() && condition.native_reads() == 3 && diagnostic.empty());
        assert((source == std::array<int, 2>{ 0, 2 }));

        FakeShieldCondition shield;
        const auto evaluate_shield = [&]
        {
            return oar::evaluate_ied_condition(shield, oar::Ied_Provider_Version, false,
                [&] { return shield.Evaluate(false); },
                [&](std::string_view reason) { diagnostic = reason; });
        };
        assert(!evaluate_shield() && diagnostic.empty());
        shield.set_negated(true);
        assert(evaluate_shield() && diagnostic.empty());
    }

    void historical_hand_scope_rejects_before_native_read()
    {
        FakePlacementCondition condition;
        const std::array<int, 2> source{ 0, 2 };
        std::string diagnostic;
        const bool matched = oar::evaluate_ied_condition(condition, oar::Ied_Provider_Version, true,
            [&] { return condition.Evaluate(source); },
            [&](std::string_view reason) { diagnostic = reason; });
        assert(!matched && condition.native_reads() == 0);
        assert(diagnostic == "IED equipped placement hint has no snapshot for a changed historical hand");
    }

    void historical_snapshot_matches_the_weapon_and_hand()
    {
        constexpr std::uint32_t axe = 0xE3C16;
        constexpr std::uint32_t sword = 0x56C2A4C;
        oar::IedWeaponPlacements snapshot{ oar::IedWeaponPlacement{ axe, 0 },
            oar::IedWeaponPlacement{ sword, 2 } };
        FakePlacementCondition condition;
        const auto rejection = [&](std::optional<std::uint8_t> placement)
        {
            return oar::ied_condition_rejection(condition, oar::Ied_Provider_Version, true, false, placement);
        };
        const auto right = oar::saved_ied_placement(snapshot, false, axe);
        assert(right == 0 && rejection(right).empty());
        const auto left = oar::saved_ied_placement(snapshot, true, sword);
        assert(left == 2 && rejection(left).empty());
        assert(!rejection(oar::saved_ied_placement(snapshot, false, sword)).empty());
        assert(!rejection(oar::saved_ied_placement(snapshot, true, axe)).empty());
        assert(!oar::ied_condition_rejection(condition, std::nullopt, true, false, right).empty());
        snapshot = {};
        assert(!rejection(oar::saved_ied_placement(snapshot, false, axe)).empty());
    }
}

int main()
{
    placement_admission_preserves_native_configuration_and_false();
    historical_hand_scope_rejects_before_native_read();
    historical_snapshot_matches_the_weapon_and_hand();
    std::cout << "IED condition adapter: 3 cases passed\n";
}
