//
// Created by AmazingBuff on 2026/10/16.
//

#include "animation/controller.h"

#include "animation/clip.h"
#include "character/equipment_mirror.h"
#include "animation/pose.h"
#include "oar/hooks.h"
#include "oar/preview_clip.h"
#include "animation/sequence.h"
#include "animation/variant_state.h"
#include "character/snapshot_transform.h"

#include "RE/A/AnimationFileManagerSingleton.h"
#include "RE/B/BSAnimationGraphManager.h"
#include "RE/B/BShkbAnimationGraph.h"
#include "RE/B/BSVisit.h"
#include "RE/H/hkbAnimationBindingSet.h"
#include "RE/H/hkbBehaviorGraph.h"
#include "RE/H/hkbCharacterSetup.h"
#include "RE/N/NiQuaternion.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <unordered_map>

#undef MEM_COMMIT
#undef PAGE_NOACCESS

PLUGIN_NAMESPACE_BEGIN
namespace animation
{
namespace
{
    bool readable(void const* address, std::size_t size)
    {
        if (!address)
            return false;
        REX::W32::MEMORY_BASIC_INFORMATION info{};
        if (!REX::W32::VirtualQuery(address, &info, sizeof(info)) || info.state != REX::W32::MEM_COMMIT ||
            (info.protect & (REX::W32::PAGE_NOACCESS | 0x100)) != 0)
            return false;
        const std::uintptr_t first = reinterpret_cast<std::uintptr_t>(address);
        const std::uintptr_t end = reinterpret_cast<std::uintptr_t>(info.baseAddress) + info.regionSize;
        return first <= end && size <= end - first;
    }

    RE::BSTSmartPointer<RE::BShkbAnimationGraph> source_graph(RE::Actor& source)
    {
        RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
        if (!source.GetAnimationGraphManager(manager) || !manager)
            return {};
        RE::BSTSmartPointer<RE::BShkbAnimationGraph> fallback;
        for (RE::BSTSmartPointer<RE::BShkbAnimationGraph> const& graph : manager->graphs)
        {
            if (!graph || graph->holder != &source || !graph->characterInstance.setup ||
                !graph->characterInstance.setup->animationSkeleton || !oar::character_strings(graph->characterInstance))
                continue;
            if (graph->rootNode == source.Get3D(false))
                return graph;
            char const* const project_name = graph->projectName.c_str();
            const std::string name = oar::canonical_animation_path(project_name ? project_name : "");
            if (!fallback && name.find("1stperson") == std::string::npos)
                fallback = graph;
        }
        return fallback;
    }

    oar::PreviewState preview_state(bool drawn, bool equipping = false, bool unequipping = false)
    {
        oar::PreviewState state;
        state.weapon_drawn = drawn;
        for (char const* name : { "Speed", "SpeedSampled", "Direction", "TurnDelta", "IsSneaking",
                 "IsSprinting", "IsSwimming", "bInJumpState", "IsInAir", "IsAttacking", "IsBlocking" })
            state.graph_variables.emplace(name, 0.0f);
        state.graph_variables.emplace("WeaponDrawn", drawn ? 1.0f : 0.0f);
        state.graph_variables.emplace("IsEquipping", equipping ? 1.0f : 0.0f);
        state.graph_variables.emplace("IsUnequipping", unequipping ? 1.0f : 0.0f);
        return state;
    }

    class LoadedAsset
    {
    public:
        LoadedAsset(RE::BSTSmartPointer<RE::BShkbAnimationGraph> graph, oar::AnimationSelection selection, bool loop,
            bool full_body, std::uint64_t generation, std::string_view weapon_hint = {}) :
            m_graph(std::move(graph)), m_selection(std::move(selection)), m_loop(loop), m_full_body(full_body),
            m_weapon_hint(weapon_hint), m_actor_id(m_graph->holder->GetFormID()), m_generation(generation)
        {
            m_behavior = m_graph->characterInstance.behaviorGraph;
            queue();
        }
        ~LoadedAsset() { release(); }

