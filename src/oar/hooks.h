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

    // The standing-window pre-condition (state-idle v3, 2026-10-15, user): it is armed on the
    // INVENTORY-OPEN edge, not while the panel is already up - at that moment the world is still
    // advancing and OAR is evaluating for the player, so the panel can hook one real decision and
    // take the clip OAR selects for a STANDING figure in the player's weapon stance. Closing the
    // panel, or simply letting the window run out, drops the flag: the engine's original path
    // answers again and nothing in the world is driven.
    void arm_standing_window(std::uint32_t a_frames);
    void tick_standing_window();
    [[nodiscard]] bool standing_window_open();

    // The weapon stance the standing figure answers with: the form the player holds.
    void set_standing_weapon_form(WeaponForm a_form);
    [[nodiscard]] WeaponForm standing_weapon_form();

    // What OAR chose during the window: the clip generator it was evaluating, kept as a handle the
    // copy re-resolves through the full chain (never dereferenced here). Empty when the window saw
    // no decision, in which case the running clip serves.
    struct SelectedClip
    {
        RE::hkbClipGenerator const* generator = nullptr;
    };
    [[nodiscard]] SelectedClip selected_clip();
    void clear_selected_clip();

    // Evaluates an audited condition tree. Selection/loading/playback are separate responsibilities.
    [[nodiscard]] EvaluationResult evaluate_condition(::Conditions::ICondition const& condition,
        RE::TESObjectREFR& source, RE::hkbClipGenerator& preview_clip, PreviewState const& state,
        void* parent_submod = nullptr);
}
PLUGIN_NAMESPACE_END
