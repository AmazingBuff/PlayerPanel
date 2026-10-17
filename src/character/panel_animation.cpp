//
// Created by AmazingBuff on 2026/10/16.
//

#include "character/panel_animation.h"

#include "character/animation_source.h"
#include "character/equipment_mirror.h"
#include "character/panel_animation_policy.h"
#include "oar/hooks.h"
#include "oar/preview_clip.h"
#include "panel/panel.h"

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

    struct EquipmentState
    {
        RE::FormID right;
        RE::FormID left;
        oar::WeaponForm form;
        bool drawn;
        RE::FormID raw_right;
        RE::FormID raw_left;
        bool operator==(EquipmentState const& other) const
        {
            return right == other.right && left == other.left && form == other.form && drawn == other.drawn;
        }
    };

    EquipmentState equipment_state(RE::Actor& actor)
    {
        const std::array<RE::FormID, 2> hands = equipment_hand_ids(actor);
        RE::TESForm const* const right = actor.GetEquippedObject(false);
        RE::TESForm const* const left = actor.GetEquippedObject(true);
        return { hands[0], hands[1], equipment_weapon_form(actor), actor.IsWeaponDrawn(),
            right ? right->GetFormID() : 0, left ? left->GetFormID() : 0 };
    }

    // The panel figure must answer for the stance the player ENTERED the inventory with, not the
    // menu-time stance: the engine sheathes the weapon while the menu is up, so a live read says
    // drawn=false for the whole session even when the player walked in with the sword out
    // (oar-idle2 report, problem 2: 168 queries, drawn=false in all of them, and no weapon idle
    // ever queried). The menu sink samples the entering stance on the rising edge; while that
    // snapshot is valid the panel keeps it as the drawn answer. Any equip/unequip event bumps the
    // revision — the caller treats a revision newer than the open edge as "the player acted inside
    // the menu" and falls back to the live read.
    // The panel figure must answer for the stance the player ENTERED the inventory with, not the
    // menu-time stance: the engine freezes weaponState while the menu is up, so a live read says
    // drawn=false for a player who walked in mid-combat with a weapon out (oar-idle2 report,
    // problem 2). oar-idle3 report refinement: the menu-time weaponState is frozen EVEN for a
    // player who equips a weapon inside the menu, so the drawn answer needs three inputs —
    //   1. the entering-stance snapshot (weaponState sampled on the menu-open rising edge),
    //   2. combat: a combat player holding a weapon shows the weapon stance even if the
    //      weaponState bit says sheathed (combat, "武器在手即持械"; empty hands + combat stays
    //      the unarmed fighting idle through the form rule below),
    //   3. the live read, which still wins for an explicit sheathe/draw the engine did process.
    // The user's definition, final form (three in-game rounds proved every weaponState-based
    // read loses to the engine's menu freeze): "holding a weapon" is decided by the HAND SLOT,
    // not by any drawn bit. A player who carries a weapon in a hand shows that weapon family's
    // stance; empty hands show the plain standing idle (or the h2h family while the unarmed
    // bases are present). weaponState and its frame-observed ghost are retired from the rule.
    bool panel_drawn_state(RE::Actor& actor, oar::WeaponForm form)
    {
        (void)actor;
        return form != oar::WeaponForm::kNone;
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
        LoadedAsset(RE::BSTSmartPointer<RE::BShkbAnimationGraph> graph, oar::AnimationSelection selection, bool loop, bool full_body) :
            m_graph(std::move(graph)), m_selection(std::move(selection)), m_loop(loop), m_full_body(full_body)
        {
            m_behavior = m_graph->characterInstance.behaviorGraph;
            queue();
        }
        ~LoadedAsset() { release(); }

        void poll(RE::NiAVObject& root)
        {
            if (m_failed || m_player.ready() || !m_queued)
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
                        m_player.set_clip(m_playback, m_loop);
                        logger::info("SCOPY PANEL asset ready index={} base={} replaced={} name='{}' mapping={} {}",
                            m_selection.index, m_selection.base_index, m_selection.replaced,
                            m_selection.name, mapping, coverage);
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
                m_player.retarget(m_playback);
            }
            else
            {
                // Never retain node pointers into the retired copy after a failed retarget.
                m_player.reset();
                m_playback = {};
            }
        }

        [[nodiscard]] bool ready() const { return m_player.ready(); }
        [[nodiscard]] bool failed() const { return m_failed; }
        [[nodiscard]] bool finished() const { return m_player.finished(); }
        [[nodiscard]] bool loop_boundary(bool eligible) { return m_cycle.crossed(m_player.cycles(), eligible); }
        [[nodiscard]] ClipPlayback const& playback() const { return m_playback; }
        [[nodiscard]] std::string const& name() const { return m_selection.name; }
        [[nodiscard]] oar::AnimationSelection const& selection() const { return m_selection; }
        [[nodiscard]] bool same_choice(oar::AnimationSelection const& choice) const
        {
            return m_selection.clip_key == choice.clip_key && m_selection.index == choice.index &&
                m_selection.name == choice.name && m_selection.replacement_key == choice.replacement_key &&
                m_selection.variant.position == choice.variant.position;
        }
        void accept(oar::QuerySession& session, bool restart = true)
        {
            if (restart)
            {
                m_player.restart();
                m_cycle.reset();
            }
            if (m_selection.replaced && m_selection.variants)
                session.selection.commit(*m_selection.variants, m_selection.variant, m_selection.clip_key);
            else
                session.selection.deactivate(m_selection.clip_key);
        }
        void update_selection(oar::AnimationSelection selection) { m_selection = std::move(selection); }
        std::size_t advance(RE::NiAVObject& root) { return m_player.advance(root); }

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
            m_player.reset();
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
            logger::warn("SCOPY PANEL asset rejected index={} name='{}' reason={}", m_selection.index, m_selection.name, reason);
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
        bool m_queued = false;
        bool m_failed = false;
        std::chrono::steady_clock::time_point m_started{};
        oar::PreviewClip m_carrier;
        ClipPlayback m_playback{};
        ClipPlayer m_player;
        IdleCycleGate m_cycle;
    };

    struct AnimationSet
    {
        std::array<std::unique_ptr<LoadedAsset>, 3> idles;
        std::unique_ptr<LoadedAsset> transition;
        EquipmentState equipment;
        std::uint64_t generation = 0;
    };

    // The panel-aware equipment read: the entering-stance snapshot and combat answer `drawn` (see
    // panel_drawn_state), everything else is live. All equality checks against an AnimationSet's
    // recorded equipment go through this, so the snapshot stance stays self-consistent.
    EquipmentState panel_equipment(RE::Actor& actor)
    {
        EquipmentState state = equipment_state(actor);
        // Hand slot decides the stance: a weapon in a hand = the weapon family's idle; empty
        // hands = the plain standing idle (the kNone drawn answer the transition block may pass
        // for a sheathed swap still queries the h2h/MT bases correctly).
        state.drawn = panel_drawn_state(actor, state.form);
        return state;
    }

}

