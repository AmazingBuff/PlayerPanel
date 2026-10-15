#pragma once

#include "oar/context.h"

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

    // Evaluates an audited condition tree. Selection/loading/playback are separate responsibilities.
    [[nodiscard]] EvaluationResult evaluate_condition(::Conditions::ICondition const& condition,
        RE::TESObjectREFR& source, RE::hkbClipGenerator& preview_clip, PreviewState const& state,
        void* parent_submod = nullptr);
}
PLUGIN_NAMESPACE_END
