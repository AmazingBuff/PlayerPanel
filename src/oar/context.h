#pragma once

#include <cstdint>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace RE
{
    class TESObjectREFR;
    class hkbClipGenerator;
}

PLUGIN_NAMESPACE_BEGIN
namespace oar
{
    struct PreviewState
    {
        bool weapon_drawn = false;
        std::unordered_map<std::string, float> graph_variables;
        std::optional<std::array<std::uint32_t, 2>> hand_override;
    };

    enum class ScopeMode
    {
        // Conditions outside the audited set make the query unsupported: a caller that picks a
        // candidate must not accept one it cannot vouch for.
        kAudited,
        // Conditions outside the projected set keep the engine's own answer: a caller that wraps
        // OAR's own evaluation must not veto decisions it does not model.
        kProjected
    };

    // What a projection answered per condition, and how often the engine's own answer disagreed.
    // The disagreement count is the direct measurement of "this projection changed an input OAR
    // decides on": it compares both answers for the SAME evaluation, not two points in time.
    struct ConditionTally
    {
        std::uint32_t overrides = 0;
        std::uint32_t differing = 0;
    };
    using OverrideTally = std::unordered_map<std::string, ConditionTally>;

    // A synchronous query owns this scope; it never changes the source Actor.
    class ScopedEvaluation final
    {
    public:
        ScopedEvaluation(RE::TESObjectREFR* source, RE::hkbClipGenerator* clip, PreviewState const& state,
            ScopeMode mode = ScopeMode::kAudited, OverrideTally* tally = nullptr)
            : m_source(source), m_clip(clip), m_state(state), m_mode(mode), m_tally(tally),
              m_previous(std::exchange(s_current, this)) {}
        ~ScopedEvaluation() { s_current = m_previous; }
        ScopedEvaluation(ScopedEvaluation const&) = delete;
        ScopedEvaluation& operator=(ScopedEvaluation const&) = delete;

        [[nodiscard]] static ScopedEvaluation* current() { return s_current; }
        // A null clip covers every clip generator the scoped source evaluates; OAR picks the clip
        // inside its own activate hook, so a wrapping scope cannot name it up front.
        [[nodiscard]] bool matches(RE::TESObjectREFR* source, RE::hkbClipGenerator* clip) const
        {
            return m_source && source == m_source && (!m_clip || clip == m_clip);
        }
        [[nodiscard]] ScopeMode mode() const { return m_mode; }
        [[nodiscard]] PreviewState const& state() const { return m_state; }
        [[nodiscard]] bool supported() const { return m_diagnostic.empty(); }
        [[nodiscard]] std::string const& diagnostic() const { return m_diagnostic; }
        [[nodiscard]] std::uint32_t evaluated() const { return m_evaluated; }
        [[nodiscard]] std::uint32_t overrides() const { return m_overrides; }
        void absorb(ScopedEvaluation const& child)
        {
            m_evaluated += child.m_evaluated;
            m_overrides += child.m_overrides;
        }
        void note_evaluated() { ++m_evaluated; }
        void note_override(std::string_view name, bool differs)
        {
            ++m_overrides;
            if (!m_tally || name.empty())
                return;
            ConditionTally& entry = (*m_tally)[std::string{ name }];
            ++entry.overrides;
            if (differs)
                ++entry.differing;
        }
        void reject(std::string reason)
        {
            if (m_diagnostic.empty())
                m_diagnostic = std::move(reason);
        }

    private:
        friend class UnprojectedEvaluation;
        inline static thread_local ScopedEvaluation* s_current = nullptr;
        RE::TESObjectREFR* m_source;
        RE::hkbClipGenerator* m_clip;
        PreviewState const& m_state;
        ScopeMode m_mode;
        OverrideTally* m_tally;
        ScopedEvaluation* m_previous;
        std::string m_diagnostic;
        std::uint32_t m_evaluated = 0;
        std::uint32_t m_overrides = 0;
    };

    class UnprojectedEvaluation final
    {
    public:
        UnprojectedEvaluation() : m_previous(std::exchange(ScopedEvaluation::s_current, nullptr)) {}
        ~UnprojectedEvaluation() { ScopedEvaluation::s_current = m_previous; }
        UnprojectedEvaluation(UnprojectedEvaluation const&) = delete;
        UnprojectedEvaluation& operator=(UnprojectedEvaluation const&) = delete;
    private:
        ScopedEvaluation* m_previous;
    };
}
PLUGIN_NAMESPACE_END
