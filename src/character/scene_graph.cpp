//
// Created by AmazingBuff on 2026/10/19.
//
#include "character/scene_graph.h"
#include "character/equipment_state.h"
#include "character/figure_audit.h"
#include "character/snapshot_transform.h"
#include "animation/clip.h"
#include "render/studio/figure_passes.h"
#include "render/studio/light.h"
#include "RE/B/BSAnimationGraphManager.h"
#include "RE/B/BShkbAnimationGraph.h"
PLUGIN_NAMESPACE_BEGIN
namespace
{
    constexpr float Studio_Figure_Scale = 0.35f;
    constexpr float Studio_Depth = 485.0f;

    struct SkinnedBound
    {
        RE::NiPoint3 center;
        float radius;
        bool valid;
    };

    void restore_fade(RE::NiAVObject* graph)
    {
        if (RE::BSFadeNode* fade = graph->AsFadeNode())
        {
            fade->unk128 = 1.0f;
            fade->unk12C = 0.0f;
            fade->currentFade = 1.0f;
        }
    }

    bool is_collision_helper(const char* name)
    {
        return name && name[0] && figure_is_collision_helper(name);
    }

    // Weapon attachment subtrees (WeaponSword/WeaponDagger/Quiver/Ammo under the skeleton). The
    // figure_audit oracle already classes the Weapon* nodes as attachment outliers the animation
    // does not drive (AFP8); their geometry rides wherever the captured pose parked them, so a
    // bow or quiver drags the framing bound and the re-centering off the body — the oar-idle2
    // report's bow/claw offset (problem 4). Framing measure only; the geometry still draws.
    bool is_framing_attachment(RE::NiAVObject* object)
    {
        for (RE::NiAVObject* node = object; node; node = node->parent)
        {
            const char* const name = node->name.c_str();
            if (!name || !name[0])
                continue;
            if (std::strstr(name, "Weapon") != nullptr || std::strstr(name, "Quiver") != nullptr ||
                std::strstr(name, "Ammo") != nullptr)
                return true;
        }
        return false;
    }

    // A captured player graph carries more than the character: spell and ability visuals,
    // blood decals, script-spawned markers and the effect's own light are parented into it
    // and were never drawn by the world renderer, so their device buffers stay null and
    // they cannot be drawn in the studio either. Keep the body, its gear and equipped
    // weapons, and drop the rest (the user asked for exactly this scope).
    bool is_effect_object(RE::NiAVObject* object)
    {
        if (object->AsParticlesGeom())
            return true;
        // Spell-effect meshes (rings, flares, breath cones) carry an effect shader
        // property whatever they are named. The name tokens below miss whole
        // families (Circle/GeoSphere/Tube all survived a prune), and an effect
        // geometry's pass generation inside the UI3D accumulator window is the
        // one engine call there that can flip DIRTY_RENDERTARGET — which makes
        // every later SetupAndDrawPass re-apply the engine's ledger target
        // instead of the studio one (v6.45 mechanism).
        if (RE::BSGeometry* const geometry = object->AsGeometry())
        {
            const RE::BSShaderProperty* const property = geometry->GetGeometryRuntimeData().shaderProperty.get();
            const char* const shader_rtti = property && property->GetRTTI() ? property->GetRTTI()->GetName() : nullptr;
            if (shader_rtti && std::strstr(shader_rtti, "EffectShader") != nullptr)
                return true;
        }
        if (const char* name = object->name.c_str(); name && name[0] && is_collision_helper(name))
            return true;
        constexpr std::string_view tokens[] = { "Blood", "Flash", "Wisps", "SuperSpray", "pSmallFlare", "Scb", "Diamond", "ParticleSystem" };
        const std::string_view view{ object->name.c_str() ? object->name.c_str() : "" };
        for (const std::string_view token : tokens)
            if (view.find(token) != std::string_view::npos)
                return true;
        // A duplicated spell light would keep lighting the world from the copy. NiAVObject
        // exposes no AsLight(), so the runtime type name is the available discriminator.
        const char* const rtti = object->GetRTTI() ? object->GetRTTI()->GetName() : nullptr;
        return rtti && std::strstr(rtti, "Light") != nullptr;
    }

