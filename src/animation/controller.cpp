//
// Created by AmazingBuff on 2026/10/16.
//

#include "animation/controller.h"

#include "animation/clip.h"
#include "animation/settings.h"
#include "animation/pose.h"
#include "oar/hooks.h"
#include "oar/preview_clip.h"
#include "animation/sequence.h"
#include "animation/event_clock.h"
#include "dac/session.h"
#include "RE/H/hkaAnimation.h"
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

    oar::PreviewState preview_state(bool drawn)
    {
        oar::PreviewState state;
        state.weapon_drawn = drawn;
        for (char const* name : { "Speed", "SpeedSampled", "Direction", "TurnDelta", "IsSneaking",
                 "IsSprinting", "IsSwimming", "bInJumpState", "IsInAir", "IsAttacking", "IsBlocking" })
            state.graph_variables.emplace(name, 0.0f);
        state.graph_variables.emplace("WeaponDrawn", drawn ? 1.0f : 0.0f);
        state.graph_variables.emplace("IsEquipping", 0.0f);
        state.graph_variables.emplace("IsUnequipping", 0.0f);
        return state;
    }

    class LoadedAsset
    {
    public:
        LoadedAsset(RE::BSTSmartPointer<RE::BShkbAnimationGraph> graph, oar::AnimationSelection selection, bool loop,
            bool full_body, std::uint64_t generation, std::optional<oar::AnimationSelection> fallback = {}) :
            m_graph(std::move(graph)), m_selection(std::move(selection)), m_loop(loop), m_full_body(full_body),
            m_fallback(std::move(fallback)), m_actor_id(m_graph->holder->GetFormID()), m_generation(generation)
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
                    if (resolve_binding_playback(*m_graph, root, binding, m_selection.name, m_playback, mapping, coverage))
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
                    rebound, mapping, coverage))
            {
                m_playback = std::move(rebound);

            }
            else
            {
                // Never retain node pointers into the retired copy after a failed retarget.

                m_playback = {};
            }
        }

        void refresh_selection(oar::AnimationSelection selection) { m_selection = std::move(selection); }
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
            if (m_fallback && m_selection.index != m_fallback->index)
            {
                m_selection = std::move(*m_fallback);
                m_fallback.reset();
                queue();
            }
            else if (m_selection.index != m_selection.base_index)
            {
                m_selection.index = m_selection.base_index;
                m_selection.name = m_selection.base_name;
                m_selection.replaced = false;
                m_selection.replacement_key = 0;
                m_selection.interruptible = false;
                m_selection.interrupt_blend = 0.0f;
                m_selection.loop_blend = 0.0f;
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
        std::optional<oar::AnimationSelection> m_fallback;
        RE::FormID m_actor_id;
        std::uint64_t m_generation;

        bool m_queued = false;
        bool m_failed = false;
        std::chrono::steady_clock::time_point m_started{};
        oar::PreviewClip m_carrier;
        ClipPlayback m_playback{};

    };


    enum class ChoiceReason { e_request, e_interrupt, e_boundary, e_refresh };
    struct Proposal
    {
        std::uint64_t generation = 0;
        std::uint64_t effect_revision = 0;
        Stage stage;
        std::unique_ptr<LoadedAsset> asset;
        std::unique_ptr<LoadedAsset> standing;
        std::unique_ptr<LoadedAsset> calibration;
        SelectionState history;
        oar::ConditionRandomState conditions;
        ChoiceReason reason = ChoiceReason::e_request;
        bool direct = false;
    };
}

class AnimationController::State
{
public:
    RE::FormID actor_id = 0;
    RE::ActorHandle source_handle;
    std::uint64_t generation = 0;
    std::uint64_t source_generation = 0;
    SourceObservation observed;
    bool has_observation = false;
    IdleTarget target{ WeaponForm::kNone, false, false };
    bool waiting_goal = false;
    ChoiceReason waiting_reason = ChoiceReason::e_request;
    bool base_fallback = false;
    RE::BSTSmartPointer<RE::BShkbAnimationGraph> graph;
    RE::NiAVObject* root = nullptr;
    oar::QuerySession query;
    SelectionState variants;
    dac::Session* session = nullptr;
    std::uint64_t observed_effect_revision = 0;
    EventClock clock;
    std::unique_ptr<Proposal> pending;
    std::unique_ptr<Proposal> active;
    std::unique_ptr<Proposal> outgoing;
    std::unordered_map<std::string, RE::NiAVObject*> nodes;
    std::unordered_map<std::string, RE::NiTransform> pose;
    std::chrono::steady_clock::time_point last_tick{};
    double base_seconds = 0.0;
    double blend_seconds = 0.0;
    double blend_duration = 0.0;
    double outgoing_seconds = 0.0;
    double handoff_seconds = 0.0;
    bool idle_due = false;

