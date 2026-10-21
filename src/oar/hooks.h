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
        bool interruptible = false;
        float interrupt_blend = 0.0f;
        float loop_blend = 0.0f;
        VariantPick variant;
        std::string binding_name;
        std::optional<VariantOptions> variants;
        std::vector<struct AnimationDescriptor> candidates;
    };
    struct AnimationDescriptor
    {
        std::uint16_t index = 0xFFFF;
        std::string path;
        std::string binding_name;
        std::uint64_t replacement_key = 0;
        bool reevaluate_on_loop = true;
        bool interruptible = false;
        float interrupt_blend = 0.0f;
        float loop_blend = 0.0f;
        std::optional<VariantOptions> variants;
    };
    struct QueryRequest
    {
        std::uint32_t channel = 0;
    };
    [[nodiscard]] bool query_supported();
    [[nodiscard]] RE::hkbCharacterStringData* character_strings(RE::hkbCharacter const& character);
    [[nodiscard]] bool prepare_base_animations(RE::hkbCharacter& character, std::string_view stage);
    [[nodiscard]] std::optional<BaseAnimation> base_animation(RE::hkbCharacter& character,
        std::span<std::string_view const> candidates);
    [[nodiscard]] std::string animation_binding_name(RE::hkbCharacter& character, std::uint16_t index);
    [[nodiscard]] AnimationSelection query_animation(RE::TESObjectREFR& source, RE::hkbCharacter& character,
        std::span<std::string_view const> bases, PreviewState state, QuerySession& session,
        QueryRequest const& request = {});

    // Evaluates an audited condition tree. Selection/loading/playback are separate responsibilities.
    [[nodiscard]] EvaluationResult evaluate_condition(::Conditions::ICondition const& condition,
        RE::TESObjectREFR& source, RE::hkbClipGenerator& preview_clip, PreviewState const& state,
        void* parent_submod = nullptr);
}
PLUGIN_NAMESPACE_END