    size_t prune_effect_objects(RE::NiAVObject* root)
    {
        std::vector<RE::NiNode*> stack{ root->AsNode() };
        std::vector<RE::NiAVObject*> dropped;
        while (!stack.empty())
        {
            RE::NiNode* node = stack.back();
            stack.pop_back();
            if (!node)
                continue;
            for (const RE::NiPointer<RE::NiAVObject>& child : node->children)
            {
                if (!child)
                    continue;
                if (is_effect_object(child.get()))
                {
                    dropped.push_back(child.get());
                    continue;
                }
                if (RE::NiNode* child_node = child->AsNode())
                    stack.push_back(child_node);
            }
        }
        for (RE::NiAVObject* object : dropped)
        {
            if (RE::NiNode* parent = object->parent)
                parent->DetachChild(object);
        }
        return dropped.size();
    }

    void union_sphere(SkinnedBound& out, const RE::NiPoint3& center, float radius)
    {
        if (!out.valid)
        {
            out.center = center;
            out.radius = radius;
            out.valid = true;
            return;
        }
        const RE::NiPoint3 d = center - out.center;
        const float dist = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        if (out.radius >= radius + dist)
            return;  // fully contained
        if (radius >= out.radius + dist)
        {
            out.center = center;
            out.radius = radius;
            return;
        }
        const float merged = (dist + out.radius + radius) * 0.5f;
        const float t = dist > 1e-6f ? (merged - out.radius) / dist : 0.0f;
        out.center = out.center + d * t;
        out.radius = merged;
    }

    SkinnedBound measure_skinned_bound(RE::NiAVObject* root)
    {
        SkinnedBound out{.radius = 0.f, .valid = false};
        RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* geometry)
        {
            if (!geometry->GetGeometryRuntimeData().skinInstance)
                return RE::BSVisit::BSVisitControl::kContinue;
            // Framing must follow the visible figure. Body mods skin their collision helpers
            // (3BA/UBE 3BCA_* parts, VirtualGround, CollisionStopper) with the same skeleton,
            // and those sit far enough from the body to drag the bound and the centering off
            // it — which is what pushed the UBE figure out of frame.
            if (is_collision_helper(geometry->name.c_str()))
                return RE::BSVisit::BSVisitControl::kContinue;
            // Weapon attachment geometry (bow, quiver, claws on their Weapon* nodes) is the same
            // class of framing pollution: carried at whatever offset the captured pose holds it,
            // not part of the standing body the studio frames (oar-idle2 problem 4).
            if (is_framing_attachment(geometry))
                return RE::BSVisit::BSVisitControl::kContinue;
            const auto& wb = geometry->worldBound;
            union_sphere(out, wb.center, wb.radius);
            return RE::BSVisit::BSVisitControl::kContinue;
        });
        return out;
    }

}

SceneGraph::SceneGraph(RE::Actor& source)
{
    m_monitor.source = source.GetHandle();
    m_source_id = source.GetFormID();
}
SceneGraph::~SceneGraph()
{
    close_session();
    retire_graph(std::move(m_graph_object));
}

void SceneGraph::request(Command command)
{
    m_command.store(command, std::memory_order_release);
}

