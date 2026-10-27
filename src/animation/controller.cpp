//
// Created by AmazingBuff on 2026/10/16.
//

#include "animation/controller.h"

#include "animation/behavior_graph/independent_graph.h"
#include "animation/behavior_graph/ownership_probe.h"
#include "animation/clip.h"
#include "animation/diagnostic_trace.h"
#include "animation/mapping_report.h"
#include "animation/settings.h"
#include "animation/pose.h"
#include "animation/oar/hooks.h"
#include "animation/oar/preview_clip.h"
#include "animation/sequence.h"
#include "animation/event_clock.h"
#include "animation/dac/session.h"
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
#include "RE/H/hkaBone.h"
#include "RE/H/hkaSkeleton.h"
#include "RE/N/NiCamera.h"
#include "RE/N/NiQuaternion.h"
#include "RE/T/TESObjectLIGH.h"
#include "RE/T/TESObjectARMO.h"
#include "RE/T/TESObjectWEAP.h"
#include "RE/U/UI3DSceneManager.h"

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
    std::string_view action_name(ActionKind kind)
    {
        switch (kind)
        {
        case ActionKind::e_standing_idle: return "standing-idle";
        case ActionKind::e_weapon_idle: return "weapon-idle";
        case ActionKind::e_equip: return "equip";
        case ActionKind::e_unequip: return "unequip";
        }
        return "unknown";
    }

    std::string transform_report(RE::NiTransform const& transform)
    {
        const RE::NiQuaternion rotation(transform.rotate);
        return fmt::format("t=({:.4f},{:.4f},{:.4f}) qxyzw=({:.4f},{:.4f},{:.4f},{:.4f}) s={:.6f}",
            transform.translate.x, transform.translate.y, transform.translate.z,
            rotation.x, rotation.y, rotation.z, rotation.w, transform.scale);
    }

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
            bool full_body, std::uint64_t generation, std::optional<oar::AnimationSelection> fallback = {}, std::string_view role = "target") :
            m_graph(std::move(graph)), m_selection(std::move(selection)), m_loop(loop), m_full_body(full_body),
            m_fallback(std::move(fallback)), m_actor_id(m_graph->holder->GetFormID()), m_generation(generation), m_role(role)
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
                        if (detailed_animation_trace())
                            logger::info("ANIMATION LOAD actor={:08x} asset-request={} role={} outcome=ready graph={} index={} path='{}' queue-to-ready-ms={:.3f}",
                                m_actor_id, m_generation, m_role, static_cast<void*>(&root), m_selection.index, m_selection.name,
                                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - m_started).count());
                        return;
                    }
                }
            }
            if (std::chrono::steady_clock::now() - m_started > std::chrono::seconds(2))
                fallback("animation load timed out");
        }

        void rebind(RE::NiAVObject& root, std::uint64_t request)
        {
            if (!m_playback.binding_owner)
            {
                if (detailed_animation_trace())
                    logger::info("ANIMATION REBIND actor={:08x} request={} asset-request={} role={} graph={} index={} outcome=no-retained-binding",
                        m_actor_id, request, m_generation, m_role, static_cast<void*>(&root), m_selection.index);
                return;
            }
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
            if (detailed_animation_trace())
                logger::info("ANIMATION REBIND actor={:08x} request={} asset-request={} role={} graph={} index={} path='{}' outcome={} mapping={} coverage='{}'",
                    m_actor_id, request, m_generation, m_role, static_cast<void*>(&root), m_selection.index, m_selection.name,
                    m_playback.valid() ? "ready" : "failed-cleared", mapping, coverage);
        }

        void refresh_selection(oar::AnimationSelection selection) { m_selection = std::move(selection); }
        [[nodiscard]] bool ready() const { return m_playback.valid(); }
        [[nodiscard]] bool failed() const { return m_failed; }
        [[nodiscard]] ClipPlayback const& playback() const { return m_playback; }
        [[nodiscard]] oar::AnimationSelection const& selection() const { return m_selection; }
        [[nodiscard]] RE::hkaSkeleton const* diagnostic_skeleton() const
        { return m_graph->characterInstance.setup ? m_graph->characterInstance.setup->animationSkeleton.get() : nullptr; }
        [[nodiscard]] RE::BShkbAnimationGraph const* diagnostic_graph() const { return m_graph.get(); }

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
                if (detailed_animation_trace())
                    logger::info("ANIMATION LOAD actor={:08x} asset-request={} role={} outcome=queued index={} path='{}' loop={}",
                        m_actor_id, m_generation, m_role, m_selection.index, m_selection.name, m_loop);
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
            if (detailed_animation_trace())
                logger::info("ANIMATION LOAD actor={:08x} asset-request={} role={} outcome=fallback index={} path='{}' reason='{}' target={}",
                    m_actor_id, m_generation, m_role, m_selection.index, m_selection.name, reason,
                    m_fallback ? "explicit-base" : m_selection.index != m_selection.base_index ? "original-base" : "unavailable");
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
        std::string_view m_role;

        bool m_queued = false;
        bool m_failed = false;
        std::chrono::steady_clock::time_point m_started{};
        oar::PreviewClip m_carrier;
        ClipPlayback m_playback{};

    };


    enum class ChoiceReason { e_request, e_interrupt, e_boundary, e_refresh };
    std::string_view choice_reason_name(ChoiceReason reason)
    {
        switch (reason)
        {
        case ChoiceReason::e_request: return "request";
        case ChoiceReason::e_interrupt: return "interrupt";
        case ChoiceReason::e_boundary: return "boundary";
        case ChoiceReason::e_refresh: return "refresh";
        }
        return "unknown";
    }
    struct Proposal
    {
        std::uint64_t generation = 0;
        std::uint64_t effect_revision = 0;
        Stage stage;
        std::unique_ptr<LoadedAsset> asset;
        std::unique_ptr<LoadedAsset> standing;
        std::unique_ptr<LoadedAsset> calibration;
        std::unique_ptr<LoadedAsset> torch;
        std::optional<oar::AnimationSelection> torch_clip;
        double torch_seconds = 0.0;
        SelectionState history;
        oar::ConditionRandomState conditions;
        ChoiceReason reason = ChoiceReason::e_request;
        bool direct = false;
        std::optional<oar::PreviewState> transition_preview;
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
    std::chrono::steady_clock::time_point pose_probe_started{};
    PoseTraceBudget pose_budget;
    std::string_view pose_profile;
    bool trace_torch_request = false;
    bool trace_axe_request = false;
    bool trace_shield_request = false;
    std::uint64_t pose_probe_epoch = 0;
    std::uint32_t pose_probe_wraps = 0;
    MappingPublication mapping_publication;

    void trace_decision(std::string_view decision, std::string_view reason, Proposal const* subject)
    {
        if (!detailed_animation_trace()) return;
        logger::info("ANIMATION DECISION actor={:08x} request={} accepted-request={} decision={} reason={} graph={} subject-request={} subject-action={} subject-index={} subject-path='{}' subject-effect-revision={} accepted-index={} accepted-path='{}'",
            actor_id, generation, active ? active->generation : 0, decision, reason, static_cast<void*>(root),
            subject ? subject->generation : 0, subject ? action_name(subject->stage.action.kind) : "none",
            subject ? subject->stage.clip.index : 0xFFFF, subject ? subject->stage.clip.name : "",
            subject ? subject->effect_revision : 0, active ? active->stage.clip.index : 0xFFFF, active ? active->stage.clip.name : "");
    }

    void trace_probe_limit()
    {
        if (pose_budget.exhausted_notice())
            logger::info("POSE BUDGET actor={:08x} request={} accepted-request={} graph={} outcome=exhausted windows={} samples={} limit=4-windows/24-samples",
                actor_id, generation, active ? active->generation : 0, static_cast<void*>(root), pose_budget.windows(), pose_budget.samples());
    }

    void publish_mapping()
    {
        if (!active || !active->asset->ready()) return;
        const auto started = std::chrono::steady_clock::now();
        const oar::AnimationSelection& clip = active->stage.clip;
        const AcceptedMapping mapping{
            clip.base_name, clip.replaced ? clip.name : "", oar::animation_binding_name(graph->characterInstance, clip.index),
            clip.base_index, clip.index, clip.variant.index, clip.variant.position, static_cast<int>(active->stage.action.kind), clip.replaced
        };
        if (!mapping_publication.accept(generation, mapping)) return;
        const std::string variant = clip.variants ? fmt::format("index={} position={}", mapping.variant_index, mapping.variant_position) : "none";
        const double build_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        logger::info("ANIMATION PANEL actor={:08x} request={} accepted action={} original='{}' base-index={} logical='{}' variant='{}' index={} bound='{}' status={} clip-time={:.3f} blend={:.3f} build-ms={:.3f}",
            actor_id, generation, mapping.action, mapping.original, mapping.base_index, mapping.logical,
            variant, mapping.index, mapping.bound.empty() ? "<unavailable>" : mapping.bound,
            mapping.replaced ? "replacement" : "base", clock.seconds(), blend_duration, build_ms);
    }

    void begin_pose_probe(std::string_view reason)
    {
        pose_budget.stop();
        pose_profile = {};
        if (!detailed_animation_trace() || !active || !active->asset->ready()) return;
        const WeaponForm form = active->stage.action.form;
        if (trace_axe_request || form == WeaponForm::kTwoHandedAxe) pose_profile = "axe";
        else if (form == WeaponForm::kTorch || form == WeaponForm::kShield || form == WeaponForm::kBow || form == WeaponForm::kCrossbow)
            pose_profile = weapon_form_name(form);
        else if (form == WeaponForm::kOneHanded)
        {
            RE::TESObjectARMO const* const shield = observed.hands[1] ? RE::TESForm::LookupByID<RE::TESObjectARMO>(observed.hands[1]) : nullptr;
            if (trace_torch_request) pose_profile = "torch-arms";
            else if (trace_shield_request || (shield && shield->IsShield())) pose_profile = "shield-arms";
        }
        if (pose_profile.empty()) return;
        if (!pose_budget.arm(generation)) { pose_profile = {}; trace_probe_limit(); return; }
        pose_probe_started = std::chrono::steady_clock::now();
        pose_probe_wraps = 0;
        ++pose_probe_epoch;
        const ClipPlayback& clip = active->asset->playback();
        const RE::hkaAnimationBinding& binding = *clip.binding_owner;
        const RE::hkaSkeleton* const skeleton = active->asset->diagnostic_skeleton();
        logger::info("POSE PROBE actor={:08x} request={} accepted-request={} probe={} profile={} begin={} graph={} path='{}' clip-time={:.4f} root-local=[{}] root-world=[{}] translation-units=game scale=ratio",
            actor_id, generation, active->generation, pose_probe_epoch, pose_profile, reason, static_cast<void*>(root), clip.name, clock.seconds(),
            transform_report(root->local), transform_report(root->world));
        logger::info("POSE MAP actor={:08x} request={} probe={} graph={} target-skeleton-owner-graph={} path='{}' original-skeleton='{}' mapping={}",
            actor_id, generation, pose_probe_epoch, static_cast<void*>(root),
            static_cast<void const*>(active->asset->diagnostic_graph()), clip.name,
            binding.originalSkeletonName.c_str() ? binding.originalSkeletonName.c_str() : "<missing>",
            binding.transformTrackToBoneIndices.empty() ? "identity-unverified" : "table");
        for (std::size_t i = 0; i < clip.tracks.size(); ++i)
        {
            const std::uint16_t track = clip.tracks[i];
            const std::int32_t bone = binding.transformTrackToBoneIndices.empty() ? track : binding.transformTrackToBoneIndices[track];
            const char* const raw_name = skeleton && bone >= 0 && bone < skeleton->bones.size() ? skeleton->bones[bone].name.c_str() : "<unavailable>";
            const RE::NiAVObject* const node = clip.nodes[i];
            const std::string_view node_name = node && node->name.c_str() ? node->name.c_str() : "<missing>";
            if (track > 4 && track != 43 && std::ranges::find(Trace_Body_Nodes, node_name) == Trace_Body_Nodes.end()) continue;
            logger::info("POSE MAP actor={:08x} request={} probe={} track={} target-bone={} target-name='{}' copy-node={} copy-name='{}'",
                actor_id, generation, pose_probe_epoch, track, bone, raw_name ? raw_name : "<missing>",
                static_cast<void const*>(node), node_name);
        }
    }

    void trace_pose(PoseBuffer const& incoming, std::chrono::steady_clock::time_point now)
    {
        if (!detailed_animation_trace() || pose_profile.empty()) return;
        const double elapsed = std::chrono::duration<double>(now - pose_probe_started).count();
        const std::optional<std::size_t> slot = pose_budget.sample(elapsed);
        if (!slot) { if (elapsed > 4.1) pose_profile = {}; trace_probe_limit(); return; }
        const std::size_t sample = *slot;
        const auto trace_scale = [&](LoadedAsset const& asset, double seconds, bool loop, std::string_view role)
        {
            std::vector<SampledScale> scales;
            (void)sample_animation_clip(asset.playback(), static_cast<float>(seconds), loop, &scales);
            for (SampledScale const& raw : scales)
                if (raw.track <= 4 || raw.track == 43 || std::ranges::find(Trace_Body_Nodes, raw.name) != Trace_Body_Nodes.end())
                    logger::info("POSE SCALE actor={:08x} request={} probe={} sample={} role={} path='{}' track={} node='{}' raw-xyz=({:.6f},{:.6f},{:.6f}) conversion=scale-x",
                        actor_id, generation, pose_probe_epoch, sample, role, asset.playback().name, raw.track, raw.name,
                        raw.xyz[0], raw.xyz[1], raw.xyz[2]);
        };
        trace_scale(*active->asset, clock.seconds(), active->stage.action.loop, "body");
        if (active->torch && active->torch->ready()) trace_scale(*active->torch, active->torch_seconds, true, "torch-arm");
        if (outgoing && outgoing->asset->ready()) trace_scale(*outgoing->asset, outgoing_seconds, outgoing->stage.action.loop, "outgoing-body");
        const double weight = outgoing ? std::min(blend_seconds / blend_duration, 1.0) : 1.0;
        logger::info("POSE SAMPLE actor={:08x} request={} accepted-request={} probe={} profile={} sample={} graph={} elapsed-wall={:.4f} clip-time={:.4f} loop={} wraps={} path='{}' outgoing='{}' outgoing-time={:.4f} blend-time={:.4f} blend-duration={:.4f} incoming-weight={:.4f}",
            actor_id, generation, active->generation, pose_probe_epoch, pose_profile, sample, static_cast<void*>(root), elapsed, clock.seconds(),
            active->stage.action.loop, pose_probe_wraps, active->stage.clip.name,
            outgoing ? outgoing->stage.clip.name : "", outgoing_seconds, blend_seconds, blend_duration, weight);
        const RE::UI3DSceneManager* const ui3d = RE::UI3DSceneManager::GetSingleton();
        RE::NiCamera* const camera = ui3d ? ui3d->camera.get() : nullptr;
        if (camera)
        {
            const RE::NiCamera::RUNTIME_DATA2& data = camera->GetRuntimeData2();
            const RE::NiFrustum& frustum = data.viewFrustum;
            logger::info("POSE CAMERA actor={:08x} request={} probe={} sample={} camera={} world=[{}] frustum-lrtbnf=({:.4f},{:.4f},{:.4f},{:.4f},{:.4f},{:.4f}) ortho={} port-width-height=({:.4f},{:.4f})",
                actor_id, generation, pose_probe_epoch, sample, static_cast<void*>(camera), transform_report(camera->world),
                frustum.fLeft, frustum.fRight, frustum.fTop, frustum.fBottom, frustum.fNear, frustum.fFar,
                frustum.bOrtho, data.port.GetWidth(), data.port.GetHeight());
        }
        else logger::info("POSE CAMERA actor={:08x} request={} probe={} sample={} camera=missing", actor_id, generation, pose_probe_epoch, sample);
        const auto trace_node = [&](std::string_view requested, RE::NiAVObject const* node)
        {
            if (!node)
            {
                logger::info("POSE NODE actor={:08x} request={} probe={} sample={} requested='{}' node=missing",
                    actor_id, generation, pose_probe_epoch, sample, requested);
                return;
            }
            const std::string_view name = node->name.c_str() ? node->name.c_str() : "";
            const auto authored = std::ranges::find_if(incoming, [&](auto const& entry) { return entry.first == name; });
            float x = 0.0f, y = 0.0f, z = 0.0f;
            const bool projected = camera && camera->WorldPtToScreenPt3(node->world.translate, x, y, z, 1e-5f) &&
                std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
            logger::info("POSE NODE actor={:08x} request={} probe={} sample={} requested='{}' name='{}' node={} incoming-local=[{}] local=[{}] world=[{}] screen-port-xyz=[{}]",
                actor_id, generation, pose_probe_epoch, sample, requested, name, static_cast<void const*>(node),
                authored == incoming.end() ? "unmapped" : transform_report(authored->second),
                transform_report(node->local), transform_report(node->world),
                projected ? fmt::format("{:.6f},{:.6f},{:.6f}", x, y, z) : "unavailable");
        };
        trace_node("copy-root", root);
        for (std::string_view name : Trace_Body_Nodes)
        {
            const auto found = nodes.find(std::string(name));
            trace_node(name, found == nodes.end() ? nullptr : found->second);
        }
        const ClipPlayback& clip = active->asset->playback();
        for (std::size_t i = 0; i < clip.tracks.size(); ++i)
            if (clip.tracks[i] == 4 || clip.tracks[i] == 43)
                trace_node(clip.tracks[i] == 4 ? "track-4-target" : "track-43-target", clip.nodes[i]);
        trace_probe_limit();
    }

    oar::AnimationSelection select(RE::Actor& source, std::span<std::string_view const> bases,
        oar::PreviewState preview, Proposal& proposal, std::uint32_t channel, bool activation)
    {
        std::swap(query.conditions, proposal.conditions);
        oar::AnimationSelection result = oar::query_animation(source, graph->characterInstance, bases,
            std::move(preview), query, { channel, generation });
        std::swap(query.conditions, proposal.conditions);
        if (activation) proposal.history.activate(result.clip_key);
        // An interrupt checks replacement eligibility, not the next member of an accepted sequence.
        const oar::AnimationSelection* const accepted = !active ? nullptr : channel == 3 ?
            (active->torch_clip ? &*active->torch_clip : nullptr) : &active->stage.clip;
        if (accepted && (proposal.reason == ChoiceReason::e_interrupt || proposal.reason == ChoiceReason::e_refresh) &&
            result.replacement_key == accepted->replacement_key && result.base_index == accepted->base_index)
        {
            result.variant = accepted->variant;
            result.index = accepted->index;
            result.name = accepted->name;
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
        result.binding_name = oar::animation_binding_name(graph->characterInstance, result.index);
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
        if (action.kind == ActionKind::e_equip || action.kind == ActionKind::e_unequip)
            proposal->transition_preview = preview;
        if (detailed_animation_trace())
        {
            const RE::TESForm* const right = source.GetEquippedObject(false);
            const RE::TESForm* const left = source.GetEquippedObject(true);
            const auto hands = preview.hand_override.value_or(std::array<std::uint32_t, 2>{});
            std::string requested_bases;
            for (std::string_view base : bases) requested_bases += fmt::format("{}{}", requested_bases.empty() ? "" : "|", base);
            logger::info("ANIMATION QUERY actor={:08x} request={} accepted-request={} pending-action={} reason={} target={} bases='{}' source-hands=({:08x},{:08x}) source-drawn={} projected-drawn={} hand-override={} override-hands=({:08x},{:08x}) IsEquipping={} IsUnequipping={} effect-revision={} accepted-path='{}'",
                actor_id, generation, active ? active->generation : 0, action_name(action.kind), choice_reason_name(reason), weapon_form_name(target.form),
                requested_bases, right ? right->GetFormID() : 0, left ? left->GetFormID() : 0, source.IsWeaponDrawn(), preview.weapon_drawn,
                preview.hand_override.has_value(), hands[0], hands[1], preview.graph_variables.at("IsEquipping"), preview.graph_variables.at("IsUnequipping"),
                proposal->effect_revision, active ? active->stage.clip.name : "");
        }
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
            const std::optional<oar::BaseAnimation> base = oar::base_animation(graph->characterInstance, bases);
            if (base)
            {
                selected.found = true;
                selected.base_index = selected.index = base->index;
                selected.base_name = selected.name = selected.binding_name = base->name;
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
        selected.binding_name = oar::animation_binding_name(graph->characterInstance, selected.index);
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
        if (observed.left_torch && action.form == WeaponForm::kOneHanded)
        {
            oar::AnimationSelection torch = select(source, panel_torch_arm_bases(), preview_state(true), *proposal, 3, activation);
            if (torch.found) proposal->torch_clip = std::move(torch);
            else logger::warn("ANIMATION TORCH actor={:08x} request={} outcome=body-only reason=arm-base-unavailable diagnostic='{}'",
                actor_id, generation, torch.diagnostic);
        }
        const auto same_torch = [](Proposal const& first, Proposal const& second)
        {
            if (first.torch_clip.has_value() != second.torch_clip.has_value()) return false;
            return !first.torch_clip || (first.torch_clip->index == second.torch_clip->index &&
                first.torch_clip->replacement_key == second.torch_clip->replacement_key);
        };
        trace_decision("pending-choice", choice_reason_name(reason), proposal.get());
        if (active && active->asset->ready() && active->standing->ready() &&
            (!active->torch_clip || (active->torch && active->torch->ready())) &&
            reason != ChoiceReason::e_request && active->stage.action.kind == action.kind && active->direct == direct &&
            selected.index == active->stage.clip.index && selected.replacement_key == active->stage.clip.replacement_key && same_torch(*active, *proposal))
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
                if (proposal->torch_clip && proposal->torch_clip->variants)
                    variants.commit(*proposal->torch_clip->variants, proposal->torch_clip->variant, proposal->torch_clip->clip_key);
                active->stage.clip = std::move(selected);
            }
            active->generation = generation;
            active->effect_revision = proposal->effect_revision;
            active->transition_preview = std::move(proposal->transition_preview);
            active->torch_clip = std::move(proposal->torch_clip);
            if (active->torch && active->torch_clip) active->torch->refresh_selection(*active->torch_clip);
            idle_due = false;
            pending.reset();
            trace_decision("accepted-same-asset", choice_reason_name(reason), active.get());
            publish_mapping();
            return;
        }
        if (reusable && reusable->stage.action.kind == action.kind &&
            reusable->asset->selection().index == selected.index && reusable->stage.clip.replacement_key == selected.replacement_key)
        {
            proposal->asset = std::move(reusable->asset);
            proposal->asset->refresh_selection(selected);
            proposal->standing = std::move(reusable->standing);
            proposal->calibration = std::move(reusable->calibration);
            if (same_torch(*reusable, *proposal))
            {
                proposal->torch = std::move(reusable->torch);
                proposal->torch_seconds = reusable->torch_seconds;
                if (proposal->torch && proposal->torch_clip) proposal->torch->refresh_selection(*proposal->torch_clip);
            }
            else if (proposal->torch_clip)
                proposal->torch = std::make_unique<LoadedAsset>(graph, *proposal->torch_clip, true, false, generation, std::nullopt, "torch-arm");
            pending = std::move(proposal);
            trace_decision("reuse-pending-load", choice_reason_name(reason), pending.get());
            return;
        }
        const std::array<std::string_view, 1> standing_bases{ "Animations\\MT_Idle.hkx" };
        const std::optional<oar::BaseAnimation> base = oar::base_animation(graph->characterInstance, standing_bases);
        if (!base) return;
        oar::AnimationSelection standard;
        standard.found = true;
        standard.base_index = standard.index = base->index;
        standard.base_name = standard.name = standard.binding_name = base->name;
        proposal->standing = std::make_unique<LoadedAsset>(graph, standard, true, true, generation, std::nullopt, "standing");
        proposal->calibration = std::make_unique<LoadedAsset>(graph, standard, true, true, generation, std::nullopt, "calibration");
        proposal->asset = std::make_unique<LoadedAsset>(graph, selected, action.loop,
            action.kind == ActionKind::e_standing_idle, generation,
            target.empty() && !direct ? std::optional<oar::AnimationSelection>(standard) : std::nullopt);
        if (proposal->torch_clip)
        {
            proposal->torch = std::make_unique<LoadedAsset>(graph, *proposal->torch_clip, true, false, generation, std::nullopt, "torch-arm");
            if (active && same_torch(*active, *proposal)) proposal->torch_seconds = active->torch_seconds;
        }
        if (detailed_animation_trace())
            logger::info("ANIMATION actor={:08x} request={} proposal state={} choice={} base='{}' path='{}' index={} first-query-rejection='{}'",
                actor_id, generation, session->revision(), static_cast<int>(reason), selected.base_name, selected.name, selected.index, selected.diagnostic);
        pending = std::move(proposal);
    }

    void queue_idle(RE::Actor& source, ChoiceReason reason, Proposal* reusable = nullptr)
    {
        bool direct = target.direct || base_fallback;
        if (!direct)
        {
            const dac::IdleReadiness readiness = session->idle_readiness(generation);
            if (readiness == dac::IdleReadiness::e_pending)
            {
                if (!waiting_goal) trace_decision("waiting-goal-enter", choice_reason_name(reason), pending.get());
                waiting_goal = true; waiting_reason = reason; return;
            }
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
        if (waiting_goal) trace_decision("waiting-goal-leave", choice_reason_name(reason), pending.get());
        waiting_goal = false;
        std::vector<std::string_view> bases = target.direct || (direct && target.empty()) ?
            std::vector<std::string_view>{ "Animations\\MT_Idle.hkx" } : target.bases();
        const bool plain = target.direct || (direct && target.empty());
        // Opt-in marker evidence reads the same actual effects as OAR's HasMagicEffect.
        if (detailed_animation_trace())
            logger::info("ANIMATION actor={:08x} request={} smooth markers='{}'",
                actor_id, generation, session ? session->smooth_markers() : std::string{});
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

void AnimationController::prepare_base_indices(RE::Actor& source)
{
    const RE::BSTSmartPointer<RE::BShkbAnimationGraph> graph = source_graph(source);
    if (graph) (void)oar::prepare_base_animations(graph->characterInstance, "world-prewarm");
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
    const std::array<RE::FormID, 2> previous_hands = state.has_observation ? state.observed.hands : std::array<RE::FormID, 2>{};
    state.actor_id = source.GetFormID();
    state.source_handle = source.GetHandle();
    state.generation = observation.request_generation;
    state.pose_budget.request(state.generation);
    state.trace_torch_request = observation.left_torch || state.observed.left_torch ||
        target.form == WeaponForm::kTorch || previous_form == WeaponForm::kTorch;
    const auto is_axe = [](RE::FormID id)
    {
        RE::TESObjectWEAP const* const weapon = id ? RE::TESForm::LookupByID<RE::TESObjectWEAP>(id) : nullptr;
        return weapon && (weapon->GetWeaponType() == RE::WEAPON_TYPE::kOneHandAxe ||
            weapon->GetWeaponType() == RE::WEAPON_TYPE::kTwoHandAxe);
    };
    state.trace_axe_request = is_axe(observation.hands[0]) || is_axe(observation.hands[1]) ||
        is_axe(previous_hands[0]) || is_axe(previous_hands[1]);
    state.trace_shield_request = false;
    state.source_generation = observation.source_graph_generation;
    state.observed = observation;
    state.target = target;
    if (equipment_changed) state.base_fallback = false;
    state.has_observation = true;
    state.graph = std::move(graph);
    if (!state.graph) { state.pending.reset(); return; }
    if (behavior_graph_probe())
        behavior_graph::observe_source_baseline(*state.graph, state.actor_id, state.generation, state.source_generation);
    if (observation.reason == "panel-open")
        behavior_graph::run_independent_graph_experiment(*state.graph, state.actor_id, state.generation, state.source_generation);
    logger::info("ANIMATION actor={:08x} request={} reason='{}' source-generation={} equipment-target={}", state.actor_id,
        state.generation, observation.reason, state.source_generation, weapon_form_name(target.form));
    if (observation.reason == "panel-open" || observation.reason == "equipment")
    {
        const auto started = std::chrono::steady_clock::now();
        const std::string report = oar::world_clip_report(state.graph->characterInstance);
        const double build_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        logger::info("ANIMATION WORLD actor={:08x} request={} snapshot=paused-source reason='{}' hands=({:08x},{:08x}) drawn={} build-ms={:.3f} clips={}",
            state.actor_id, state.generation, observation.reason, observation.hands[0], observation.hands[1],
            source.IsWeaponDrawn(), build_ms, report);
    }
    // Panel-open forces the observed loop phase; an equipment change must NOT, or the equip
    // clip's own Start markers (cast by its annotations) are overwritten by loop markers before
    // the Start member can be selected, which skips the world's Equip -> Start -> Loop order.
    const bool transition_in_progress = !equipment_changed &&
        ((state.pending && state.pending->transition_preview) ||
            (state.active && state.active->transition_preview && !state.clock.finished()));
    if (!target.direct && !state.base_fallback && observation.reason != "equipment" && !transition_in_progress)
        session.prepare_idle(state.generation, !equipment_changed && !new_lifetime);
    if (!equipment_changed && state.active) state.active->generation = state.generation;
    std::unique_ptr<Proposal> loading = std::move(state.pending);
    state.idle_due = false;

    // An in-inventory equipment change plays the world's draw/sheathe move once, then hands
    // over to the idle (the boundary in advance() queues the idle). Opening the panel keeps the
    // immediate-idle behaviour: there is no world transition to mirror at that edge.
    if (equipment_changed && observation.reason == "equipment")
    {
        bool unequip = target.empty();
        WeaponForm form = unequip ? previous_form : target.form;
        std::optional<ShieldTransition> shield_transition;
        if (target.form == WeaponForm::kOneHanded)
        {
            const auto is_shield = [](RE::FormID id)
            {
                const RE::TESObjectARMO* const armor = id ? RE::TESForm::LookupByID<RE::TESObjectARMO>(id) : nullptr;
                return armor && armor->IsShield();
            };
            shield_transition = left_shield_transition(previous_hands, observation.hands,
                is_shield(previous_hands[1]), is_shield(observation.hands[1]));
            if (shield_transition) unequip = shield_transition->kind == ActionKind::e_unequip;
            state.trace_shield_request = shield_transition.has_value();
        }
        if (state.trace_torch_request)
        {
            const auto kind_of = [](RE::FormID a_id) -> WeaponForm
            {
                const RE::TESForm* item = a_id ? RE::TESForm::LookupByID(a_id) : nullptr;
                if (!item) return WeaponForm::kNone;
                if (item->As<RE::TESObjectLIGH>()) return WeaponForm::kTorch;
                if (const RE::TESObjectWEAP* const weapon = item->As<RE::TESObjectWEAP>())
                {
                    switch (weapon->GetWeaponType())
                    {
                    case RE::WEAPON_TYPE::kBow: return WeaponForm::kBow;
                    case RE::WEAPON_TYPE::kCrossbow: return WeaponForm::kCrossbow;
                    case RE::WEAPON_TYPE::kTwoHandSword: return WeaponForm::kTwoHanded;
                    case RE::WEAPON_TYPE::kTwoHandAxe: return WeaponForm::kTwoHandedAxe;
                    case RE::WEAPON_TYPE::kStaff: return WeaponForm::kStaff;
                    default: return WeaponForm::kOneHanded;
                    }
                }
                if (item->IsArmor()) return WeaponForm::kShield;
                if (item->Is(RE::FormType::Spell) || item->Is(RE::FormType::Scroll) || item->Is(RE::FormType::Shout))
                    return WeaponForm::kMagic;
                return WeaponForm::kNone;
            };
            WeaponForm changed_form = WeaponForm::kNone;
            bool changed_unequip = false;
            for (std::size_t hand = 0; hand < 2; ++hand)
            {
                if (previous_hands[hand] == observation.hands[hand]) continue;
                const WeaponForm current = kind_of(observation.hands[hand]);
                const WeaponForm previous = kind_of(previous_hands[hand]);
                if (current == WeaponForm::kTorch && (previous == WeaponForm::kNone || previous == WeaponForm::kTorch)) continue;
                if (previous == WeaponForm::kTorch && current == WeaponForm::kNone) continue;
                if (current != WeaponForm::kNone && current != WeaponForm::kTorch) changed_form = current;
                else { changed_form = previous; changed_unequip = true; }
            }
            form = changed_form == WeaponForm::kTorch ? WeaponForm::kNone : changed_form;
            if (form != WeaponForm::kNone) unequip = changed_unequip;
        }
        const std::vector<std::string_view> bases = panel_transition_bases(form, unequip);
        if (!bases.empty() && oar::base_animation(state.graph->characterInstance, bases))
        {
            oar::PreviewState preview = preview_state(!unequip);
            preview.hand_override = transition_hands(unequip ? ActionKind::e_unequip : ActionKind::e_equip, previous_hands);
            logger::info("ANIMATION actor={:08x} request={} transition={} form={} query-hands={} previous=({:08x},{:08x}) current=({:08x},{:08x})",
                state.actor_id, state.generation, unequip ? "unequip" : "equip", weapon_form_name(form),
                preview.hand_override ? "previous" : "current", previous_hands[0], previous_hands[1], observation.hands[0], observation.hands[1]);
            state.queue(source, bases, { unequip ? ActionKind::e_unequip : ActionKind::e_equip, form, false },
                std::move(preview), ChoiceReason::e_request, false, false, loading.get());
            return;
        }
        logger::warn("ANIMATION actor={:08x} request={} transition base unavailable form={}; idle only", state.actor_id,
            state.generation, weapon_form_name(form));
    }
    Proposal* const retained_transition = loading && loading->transition_preview ? loading.get() :
        state.active && state.active->transition_preview && !state.clock.finished() ? state.active.get() : nullptr;
    if (!equipment_changed && retained_transition)
    {
        const ActionPrototype action = retained_transition->stage.action;
        state.queue(source, panel_transition_bases(action.form, action.kind == ActionKind::e_unequip), action,
            *retained_transition->transition_preview, ChoiceReason::e_refresh, retained_transition->direct, false, loading.get());
    }
    else state.queue_idle(source, equipment_changed ? ChoiceReason::e_request : ChoiceReason::e_refresh, loading.get());
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
            proposal->standing->rebind(root, state.generation);
            if (proposal->calibration) proposal->calibration->rebind(root, state.generation);
            proposal->asset->rebind(root, state.generation);
            if (proposal->torch) proposal->torch->rebind(root, state.generation);
        }
    state.last_tick = std::chrono::steady_clock::now();
    logger::info("ANIMATION actor={:08x} request={} graph retarget={} time={:.2f}", state.actor_id,
        state.generation, static_cast<void*>(&root), state.clock.seconds());
    state.begin_pose_probe("retarget");
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
            if (detailed_animation_trace())
                logger::info("ANIMATION EFFECT-REV actor={:08x} request={} accepted-request={} previous={} current={}",
                    state.actor_id, state.generation, state.active ? state.active->generation : 0, state.observed_effect_revision, revision);
            state.trace_decision("effect-revision", "observed-native-effects", state.active.get());
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
    if (state.pending->generation != state.generation)
    {
        state.trace_decision("cancel-pending", "stale-request", state.pending.get());
        state.pending.reset(); return;
    }
    const std::uint64_t current_effect_revision = state.session->revision();
    if (state.pending->effect_revision != current_effect_revision)
    {
        const auto source = state.source_handle.get();
        if (!source)
        {
            state.trace_decision("cancel-pending", "effect-refresh-no-source", state.pending.get());
            state.pending.reset(); return;
        }
        if (detailed_animation_trace())
            logger::info("ANIMATION PENDING-REV actor={:08x} request={} accepted-request={} proposal={} observed={} current={}",
                state.actor_id, state.generation, state.active ? state.active->generation : 0,
                state.pending->effect_revision, state.observed_effect_revision, current_effect_revision);
        // Rebuild the exact typed intent against the new snapshot; accepted playback continues.
        std::unique_ptr<Proposal> old = std::move(state.pending);
        state.trace_decision("refresh-pending", old->transition_preview ? "effect-revision-retain-transition-intent" : "effect-revision-requery-idle", old.get());
        if (old->transition_preview)
            state.queue(*source, panel_transition_bases(old->stage.action.form, old->stage.action.kind == ActionKind::e_unequip),
                old->stage.action, *old->transition_preview, old->reason, old->direct, false, old.get());
        else state.queue_idle(*source, old->reason, old.get());
        return;
    }
    Proposal& proposal = *state.pending;
    proposal.standing->poll(*state.root);
    if (proposal.calibration) proposal.calibration->poll(*state.root);
    proposal.asset->poll(*state.root);
    if (proposal.torch)
    {
        proposal.torch->poll(*state.root);
        if (proposal.torch->failed())
        {
            logger::warn("ANIMATION TORCH actor={:08x} request={} outcome=body-only reason=arm-load-failed path='{}'",
                state.actor_id, state.generation, proposal.torch->selection().name);
            proposal.torch.reset();
            proposal.torch_clip.reset();
        }
        else if (!proposal.torch->ready()) return;
        else proposal.torch_clip = proposal.torch->selection();
    }
    if (proposal.standing->failed() || proposal.asset->failed())
    {
        state.trace_decision("cancel-pending", "target-or-standing-load-failed", state.pending.get());
        logger::warn("ANIMATION actor={:08x} request={} load failed path='{}'; visible playback retained", state.actor_id,
            state.generation, proposal.stage.clip.name);
        state.pending.reset();
        return;
    }
    if (proposal.calibration && proposal.calibration->failed())
    {
        state.trace_decision("drop-calibration", "calibration-load-failed", state.pending.get());
        proposal.calibration.reset();
    }
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
    const bool retain_body_clock = state.outgoing && state.active->stage.action.loop && state.outgoing->stage.action.loop &&
        state.active->stage.action.kind == state.outgoing->stage.action.kind &&
        state.active->stage.clip.index == state.outgoing->stage.clip.index &&
        state.active->stage.clip.replacement_key == state.outgoing->stage.clip.replacement_key &&
        (state.active->torch_clip.has_value() || state.outgoing->torch_clip.has_value());
    if (state.outgoing && state.active->torch_clip && state.outgoing->torch_clip &&
        state.active->torch_clip->index == state.outgoing->torch_clip->index &&
        state.active->torch_clip->replacement_key == state.outgoing->torch_clip->replacement_key)
        state.active->torch_seconds = state.outgoing->torch_seconds;
    state.variants = std::move(state.active->history);
    state.query.conditions = std::move(state.active->conditions);
    const auto& selection = state.active->stage.clip;
    if (selection.variants) state.variants.commit(*selection.variants, selection.variant, selection.clip_key);
    if (state.active->torch_clip && state.active->torch_clip->variants)
        state.variants.commit(*state.active->torch_clip->variants, state.active->torch_clip->variant, state.active->torch_clip->clip_key);
    const ClipPlayback& clip = state.active->asset->playback();
    auto annotations = clip_annotations(*clip.animation);
    state.handoff_seconds = std::max(static_cast<double>(clip.duration), 0.05) * 0.5;
    const char* handoff_source = "duration-midpoint";
    for (const Annotation& annotation : annotations)
        if (annotation.event == "weaponDraw" || annotation.event == "weaponSheathe")
        {
            state.handoff_seconds = annotation.time;
            handoff_source = annotation.event == "weaponDraw" ? "weaponDraw" : "weaponSheathe";
            break;
        }
    if (!retain_body_clock)
        state.clock.start(std::move(annotations), std::max(static_cast<double>(clip.duration), 0.05), 0.0, false);
    state.blend_seconds = 0.0;
    state.idle_due = false;
    state.last_tick = std::chrono::steady_clock::now();
    if (state.blend_duration == 0.0) state.outgoing.reset();
    state.trace_decision("accepted-handover", choice_reason_name(state.active->reason), state.active.get());
    if (state.active->torch_clip)
        logger::info("ANIMATION TORCH actor={:08x} request={} outcome=composed body='{}' arm='{}' index={} mask=left-arm time={:.3f}",
            state.actor_id, state.generation, state.active->stage.clip.name, state.active->torch_clip->name,
            state.active->torch_clip->index, state.active->torch_seconds);
    if (detailed_animation_trace())
        logger::info("ANIMATION HANDOFF actor={:08x} request={} accepted-request={} action={} index={} path='{}' handoff-seconds={:.4f} source={} duration={:.4f}",
            state.actor_id, state.generation, state.active->generation, action_name(state.active->stage.action.kind), selection.index,
            selection.name, state.handoff_seconds, handoff_source, clip.duration);
    state.publish_mapping();
    state.begin_pose_probe("handover");
}

bool AnimationController::ready() const
{
    return m_state->active && m_state->active->asset->ready() && m_state->active->standing->ready() &&
        (!m_state->active->torch || m_state->active->torch->ready());
}
std::uint64_t AnimationController::diagnostic_accepted_request() const { return m_state->active ? m_state->active->generation : 0; }
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
    // v27 display handoff: with DisplayPose on and a candidate retained, the panel shows that
    // candidate's pose instead of the OAR sampler. The pose is frozen at the last driven frame;
    // this branch adds no native call, it only reads the retained character's poseLocal.
    if (display_pose())
    {
        PoseBuffer held;
        if (behavior_graph::held_display_pose(held))
        {
            static bool display_source_logged = false;
            if (!display_source_logged)
            {
                display_source_logged = true;
                logger::info("ANIMATION DISPLAY source=independent-graph bones={} note=frozen-at-last-driven-step", held.size());
            }
            state.pose.clear();
            for (auto& entry : held)
                state.pose.emplace(entry.first, entry.second);
            for (auto const& [name, local] : state.pose)
                if (auto target = state.nodes.find(name); target != state.nodes.end())
                    target->second->local = local;
            recompute_subtree_worlds(root, 0);
            return state.pose.size();
        }
    }
    const auto now = std::chrono::steady_clock::now();
    const double delta = std::clamp(std::chrono::duration<double>(now - state.last_tick).count(), 0.0, 0.1);
    state.last_tick = now;
    const auto source = state.source_handle.get();
    ChoiceReason reason = ChoiceReason::e_interrupt;
    state.clock.advance(delta, state.active->stage.action.loop,
        [](double) {},
        [&](Annotation const& annotation)
        {
            if (detailed_animation_trace())
                logger::info("ANIMATION ANNOTATION actor={:08x} request={} accepted-request={} action={} index={} path='{}' event='{}' annotation-time={:.3f} clip-time={:.3f} loop={} decision={}",
                    state.actor_id, state.generation, state.active->generation, action_name(state.active->stage.action.kind), state.active->stage.clip.index,
                    state.active->stage.clip.name, annotation.event, annotation.time, state.clock.seconds(), state.active->stage.action.loop,
                    state.active->direct ? "direct-no-dispatch" : "queue-native-DAC");
            if (!state.active->direct) state.session->dispatch(annotation.event, state.active->generation);
        },
        [&]()
        {
            state.idle_due = true;
            reason = ChoiceReason::e_boundary;
            if (!state.pose_profile.empty() && state.active->stage.action.loop) ++state.pose_probe_wraps;
            state.trace_decision("boundary", "accepted-clock-boundary", state.active.get());
        });
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
    state.active->torch_seconds += delta;
    if (state.active->torch && state.active->torch->ready())
    {
        const PoseBuffer arm = sample_animation_clip(state.active->torch->playback(), static_cast<float>(state.active->torch_seconds), true);
        compose_contributor(state.pose, std::span(arm), BoneMask::e_left_arm);
    }
    if (state.outgoing)
    {
        state.outgoing_seconds += delta;
        state.blend_seconds += delta;
        const float weight = static_cast<float>(std::min(state.blend_seconds / state.blend_duration, 1.0));
        std::unordered_map<std::string, RE::NiTransform> before;
        const PoseBuffer previous_base = sample_animation_clip(state.outgoing->standing->playback(), static_cast<float>(state.base_seconds), true);
        compose_contributor(before, std::span(previous_base), BoneMask::e_full_body);
        const PoseBuffer previous_body = sample_animation_clip(state.outgoing->asset->playback(), static_cast<float>(state.outgoing_seconds), state.outgoing->stage.action.loop);
        compose_contributor(before, std::span(previous_body), BoneMask::e_full_body);
        state.outgoing->torch_seconds += delta;
        if (state.outgoing->torch && state.outgoing->torch->ready())
        {
            const PoseBuffer arm = sample_animation_clip(state.outgoing->torch->playback(), static_cast<float>(state.outgoing->torch_seconds), true);
            compose_contributor(before, std::span(arm), BoneMask::e_left_arm);
        }
        for (auto const& [name, local] : before)
            if (auto incoming = state.pose.find(name); incoming != state.pose.end()) incoming->second = blend_panel_pose(local, incoming->second, weight);
        if (weight >= 1.0f) state.outgoing.reset();
    }
    for (auto const& [name, local] : state.pose)
        if (auto target = state.nodes.find(name); target != state.nodes.end()) target->second->local = local;
    recompute_subtree_worlds(root, 0);
    state.trace_pose(current, now);
    return state.pose.size();
}
}
PLUGIN_NAMESPACE_END