        void poll(RE::NiAVObject& root)
        {
            if (m_failed || m_playback.valid() || !m_queued)
                return;
            RE::hkbCharacter& character = m_graph->characterInstance;
            oar::PreviewContext context(&character, character.behaviorGraph.get());
            RE::AnimationFileManagerSingleton* const loader = RE::AnimationFileManagerSingleton::GetSingleton();
            if (loader && m_carrier.user_data() == 8)
                loader->Load(context.get(), m_carrier.get(), nullptr);
            auto* bindings = static_cast<RE::hkbAnimationBindingSet*>(character.animationBindingSet.get());
            if (!bindings)
                bindings = character.setup->animationBindingSet.get();
            if (bindings && m_selection.index < bindings->bindings.size())
            {
                void const* const entry = bindings->bindings[m_selection.index];
                if (readable(entry, 0x18))
                {
                    RE::hkaAnimationBinding* binding = nullptr;
                    std::memcpy(&binding, static_cast<std::uint8_t const*>(entry) + 0x10, sizeof(binding));
                    std::string mapping;
                    std::string coverage;
                    if (resolve_binding_playback(*m_graph, root, binding, m_selection.name, m_playback, mapping, coverage, m_weapon_hint))
                    {
                        if (binding->blendHint != RE::hkaAnimationBinding::BlendHint::kNormal)
                        {
                            fallback("additive binding requires a separate pose adapter");
                            return;
                        }
                        if (m_full_body)
                        {
                            std::vector<std::string_view> resolved;
                            for (RE::NiAVObject const* node : m_playback.nodes)
                                if (node && node->name.c_str())
                                    resolved.emplace_back(node->name.c_str());
                            if (!covers_standing_body(resolved))
                            {
                                fallback("standing body coverage incomplete");
                                return;
                            }
                        }

                        logger::info("ANIMATION actor={:08x} request={} asset ready index={} base={} replaced={} duration={:.2f} path='{}' mapping={} {}",
                            m_actor_id, m_generation, m_selection.index, m_selection.base_index, m_selection.replaced,
                            m_playback.duration, m_selection.name, mapping, coverage);
                        return;
                    }
                }
            }
            if (std::chrono::steady_clock::now() - m_started > std::chrono::seconds(2))
                fallback("animation load timed out");
        }

        void rebind(RE::NiAVObject& root)
        {
            if (!m_playback.binding_owner)
                return;
            std::string mapping;
            std::string coverage;
            ClipPlayback rebound;
            if (resolve_binding_playback(*m_graph, root, m_playback.binding_owner.get(), m_selection.name,
                    rebound, mapping, coverage, m_weapon_hint))
            {
                m_playback = std::move(rebound);

            }
            else
            {
                // Never retain node pointers into the retired copy after a failed retarget.

                m_playback = {};
            }
        }

        [[nodiscard]] bool ready() const { return m_playback.valid(); }
        [[nodiscard]] bool failed() const { return m_failed; }
        [[nodiscard]] ClipPlayback const& playback() const { return m_playback; }
        [[nodiscard]] oar::AnimationSelection const& selection() const { return m_selection; }

    private:
        void queue()
        {
            m_carrier.initialize(m_selection.index, m_selection.name, m_loop);
            RE::hkbCharacter& character = m_graph->characterInstance;
            oar::PreviewContext context(&character, character.behaviorGraph.get());
            if (RE::AnimationFileManagerSingleton* loader = RE::AnimationFileManagerSingleton::GetSingleton())
            {
                loader->Queue(context.get(), m_carrier.get(), nullptr);
                m_queued = true;
                m_started = std::chrono::steady_clock::now();
            }
            else
            {
                m_failed = true;
            }
        }