bool SceneGraph::update(PreviewFrame const& frame, std::uint64_t revision)
{
    m_usable = frame.usable;
    const Command command = m_command.exchange(Command::e_none, std::memory_order_acq_rel);
    if (command != Command::e_none && !frame.usable)
        logger::info("SceneGraph actor={:08x} command={} ignored outside paused inventory", source_id(), static_cast<int>(command));
    const auto source = m_monitor.source.get();
    if (!source)
    {
        close_session();
        return false;
    }
    if (!source->Get3D(false))
    {
        close_session();
        (void)update_source_graph(*source);
        return true;
    }
    update_monitor(*source, revision, frame.paused, frame.open, frame.opening, frame.closing, frame.usable);
    if (!frame.usable) return true;
    if (command == Command::e_toggle_draw) toggle_draw();
    if (capture_due(*source, revision, command == Command::e_capture))
    {
        RE::NiPointer<RE::NiAVObject> graph = capture_graph(*source);
        const bool accepted = graph != nullptr;
        if (accepted) replace_graph(std::move(graph));
        capture_result(accepted, frame.opening);
    }
    if (m_graph_object) m_animation.retarget(*m_graph_object);
    m_animation.poll();
    return true;
}

void SceneGraph::replace_graph(RE::NiPointer<RE::NiAVObject> graph)
{
    RE::NiPointer<RE::NiAVObject> previous = std::move(m_graph_object);
    m_graph_object = std::move(graph);
    m_snapshot_nodes.clear();
    m_recentre_pending = true;
    restore_fade(m_graph_object.get());
    const size_t pruned = prune_effect_objects(m_graph_object.get());
    m_snapshot_root_world = m_graph_object->world;
    const SkinnedBound bound = measure_skinned_bound(m_graph_object.get());
    m_snapshot_center = bound.valid ? bound.center : m_graph_object->worldBound.center;
    logger::info("SCOPY FRAME actor={:08x} skinned-bound valid={} radius={:.1f} center=({:.1f},{:.1f},{:.1f}) graph-bound radius={:.1f} scale={:.2f} pruned-nodes={}",
        source_id(), bound.valid, bound.radius, m_snapshot_center.x, m_snapshot_center.y, m_snapshot_center.z, m_graph_object->worldBound.radius, Studio_Figure_Scale, pruned);
    RE::BSVisit::TraverseScenegraphObjects(m_graph_object.get(), [&](RE::NiAVObject* object)
    {
        m_snapshot_nodes.push_back({ object, object->world, object->worldBound });
        return RE::BSVisit::BSVisitControl::kContinue;
    });
    m_animation.retarget(*m_graph_object);
    retire_graph(std::move(previous));
}

bool SceneGraph::update_source_graph(RE::Actor& source)
{
    MonitorState& monitor = m_monitor;
    RE::NiAVObject* const root = source.Get3D(false);
    RE::BSTSmartPointer<RE::BSAnimationGraphManager> graphs;
    void const* source_graph = nullptr;
    if (source.GetAnimationGraphManager(graphs) && graphs)
        for (auto const& graph : graphs->graphs)
            if (graph && graph->holder == &source && graph->rootNode == root) { source_graph = graph.get(); break; }
    if (monitor.source_root != root || monitor.source_graph != source_graph)
    {
        monitor.source_root = root;
        monitor.source_graph = source_graph;
        ++monitor.source_graph_generation;
        monitor.dirty_reason = "source-graph";
        monitor.capture_pending = true;
        return true;
    }
    return false;
}

