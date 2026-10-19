//
// Created by AmazingBuff on 2026/10/19.
//
#pragma once
#include "oar/selection_state.h"
PLUGIN_NAMESPACE_BEGIN
namespace animation
{
    using oar::DataScope;
    using oar::StateKey;
    using oar::StateKeyHash;
    using oar::VariantOptions;
    using oar::VariantEntry;
    using oar::VariantPick;
    enum class LoopDecision { e_recheck, e_continue_variants, e_keep };

    [[nodiscard]] inline LoopDecision idle_loop_decision(bool reevaluate, bool has_variants, bool in_sequence)
    {
        if (has_variants && (!reevaluate || in_sequence))
            return LoopDecision::e_continue_variants;
        return reevaluate ? LoopDecision::e_recheck : LoopDecision::e_keep;
    }

    // Preview-only state. Queries may propose choices, but sequence/PlayOnce history advances
    // only when playback accepts a choice. No entry belongs to OAR's world-reference stores.
    class SelectionState
    {
    public:
        explicit SelectionState(std::uint32_t seed = 0x50414E4C) : m_random(seed) {}
        void reset() { m_conditions.clear(); m_variants.clear(); }

        [[nodiscard]] std::optional<float> random(StateKey key, std::uint64_t condition,
            std::uint64_t clip, bool reset_on_loop, float minimum, float maximum)
        {
            if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum > maximum)
                return std::nullopt;
            const RandomKey lookup{ key, condition };
            auto [entry, added] = m_conditions.try_emplace(lookup);
            if (added)
            {
                entry->second.value = static_cast<float>(static_cast<double>(minimum) +
                    (static_cast<double>(maximum) - minimum) * unit_random());
                entry->second.reset_on_loop = reset_on_loop;
            }
            register_clip(entry->second.clips, clip);
            return entry->second.value;
        }

        [[nodiscard]] std::optional<VariantPick> variant(VariantOptions const& options, std::uint64_t clip)
        {
            auto [entry, added] = m_variants.try_emplace(options.key);
            VariantState& state = entry->second;
            if (added)
                state.reset_on_loop = options.reset_on_loop;
            History const& history = state.histories[clip];
            if (!options.sequence.empty())
            {
                const std::size_t start = history.next % options.sequence.size();
                for (std::size_t offset = 0; offset < options.sequence.size(); ++offset)
                {
                    const std::size_t position = (start + offset) % options.sequence.size();
                    if (!options.sequence[position].once || !played(state, history, options, position))
                        return VariantPick{ options.sequence[position].index, position };
                }
            }
            if (options.sequential || options.random.empty() || options.random.size() != options.cumulative.size())
                return std::nullopt;
            float previous = 0.0f;
            for (float weight : options.cumulative)
            {
                if (!std::isfinite(weight) || weight < previous || weight > 1.00001f)
                    return std::nullopt;
                previous = weight;
            }
            if (previous < 0.99999f)
                return std::nullopt;
            if (!state.random)
                state.random = unit_random();
            const auto found = std::upper_bound(options.cumulative.begin(), options.cumulative.end(), *state.random);
            const std::size_t index = found == options.cumulative.end() ? options.random.size() - 1 :
                static_cast<std::size_t>(found - options.cumulative.begin());
            return VariantPick{ options.random[index] };
        }

        void commit(VariantOptions const& options, VariantPick const& pick, std::uint64_t clip)
        {
            deactivate(clip, &options.key);
            VariantState& state = m_variants[options.key];
            register_clip(state.active, clip);
            if (pick.position >= options.sequence.size())
                return;
            History& history = state.histories[clip];
            if (options.sequence[pick.position].once)
            {
                std::vector<bool>& flags = options.share_history ? state.shared_played : history.played;
                flags.resize(std::max(flags.size(), options.sequence.size()), false);
                flags[pick.position] = true;
            }
            history.next = (pick.position + 1) % options.sequence.size();
            for (std::size_t offset = 0; offset < options.sequence.size(); ++offset)
            {
                const std::size_t position = (history.next + offset) % options.sequence.size();
                if (!options.sequence[position].once || !played(state, history, options, position))
                {
                    history.next = position;
                    return;
                }
            }
            // Every member played out. A sequence that ends on a non-once member (the usual
            // Start → Loop authoring) DWELLS there in OAR: looping re-checks keep picking the
            // loop body, never the entry move again (the in-game report showed the panel
            // restarting the Start move every cycle — the "attack loop" look). Only a fully
            // once-only sequence restarts from the top.
            if (options.sequential)
            {
                for (std::size_t offset = 0; offset < options.sequence.size(); ++offset)
                {
                    const std::size_t position = (history.next + options.sequence.size() - 1 - offset) % options.sequence.size();
                    if (!options.sequence[position].once)
                    {
                        history.next = position;
                        return;
                    }
                }
                history.played.clear();
                history.next = 0;
            }
        }