    oar::AnimationSelection select(RE::Actor& source, std::span<std::string_view const> bases,
        oar::PreviewState preview, Proposal& proposal, std::uint32_t channel, bool activation)
    {
        std::swap(query.conditions, proposal.conditions);
        oar::AnimationSelection result = oar::query_animation(source, graph->characterInstance, bases,
            std::move(preview), query, { channel });
        std::swap(query.conditions, proposal.conditions);
        if (activation) proposal.history.activate(result.clip_key);
        // An interrupt checks replacement eligibility, not the next member of an accepted sequence.
        if (active && (proposal.reason == ChoiceReason::e_interrupt || proposal.reason == ChoiceReason::e_refresh) &&
            result.replacement_key == active->stage.clip.replacement_key && result.base_index == active->stage.clip.base_index)
        {
            result.variant = active->stage.clip.variant;
            result.index = active->stage.clip.index;
            result.name = active->stage.clip.name;
        }
        else if (result.variants)
        {
            if (std::optional<oar::VariantPick> pick = proposal.history.variant(*result.variants, result.clip_key))
            {
                result.variant = *pick;
                result.index = pick->index;
                if (pick->position < result.variants->sequence.size()) result.name = result.variants->sequence[pick->position].path;
                else
                {
                    const auto at = std::ranges::find(result.variants->random, pick->index);
                    result.name = result.variants->random_paths[static_cast<std::size_t>(at - result.variants->random.begin())];
                }
            }
            else
            {
                result = direct_selection(result, result.base_index, result.base_name);
                result.replaced = false;
                result.replacement_key = 0;
            }
        }
        const std::span<std::string const> names = oar::animation_names(graph->characterInstance, query);
        if (result.index < names.size()) result.binding_name = names[result.index];
        return result;
    }

