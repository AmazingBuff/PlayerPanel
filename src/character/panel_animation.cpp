//
// Created by AmazingBuff on 2026/10/16.
//

#include "character/panel_animation.h"

#include "character/animation_source.h"
#include "character/equipment_mirror.h"
#include "character/panel_animation_policy.h"
#include "oar/hooks.h"
#include "oar/preview_clip.h"
#include "panel/panel_monitor.h"

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
    // The entering-stance observation is the only drawn signal that survived every in-game
    // round (world-frame reads beat both the menu-event read and the frozen menu-session bit).
    // Sheathed entries answer the plain standing idle — a weapon mounted at the hip or back is
    // NOT the drawn stance (fifth-round report problem 1).
    // Did the player SWAP a weapon while the menu was open? An in-menu equip of a weapon puts
    // it in the hand (game semantics: equipping draws it — the world plays the equip move and
    // ends in the weapon stance), so the panel's figure holds it too and answers that family's
    // idle. Recorded by request() when `changed` and the hand slot actually carries a weapon.
    bool g_in_menu_swap_drawn = false;

    bool panel_drawn_state(RE::Actor& actor)
    {
        // An in-menu weapon swap overrides every world-side read: the player's intent was "hold
        // this weapon now" (fourteenth-round report: the panel played mt_idle after the equip
        // move because the entering stance was sheathed).
        if (g_in_menu_swap_drawn)
            return true;
        const PanelMonitor::StanceSnapshot snapshot = PanelMonitor::instance().stance_at_open();
        if (snapshot.valid)
            return snapshot.drawn;
        return actor.IsWeaponDrawn();
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

    // A direct selection by NAME (the pack's sequence sibling): index and path come from the
    // project's own name table, so the engine loader queues the binding exactly like an
    // OAR-decided index. OAR itself cannot answer for the sibling while the panel pauses the
    // world: Smooth Moveset gates its Start/Loop members on magic effects that animation events
    // toggle, and neither the events nor the source effects advance in a paused menu (round 17).
    std::uint64_t sibling_key(std::string_view name, std::uint32_t channel)
    {
        std::uint64_t hash = 1469598103934665603ULL;
        for (unsigned char character : name)
        {
            hash ^= character;
            hash *= 1099511628211ULL;
        }
        return hash ^ channel;
    }

    oar::AnimationSelection sibling_selection(std::uint16_t index, std::string name, std::uint32_t channel)
    {
        oar::AnimationSelection selection;
        selection.found = true;
        selection.replaced = false;
        selection.index = index;
        selection.base_index = index;
        selection.name = std::move(name);
        selection.base_name = selection.name;
        selection.clip_key = sibling_key(selection.name, channel);
        return selection;
    }

    class LoadedAsset
    {
    public:
        LoadedAsset(RE::BSTSmartPointer<RE::BShkbAnimationGraph> graph, oar::AnimationSelection selection, bool loop,
            bool full_body, std::string_view weapon_hint = {}, bool direct = false, double start_seconds = 0.0) :
            m_graph(std::move(graph)), m_selection(std::move(selection)), m_loop(loop), m_full_body(full_body),
            m_weapon_hint(weapon_hint), m_direct(direct), m_start_seconds(std::max(start_seconds, 0.0))
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
                        m_player.set_clip(m_playback, m_loop, m_start_seconds);
                        // duration fingerprints the loaded animation: the pack's Start and Loop
                        // members have very different lengths, so a member whose duration
                        // matches the entry move's is loading the WRONG binding (round 18).
                        logger::info("SCOPY PANEL asset ready index={} base={} replaced={} duration={:.2f} name='{}' mapping={} {}",
                            m_selection.index, m_selection.base_index, m_selection.replaced,
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
        // True when the selection came from the pack's name-table sibling (not OAR): its loop
        // re-checks stay with the same clip instead of re-asking OAR, whose frozen phase would
        // answer the entry member again (round 17).
        [[nodiscard]] bool direct() const { return m_direct; }
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
                // The entry action restarts to the SOURCE phase it was seeded with, not to zero:
                // accept() runs when the load lands, which must not rewind the mirrored move
                // back to its first frame (round 18).
                m_player.restart(m_start_seconds);
                m_cycle.reset();
            }
            if (m_selection.replaced && m_selection.variants)
            {
                // A mid-sequence landing (seek_dwell) pins the sequence history instead of
                // advancing it: commit() moves "next" past the trailing loop body, and the next
                // loop re-check then replays the entry member (round 16).
                if (m_selection.dwell)
                    session.selection.dwell(*m_selection.variants, m_selection.variant, m_selection.clip_key);
                else
                    session.selection.commit(*m_selection.variants, m_selection.variant, m_selection.clip_key);
            }
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
        // The exact hand-attachment variant this asset's Weapon bone resolves to ("WeaponSword",
        // "WeaponBow", …). The longest-prefix alias landed on the wrong variant in-game and the
        // visible weapon never moved (fifth-round report).
        std::string m_weapon_hint;
        bool m_direct = false;
        // The source clip's phase for an entry action; zero for every OAR-decided asset.
        double m_start_seconds = 0.0;
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

    // One idle layer's sequence plan (round 18, user model): the pack's family members collected
    // ONCE from the name table, ordered Start → Loop → Loop2. The layer plays through the plan
    // and dwells on the last member — with ZERO OAR queries at the member boundaries, whose
    // frozen phase answer is what every earlier round was working around.
    struct SequencePlan
    {
        bool active = false;
        std::string stem;
        std::vector<oar::SequenceMember> members;
        std::size_t position = 0;  // the member the layer is currently playing
    };

    // The sequence plans persist across panel sessions (round 18 verdict): closing and reopening
    // the inventory resumes the family's plan at its current member instead of replaying the
    // Start move — the per-open replay was the "extra round of Start" the user reported. The
    // plans are invalidated at load boundaries, where the project name table is rebuilt.
    std::array<SequencePlan, 3> s_sequence_plans;

    // The panel-aware equipment read: the entering-stance snapshot and combat answer `drawn` (see
    // panel_drawn_state), everything else is live. All equality checks against an AnimationSet's
    // recorded equipment go through this, so the snapshot stance stays self-consistent.
    EquipmentState panel_equipment(RE::Actor& actor)
    {
        EquipmentState state = equipment_state(actor);
        state.drawn = panel_drawn_state(actor);
        // The player unequipping inside the menu wins over the entering-stance snapshot — but a
        // COMBAT player with empty hands keeps the drawn answer: form kNone + drawn queries the
        // unarmed fighting family (h2h_idle), which is exactly the "空手战斗" stance.
        if (state.form == oar::WeaponForm::kNone && !actor.IsInCombat())
            state.drawn = false;
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
        oar::PreviewState state, std::uint32_t channel, LoadedAsset const* current, oar::QueryEvent event,
        bool seek_dwell = false)
    {
        oar::AnimationSelection selected = oar::query_animation(source, graph->characterInstance, bases,
            std::move(state), session, { channel, event, current ? &current->selection() : nullptr, seek_dwell });
        logger::info("SCOPY PANEL query channel={} generation={} project='{}' base={} index={} replaced={} evaluated={} overridden={} rejected={} name='{}' diagnostic='{}'",
            channel, latest.generation(),
            graph->projectName.c_str(), selected.base_index, selected.index, selected.replaced,
            selected.evaluated, selected.overrides, selected.rejected, selected.name, selected.diagnostic);
        return selected;
    }

    std::unique_ptr<LoadedAsset> asset(RE::Actor& source, std::span<std::string_view const> bases,
        oar::PreviewState state, bool loop, bool full_body, std::uint32_t channel,
        LoadedAsset const* current = nullptr, oar::QueryEvent event = oar::QueryEvent::e_refresh,
        std::string_view weapon_hint = {}, bool seek_dwell = false)
    {
        oar::AnimationSelection selected = select(source, bases, std::move(state), channel, current, event, seek_dwell);
        return selected.found ? std::make_unique<LoadedAsset>(graph, std::move(selected), loop, full_body, weapon_hint) : nullptr;
    }

    LoadedAsset const* idle(std::size_t index) const { return active ? active->idles[index].get() : nullptr; }

    [[nodiscard]] std::span<std::string const> catalogue()
    {
        return graph ? oar::animation_names(graph->characterInstance, session) : std::span<std::string const>{};
    }

    SequencePlan build_sequence_plan(std::span<std::string const> names,
        oar::AnimationSelection const& anchor, std::uint16_t exclude_index)
    {
        SequencePlan plan;
        auto members = oar::collect_sequence_family(names, anchor.name);
        std::erase_if(members, [&](oar::SequenceMember const& member) { return member.index == exclude_index; });
        if (members.empty())
            return plan;
        plan.active = true;
        plan.stem = oar::path_sequence_stem(oar::path_parent(anchor.name));
        plan.members = std::move(members);
        plan.position = 0;
        return plan;
    }

    // Loads the plan's current member onto the layer.
    void load_plan_member(std::size_t index, std::uint32_t channel, std::string_view weapon_hint)
    {
        const SequencePlan& plan = s_sequence_plans[index];
        const oar::SequenceMember& member = plan.members[plan.position];
        idle_pending[index] = std::make_unique<LoadedAsset>(graph,
            sibling_selection(member.index, member.name, channel), true, index == 0, weapon_hint, true);
        logger::info("SCOPY PANEL sequence layer={} member={}/{} name='{}'",
            index, plan.position + 1, plan.members.size(), member.name);
    }

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
        // A hand-slot change while the menu is open = the player drew the new weapon inside the
        // panel. It must be recorded BEFORE this request reads the equipment: panel_drawn_state()
        // answers from this flag, and reading it first built a sheathed set (no weapon idle
        // layer) that the next poll() then discarded as "equipment changed" (round 16).
        // Cleared with the panel session in PanelAnimation::reset.
        if (changed && active)
        {
            const std::array<RE::FormID, 2> hands = equipment_hand_ids(source);
            if (equipment_weapon_form(source) != oar::WeaponForm::kNone &&
                (hands[0] != active->equipment.right || hands[1] != active->equipment.left))
            {
                g_in_menu_swap_drawn = true;
                logger::info("SCOPY PANEL in-menu weapon swap -> the figure holds the new weapon");
            }
        }
        EquipmentState desired = panel_equipment(source);
        auto next = std::make_unique<AnimationSet>();
        next->equipment = desired;
        next->generation = latest.generation();
        // Entry action (round 18): replay the clip the player was IN at the open edge, starting
        // from its own phase. Only the first request of a session consumes it; the world made
        // that decision and the paused menu cannot re-derive it. After the move plays out, the
        // sequence plan takes over (see the plan build below).
        PanelMonitor::EntryAction entry;
        oar::EntryActionKind entry_kind = oar::EntryActionKind::e_none;
        if (!active)
        {
            entry = PanelMonitor::instance().take_entry_action();
            const std::span<std::string const> names = catalogue();
            if (entry.valid && entry.index < names.size())
            {
                const std::string& entry_name = names[entry.index];
                entry_kind = oar::entry_action_kind(entry_name);
                if (entry_kind != oar::EntryActionKind::e_none)
                {
                    oar::AnimationSelection selection = sibling_selection(entry.index, entry_name, 0);
                    next->transition = std::make_unique<LoadedAsset>(graph, std::move(selection), false, false,
                        panel_weapon_node(desired.form), false, static_cast<double>(entry.local_time));
                    logger::info("SCOPY PANEL entry action kind={} index={} time={:.2f} name='{}'",
                        static_cast<int>(entry_kind), entry.index, entry.local_time, entry_name);
                }
            }
        }
        const std::array<std::string_view, 1> body{ "Animations\\MT_Idle.hkx" };
        // The panel lands wherever the WORLD is in its own sequence (user direction: the world
        // already played Equip→Start before the menu opened, so the panel shows the loop body
        // the world is dwelling on) — every entering-panel idle query seeks the dwell position.
        constexpr bool k_entering_panel = true;
        next->idles[0] = asset(source, body, preview_state(desired.drawn), true, true, 1, idle(0),
            oar::QueryEvent::e_activate, {}, k_entering_panel);
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
            // The game's own answer first (fifth-round report problem 2): the world side showed a
            // concrete idle clip for this stance on the last unpaused frames — prefer loading
            // THAT clip by name (find_base_animation resolves it in the same project), and fall
            // back to the OAR standing-base query only when the observation is empty.
            //
            // The observation is frozen while the panel is open (panel.cpp only records world
            // frames), so it describes the stance the player ENTERED with. It must also belong to
            // the family this request is building: once the hand slot changes inside the menu the
            // observation is stale, and the old code loaded MT_Idle (or the previous weapon's
            // idle) as the new weapon's idle layer (round 16).
            std::vector<std::string_view> world_bases;
            const std::string& observed = PanelMonitor::instance().last_idle_clip();
            if (!observed.empty() && observed.find("idle") != std::string::npos)
            {
                if (!g_in_menu_swap_drawn && panel_clip_is_family(observed, desired.form))
                    world_bases.push_back(observed);
                else
                    logger::info("SCOPY PANEL world idle '{}' belongs to another stance; using the OAR bases for {}",
                        observed, oar::weapon_form_name(desired.form));
            }
            if (!world_bases.empty())
            {
                next->idles[1] = asset(source, world_bases, preview_state(true), true, false, 2, idle(1),
                    oar::QueryEvent::e_activate, panel_weapon_node(desired.form), k_entering_panel);
                if (!next->idles[1])
                    logger::info("SCOPY PANEL world idle clip not resolvable, falling back to OAR bases: '{}'", observed);
            }
            if (!next->idles[1])
                next->idles[1] = asset(source, bases, preview_state(true), true, false, 2, idle(1),
                    oar::QueryEvent::e_activate, panel_weapon_node(desired.form), k_entering_panel);
            if (spell(left) && !prefer_left)
            {
                // Hand layer SUPPRESSED (round 18 verdict): the pack's mlh clips are full-skeleton
                // exports (~95 of 99 tracks resolve), the identity track mapping writes the whole
                // body, and the hand layer draws AFTER the stance layer — its mlh Start member
                // overwrote the weapon idle and the panel looped the Start move forever. The
                // left-hand stance pose now comes from the stance layer alone.
                logger::info("SCOPY PANEL hand layer suppressed (identity mapping covers the whole body)");
            }
            else if (prefer_left && spell(right))
            {
                logger::info("SCOPY PANEL hand layer suppressed (identity mapping covers the whole body)");
            }
            else if (staff(left) && !prefer_left)
            {
                logger::info("SCOPY PANEL hand layer suppressed (identity mapping covers the whole body)");
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
                    nullptr, oar::QueryEvent::e_activate,
                    panel_weapon_node(unequip ? active->equipment.form : desired.form));
            }
        }
        // Sequence plans (round 18, user model): the pack's family members ordered once per
        // weapon-idle layer, Start → Loop → Loop2; the plan layer plays through them and dwells
        // on the last, with ZERO OAR queries at the member boundaries. The plans PERSIST across
        // panel sessions — closing and reopening resumes at the current member instead of
        // replaying the Start move (the user's "extra round of Start"). A layer whose idle has
        // no sequence folder (DAR-style packs, the body layer) keeps the ordinary OAR path.
        const std::uint16_t entry_exclude = entry_kind == oar::EntryActionKind::e_sequence_start ? entry.index : 0xFFFF;
        for (std::size_t layer = 1; layer < next->idles.size(); ++layer)
        {
            if (!next->idles[layer])
                continue;  // sheathed: the persistent plan waits for the next draw
            const std::string stem(oar::path_sequence_stem(oar::path_parent(next->idles[layer]->selection().name)));
            if (stem.empty())
            {
                s_sequence_plans[layer] = {};  // no sequence family: ordinary OAR looping
                continue;
            }
            auto& plan = s_sequence_plans[layer];
            if (!plan.active || plan.stem != stem)
            {
                plan = build_sequence_plan(catalogue(), next->idles[layer]->selection(), entry_exclude);
                // Where the plan starts: an Equip entry plays the family's first member next; a
                // Start entry has already played Start (it is the entry move and is excluded);
                // with no entry move the panel enters at the member the world was frozen at.
                if (entry_kind == oar::EntryActionKind::e_none)
                {
                    const std::uint16_t anchor_index = next->idles[layer]->selection().index;
                    const auto anchor = std::ranges::find_if(plan.members,
                        [&](oar::SequenceMember const& m) { return m.index == anchor_index; });
                    plan.position = anchor != plan.members.end()
                        ? static_cast<std::size_t>(anchor - plan.members.begin()) : 0;
                }
                else
                    plan.position = 0;
            }
            if (plan.members.empty())
                continue;
            const oar::SequenceMember& member = plan.members[plan.position];
            if (next->idles[layer]->selection().index != member.index)
                next->idles[layer] = std::make_unique<LoadedAsset>(graph,
                    sibling_selection(member.index, member.name, static_cast<std::uint32_t>(layer + 1)),
                    true, false, layer == 1 ? panel_weapon_node(desired.form) : std::string_view{}, true);
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
        // Plan layers (round 18, user model): advance to the next member and dwell on the last —
        // ZERO OAR queries at the member boundaries. The frozen phase answer is what every
        // earlier round was working around; the plan removed the need to ask at all. Non-plan
        // layers (variant packs, DAR, single-clip packs) keep the ordinary OAR loop re-check.
        if (s_sequence_plans[index].active)
        {
            auto& plan = s_sequence_plans[index];
            if (plan.position + 1 < plan.members.size())
            {
                ++plan.position;
                load_plan_member(index, static_cast<std::uint32_t>(index + 1),
                    index == 1 ? panel_weapon_node(active->equipment.form) : std::string_view{});
            }
            return;
        }
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
            // The stance layer keeps the hand-attachment hint the request supplied: without it
            // resolve_binding_playback falls back to prefix matching and can bind the Weapon
            // track to the wrong variant (round 16).
            idle_pending[index] = std::make_unique<LoadedAsset>(graph, std::move(selected), true, index == 0,
                index == 1 ? panel_weapon_node(active->equipment.form) : std::string_view{});
        }
    }

    // A transition just finished (the entry action or an in-menu swap). Plan layers need nothing:
    // they are already playing their sequence member and the plan carries them forward. Every
    // other layer re-asks OAR once for the stance it should hold now.
    void restart_idle_after_transition(RE::Actor& source)
    {
        if (!active)
            return;
        for (std::size_t i = 0; i < active->idles.size(); ++i)
        {
            if (s_sequence_plans[i].active)
                continue;  // the plan layer is already on its member; the sequence continues
            auto& idle = active->idles[i];
            if (!idle || !idle->ready())
                continue;
            // The world-name preference first (fourteenth round, problem 4): the plain
            // base-name query walked into replacements carrying third-party conditions the
            // preview must reject, and the whole candidate set died to the vanilla clip
            // (replaced=false). The world-side recorded idle name pinpoints the same
            // replacement family — its condition set already answered true for this stance.
            //
            // The world clip is only usable while it still describes this stance: an in-menu
            // weapon swap happened after it was recorded, so it belongs to the previous family
            // (round 16).
            const std::string world_clip = i > 0 ? PanelMonitor::instance().last_idle_clip() : std::string{};
            std::vector<std::string_view> bases;
            if (!world_clip.empty() && !g_in_menu_swap_drawn &&
                panel_clip_is_family(world_clip, active->equipment.form))
                bases.push_back(world_clip);
            bases.push_back(idle->selection().base_name);
            oar::AnimationSelection selected = select(source, bases, preview_state(active->equipment.drawn),
                static_cast<std::uint32_t>(i + 1), nullptr, oar::QueryEvent::e_activate);
            if (!selected.found)
                continue;
            if (idle->same_choice(selected))
            {
                idle->update_selection(std::move(selected));
                idle->accept(session, false);
            }
            else
            {
                idle_pending[i] = std::make_unique<LoadedAsset>(graph, std::move(selected), true, i == 0,
                    i == 1 ? panel_weapon_node(active->equipment.form) : std::string_view{});
            }
        }
    }
};