        // Landing mid-sequence (the panel equipped before opening the menu): pin the clip's
        // sequence history to the picked dwell position, so the NEXT loop recheck stays here
        // instead of resuming the entry moves the world already played.
        void seek_dwell(StateKey const& key, std::uint64_t clip, VariantPick const& pick)
        {
            VariantState& state = m_variants[key];
            state.histories[clip].next = pick.position;
        }

        // Accepting a dwell pick must NOT advance the sequence: the picked member is where the
        // clip is meant to stay. Register the clip as active (loop()/keep_current read that) and
        // clear the member's PlayOnce mark so the next loop re-check can pick it again. commit()
        // would move "next" past it and replay the entry member on the following cycle (round 16).
        void dwell(VariantOptions const& options, VariantPick const& pick, std::uint64_t clip)
        {
            deactivate(clip, &options.key);
            VariantState& state = m_variants[options.key];
            register_clip(state.active, clip);
            History& history = state.histories[clip];
            std::vector<bool>& flags = options.share_history ? state.shared_played : history.played;
            if (pick.position < flags.size())
                flags[pick.position] = false;
            if (pick.position < options.sequence.size())
                history.next = pick.position;
        }

        [[nodiscard]] bool in_sequence(VariantOptions const& options, std::uint64_t clip) const
        {
            const auto state = m_variants.find(options.key);
            if (state == m_variants.end() || options.sequence.empty())
                return false;
            const auto history = state->second.histories.find(clip);
            if (history == state->second.histories.end())
                return false;
            const std::size_t next = history->second.next % options.sequence.size();
            for (std::size_t i = 0; i < next; ++i)
                if (!options.sequence[i].once)
                    return true;
            return false;
        }

        void loop(std::uint64_t clip)
        {
            std::erase_if(m_conditions, [&](auto const& entry)
            {
                return entry.second.reset_on_loop && std::ranges::find(entry.second.clips, clip) != entry.second.clips.end();
            });
            for (auto& [key, state] : m_variants)
            {
                (void)key;
                if (state.reset_on_loop && !state.active.empty() && state.active.front() == clip)
                    state.random.reset();
            }
        }

        void deactivate(std::uint64_t clip, StateKey const* keep = nullptr)
        {
            for (auto& [key, state] : m_variants)
                if (!keep || key != *keep)
                    std::erase(state.active, clip);
        }

        void activate(std::uint64_t clip)
        {
            deactivate(clip);
            std::erase_if(m_conditions, [&](auto const& entry)
            {
                return entry.first.key.scope == DataScope::e_local && entry.first.key.owner == clip;
            });
            std::erase_if(m_variants, [&](auto const& entry)
            {
                return entry.first.scope == DataScope::e_local && entry.first.owner == clip;
            });
            // An activation restarts the sequence from its first member: forget this clip's
            // progress for every scope, not only the local one, so a dwell landing or a PlayOnce
            // mark cannot leak into the restart (round 16).
            for (auto& [key, state] : m_variants)
            {
                (void)key;
                state.histories.erase(clip);
            }
        }

    private:
        struct RandomKey
        {
            StateKey key;
            std::uint64_t condition;
            bool operator==(RandomKey const&) const = default;
        };
        struct RandomKeyHash
        {
            std::size_t operator()(RandomKey const& key) const
            {
                return StateKeyHash{}(key.key) ^ static_cast<std::size_t>(key.condition * 1099511628211ULL);
            }
        };
        struct RandomState
        {
            float value = 0.0f;
            bool reset_on_loop = false;
            std::vector<std::uint64_t> clips;
        };
        struct History
        {
            std::size_t next = 0;
            std::vector<bool> played;
        };
        struct VariantState
        {
            bool reset_on_loop = true;
            std::optional<double> random;
            std::unordered_map<std::uint64_t, History> histories;
            std::vector<bool> shared_played;
            std::vector<std::uint64_t> active;
        };

        static void register_clip(std::vector<std::uint64_t>& clips, std::uint64_t clip)
        {
            if (std::ranges::find(clips, clip) == clips.end())
                clips.push_back(clip);
        }
        static bool played(VariantState const& state, History const& history,
            VariantOptions const& options, std::size_t position)
        {
            std::vector<bool> const& flags = options.share_history ? state.shared_played : history.played;
            return position < flags.size() && flags[position];
        }
        double unit_random() { return static_cast<double>(m_random() >> 8) / 16777216.0; }

        std::mt19937 m_random;
        std::unordered_map<RandomKey, RandomState, RandomKeyHash> m_conditions;
        std::unordered_map<StateKey, VariantState, StateKeyHash> m_variants;
    };
}
PLUGIN_NAMESPACE_END