    void queue(RE::Actor& source, std::vector<std::string_view> bases, ActionPrototype action,
        oar::PreviewState preview, ChoiceReason reason, bool direct = false,
        bool activation = false, Proposal* reusable = nullptr)
    {
        auto proposal = std::make_unique<Proposal>();
        proposal->generation = generation;
        proposal->effect_revision = session->revision();
        proposal->history = variants;
        proposal->conditions = query.conditions;
        if (activation) proposal->conditions.reset();
        proposal->reason = reason;
        proposal->direct = direct;
        const bool continuing_sequence = reason == ChoiceReason::e_boundary && active && active->stage.clip.variants &&
            variants.in_sequence(*active->stage.clip.variants, active->stage.clip.clip_key);
        if (reason == ChoiceReason::e_boundary && active && !continuing_sequence)
        {
            proposal->conditions.loop(active->stage.clip.clip_key);
            proposal->history.loop(active->stage.clip.clip_key);
        }
        const std::uint32_t channel = 2;
        oar::AnimationSelection selected;
        const bool native_boundary = reason == ChoiceReason::e_boundary && active && active->stage.action.loop &&
            (!active->stage.clip.reevaluate_on_loop || continuing_sequence);
        if (direct)
        {
            const auto names = oar::animation_names(graph->characterInstance, query);
            const std::optional<std::uint16_t> base = oar::find_base_animation(names, bases);
            if (base)
            {
                selected.found = true;
                selected.base_index = selected.index = *base;
                selected.base_name = selected.name = selected.binding_name = names[*base];
            }
        }
        else if (native_boundary)
        {
            selected = active->stage.clip;
            if (selected.variants)
                if (std::optional<oar::VariantPick> pick = proposal->history.variant(*selected.variants, selected.clip_key))
                {
                    selected.variant = *pick;
                    selected.index = pick->index;
                    if (pick->position < selected.variants->sequence.size()) selected.name = selected.variants->sequence[pick->position].path;
                    else
                    {
                        const auto at = std::ranges::find(selected.variants->random, pick->index);
                        selected.name = selected.variants->random_paths[static_cast<std::size_t>(at - selected.variants->random.begin())];
                    }
                }
        }
        else selected = select(source, bases, preview, *proposal, channel, activation);
        const auto binding_names = oar::animation_names(graph->characterInstance, query);
        if (selected.index < binding_names.size()) selected.binding_name = binding_names[selected.index];
        if (target.empty() && !direct && action.kind != ActionKind::e_unequip && (!selected.found || !selected.replaced))
        {
            base_fallback = true;
            logger::warn("ANIMATION actor={:08x} request={} unarmed replacement unavailable; direct MT_Idle fallback", actor_id, generation);
            queue(source, { "Animations\\MT_Idle.hkx" }, { ActionKind::e_standing_idle, WeaponForm::kNone, true },
                preview_state(false), reason, true, activation, reusable);
            return;
        }
        if (!selected.found)
        {
            logger::warn("ANIMATION actor={:08x} request={} idle base/query unavailable diagnostic='{}'", actor_id, generation, selected.diagnostic);
            // A missing family base must not blank the panel: the previous behaviour returned
            // without a proposal, so `ready()` stayed false and the studio draw never composited.
            // Fall back to the standing base (one level only; direct avoids a recursion loop).
            if (!direct)
                queue(source, { "Animations\\MT_Idle.hkx" }, { ActionKind::e_standing_idle, WeaponForm::kNone, true },
                    preview_state(false), reason, true, activation, reusable);
            return;
        }
        proposal->stage = { action, selected };
        if (active && reason != ChoiceReason::e_request && active->stage.action.kind == action.kind && active->direct == direct &&
            selected.index == active->stage.clip.index && selected.replacement_key == active->stage.clip.replacement_key)
        {
            // Reaching a boundary accepts native history even when the next member is identical;
            // an interrupt against the same replacement leaves accepted history untouched.
            if (reason == ChoiceReason::e_refresh || reason == ChoiceReason::e_interrupt)
            {
                query.conditions = std::move(proposal->conditions);
                active->generation = generation;
                active->stage.clip = selected;
                active->asset->refresh_selection(selected);
            }
            if (reason == ChoiceReason::e_boundary)
            {
                variants = std::move(proposal->history);
                query.conditions = std::move(proposal->conditions);
                if (selected.variants) variants.commit(*selected.variants, selected.variant, selected.clip_key);
                active->stage.clip = std::move(selected);
            }
            active->generation = generation;
            idle_due = false;
            pending.reset();
            return;
        }
        if (reusable && reusable->stage.action.kind == action.kind &&
            reusable->asset->selection().index == selected.index && reusable->stage.clip.replacement_key == selected.replacement_key)
        {
            proposal->asset = std::move(reusable->asset);
            proposal->asset->refresh_selection(selected);
            proposal->standing = std::move(reusable->standing);
            proposal->calibration = std::move(reusable->calibration);
            pending = std::move(proposal);
            return;
        }
        const std::array<std::string_view, 1> standing_bases{ "Animations\\MT_Idle.hkx" };
        const auto names = oar::animation_names(graph->characterInstance, query);
        const std::optional<std::uint16_t> base = oar::find_base_animation(names, standing_bases);
        if (!base) return;
        oar::AnimationSelection standard;
        standard.found = true;
        standard.base_index = standard.index = *base;
        standard.base_name = standard.name = standard.binding_name = names[*base];
        proposal->standing = std::make_unique<LoadedAsset>(graph, standard, true, true, generation);
        proposal->calibration = std::make_unique<LoadedAsset>(graph, standard, true, true, generation);
        proposal->asset = std::make_unique<LoadedAsset>(graph, selected, action.loop,
            action.kind == ActionKind::e_standing_idle, generation,
            target.empty() && !direct ? std::optional<oar::AnimationSelection>(standard) : std::nullopt);
        logger::info("ANIMATION actor={:08x} request={} state={} choice={} path='{}' index={} diagnostic='{}'",
            actor_id, generation, session->revision(), static_cast<int>(reason), selected.name, selected.index, selected.diagnostic);
        pending = std::move(proposal);
    }