class PanelAnimation::State
{
public:
    State() : session(static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count())) {}
    std::unique_ptr<AnimationSet> active;
    std::unique_ptr<AnimationSet> pending;
    std::array<std::unique_ptr<LoadedAsset>, 3> idle_pending;
    oar::QuerySession session;
    LatestAnimationRequest latest;
    RE::BSTSmartPointer<RE::BShkbAnimationGraph> graph;
    RE::NiAVObject* root = nullptr;
    std::unordered_map<std::string, RE::NiAVObject*> nodes;
    std::unordered_map<std::string, RE::NiTransform> blend_from;
    std::chrono::steady_clock::time_point blend_start{};
    PanelAnimationPhase phase = PanelAnimationPhase::e_loading;
    bool root_fresh = false;

    void remember_pose()
    {
        blend_from.clear();
        if (!active)
            return;
        for (auto const& [name, node] : nodes)
            blend_from.emplace(name, node->local);
        blend_start = std::chrono::steady_clock::now();
    }

    oar::AnimationSelection select(RE::Actor& source, std::span<std::string_view const> bases,
        oar::PreviewState state, std::uint32_t channel, LoadedAsset const* current, oar::QueryEvent event)
    {
        oar::AnimationSelection selected = oar::query_animation(source, graph->characterInstance, bases,
            std::move(state), session, { channel, event, current ? &current->selection() : nullptr });
        logger::info("SCOPY PANEL query channel={} generation={} project='{}' base={} index={} replaced={} evaluated={} overridden={} rejected={} name='{}' diagnostic='{}'",
            channel, latest.generation(),
            graph->projectName.c_str(), selected.base_index, selected.index, selected.replaced,
            selected.evaluated, selected.overrides, selected.rejected, selected.name, selected.diagnostic);
        return selected;
    }

    std::unique_ptr<LoadedAsset> asset(RE::Actor& source, std::span<std::string_view const> bases,
        oar::PreviewState state, bool loop, bool full_body, std::uint32_t channel,
        LoadedAsset const* current = nullptr, oar::QueryEvent event = oar::QueryEvent::e_refresh)
    {
        oar::AnimationSelection selected = select(source, bases, std::move(state), channel, current, event);
        return selected.found ? std::make_unique<LoadedAsset>(graph, std::move(selected), loop, full_body) : nullptr;
    }

    LoadedAsset const* idle(std::size_t index) const { return active ? active->idles[index].get() : nullptr; }

    void observe(RE::Actor& source, std::uint64_t revision)
    {
        if (!latest.observe(revision))
            return;
        pending.reset();
        for (auto& next : idle_pending)
            next.reset();
        logger::info("SCOPY PANEL equipment target generation={} (older pending requests cancelled)", latest.generation());
        if (graph && root)
            request(source, true);
    }

    void request(RE::Actor& source, bool changed)
    {
        if (!graph)
            return;
        EquipmentState desired = panel_equipment(source);
        auto next = std::make_unique<AnimationSet>();
        next->equipment = desired;
        next->generation = latest.generation();
        const std::array<std::string_view, 1> body{ "Animations\\MT_Idle.hkx" };
        next->idles[0] = asset(source, body, preview_state(desired.drawn), true, true, 1, idle(0));
        if (desired.drawn)
        {
            std::vector<std::string_view> bases = panel_idle_bases(desired.form);
            RE::TESForm const* const right = source.GetEquippedObject(false);
            RE::TESForm const* const left = source.GetEquippedObject(true);
            const auto spell = [](RE::TESForm const* form)
            {
                return form && (form->Is(RE::FormType::Spell) || form->Is(RE::FormType::Scroll));
            };
            const auto staff = [](RE::TESForm const* form)
            {
                RE::TESObjectWEAP const* weapon = form ? form->As<RE::TESObjectWEAP>() : nullptr;
                return weapon && weapon->GetWeaponType() == RE::WEAPON_TYPE::kStaff;
            };
            const bool prefer_left = (desired.form == oar::WeaponForm::kMagic && spell(left) && !spell(right)) ||
                (desired.form == oar::WeaponForm::kStaff && staff(left) && !staff(right));
            if (prefer_left && bases.size() > 1)
                std::swap(bases[0], bases[1]);
            next->idles[1] = asset(source, bases, preview_state(true), true, false, 2, idle(1));
            if (spell(left) && !prefer_left)
            {
                const std::array<std::string_view, 1> left_idle{ "Animations\\MLh_Idle.hkx" };
                next->idles[2] = asset(source, left_idle, preview_state(true), true, false, 3, idle(2));
            }
            else if (prefer_left && spell(right))
            {
                const std::array<std::string_view, 1> right_idle{ "Animations\\MRh_Idle.hkx" };
                next->idles[2] = asset(source, right_idle, preview_state(true), true, false, 3, idle(2));
            }
            else if (staff(left) && !prefer_left)
            {
                const std::array<std::string_view, 1> left_idle{ "Animations\\StaffLeft_Idle.hkx" };
                next->idles[2] = asset(source, left_idle, preview_state(true), true, false, 3, idle(2));
            }
        }
        if (changed && active)
        {
            const bool hands_changed = desired.right != active->equipment.right || desired.left != active->equipment.left ||
                desired.drawn != active->equipment.drawn;
            // oar-idle2 report, problem 3: gating on "a hand holds something drawn" made sheathed
            // weapon swaps (sword to sword inside the menu) never produce a transition. The weapon
            // family changed whenever the form changed; play the switch for it too.
            const bool form_changed = desired.form != active->equipment.form;
            if (hands_changed && (desired.drawn || active->equipment.drawn || form_changed))
            {
                // oar-idle2 report, problem 3 (continued): a sheathed swap is "put A back, take B"
                // — the engine plays the B equip in the world, so the panel plays an equip, not a
                // sheathe. A true unequip keeps its meaning only when the weapon family went away
                // (empty hands or the same form with nothing drawn anymore).
                const bool sheathed_swap = !desired.drawn && !active->equipment.drawn && form_changed &&
                    desired.form != oar::WeaponForm::kNone;
                const bool unequip = sheathed_swap ? false :
                    (!desired.drawn || desired.form == oar::WeaponForm::kNone);
                const auto form = unequip ? active->equipment.form : desired.form;
                std::vector<std::string_view> bases = panel_transition_bases(form, unequip);
                const RE::FormID right_id = unequip ? active->equipment.raw_right : desired.raw_right;
                const RE::FormID left_id = unequip ? active->equipment.raw_left : desired.raw_left;
                if (right_id == 0 && left_id != 0 && (form == oar::WeaponForm::kMagic || form == oar::WeaponForm::kStaff))
                    bases.insert(bases.begin(), form == oar::WeaponForm::kMagic
                        ? (unequip ? "Animations\\MLh_Unequip.hkx" : "Animations\\MLh_Equip.hkx")
                        : (unequip ? "Animations\\StaffLeft_Unequip.hkx" : "Animations\\StaffLeft_Equip.hkx"));
                oar::PreviewState transition_state = preview_state(true, !unequip, unequip);
                if (unequip)
                    transition_state.hand_override = std::array<std::uint32_t, 2>{
                        active->equipment.raw_right, active->equipment.raw_left };
                next->transition = asset(source, bases, std::move(transition_state), false, false, 4,
                    nullptr, oar::QueryEvent::e_activate);
            }
        }
        pending = std::move(next);
        for (auto& next_idle : idle_pending)
            next_idle.reset();
    }

    void bind(RE::NiAVObject& target)
    {
        if (root == &target)
            return;
        remember_pose();
        root = &target;
        root_fresh = true;
        nodes.clear();
        RE::BSVisit::TraverseScenegraphObjects(root, [&](RE::NiAVObject* node)
        {
            if (node != root && node->name.c_str())
                nodes.try_emplace(node->name.c_str(), node);
            return RE::BSVisit::BSVisitControl::kContinue;
        });
        if (active)
        {
            for (auto& idle : active->idles)
                if (idle)
                    idle->rebind(target);
            if (active->transition)
                active->transition->rebind(target);
        }
        if (pending)
        {
            for (auto& idle : pending->idles)
                if (idle)
                    idle->rebind(target);
            if (pending->transition)
                pending->transition->rebind(target);
        }
        for (auto& next : idle_pending)
            if (next)
                next->rebind(target);
    }

    void poll(RE::Actor& source)
    {
        if (!root)
            return;
        if (pending && (!latest.accepts(pending->generation) || pending->equipment != panel_equipment(source)))
        {
            logger::info("SCOPY PANEL discard stale equipment proposal generation={}", pending->generation);
            pending.reset();
        }
        if (pending)
        {
            bool loaded = pending->idles[0] != nullptr;
            for (auto& idle : pending->idles)
            {
                if (!idle)
                    continue;
                idle->poll(*root);
                loaded = loaded && (idle->ready() || idle->failed());
            }
            if (pending->transition)
            {
                pending->transition->poll(*root);
                loaded = loaded && (pending->transition->ready() || pending->transition->failed());
            }
            if (loaded && pending->idles[0]->ready())
            {
                if (!root_fresh)
                    remember_pose();
                bool carried_transition = false;
                if (active && phase == PanelAnimationPhase::e_transition && active->transition &&
                    !active->transition->finished() && active->equipment == pending->equipment &&
                    !pending->transition)
                {
                    pending->transition = std::move(active->transition);
                    carried_transition = true;
                }
                for (std::size_t i = 0; i < pending->idles.size(); ++i)
                {
                    auto& next = pending->idles[i];
                    if (active && active->idles[i] && next && active->idles[i]->ready() &&
                        active->idles[i]->same_choice(next->selection()))
                    {
                        active->idles[i]->update_selection(next->selection());
                        next = std::move(active->idles[i]);
                    }
                    else
                    {
                        if (active && active->idles[i] && (!next || !next->ready() ||
                            active->idles[i]->selection().clip_key != next->selection().clip_key))
                            session.selection.deactivate(active->idles[i]->selection().clip_key);
                        if (next && next->ready())
                            next->accept(session);
                    }
                }
                if (active && active->transition)
                    session.selection.deactivate(active->transition->selection().clip_key);
                if (pending->transition && pending->transition->ready() && !carried_transition)
                    pending->transition->accept(session);
                active = std::move(pending);
                phase = panel_phase(true, active->transition && active->transition->ready(), false);
                logger::info("SCOPY PANEL phase={} form={} drawn={} (independent clip; source graph not advanced)",
                    phase == PanelAnimationPhase::e_transition ? "transition" : "idle",
                    oar::weapon_form_name(active->equipment.form), active->equipment.drawn);
            }
        }
        if (!pending && active && latest.accepts(active->generation))
        {
            for (std::size_t i = 0; i < idle_pending.size(); ++i)
            {
                auto& next = idle_pending[i];
                if (!next)
                    continue;
                next->poll(*root);
                if (next->ready())
                {
                    if (active->idles[i] && active->idles[i]->same_choice(next->selection()))
                    {
                        active->idles[i]->update_selection(next->selection());
                        active->idles[i]->accept(session, false);
                        next.reset();
                        continue;
                    }
                    remember_pose();
                    if (active->idles[i] && active->idles[i]->selection().clip_key != next->selection().clip_key)
                        session.selection.deactivate(active->idles[i]->selection().clip_key);
                    active->idles[i] = std::move(next);
                    active->idles[i]->accept(session);
                    logger::info("SCOPY PANEL idle layer={} accepted at its own loop", i);
                }
                else if (next->failed())
                    next.reset();
            }
        }
    }

    void next_idle(RE::Actor& source, std::size_t index)
    {
        LoadedAsset& current = *active->idles[index];
        const std::array<std::string_view, 1> base{ current.selection().base_name };
        oar::AnimationSelection selected = select(source, base, preview_state(active->equipment.drawn),
            static_cast<std::uint32_t>(index + 1), &current, oar::QueryEvent::e_loop);
        if (!selected.found)
            return;
        if (current.same_choice(selected))
        {
            current.update_selection(std::move(selected));
            current.accept(session, false);
        }
        else
        {
            idle_pending[index] = std::make_unique<LoadedAsset>(graph, std::move(selected), true, index == 0);
        }
    }
};

