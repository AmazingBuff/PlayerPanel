#pragma once

#include "oar/context.h"
#include "oar/policy.h"
#include "oar/query_policy.h"

namespace Conditions
{
    class ICondition;
}

namespace RE
{
    class hkbCharacter;
    class hkbCharacterStringData;
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

    struct AnimationSelection
    {
        bool found = false;
        bool replaced = false;
        std::uint16_t base_index = 0xFFFF;
        std::uint16_t index = 0xFFFF;
        std::string base_name;
        std::string name;
        std::string diagnostic;
        std::uint32_t evaluated = 0;
        std::uint32_t overrides = 0;
        std::uint32_t rejected = 0;
        std::uint64_t clip_key = 0;
        std::uint64_t replacement_key = 0;
        bool reevaluate_on_loop = true;
        // Set when the query landed mid-sequence (seek_dwell): the playback accept path pins the
        // sequence history at the picked member instead of advancing past it (round 16).
        bool dwell = false;
        std::optional<VariantOptions> variants;
        VariantPick variant;
    };
    enum class QueryEvent { e_activate, e_refresh, e_loop };
    struct QueryRequest
    {
        std::uint32_t channel = 0;
        QueryEvent event = QueryEvent::e_activate;
        AnimationSelection const* current = nullptr;
        // e_activate with seek_dwell: the panel is landing MID-sequence (the player equipped
        // before opening the menu — the world already played Equip and Start), so a replacement
        // with a sequence skips its once-members and lands directly on the trailing loop body.
        bool seek_dwell = false;
    };
    [[nodiscard]] bool query_supported();
    [[nodiscard]] RE::hkbCharacterStringData* character_strings(RE::hkbCharacter const& character);
    // The project's animation-name catalogue, read through the same guards the query uses. The
    // panel uses it to find a pack's sequence siblings by name (round 17).
    [[nodiscard]] std::span<std::string const> animation_names(RE::hkbCharacter& character, QuerySession& session);
    [[nodiscard]] AnimationSelection query_animation(RE::TESObjectREFR& source, RE::hkbCharacter& character,
        std::span<std::string_view const> bases, PreviewState state, QuerySession& session,
        QueryRequest const& request = {});

    // Evaluates an audited condition tree. Selection/loading/playback are separate responsibilities.
    [[nodiscard]] EvaluationResult evaluate_condition(::Conditions::ICondition const& condition,
        RE::TESObjectREFR& source, RE::hkbClipGenerator& preview_clip, PreviewState const& state,
        void* parent_submod = nullptr);
}
PLUGIN_NAMESPACE_END