    void queue_idle(RE::Actor& source, ChoiceReason reason, Proposal* reusable = nullptr)
    {
        bool direct = target.direct || base_fallback;
        if (!direct)
        {
            const dac::IdleReadiness readiness = session->idle_readiness(generation);
            if (readiness == dac::IdleReadiness::e_pending)
            { waiting_goal = true; waiting_reason = reason; return; }
            // e_none means no preparation was requested for this generation (the equipment-
            // change follow-up deliberately lets the equip clip's annotations set the phase);
            // query now instead of waiting for a preparation that will never start.
            if (readiness == dac::IdleReadiness::e_failed)
            {
                // Native preparation is unavailable. Query OAR against the source's own effects
                // anyway: a real replacement is still the authored idle, and the query itself
                // falls back to the base when its conditions do not hold. The old path forced
                // `direct` here, which skipped the query entirely and always showed vanilla.
                logger::warn("ANIMATION actor={:08x} request={} native idle goal unavailable; querying OAR without preparation", actor_id, generation);
            }
        }
        waiting_goal = false;
        std::vector<std::string_view> bases = target.direct || (direct && target.empty()) ?
            std::vector<std::string_view>{ "Animations\\MT_Idle.hkx" } : target.bases();
        const bool plain = target.direct || (direct && target.empty());
        logger::info("ANIMATION actor={:08x} request={} goal=non-combat-idle family={} direct={} drawn={}",
            actor_id, generation, weapon_form_name(target.form), direct, !plain);
        queue(source, std::move(bases), { plain ? ActionKind::e_standing_idle : ActionKind::e_weapon_idle,
            target.form, true }, preview_state(!plain), reason, direct, reason == ChoiceReason::e_request, reusable);
    }


};

AnimationController::AnimationController() : m_state(std::make_unique<State>()) {}
AnimationController::~AnimationController() = default;
void AnimationController::reset()
{
    if (m_state->actor_id) logger::info("ANIMATION actor={:08x} request={} lifetime reset", m_state->actor_id, m_state->generation);
    m_state = std::make_unique<State>();
}

void AnimationController::request(RE::Actor& source, SourceObservation const& observation, dac::Session& session)
{
    session.request(observation.request_generation);
    RE::BSTSmartPointer<RE::BShkbAnimationGraph> graph = source_graph(source);
    State& old = *m_state;
    const bool new_lifetime = old.actor_id != source.GetFormID() || old.graph.get() != graph.get() ||
        old.source_generation != observation.source_graph_generation;
    if (new_lifetime)
    {
        RE::NiAVObject* root = old.root;
        reset();
        m_state->root = root;
        if (root) { m_state->root = nullptr; retarget(*root); }
    }
    State& state = *m_state;
    state.session = &session;
    const IdleTarget target = idle_target(observation.form, observation.left_primary, unarmed_idle_setting());
    const bool equipment_changed = !state.has_observation || state.observed.hands != observation.hands || state.target != target;
    const WeaponForm previous_form = state.has_observation ? state.observed.form : WeaponForm::kNone;
    state.actor_id = source.GetFormID();
    state.source_handle = source.GetHandle();
    state.generation = observation.request_generation;
    state.source_generation = observation.source_graph_generation;
    state.observed = observation;
    state.target = target;
    if (equipment_changed) state.base_fallback = false;
    state.has_observation = true;
    state.graph = std::move(graph);
    if (!state.graph) { state.pending.reset(); return; }
    logger::info("ANIMATION actor={:08x} request={} reason='{}' source-generation={} equipment-target={}", state.actor_id,
        state.generation, observation.reason, state.source_generation, weapon_form_name(target.form));
    // Panel-open forces the observed loop phase; an equipment change must NOT, or the equip
    // clip's own Start markers (cast by its annotations) are overwritten by loop markers before
    // the Start member can be selected, which skips the world's Equip -> Start -> Loop order.
    if (!target.direct && !state.base_fallback && observation.reason != "equipment")
        session.prepare_idle(state.generation, !equipment_changed && !new_lifetime);
    if (!equipment_changed && state.active) state.active->generation = state.generation;
    std::unique_ptr<Proposal> loading = std::move(state.pending);
    state.idle_due = false;

    // An in-inventory equipment change plays the world's draw/sheathe move once, then hands
    // over to the idle (the boundary in advance() queues the idle). Opening the panel keeps the
    // immediate-idle behaviour: there is no world transition to mirror at that edge.
    if (equipment_changed && observation.reason == "equipment")
    {
        const bool unequip = target.empty();
        const WeaponForm form = unequip ? previous_form : target.form;
        const std::vector<std::string_view> bases = panel_transition_bases(form, unequip);
        const auto names = oar::animation_names(state.graph->characterInstance, state.query);
        if (!bases.empty() && oar::find_base_animation(names, bases))
        {
            logger::info("ANIMATION actor={:08x} request={} transition={} form={}", state.actor_id, state.generation,
                unequip ? "unequip" : "equip", weapon_form_name(form));
            state.queue(source, bases, { unequip ? ActionKind::e_unequip : ActionKind::e_equip, form, false },
                preview_state(!unequip), ChoiceReason::e_request, false, false, loading.get());
            return;
        }
        logger::warn("ANIMATION actor={:08x} request={} transition base unavailable form={}; idle only", state.actor_id,
            state.generation, weapon_form_name(form));
    }
    state.queue_idle(source, equipment_changed ? ChoiceReason::e_request : ChoiceReason::e_refresh, loading.get());
    if (state.waiting_goal) state.pending = std::move(loading);
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
    for (Proposal* proposal : { state.active.get(), state.pending.get(), state.outgoing.get() })
        if (proposal)
        {
            proposal->standing->rebind(root);
            if (proposal->calibration) proposal->calibration->rebind(root);
            proposal->asset->rebind(root);
        }
    state.last_tick = std::chrono::steady_clock::now();
    logger::info("ANIMATION actor={:08x} request={} graph retarget={} time={:.2f}", state.actor_id,
        state.generation, static_cast<void*>(&root), state.clock.seconds());
}