void SceneGraph::observe_source(RE::Actor& source, bool opening)
{
    MonitorState& monitor = m_monitor;
    animation::SourceObservation& observation = monitor.observed;
    (void)update_source_graph(source);
    const RunningIdleSample sample = strongest_idle_clip(source);
    if (!opening) observation.drawn = source.IsWeaponDrawn();
    const animation::ClipStance stance = animation::clip_stance(sample.body_name);
    if (stance != animation::ClipStance::e_undecided)
        observation.drawn = stance == animation::ClipStance::e_drawn;
    observation.form = equipment_weapon_form(source);
    observation.hands = equipment_hand_ids(source);
    RE::TESForm const* const right = source.GetEquippedObject(false);
    RE::TESForm const* const left = source.GetEquippedObject(true);
    observation.raw_hands = { right ? right->GetFormID() : 0, left ? left->GetFormID() : 0 };
    const bool right_spell = right && (right->Is(RE::FormType::Spell) || right->Is(RE::FormType::Scroll));
    const bool left_spell = left && (left->Is(RE::FormType::Spell) || left->Is(RE::FormType::Scroll));
    RE::TESObjectWEAP const* const right_weapon = right ? right->As<RE::TESObjectWEAP>() : nullptr;
    RE::TESObjectWEAP const* const left_weapon = left ? left->As<RE::TESObjectWEAP>() : nullptr;
    observation.left_primary = (observation.form == animation::WeaponForm::kMagic && left_spell && !right_spell) ||
        (observation.form == animation::WeaponForm::kStaff && left_weapon &&
            left_weapon->GetWeaponType() == RE::WEAPON_TYPE::kStaff &&
            (!right_weapon || right_weapon->GetWeaponType() != RE::WEAPON_TYPE::kStaff));
    observation.clip_name = sample.body_name;
    observation.clip_index = sample.body_index;
    observation.clip_time = sample.body_time;
    observation.clip_looping = sample.body_looping;
    observation.source_graph_generation = monitor.source_graph_generation;
}

void SceneGraph::schedule_animation(RE::Actor& source, std::uint64_t revision, std::string reason)
{
    MonitorState& monitor = m_monitor;
    if (reason == "equipment")
    {
        const auto previous_hands = monitor.observed.hands;
        const bool drawn = monitor.observed.drawn;
        observe_source(source, true);
        monitor.observed.drawn = drawn;
        if (previous_hands != monitor.observed.hands)
            monitor.observed.drawn = monitor.observed.form != animation::WeaponForm::kNone || source.IsInCombat();
    }
    monitor.equipment_revision = revision;
    monitor.observed.request_generation = ++monitor.request_generation;
    monitor.observed.reason = std::move(reason);
    monitor.dirty_reason.clear();
    monitor.session_active = true;
    m_animation.request(source, monitor.observed);
    if (m_graph_object) m_animation.retarget(*m_graph_object);
}

void SceneGraph::close_session()
{
    if (m_monitor.session_active) m_animation.reset();
    m_monitor.session_active = false;
    m_monitor.capture_pending = true;
    m_monitor.dirty_reason.clear();
}

void SceneGraph::update_monitor(RE::Actor& source, std::uint64_t revision,
    bool paused, bool open, bool opening, bool closing, bool usable)
{
    if (!paused) observe_source(source, false);
    if (!open)
    {
        if (closing) close_session();
        m_monitor.refresh.reset(revision);
        return;
    }
    if (!usable) return;
    if (opening)
    {
        close_session();
        observe_source(source, true);
        schedule_animation(source, revision, "panel-open");
        m_monitor.refresh.reset(revision);
    }
    else if (revision != m_monitor.equipment_revision)
    {
        schedule_animation(source, revision, "equipment");
        m_monitor.refresh.retry();
    }
    else if (update_source_graph(source))
    {
        observe_source(source, true);
        schedule_animation(source, revision, "source-graph");
    }
}

bool SceneGraph::capture_due(RE::Actor& source, std::uint64_t revision, bool manual)
{
    const bool sample = m_monitor.refresh.needs_sample(revision);
    const bool settled = sample && m_monitor.refresh.sample(revision, figure_measure_assembly(*source.Get3D(false)).signature());
    return m_monitor.capture_pending || settled || manual;
}

void SceneGraph::capture_result(bool accepted, bool opening)
{
    if (accepted)
    {
        m_monitor.capture_pending = false;
        if (opening) m_monitor.draw_enabled = true;
    }
    else m_monitor.refresh.retry();
}

void SceneGraph::toggle_draw()
{
    m_monitor.draw_enabled = !m_monitor.draw_enabled;
    logger::info("SceneGraph draw actor={:08x} enabled={}", source_id(), m_monitor.draw_enabled);
}

