//
// Created by AmazingBuff on 2026/10/16.
//

#include "animation/controller.h"

#include "animation/clip.h"
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
            if (m_selection.index != m_selection.base_index)
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
        bool resume = false;
        std::optional<std::array<std::uint32_t, 2>> hands;
        std::vector<std::string> bases;
        oar::PreviewState preview;
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
        oar::PreviewState preview, ChoiceReason reason, double start = 0.0, bool resume = false,
        bool activation = false, Proposal* reusable = nullptr)
    {
        auto proposal = std::make_unique<Proposal>();
        proposal->generation = generation;
        proposal->effect_revision = session->revision();
        proposal->history = variants;
        proposal->conditions = query.conditions;
        if (activation) proposal->conditions.reset();
        proposal->reason = reason;
        proposal->resume = resume;
        proposal->hands = preview.hand_override;
        proposal->preview = preview;
        for (std::string_view base : bases) proposal->bases.emplace_back(base);
        const bool continuing_sequence = reason == ChoiceReason::e_boundary && active && active->stage.clip.variants &&
            variants.in_sequence(*active->stage.clip.variants, active->stage.clip.clip_key);
        if (reason == ChoiceReason::e_boundary && active && !continuing_sequence)
        {
            proposal->conditions.loop(active->stage.clip.clip_key);
            proposal->history.loop(active->stage.clip.clip_key);
        }
        const std::uint32_t channel = action.kind == ActionKind::e_equip || action.kind == ActionKind::e_unequip ? 4 : 2;
        oar::AnimationSelection selected;
        const bool native_boundary = reason == ChoiceReason::e_boundary && active && active->stage.action.loop &&
            (!active->stage.clip.reevaluate_on_loop || continuing_sequence);
        if (native_boundary)
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
        if (!selected.found) return;
        if (resume && observed.clip_index != 0xFFFF)
        {
            // Match copied descriptors, including actual native variant membership, so a source
            // phase resume retains the selected replacement's interrupt/loop policy.
            bool matched = false;
            for (oar::AnimationDescriptor const& descriptor : selected.candidates)
            {
                const bool variant = descriptor.variants &&
                    (std::ranges::any_of(descriptor.variants->sequence, [&](oar::VariantEntry const& value) { return value.index == observed.clip_index; }) ||
                     std::ranges::find(descriptor.variants->random, observed.clip_index) != descriptor.variants->random.end());
                if (descriptor.index != observed.clip_index && !variant) continue;
                matched = true;
                selected.replacement_key = descriptor.replacement_key;
                selected.replaced = true;
                selected.interruptible = descriptor.interruptible;
                selected.interrupt_blend = descriptor.interrupt_blend;
                selected.loop_blend = descriptor.loop_blend;
                selected.reevaluate_on_loop = descriptor.reevaluate_on_loop;
                selected.variants = descriptor.variants;
                selected.name = descriptor.path;
                if (selected.variants)
                {
                    const auto& sequence = selected.variants->sequence;
                    const auto member = std::ranges::find_if(sequence, [&](oar::VariantEntry const& value) { return value.index == observed.clip_index; });
                    if (member != sequence.end())
                    {
                        selected.variant = { member->index, static_cast<std::size_t>(member - sequence.begin()) };
                        selected.name = member->path;
                        for (std::size_t i = 0; i < selected.variant.position; ++i)
                            proposal->history.commit(*selected.variants, { sequence[i].index, i }, selected.clip_key);
                    }
                    else
                    {
                        const auto at = std::ranges::find(selected.variants->random, observed.clip_index);
                        selected.variant = { observed.clip_index };
                        if (at != selected.variants->random.end()) selected.name = selected.variants->random_paths[static_cast<std::size_t>(at - selected.variants->random.begin())];
                    }
                }
                break;
            }
            if (!matched)
            {
                selected = direct_selection(selected, observed.clip_index, observed.clip_name);
                selected.replaced = false;
            }
            const auto names = oar::animation_names(graph->characterInstance, query);
            if (observed.clip_index < names.size())
            {
                selected.index = observed.clip_index;
                selected.binding_name = names[selected.index];
                // Source index can be the base or a replacement not admitted by our audited
                // query. Its binding remains usable, but no unmatched replacement policy leaks.
                if (!selected.replaced) selected.name = selected.binding_name;
            }
        }
        const auto binding_names = oar::animation_names(graph->characterInstance, query);
        if (selected.index < binding_names.size()) selected.binding_name = binding_names[selected.index];
        proposal->stage = { action, selected, start };
        if (active && reason != ChoiceReason::e_request && active->stage.action.kind == action.kind &&
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
            proposal->stage.start_seconds = reusable->stage.start_seconds;
            proposal->resume = reusable->resume;
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
            action.kind == ActionKind::e_standing_idle, generation, panel_weapon_node(action.form));
        logger::info("ANIMATION actor={:08x} request={} state={} choice={} path='{}' index={} diagnostic='{}'",
            actor_id, generation, session->revision(), static_cast<int>(reason), selected.name, selected.index, selected.diagnostic);
        pending = std::move(proposal);
    }

    void queue_idle(RE::Actor& source, ChoiceReason reason)
    {
        std::vector<std::string_view> bases = observed.drawn ? panel_idle_bases(observed.form) : std::vector<std::string_view>{ "Animations\\MT_Idle.hkx" };
        if (observed.left_primary && bases.size() > 1) std::swap(bases[0], bases[1]);
        queue(source, std::move(bases), { observed.drawn ? ActionKind::e_weapon_idle : ActionKind::e_standing_idle,
            observed.form, true }, preview_state(observed.drawn), reason, 0.0, false,
            active && !active->stage.action.loop);
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
    const SourceObservation previous = state.observed;
    const bool weapon_changed = state.has_observation && (previous.hands != observation.hands ||
        previous.form != observation.form || previous.drawn != observation.drawn);
    state.actor_id = source.GetFormID();
    state.source_handle = source.GetHandle();
    state.generation = observation.request_generation;
    state.source_generation = observation.source_graph_generation;
    state.observed = observation;
    state.has_observation = true;
    state.graph = std::move(graph);
    if (!state.graph) { state.pending.reset(); return; }
    logger::info("ANIMATION actor={:08x} request={} reason='{}' source-generation={}", state.actor_id,
        state.generation, observation.reason, state.source_generation);
    if (!weapon_changed && state.active)
    {
        state.active->generation = state.generation;
        std::unique_ptr<Proposal> loading = std::move(state.pending);
        Proposal const& intent = loading ? *loading : *state.active;
        const ActionPrototype action = intent.stage.action;
        const oar::PreviewState preview = intent.preview;
        std::vector<std::string> owned_bases = intent.bases;
        std::vector<std::string_view> bases;
        for (std::string const& base : owned_bases) bases.push_back(base);
        state.queue(source, std::move(bases), action, preview, loading ? loading->reason : ChoiceReason::e_refresh,
            loading ? loading->stage.start_seconds : 0.0, loading && loading->resume,
            loading && loading->reason == ChoiceReason::e_request, loading.get());
        logger::info("ANIMATION actor={:08x} request={} appearance requery keeps local clock/effects until acceptance", state.actor_id, state.generation);
        return;
    }
    state.pending.reset();
    state.idle_due = false;
    const EntryActionKind entry = entry_action_kind(observation.clip_name);
    if (observation.reason == "panel-open" && observation.clip_index != 0xFFFF &&
        (entry != EntryActionKind::e_none || panel_clip_is_family(observation.clip_name, observation.form)))
    {
        const bool transition = entry != EntryActionKind::e_none;
        std::vector<std::string_view> bases = transition ? panel_transition_bases(observation.form, entry == EntryActionKind::e_unequip) :
            (observation.drawn ? panel_idle_bases(observation.form) : std::vector<std::string_view>{ "Animations\\MT_Idle.hkx" });
        const ActionKind kind = entry == EntryActionKind::e_equip ? ActionKind::e_equip : entry == EntryActionKind::e_unequip ?
            ActionKind::e_unequip : observation.drawn ? ActionKind::e_weapon_idle : ActionKind::e_standing_idle;
        state.queue(source, std::move(bases), { kind, observation.form, !transition },
            preview_state(transition || observation.drawn, entry == EntryActionKind::e_equip, entry == EntryActionKind::e_unequip),
            ChoiceReason::e_request, observation.clip_time, true, true);
    }
    else if (observation.reason == "equipment" && weapon_changed)
    {
        const bool unequip = !observation.drawn || observation.form == WeaponForm::kNone;
        const WeaponForm form = unequip ? previous.form : observation.form;
        std::vector<std::string_view> bases = panel_transition_bases(form, unequip);
        if (unequip ? previous.left_primary : observation.left_primary)
        {
            if (form == WeaponForm::kMagic && bases.size() > 1) std::swap(bases[0], bases[1]);
            if (form == WeaponForm::kStaff) bases.front() = unequip ? "Animations\\StaffLeft_Unequip.hkx" : "Animations\\StaffLeft_Equip.hkx";
        }
        oar::PreviewState preview = preview_state(true, !unequip, unequip);
        if (unequip) preview.hand_override = previous.raw_hands;
        state.queue(source, std::move(bases), { unequip ? ActionKind::e_unequip : ActionKind::e_equip, form, false },
            std::move(preview), ChoiceReason::e_request, 0.0, false, true);
    }
    else state.queue_idle(source, ChoiceReason::e_request);
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
    if (!state.root || !state.pending) return;
    if (state.pending->generation != state.generation) { state.pending.reset(); return; }
    if (state.pending->reason == ChoiceReason::e_refresh && state.active && !state.active->stage.action.loop && state.clock.finished())
    {
        const auto source = state.source_handle.get();
        if (source) state.queue_idle(*source, ChoiceReason::e_boundary);
        else state.pending.reset();
        return;
    }
    if (state.pending->effect_revision != state.session->revision())
    {
        const auto source = state.source_handle.get();
        if (!source) { state.pending.reset(); return; }
        // Rebuild the exact typed intent against the new snapshot; accepted playback continues.
        std::unique_ptr<Proposal> old = std::move(state.pending);
        std::vector<std::string_view> bases;
        for (std::string const& base : old->bases) bases.push_back(base);
        state.queue(*source, std::move(bases), old->stage.action, old->preview, old->reason,
            old->stage.start_seconds, old->resume, old->reason == ChoiceReason::e_request, old.get());
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
    proposal.stage.clip = proposal.asset->selection();
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
    state.clock.start(clip_annotations(*clip.animation), std::max(static_cast<double>(clip.duration), 0.05),
        state.active->stage.start_seconds, state.active->resume);
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
            state.session->dispatch(annotation.event, state.active->generation);
        },
        [&]() { state.idle_due = true; reason = ChoiceReason::e_boundary; });
    if (state.idle_due && source && !state.pending)
    {
        state.queue_idle(*source, reason);
        state.idle_due = false;
    }
    state.base_seconds += delta;
    state.pose.clear();
    const PoseBuffer base = sample_animation_clip(state.active->standing->playback(), static_cast<float>(state.base_seconds), true);
    compose_contributor(state.pose, std::span(base), BoneMask::e_full_body);
    const PoseBuffer current = sample_animation_clip(state.active->asset->playback(), static_cast<float>(state.clock.seconds()), state.active->stage.action.loop);
    const BoneMask mask = state.active->stage.action.kind == ActionKind::e_hand_idle ? BoneMask::e_left_hand : BoneMask::e_full_body;
    compose_contributor(state.pose, std::span(current), mask);
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