PanelAnimation::PanelAnimation() : m_state(std::make_unique<State>()) {}
PanelAnimation::~PanelAnimation()
{
    // Engine leases are released by reset at live session boundaries, not during CRT teardown.
    (void)m_state.release();
}
void PanelAnimation::reset() { m_state = std::make_unique<State>(); }

void PanelAnimation::observe_equipment(RE::Actor& source, std::uint64_t revision)
{
    m_state->observe(source, revision);
}

void PanelAnimation::prepare(RE::Actor& source, RE::NiAVObject& root, bool equipment_changed)
{
    RE::BSTSmartPointer<RE::BShkbAnimationGraph> graph = source_graph(source);
    if (!graph)
    {
        reset();
        return;
    }
    if (m_state->graph.get() != graph.get())
    {
        const LatestAnimationRequest latest = m_state->latest;
        reset();
        m_state->latest = latest;
    }
    m_state->graph = std::move(graph);
    m_state->bind(root);
    m_state->request(source, equipment_changed);
    m_state->poll(source);
}

void PanelAnimation::tick(RE::Actor& source, RE::NiAVObject& root)
{
    if (!m_state->graph)
        prepare(source, root, false);
    m_state->poll(source);
}

bool PanelAnimation::ready() const
{
    return m_state->active && m_state->active->idles[0] && m_state->active->idles[0]->ready();
}

