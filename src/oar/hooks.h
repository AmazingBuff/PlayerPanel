#pragma once

#include "oar/context.h"
#include "oar/policy.h"

namespace Conditions
{
    class ICondition;
}

PLUGIN_NAMESPACE_BEGIN
namespace oar
{
    struct EvaluationResult
    {
        bool matched = false;
        bool supported = false;
        std::uint32_t overrides = 0;
        std::string diagnostic;
    };

    bool install_hooks();
    [[nodiscard]] bool hooks_ready();

    // The panel's pre-condition (state-idle v3, 2026-10-15, user): while the panel is open, OAR
    // decides for the panel figure as a STANDING figure in the player's current weapon stance -
    // not by replaying sampled clips, but by answering OAR's own conditions with the standing
    // pose. Everything outside the panel keeps the engine's original path untouched. The flag is
    // set on the render thread from the panel's own state (open/closed); it never changes what OAR
    // is asked, only how the pose predicates are answered while it is set.
    void set_panel_open(bool a_open);
    [[nodiscard]] bool panel_open();

    // The weapon stance the panel's standing figure answers with: the form the player holds.
    void set_panel_weapon_form(WeaponForm a_form);
    [[nodiscard]] WeaponForm panel_weapon_form();

    // Evaluates an audited condition tree. Selection/loading/playback are separate responsibilities.
    [[nodiscard]] EvaluationResult evaluate_condition(::Conditions::ICondition const& condition,
        RE::TESObjectREFR& source, RE::hkbClipGenerator& preview_clip, PreviewState const& state,
        void* parent_submod = nullptr);
}
PLUGIN_NAMESPACE_END