void AnimationController::poll()
{
    State& state = *m_state;
    if (state.session)
    {
        state.session->observe();
        const std::uint64_t revision = state.session->revision();
        if (revision != state.observed_effect_revision)
        {
            state.observed_effect_revision = revision;
            if (state.active && state.active->stage.action.loop && state.active->stage.clip.interruptible) state.idle_due = true;
        }
    }
    if (state.waiting_goal)
    {
        const auto source = state.source_handle.get();
        if (source) state.queue_idle(*source, state.waiting_reason, state.pending.get());
        if (state.waiting_goal) return;
    }
    if (!state.root || !state.pending) return;
    if (state.pending->generation != state.generation) { state.pending.reset(); return; }
    if (state.pending->effect_revision != state.session->revision())
    {
        const auto source = state.source_handle.get();
        if (!source) { state.pending.reset(); return; }
        // Rebuild the exact typed intent against the new snapshot; accepted playback continues.
        std::unique_ptr<Proposal> old = std::move(state.pending);
        state.queue_idle(*source, old->reason, old.get());
        return;
    }
    Proposal& proposal = *state.pending;
    proposal.standing->poll(*state.root);
    if (proposal.calibration) proposal.calibration->poll(*state.root);
    proposal.asset->poll(*state.root);
    if (proposal.standing->failed() || proposal.asset->failed())
    {
        logger::warn("ANIMATION actor={:08x} request={} load failed path='{}'; visible playback retained", state.actor_id,
            state.generation, proposal.stage.clip.name);
        state.pending.reset();
        return;
    }
    if (proposal.calibration && proposal.calibration->failed()) proposal.calibration.reset();
    if (!proposal.standing->ready() || !proposal.asset->ready()) return;
    // Loader fallback metadata describes the actual retained binding, not the rejected choice.
    // A one-shot transition must not pin the idle to the base: its fallback is about that move,
    // while the following idle still deserves the query.
    if (proposal.stage.action.kind != ActionKind::e_equip && proposal.stage.action.kind != ActionKind::e_unequip &&
        proposal.stage.clip.replaced && !proposal.asset->selection().replaced) state.base_fallback = true;
    proposal.stage.clip = proposal.asset->selection();
    if (state.base_fallback || (state.target.empty() && !proposal.stage.clip.replaced)) proposal.direct = true;
    state.blend_duration = 0.0;
    if (state.active)
    {
        const auto& current = state.active->stage.clip;
        state.blend_duration = proposal.reason == ChoiceReason::e_boundary ? current.loop_blend : current.interrupt_blend;
        if (proposal.reason == ChoiceReason::e_boundary && current.variants && !current.variants->blend_between &&
            current.replacement_key == proposal.stage.clip.replacement_key) state.blend_duration = 0.0;
    }
    state.outgoing_seconds = state.clock.seconds();
    state.outgoing = std::move(state.active);
    state.active = std::move(state.pending);
    state.variants = std::move(state.active->history);
    state.query.conditions = std::move(state.active->conditions);
    const auto& selection = state.active->stage.clip;
    if (selection.variants) state.variants.commit(*selection.variants, selection.variant, selection.clip_key);
    const ClipPlayback& clip = state.active->asset->playback();
    auto annotations = clip_annotations(*clip.animation);
    state.handoff_seconds = std::max(static_cast<double>(clip.duration), 0.05) * 0.5;
    for (const Annotation& annotation : annotations)
        if (annotation.event == "weaponDraw" || annotation.event == "weaponSheathe")
        { state.handoff_seconds = annotation.time; break; }
    state.clock.start(std::move(annotations), std::max(static_cast<double>(clip.duration), 0.05),
        0.0, false);
    state.blend_seconds = 0.0;
    state.idle_due = false;
    state.last_tick = std::chrono::steady_clock::now();
    if (state.blend_duration == 0.0) state.outgoing.reset();
    logger::info("ANIMATION actor={:08x} request={} handover action={} path='{}' index={} time={:.3f} blend={:.3f}",
        state.actor_id, state.generation, static_cast<int>(state.active->stage.action.kind), selection.name, selection.index,
        state.clock.seconds(), state.blend_duration);
}

