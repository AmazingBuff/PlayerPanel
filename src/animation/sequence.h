//
// Created by AmazingBuff on 2026/10/19.
//
#pragma once
#include "animation/action.h"
#include "animation/variant_state.h"
#include "oar/hooks.h"
#include <unordered_map>

PLUGIN_NAMESPACE_BEGIN
namespace animation
{
    struct Stage
    {
        ActionPrototype action;
        oar::AnimationSelection clip;
        double start_seconds = 0.0;
    };

    [[nodiscard]] inline oar::AnimationSelection direct_selection(oar::AnimationSelection const& base,
        std::uint16_t index, std::string path)
    {
        oar::AnimationSelection result = base;
        result.index = index;
        result.name = std::move(path);
        result.variants.reset();
        result.candidates.clear();
        return result;
    }

    // Folder interpretation is an animation compatibility rule, never an OAR sequence claim.
    [[nodiscard]] inline std::vector<Stage> idle_sequence(oar::AnimationSelection const& anchor,
        oar::AnimationSelection const& stable, animation::WeaponForm form, std::string& diagnostic, SelectionState const* history = nullptr)
    {
        std::vector<Stage> stages;
        if (anchor.variants && !anchor.variants->sequence.empty())
        {
            auto const& options = *anchor.variants;
            auto const& sequence = options.sequence;
            SelectionState planned = history ? *history : SelectionState{};
            for (std::size_t step = 0; step <= sequence.size(); ++step)
            {
                const auto pick = planned.variant(options, anchor.clip_key);
                if (!pick) break;
                const bool random = pick->position >= sequence.size();
                std::string path;
                if (random)
                {
                    const auto at = std::ranges::find(options.random, pick->index);
                    if (at == options.random.end()) break;
                    path = options.random_paths[static_cast<std::size_t>(at - options.random.begin())];
                }
                else path = sequence[pick->position].path;
                const bool loop = random || (options.sequential && pick->position + 1 == sequence.size() &&
                    !sequence[pick->position].once);
                oar::AnimationSelection selected = direct_selection(anchor, pick->index, std::move(path));
                selected.variants = options;
                selected.variant = *pick;
                stages.push_back({ { loop ? ActionKind::e_weapon_idle_loop : ActionKind::e_weapon_idle_start, form, loop },
                    std::move(selected), 0.0 });
                planned.commit(options, *pick, anchor.clip_key);
                if (loop || (options.sequential && step + 1 == sequence.size())) break;
            }
            if (stages.empty() || !stages.back().action.loop)
            {
                diagnostic = "native sequence has no repeating member; standing handover";
                stages.push_back({ { ActionKind::e_standing_idle, form, true }, stable, 0.0 });
            }
            return stages;
        }
        std::vector<SequenceMember> members;
        std::vector<std::string> paths;
        for (auto const& candidate : anchor.candidates) paths.push_back(candidate.path);
        auto family = collect_sequence_family(paths, anchor.name);
        for (SequenceMember& member : family)
        {
            const auto& descriptor = anchor.candidates[member.index];
            member.index = descriptor.index;
            members.push_back(std::move(member));
        }
        for (std::size_t i = 0; i < members.size(); ++i)
        {
            auto const& member = members[i];
            const bool loop = member.token == ClipSequenceToken::e_loop && i + 1 == members.size();
            stages.push_back({ { member.token == ClipSequenceToken::e_start ? ActionKind::e_weapon_idle_start : ActionKind::e_weapon_idle_loop,
                form, loop }, direct_selection(anchor, member.index, member.name), 0.0 });
        }
        if (!stages.empty())
        {
            if (!stages.back().action.loop)
            {
                diagnostic = "Start-only compatibility family; standing handover";
                stages.push_back({ { ActionKind::e_standing_idle, form, true }, stable, 0.0 });
            }
            return stages;
        }
        if (clip_sequence_token(anchor.name) == ClipSequenceToken::e_start)
        {
            diagnostic = "Start has no supported Loop metadata; standing handover";
            stages.push_back({ { ActionKind::e_weapon_idle_start, form, false }, anchor, 0.0 });
            stages.push_back({ { ActionKind::e_standing_idle, form, true }, stable, 0.0 });
        }
        else stages.push_back({ { ActionKind::e_weapon_idle_loop, form, true }, anchor, 0.0 });
        return stages;
    }

    class PlaybackTimeline
    {
    public:
        void start(std::vector<Stage> stages)
        {
            m_stages = std::move(stages);
            m_position = 0;
            m_seconds = m_stages.empty() ? 0.0 : m_stages.front().start_seconds;
        }
        void refresh_clip(oar::AnimationSelection selection) { m_stages[m_position].clip = std::move(selection); }
        [[nodiscard]] bool ready() const { return !m_stages.empty(); }
        [[nodiscard]] Stage const& current() const { return m_stages[m_position]; }
        [[nodiscard]] std::size_t position() const { return m_position; }
        [[nodiscard]] double seconds() const { return m_seconds; }
        // The caller samples the current stage before handover. Loading a successor never ticks it.
        bool advance(double delta, double duration)
        {
            m_seconds += std::max(delta, 0.0);
            if (current().action.loop || m_seconds < std::max(duration, 0.05) || m_position + 1 == m_stages.size())
                return false;
            ++m_position;
            m_seconds = current().start_seconds;
            return true;
        }
    private:
        std::vector<Stage> m_stages;
        std::size_t m_position = 0;
        double m_seconds = 0.0;
    };

    [[nodiscard]] inline std::size_t resume_continuation(std::vector<Stage>& stages, PlaybackTimeline const& playback)
    {
        Stage const& current = playback.current();
        auto continuing = std::ranges::find_if(stages, [&](Stage const& stage)
        { return stage.clip.index == current.clip.index && stage.action.kind == current.action.kind; });
        const bool same_stage = continuing != stages.end();
        if (!same_stage)
            continuing = std::ranges::find_if(stages, [](Stage const& stage) { return stage.action.loop; });
        const std::size_t skip = static_cast<std::size_t>(continuing - stages.begin());
        stages.erase(stages.begin(), continuing);
        if (same_stage) stages.front().start_seconds = playback.seconds();
        return skip;
    }

    class RequestGeneration
    {
    public:
        std::uint64_t next() { return ++m_generation; }
        void supersede(std::uint64_t generation) { m_generation = generation; }
        [[nodiscard]] std::uint64_t current() const { return m_generation; }
        [[nodiscard]] bool accepts(std::uint64_t generation) const { return generation == m_generation; }
    private:
        std::uint64_t m_generation = 0;
    };

    enum class BoneMask : std::uint8_t { e_full_body, e_left_hand, e_right_hand };
    [[nodiscard]] inline bool bone_allowed(BoneMask mask, std::string_view name)
    {
        if (mask == BoneMask::e_full_body) return true;
        const std::string_view side = mask == BoneMask::e_left_hand ? "NPC L " : "NPC R ";
        return name.starts_with(side) && (name.find("Finger") != std::string_view::npos ||
            name.find("Hand") != std::string_view::npos);
    }

    template<class T>
    void compose_contributor(std::unordered_map<std::string, T>& pose,
        std::span<std::pair<std::string, T> const> sample, BoneMask mask)
    {
        for (auto const& [name, value] : sample)
            if (bone_allowed(mask, name)) pose.insert_or_assign(name, value);
    }
}
PLUGIN_NAMESPACE_END