PanelAnimation::PanelAnimation() : m_state(std::make_unique<State>()) {}
PanelAnimation::~PanelAnimation()
{
    // Engine leases are released by reset at live session boundaries, not during CRT teardown.
    (void)m_state.release();
}
void PanelAnimation::reset()
{
    m_state = std::make_unique<State>();
    g_in_menu_swap_drawn = false;  // a new panel session starts from the entering stance again
}

void PanelAnimation::invalidate_sequence_plans()
{
    for (auto& plan : s_sequence_plans)
        plan = {};
}

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
    // A capture whose stance and equipment are already covered only re-targets the layers bind()
    // just re-pointed at the new copy. Re-issuing the request there threw an in-flight load away
    // and re-ran every OAR query four frames after the equipment event that had issued one
    // (round 16). The first capture and a hand-slot change still build a new request.
    const EquipmentState covered_equipment = panel_equipment(source);
    const bool covered = (m_state->active && m_state->active->equipment == covered_equipment) ||
        (m_state->pending && m_state->latest.accepts(m_state->pending->generation) &&
            m_state->pending->equipment == covered_equipment);
    if (!covered)
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

bool PanelAnimation::transitioning() const
{
    return m_state->phase == PanelAnimationPhase::e_transition;
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
            // The game order is Equip → Start → Loop: restart the idle layers' sequences so the
            // layer that just became current plays its Start member and dwells in its Loop.
            if (RE::Actor* const source = m_state->graph->holder)
                m_state->restart_idle_after_transition(*source);
        }
    }
    if (!m_state->blend_from.empty())
    {
        // The game's own crossfade length, measured from the player's idle-blend weights on the
        // world frames (see PanelMonitor::observe_idle_clip). Falls back to 0.4s before any
        // measurement landed.
        const float fade = PanelMonitor::instance().measured_crossfade_seconds();
        const float fade_seconds = fade > 0.05f ? fade : 0.4f;
        const float weight = std::clamp(std::chrono::duration<float>(std::chrono::steady_clock::now() -
            m_state->blend_start).count() / fade_seconds, 0.0f, 1.0f);
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