void SceneGraph::pose()
{
    RE::NiPoint3 anchor{ 0.0f, 0.0f, 0.0f };
    if (const RE::UI3DSceneManager* ui3d = RE::UI3DSceneManager::GetSingleton())
    {
        if (RE::NiCamera* cam = ui3d->camera.get())
        {
            const auto& w2c = cam->GetRuntimeData().worldToCam;
            anchor = RE::NiPoint3{ w2c[2][0] * Studio_Depth, w2c[2][1] * Studio_Depth, w2c[2][2] * Studio_Depth };
        }
    }
    m_anchor = anchor;

    RE::NiUpdateData update_data{
        0.0f,
        RE::NiUpdateData::Flag::kDirty
    };

    if (m_recentre_pending)
    {
        // Calibration pass (user design, oar-idle3): the placement must not be solved against
        // the CAPTURED pose — a jump center sits high and a sneak center low, which parks the
        // standing figure off the anchor. Instead the raw placement puts the captured graph in
        // camera space, the standing idle writes its first frame over it, and the calibration
        // below measures THAT body and freezes the correction into the root. Every later frame
        // and every other animation replays on this fixed root without measuring anything.
        RE::NiTransform raw_placement;
        raw_placement.translate = anchor;
        raw_placement.rotate.SetEulerAnglesXYZ(0.0f, 0.0f, 0.0f);
        raw_placement.scale = Studio_Figure_Scale;
        const RE::NiTransform delta = snapshot_delta(m_snapshot_root_world, raw_placement, m_snapshot_center, anchor);
        m_graph_object->local = delta * m_snapshot_root_world;
        for (const SnapshotNode& node : m_snapshot_nodes)
        {
            node.object->world = delta * node.world;
            node.object->previousWorld = node.object->world;
            node.object->worldBound.center = delta * node.bound.center;
            node.object->worldBound.radius = std::abs(delta.scale) * node.bound.radius;
        }
        return;  // the animation bridge calibrates after it first writes the idle
    }

    // Steady state: apply the calibrated root and cascade the whole tree through the engine.
    // No per-frame solving and no manual world rewrite — the cascade is authoritative for
    // every node, animated or not, so a weapon parented under a driven hand bone follows the
    // animation instead of being pinned back to its captured world each frame
    // (oar-idle3 report problem 2). previousWorld is synced for the skin pass's motion blur
    // bookkeeping only.
    m_graph_object->local = m_calibrated_root;
    m_graph_object->UpdateDownwardPass(update_data, 0);
    m_graph_object->UpdateWorldBound();
    for (const SnapshotNode& node : m_snapshot_nodes)
        node.object->previousWorld = node.object->world;
}

