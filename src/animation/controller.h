//
// Created by AmazingBuff on 2026/10/19.
//
#pragma once
#include "animation/action.h"
#include <array>
#include <memory>
namespace RE { class Actor; class NiAVObject; }
PLUGIN_NAMESPACE_BEGIN
namespace dac { class Session; }
PLUGIN_NAMESPACE_END
PLUGIN_NAMESPACE_BEGIN
namespace animation
{
    struct SourceObservation
    {
        bool left_primary = false;
        animation::WeaponForm form = animation::WeaponForm::kNone;
        std::array<std::uint32_t, 2> hands{};
        std::uint64_t request_generation = 0;
        std::uint64_t source_graph_generation = 0;
        std::string reason;
    };
    class AnimationController
    {
    public:
        AnimationController();
        ~AnimationController();
        static void prepare_base_indices(RE::Actor& source);
        void reset();
        void request(RE::Actor& source, SourceObservation const& observation, dac::Session& session);
        void retarget(RE::NiAVObject& root);
        void poll();
        std::size_t advance(RE::NiAVObject& root);
        // Writes the standing idle's first frame over the copy without advancing any clock.
        // The placement calibration measures this pose, so the frozen root is the MT_Idle
        // standard rather than whichever action happens to be displayed at capture.
        bool apply_standing_pose(RE::NiAVObject& root);
        [[nodiscard]] bool ready() const;
        [[nodiscard]] std::uint64_t diagnostic_accepted_request() const;
        // True while an equip transition has not reached its weaponDraw handoff (or an unequip
        // transition has passed its sheathe handoff). The copy's mapped weapon is parked at its
        // sheath node during that window.
        [[nodiscard]] bool weapon_sheathed() const;
        [[nodiscard]] std::string name() const;
    private:
        class State;
        std::unique_ptr<State> m_state;
    };
}
PLUGIN_NAMESPACE_END
