#pragma once

#include <cstdint>
#include <string>
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
    };

    // A synchronous query owns this scope; it never changes the source Actor.
    class ScopedEvaluation final
    {
    public:
        ScopedEvaluation(RE::TESObjectREFR* source, RE::hkbClipGenerator* clip, PreviewState const& state)
            : m_source(source), m_clip(clip), m_state(state), m_previous(std::exchange(s_current, this)) {}
        ~ScopedEvaluation() { s_current = m_previous; }
        ScopedEvaluation(ScopedEvaluation const&) = delete;
        ScopedEvaluation& operator=(ScopedEvaluation const&) = delete;

        [[nodiscard]] static ScopedEvaluation* current() { return s_current; }
        [[nodiscard]] bool matches(RE::TESObjectREFR* source, RE::hkbClipGenerator* clip) const
        {
            return m_source && m_clip && source == m_source && clip == m_clip;
        }
        [[nodiscard]] PreviewState const& state() const { return m_state; }
        [[nodiscard]] bool supported() const { return m_diagnostic.empty(); }
        [[nodiscard]] std::string const& diagnostic() const { return m_diagnostic; }
        [[nodiscard]] std::uint32_t overrides() const { return m_overrides; }
        void note_override() { ++m_overrides; }
        void reject(std::string reason)
        {
            if (m_diagnostic.empty())
                m_diagnostic = std::move(reason);
        }

    private:
        inline static thread_local ScopedEvaluation* s_current = nullptr;
        RE::TESObjectREFR* m_source;
        RE::hkbClipGenerator* m_clip;
        PreviewState const& m_state;
        ScopedEvaluation* m_previous;
        std::string m_diagnostic;
        std::uint32_t m_overrides = 0;
    };
}
PLUGIN_NAMESPACE_END