void SceneGraph::calibrate_from_posed_bound()
{
    if (!m_graph_object)
        return;
    // The animation bridge just wrote the standing idle's first frame over the raw placement.
    // Measure THAT body and freeze the correction: the placement standard becomes the idle's
    // own bound center, identical no matter what pose the player was captured in (user design,
    // oar-idle3). UpdateDownwardPass cascades the whole tree correctly; node worlds are never
    // rewritten by hand here (the white-silhouette hotfix lesson stands).
    RE::NiUpdateData update_data{ 0.0f, RE::NiUpdateData::Flag::kDirty };
    m_graph_object->UpdateDownwardPass(update_data, 0);
    m_graph_object->UpdateWorldBound();
    const SkinnedBound posed = measure_skinned_bound(m_graph_object.get());
    if (!posed.valid)
    {
        // No measurable body yet (bare-skeleton race): drop the request but keep the raw
        // placement as the steady-state root. An unset root is identity, which would park the
        // figure at the world origin on the next pose() (round 16).
        m_calibrated_root = m_graph_object->local;
        m_recentre_pending = false;
        logger::warn("SCOPY FRAME calibration skipped: no measurable skinned bound (raw placement kept)");
        return;
    }
    const RE::NiPoint3 offset{ m_anchor.x - posed.center.x, m_anchor.y - posed.center.y,
        m_anchor.z - posed.center.z };
    m_calibrated_root = m_graph_object->local;
    m_calibrated_root.translate.x += offset.x;
    m_calibrated_root.translate.y += offset.y;
    m_calibrated_root.translate.z += offset.z;
    m_recentre_pending = false;
    // Re-pose right away: the calibration runs inside the draw window between pose() and pass
    // submission, so the correction lands on the FIRST visible frame. The cascade from the new
    // root is authoritative for every node (animated locals and captured locals alike) — the
    // steady-state manual rewrite of captured worlds would clobber the just-written idle here.
    m_graph_object->local = m_calibrated_root;
    m_graph_object->UpdateDownwardPass(update_data, 0);
    m_graph_object->UpdateWorldBound();
    for (const SnapshotNode& node : m_snapshot_nodes)
        node.object->previousWorld = node.object->world;
    logger::info("SCOPY FRAME calibrated offset=({:.1f},{:.1f},{:.1f}) radius={:.1f}",
        offset.x, offset.y, offset.z, posed.radius);
}

bool SceneGraph::draw(const RE::UI3DSceneManager* ui3d, const CommonStates& states, const RenderTarget& render_target)
{
    if (!drawable()) return false;

    RE::NiAVObject* p_root = m_graph_object.get();
    if (!p_root || !ui3d || !ui3d->camera || !ui3d->unk10)
        return false;

    RE::BSGraphics::Renderer* renderer = RE::BSGraphics::Renderer::GetSingleton();
    const RE::BSGraphics::RendererData& runtime = renderer->GetRuntimeData();
    if (!runtime.context || !runtime.forwarder)
        return false;

    pose();

    // Restore placement before applying animation; the draw must retain the resulting worlds.
    const std::size_t written = m_animation.advance(*p_root);
    if (written && m_recentre_pending && !m_animation.transitioning()) calibrate_from_posed_bound();

    // Pull the detached light rig to the anchor and cascade it: the shader
    // reads NiLight::world.translate (v6.37) and this window is the only
    // place the cascade runs. Must precede the pass generation.
    StudioLight::instance().place(m_anchor);

    const FigurePassRequest request{
        .root = p_root,
        .accumulator = ui3d->unk10.get(),
        .camera = ui3d->camera.get(),
        .context = runtime.context,
        .target = &render_target,
        .states = &states,
        .light_slots = StudioLight::instance().lights(),
        .light_count = Studio_Light_Count,
        .check_render_target = !m_om_check_logged,
    };

    const FigurePassStats stats = draw_figure_passes(request);

    // One-shot per graph: read back what the context held after the batch. When
    // the shadow state says DIRTY_RENDERTARGET, SetupAndDrawPass re-applies the
    // engine's ledger target mid-batch (v6.45), which empties the studio target
    // without any log line of its own — this decides "rerouted" from
    // "drew-nothing" in one look.
    if (stats.render_target_checked)
    {
        m_om_check_logged = true;
        logger::info("SceneGraph draw OM check: bound={} studio={} match={}",
            stats.render_target_bound, static_cast<const void*>(render_target.rtv()), stats.render_target_matched);
    }

    if (stats.drawn == 0)
    {
        if (!m_empty_draw_reported)
            logger::warn("SceneGraph actor={:08x} draw unavailable: no render passes", source_id());
        m_empty_draw_reported = true;
        StudioLight::instance().park();
        return false;
    }

    m_empty_draw_reported = false;
    // Every draw-window exit parks the rig far out of gameplay space — the
    // ledger shells would otherwise keep lighting the world from the anchor
    // (the 2026-10-07 21:38 light-leak screenshot).
    StudioLight::instance().park();
    return true;
}

PLUGIN_NAMESPACE_END
