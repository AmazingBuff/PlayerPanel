//
// Created by AmazingBuff on 2026/10/19.
//
#pragma once
#include "animation/action.h"
#include <array>
#include <memory>
namespace RE { class Actor; class NiAVObject; }
PLUGIN_NAMESPACE_BEGIN
namespace animation
{
    struct SourceObservation
    {
        bool drawn = false;
        bool left_primary = false;
        animation::WeaponForm form = animation::WeaponForm::kNone;
        std::array<std::uint32_t, 2> hands{};
        std::array<std::uint32_t, 2> raw_hands{};
        std::string clip_name;
        std::uint16_t clip_index = 0xFFFF;
        double clip_time = 0.0;
        bool clip_looping = false;
        std::uint64_t request_generation = 0;
        std::uint64_t source_graph_generation = 0;
        std::string reason;
    };
    class AnimationController
    {
    public:
        AnimationController();
        ~AnimationController();
        void reset();
        void request(RE::Actor& source, SourceObservation const& observation);
        void retarget(RE::NiAVObject& root);
        void poll();
        std::size_t advance(RE::NiAVObject& root);
        // Writes the standing idle's first frame over the copy without advancing any clock.
        // The placement calibration measures this pose, so the frozen root is the MT_Idle
        // standard rather than whichever action happens to be displayed at capture.
        bool apply_standing_pose(RE::NiAVObject& root);
        [[nodiscard]] bool ready() const;
        [[nodiscard]] std::string name() const;
    private:
        class State;
        std::unique_ptr<State> m_state;
    };
}
PLUGIN_NAMESPACE_END