bool AnimationController::ready() const { return m_state->active && m_state->active->asset->ready(); }
std::string AnimationController::name() const { return ready() ? m_state->active->stage.clip.name : "animation-loading"; }

bool AnimationController::weapon_sheathed() const
{
    State const& state = *m_state;
    if (!state.active) return false;
    const ActionKind kind = state.active->stage.action.kind;
    if (kind == ActionKind::e_equip) return state.clock.seconds() < state.handoff_seconds;
    if (kind == ActionKind::e_unequip) return state.clock.seconds() >= state.handoff_seconds;
    return false;
}

bool AnimationController::apply_standing_pose(RE::NiAVObject& root)
{
    State& state = *m_state;
    if (!ready() || state.root != &root || state.nodes.empty()) return false;
    LoadedAsset* standard = state.active->calibration ? state.active->calibration.get() : state.active->standing.get();
    if (!standard || !standard->ready()) return false;
    const PoseBuffer base = sample_animation_clip(standard->playback(), 0.0f, true);
    if (base.empty()) return false;
    for (auto const& [name, local] : base)
        if (auto target = state.nodes.find(name); target != state.nodes.end()) target->second->local = local;
    recompute_subtree_worlds(root, 0);
    return true;
}

std::size_t AnimationController::advance(RE::NiAVObject& root)
{
    State& state = *m_state;
    if (!ready() || state.root != &root) return 0;
    const auto now = std::chrono::steady_clock::now();
    const double delta = std::clamp(std::chrono::duration<double>(now - state.last_tick).count(), 0.0, 0.1);
    state.last_tick = now;
    const auto source = state.source_handle.get();
    ChoiceReason reason = ChoiceReason::e_interrupt;
    state.clock.advance(delta, state.active->stage.action.loop,
        [](double) {},
        [&](Annotation const& annotation)
        {
            logger::info("ANIMATION actor={:08x} request={} event='{}' time={:.3f}", state.actor_id, state.generation, annotation.event, annotation.time);
            if (!state.active->direct) state.session->dispatch(annotation.event, state.active->generation);
        },
        [&]() { state.idle_due = true; reason = ChoiceReason::e_boundary; });
    if (state.idle_due && source && !state.pending && !state.waiting_goal)
    {
        state.queue_idle(*source, reason);
        state.idle_due = false;
    }
    state.base_seconds += delta;
    state.pose.clear();
    const PoseBuffer base = sample_animation_clip(state.active->standing->playback(), static_cast<float>(state.base_seconds), true);
    compose_contributor(state.pose, std::span(base), BoneMask::e_full_body);
    const PoseBuffer current = sample_animation_clip(state.active->asset->playback(), static_cast<float>(state.clock.seconds()), state.active->stage.action.loop);
    compose_contributor(state.pose, std::span(current), BoneMask::e_full_body);
    if (state.outgoing)
    {
        state.outgoing_seconds += delta;
        state.blend_seconds += delta;
        const float weight = static_cast<float>(std::min(state.blend_seconds / state.blend_duration, 1.0));
        const PoseBuffer before = sample_animation_clip(state.outgoing->asset->playback(), static_cast<float>(state.outgoing_seconds), state.outgoing->stage.action.loop);
        for (auto const& [name, local] : before)
            if (auto incoming = state.pose.find(name); incoming != state.pose.end()) incoming->second = blend_panel_pose(local, incoming->second, weight);
        if (weight >= 1.0f) state.outgoing.reset();
    }
    for (auto const& [name, local] : state.pose)
        if (auto target = state.nodes.find(name); target != state.nodes.end()) target->second->local = local;
    recompute_subtree_worlds(root, 0);
    return state.pose.size();
}
}
PLUGIN_NAMESPACE_END