        void release()
        {

            m_playback = {};
            if (m_queued && m_carrier.user_data() != 0)
            {
                alignas(16) std::array<std::byte, sizeof(RE::hkbCharacter)> character{};
                RE::hkbBehaviorGraph* const behavior = m_behavior.get();
                std::memcpy(character.data() + 0x58, &behavior, sizeof(behavior));
                oar::PreviewContext context(reinterpret_cast<RE::hkbCharacter*>(character.data()), behavior);
                if (RE::AnimationFileManagerSingleton* loader = RE::AnimationFileManagerSingleton::GetSingleton())
                    loader->Unload(context.get(), m_carrier.get(), nullptr);
            }
            m_queued = false;
        }

        void fallback(char const* reason)
        {
            logger::warn("ANIMATION actor={:08x} request={} asset rejected index={} path='{}' reason={}",
                m_actor_id, m_generation, m_selection.index, m_selection.name, reason);
            release();
            if (m_selection.index != m_selection.base_index)
            {
                m_selection.index = m_selection.base_index;
                m_selection.name = m_selection.base_name;
                m_selection.replaced = false;
                m_selection.replacement_key = 0;
                m_selection.variants.reset();
                m_selection.variant = {};
                m_selection.reevaluate_on_loop = true;
                queue();
            }
            else
            {
                m_failed = true;
            }
        }

        RE::BSTSmartPointer<RE::BShkbAnimationGraph> m_graph;
        RE::hkRefPtr<RE::hkbBehaviorGraph> m_behavior;
        oar::AnimationSelection m_selection;
        bool m_loop;
        bool m_full_body;
        // The exact hand-attachment variant this asset's Weapon bone resolves to ("WeaponSword",
        // "WeaponBow", …). The longest-prefix alias landed on the wrong variant in-game and the
        // visible weapon never moved (fifth-round report).
        std::string m_weapon_hint;
        RE::FormID m_actor_id;
        std::uint64_t m_generation;

        bool m_queued = false;
        bool m_failed = false;
        std::chrono::steady_clock::time_point m_started{};
        oar::PreviewClip m_carrier;
        ClipPlayback m_playback{};

    };

    struct Proposal
    {
        std::uint64_t generation;
        std::vector<Stage> stages;
        std::vector<std::unique_ptr<LoadedAsset>> assets;
        std::unique_ptr<LoadedAsset> standing;
        bool refresh_continuation = false;
    };
}

class AnimationController::State
{
public:
    RE::FormID actor_id = 0;
    RE::ActorHandle source_handle;
    std::uint64_t generation = 0;
    std::uint64_t source_generation = 0;
    RequestGeneration latest;
    SourceObservation observed;
    bool has_observation = false;
    RE::BSTSmartPointer<RE::BShkbAnimationGraph> graph;
    RE::NiAVObject* root = nullptr;
    oar::QuerySession query;
    SelectionState variants;
    std::unique_ptr<Proposal> pending;
    std::unique_ptr<Proposal> active;
    PlaybackTimeline playback;
    std::unordered_map<std::string, RE::NiAVObject*> nodes;
    std::unordered_map<std::string, RE::NiTransform> pose;
    std::unordered_map<std::string, RE::NiTransform> blend_from;
    std::chrono::steady_clock::time_point last_tick{};
    double base_seconds = 0.0;
    double blend_seconds = 0.4;

    oar::AnimationSelection select(RE::Actor& source, std::span<std::string_view const> bases,
        oar::PreviewState state, std::uint32_t channel, bool activate = false)
    {
        oar::AnimationSelection result = oar::query_animation(source, graph->characterInstance, bases,
            std::move(state), query, { channel });
        if (activate) variants.activate(result.clip_key);
        if (result.variants && result.variants->sequence.empty())
        {
            if (auto pick = variants.variant(*result.variants, result.clip_key))
            {
                result.variant = *pick;
                result.index = pick->index;
                const auto at = std::ranges::find(result.variants->random, pick->index);
                if (at != result.variants->random.end())
                    result.name = result.variants->random_paths[static_cast<std::size_t>(at - result.variants->random.begin())];
            }
        }
        logger::info("ANIMATION actor={:08x} request={} action={} path='{}' index={} diagnostic='{}'",
            actor_id, generation, channel, result.name, result.index, result.diagnostic);
        return result;
    }