std::string PanelAnimation::name() const
{
    if (!ready())
        return "panel-animation-loading";
    if (m_state->phase == PanelAnimationPhase::e_transition)
        return "panel-transition:" + m_state->active->transition->name();
    LoadedAsset const* const stance = m_state->active->idles[1].get();
    return "panel-idle:" + (stance && stance->ready() ? stance->name() : m_state->active->idles[0]->name());
}

std::size_t PanelAnimation::advance(RE::NiAVObject& root)
{
    if (!ready() || m_state->root != &root)
        return 0;
    std::size_t written = 0;
    for (auto& idle : m_state->active->idles)
        if (idle && idle->ready())
            written += idle->advance(root);
    if (m_state->phase == PanelAnimationPhase::e_transition)
    {
        m_state->active->transition->advance(root);
        if (m_state->active->transition->finished() && m_state->latest.accepts(m_state->active->generation))
        {
            m_state->remember_pose();
            m_state->phase = panel_phase(true, true, true);
            m_state->session.selection.deactivate(m_state->active->transition->selection().clip_key);
            logger::info("SCOPY PANEL transition complete -> idle");
        }
    }
    if (!m_state->blend_from.empty())
    {
        // 0.4s crossfade: the engine's own equip→idle blend is in this ballpark, and the 0.15s
        // first version read as a hard cut between the sheathing gesture and the weapon stance
        // (oar-idle3 report, second problem of the fourth round).
        const float weight = std::clamp(std::chrono::duration<float>(std::chrono::steady_clock::now() -
            m_state->blend_start).count() / 0.4f, 0.0f, 1.0f);
        for (auto const& [name, before] : m_state->blend_from)
        {
            const auto found = m_state->nodes.find(name);
            if (found != m_state->nodes.end())
                found->second->local = blend_panel_pose(before, found->second->local, weight);
        }
        RE::NiUpdateData update{};
        update.flags = RE::NiUpdateData::Flag::kDirty;
        root.UpdateDownwardPass(update, 0);
        if (weight >= 1.0f)
            m_state->blend_from.clear();
    }
    m_state->root_fresh = false;
    RE::Actor* const source = m_state->graph->holder;
    const bool eligible = m_state->phase == PanelAnimationPhase::e_idle && !m_state->pending && source &&
        m_state->latest.accepts(m_state->active->generation) && panel_equipment(*source) == m_state->active->equipment;
    for (std::size_t i = 0; i < m_state->active->idles.size(); ++i)
    {
        auto& idle = m_state->active->idles[i];
        if (idle && idle->ready() && idle->loop_boundary(eligible && !m_state->idle_pending[i]))
            m_state->next_idle(*source, i);
    }
    return written;
}

PLUGIN_NAMESPACE_END