    void handover()
    {
        blend_from = pose;
        blend_seconds = 0.0;
        if (playback.current().clip.variants)
        {
            auto const& selection = playback.current().clip;
            variants.commit(*selection.variants, selection.variant, selection.clip_key);
        }
        logger::info("ANIMATION actor={:08x} request={} stage={} action={} path='{}' index={}", actor_id,
            generation, playback.position(), static_cast<int>(playback.current().action.kind),
            playback.current().clip.name, playback.current().clip.index);
    }
};

AnimationController::AnimationController() : m_state(std::make_unique<State>()) {}
AnimationController::~AnimationController() = default;
void AnimationController::reset()
{
    if (m_state->actor_id)
        logger::info("ANIMATION actor={:08x} request={} lifetime reset", m_state->actor_id, m_state->generation);
    m_state = std::make_unique<State>();
}

void AnimationController::request(RE::Actor& source, SourceObservation const& observation)
{
    State& state = *m_state;
    state.actor_id = source.GetFormID();
    state.source_handle = source.GetHandle();
    state.generation = observation.request_generation;
    state.latest.supersede(state.generation);
    const bool weapon_changed = state.has_observation && (state.observed.hands != observation.hands ||
        state.observed.form != observation.form || state.observed.drawn != observation.drawn);
    const SourceObservation previous = state.observed;
    state.observed = observation;
    state.has_observation = true;
    RE::BSTSmartPointer<RE::BShkbAnimationGraph> graph = source_graph(source);
    if (!graph)
    {
        state.pending.reset();
        state.active.reset();
        state.playback = {};
        logger::warn("ANIMATION actor={:08x} request={} source graph unavailable", state.actor_id, state.generation);
        return;
    }
    if (state.graph.get() != graph.get() || state.source_generation != observation.source_graph_generation)
    {
        state.active.reset();
        state.playback = {};
        state.query.reset();
        state.variants.reset();
    }
    if (weapon_changed) state.query.conditions.reset();
    state.graph = std::move(graph);
    state.source_generation = observation.source_graph_generation;
    logger::info("ANIMATION actor={:08x} request={} reason='{}' source-generation={}", state.actor_id,
        state.generation, observation.reason, state.source_generation);
    auto proposal = std::make_unique<Proposal>();
    proposal->generation = state.generation;
    const std::array<std::string_view, 1> standing_bases{ "Animations\\MT_Idle.hkx" };
    oar::AnimationSelection standing = state.select(source, standing_bases, preview_state(observation.drawn), 1, weapon_changed);
    if (!standing.found) return;
    oar::AnimationSelection stable = direct_selection(standing, standing.base_index, standing.base_name);
    if (clip_sequence_token(standing.name) == ClipSequenceToken::e_start ||
        (standing.variants && !standing.variants->sequence.empty())) standing = stable;
    proposal->standing = std::make_unique<LoadedAsset>(state.graph, standing, true, true, state.generation);
    const animation::WeaponForm form = observation.form;
    const EntryActionKind entry = entry_action_kind(observation.clip_name);
    if (observation.drawn && entry != EntryActionKind::e_unequip)
    {
        auto bases = panel_idle_bases(form);
        if (observation.left_primary && bases.size() > 1) std::swap(bases[0], bases[1]);
        oar::AnimationSelection anchor = state.select(source, bases, preview_state(true), 2, weapon_changed);
        if (anchor.found)
        {
            std::string diagnostic;
            proposal->stages = idle_sequence(anchor, stable, form, diagnostic, &state.variants);
            if (!diagnostic.empty())
                logger::warn("ANIMATION actor={:08x} request={} {}", state.actor_id, state.generation, diagnostic);
            if (entry != EntryActionKind::e_equip && observation.clip_index != 0xFFFF &&
                observation.reason == "panel-open")
            {
                auto found = std::ranges::find_if(proposal->stages, [&](Stage const& stage)
                { return stage.clip.index == observation.clip_index; });
                if (found != proposal->stages.end())
                {
                    proposal->stages.erase(proposal->stages.begin(), found);
                    proposal->stages.front().start_seconds = observation.clip_time;
                }
                else if (entry != EntryActionKind::e_sequence_start)
                {
                    const auto loop = std::ranges::find_if(proposal->stages, [](Stage const& stage) { return stage.action.loop; });
                    proposal->stages.erase(proposal->stages.begin(), loop);
                }
            }
        }
    }
    if (proposal->stages.empty())
        proposal->stages.push_back({ { ActionKind::e_standing_idle, form, true }, standing, 0.0 });
    if (observation.reason == "panel-open" && observation.clip_index != 0xFFFF &&
        (entry == EntryActionKind::e_equip || entry == EntryActionKind::e_unequip))
    {
        auto selection = direct_selection(stable, observation.clip_index, observation.clip_name);
        proposal->stages.insert(proposal->stages.begin(), { { entry == EntryActionKind::e_equip ? ActionKind::e_equip : ActionKind::e_unequip,
            form, false }, std::move(selection), observation.clip_time });
    }
    else if (observation.reason == "equipment" && weapon_changed)
    {
        const bool unequip = !observation.drawn || form == animation::WeaponForm::kNone;
        const auto transition_form = unequip ? previous.form : form;
        auto bases = panel_transition_bases(transition_form, unequip);
        if (unequip ? previous.left_primary : observation.left_primary)
        {
            if (transition_form == animation::WeaponForm::kMagic && bases.size() > 1) std::swap(bases[0], bases[1]);
            if (transition_form == animation::WeaponForm::kStaff)
                bases.front() = unequip ? "Animations\\StaffLeft_Unequip.hkx" : "Animations\\StaffLeft_Equip.hkx";
        }
        oar::PreviewState transition_state = preview_state(true, !unequip, unequip);
        if (unequip) transition_state.hand_override = previous.raw_hands;
        auto selection = state.select(source, bases, std::move(transition_state), 4, true);
        if (selection.found)
            proposal->stages.insert(proposal->stages.begin(), { { !unequip ? ActionKind::e_equip : ActionKind::e_unequip,
                transition_form, false }, std::move(selection), 0.0 });
    }
    if (observation.reason == "equipment" && !weapon_changed)
    {
        const bool carrying_transition = state.active && !state.playback.current().action.loop;
        const auto matching = state.active && !carrying_transition ? std::ranges::find_if(proposal->stages, [&](Stage const& stage)
            { return stage.clip.index == state.playback.current().clip.index; }) : proposal->stages.end();
        if (matching != proposal->stages.end())
        {
            state.active->generation = state.generation;
            state.playback.refresh_clip(matching->clip);
            state.pending.reset();
            logger::info("ANIMATION actor={:08x} request={} refreshed appearance; same clip keeps phase", state.actor_id, state.generation);
            return;
        }
        proposal->refresh_continuation = state.active != nullptr;
        if (!carrying_transition)
        {
            const auto loop = std::ranges::find_if(proposal->stages, [](Stage const& stage) { return stage.action.loop; });
            proposal->stages.erase(proposal->stages.begin(), loop);
        }
        else if (state.playback.current().action.kind == ActionKind::e_equip ||
            state.playback.current().action.kind == ActionKind::e_unequip)
            proposal->stages.insert(proposal->stages.begin(), state.playback.current());
    }
    if (state.pending)
        logger::info("ANIMATION actor={:08x} request={} cancel stale proposal={}", state.actor_id, state.generation, state.pending->generation);
    state.pending.reset();
    const auto names = oar::animation_names(state.graph->characterInstance, state.query);
    for (Stage& stage : proposal->stages)
    {
        if (stage.clip.index < names.size()) stage.clip.binding_name = names[stage.clip.index];
        proposal->assets.push_back(std::make_unique<LoadedAsset>(state.graph, stage.clip, stage.action.loop,
            stage.action.kind == ActionKind::e_standing_idle, state.generation, panel_weapon_node(stage.action.form)));
    }
    state.pending = std::move(proposal);
}

void AnimationController::retarget(RE::NiAVObject& root)
{
    State& state = *m_state;
    if (state.root == &root) return;
    state.root = &root;
    state.nodes.clear();
    RE::BSVisit::TraverseScenegraphObjects(&root, [&](RE::NiAVObject* node)
    {
        if (node != &root && node->name.c_str()) state.nodes.try_emplace(node->name.c_str(), node);
        return RE::BSVisit::BSVisitControl::kContinue;
    });
    for (Proposal* proposal : { state.active.get(), state.pending.get() })
        if (proposal)
        {
            proposal->standing->rebind(root);
            for (auto& asset : proposal->assets) asset->rebind(root);
        }
    state.last_tick = std::chrono::steady_clock::now();
    logger::info("ANIMATION actor={:08x} request={} graph retarget={} stage={} time={:.2f}", state.actor_id,
        state.generation, static_cast<void*>(&root), state.playback.position(), state.playback.seconds());
}

void AnimationController::poll()
{
    State& state = *m_state;
    if (!state.root || !state.pending) return;
    if (!state.latest.accepts(state.pending->generation))
    {
        logger::info("ANIMATION actor={:08x} discard stale request={} latest={}", state.actor_id,
            state.pending->generation, state.generation);
        state.pending.reset();
        return;
    }
    Proposal& proposal = *state.pending;
    proposal.standing->poll(*state.root);
    if (proposal.standing->failed())
    {
        logger::warn("ANIMATION actor={:08x} request={} standing load unavailable; cancel proposal", state.actor_id, state.generation);
        state.pending.reset();
        return;
    }
    bool loaded = proposal.standing->ready();
    for (auto& asset : proposal.assets)
    {
        asset->poll(*state.root);
        loaded = loaded && (asset->ready() || asset->failed());
    }
    if (!loaded) return;
    for (std::size_t i = 0; i < proposal.assets.size(); ++i)
        if (proposal.assets[i]->failed())
        {
            logger::warn("ANIMATION actor={:08x} request={} load failed path='{}'; standing handover", state.actor_id,
                state.generation, proposal.stages[i].clip.name);
            proposal.stages.resize(i);
            proposal.assets.resize(i);
            Stage fallback{ { ActionKind::e_standing_idle, animation::WeaponForm::kNone, true }, proposal.standing->selection(), 0.0 };
            proposal.stages.push_back(fallback);
            proposal.assets.push_back(std::make_unique<LoadedAsset>(state.graph, fallback.clip, true, true, state.generation));
            return;
        }
    if (proposal.refresh_continuation && state.active)
    {
        const std::size_t skip = resume_continuation(proposal.stages, state.playback);
        proposal.assets.erase(proposal.assets.begin(), proposal.assets.begin() + skip);
    }
    state.active = std::move(state.pending);
    state.playback.start(state.active->stages);
    state.last_tick = std::chrono::steady_clock::now();
    state.base_seconds = 0.0;
    state.handover();
}

bool AnimationController::ready() const { return m_state->active && m_state->playback.ready(); }
bool AnimationController::transitioning() const { return ready() && !m_state->playback.current().action.loop; }
std::string AnimationController::name() const
{
    return ready() ? m_state->playback.current().clip.name : "animation-loading";
}

std::size_t AnimationController::advance(RE::NiAVObject& root)
{
    State& state = *m_state;
    if (!ready() || state.root != &root) return 0;
    const auto now = std::chrono::steady_clock::now();
    const double delta = std::clamp(std::chrono::duration<double>(now - state.last_tick).count(), 0.0, 0.1);
    state.last_tick = now;
    LoadedAsset& asset = *state.active->assets[state.playback.position()];
    if (!asset.ready()) return 0;
    state.base_seconds += delta;
    state.pose.clear();
    const PoseBuffer base = sample_animation_clip(state.active->standing->playback(), static_cast<float>(state.base_seconds), true);
    compose_contributor(state.pose, std::span(base), BoneMask::e_full_body);
    const PoseBuffer current = sample_animation_clip(asset.playback(), static_cast<float>(state.playback.seconds()),
        state.playback.current().action.loop);
    const BoneMask mask = state.playback.current().action.kind == ActionKind::e_hand_idle ? BoneMask::e_left_hand : BoneMask::e_full_body;
    compose_contributor(state.pose, std::span(current), mask);
    state.blend_seconds += delta;
    const float weight = static_cast<float>(std::min(state.blend_seconds / 0.4, 1.0));
    for (auto& [name, local] : state.pose)
        if (auto before = state.blend_from.find(name); before != state.blend_from.end())
            local = blend_panel_pose(before->second, local, weight);
    for (auto const& [name, local] : state.pose)
        if (auto target = state.nodes.find(name); target != state.nodes.end()) target->second->local = local;
    recompute_subtree_worlds(root, 0);
    if (weight >= 1.0f) state.blend_from.clear();
    const double duration = std::max(static_cast<double>(asset.playback().duration), 0.05);
    const std::uint64_t before_cycle = static_cast<std::uint64_t>(state.playback.seconds() / duration);
    if (state.playback.advance(delta, duration)) state.handover();
    else if (state.playback.current().action.loop && !state.pending &&
        static_cast<std::uint64_t>(state.playback.seconds() / duration) != before_cycle)
    {
        auto const& current_selection = state.playback.current().clip;
        const bool fixed_sequence = current_selection.variants && current_selection.variants->sequential;
        if (!fixed_sequence)
        {
            state.query.conditions.loop(current_selection.clip_key);
            state.variants.loop(current_selection.clip_key);
            const auto source = state.source_handle.get();
            if (source && (current_selection.reevaluate_on_loop || current_selection.variants))
            {
                const std::array<std::string_view, 1> bases{ current_selection.base_name };
                oar::AnimationSelection selected = current_selection.reevaluate_on_loop ?
                    state.select(*source, bases, preview_state(state.observed.drawn), 2) : current_selection;
                if (!current_selection.reevaluate_on_loop && selected.variants)
                    if (auto pick = state.variants.variant(*selected.variants, selected.clip_key))
                    {
                        selected.variant = *pick;
                        selected.index = pick->index;
                        const auto at = std::ranges::find(selected.variants->random, pick->index);
                        if (at != selected.variants->random.end())
                            selected.name = selected.variants->random_paths[static_cast<std::size_t>(at - selected.variants->random.begin())];
                    }
                if (selected.found && selected.index != current_selection.index)
                {
                    auto proposal = std::make_unique<Proposal>();
                    proposal->generation = state.generation;
                    proposal->standing = std::make_unique<LoadedAsset>(state.graph, state.active->standing->selection(), true, true, state.generation);
                    std::string diagnostic;
                    proposal->stages = idle_sequence(selected, state.active->standing->selection(), state.observed.form, diagnostic, &state.variants);
                    // A loop recheck of a compatibility family stays in its explicit loop stage.
                    if (clip_sequence_token(selected.name) != ClipSequenceToken::e_none)
                    {
                        const auto loop = std::ranges::find_if(proposal->stages, [](Stage const& stage) { return stage.action.loop; });
                        proposal->stages.erase(proposal->stages.begin(), loop);
                    }
                    for (Stage const& stage : proposal->stages)
                        proposal->assets.push_back(std::make_unique<LoadedAsset>(state.graph, stage.clip, stage.action.loop, false, state.generation, panel_weapon_node(state.observed.form)));
                    state.pending = std::move(proposal);
                }
                else if (selected.variants)
                    state.variants.commit(*selected.variants, selected.variant, selected.clip_key);
            }
        }
    }
    return state.pose.size();
}

}
PLUGIN_NAMESPACE_END
