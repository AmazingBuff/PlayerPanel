//
// Created by AmazingBuff on 2026/10/08.
//

#include "character/scene_graph_copy.h"

#include "character/character_clone.h"
#include "character/snapshot_transform.h"
#include "panel/panel.h"

#include <cmath>

PLUGIN_NAMESPACE_BEGIN

namespace
{
    constexpr size_t Max_Graph_Objects = 16384;

    // The source graph can be re-driven between the copy and the re-read (the paused
    // world still runs animation and physics), so per-node transforms are compared with
    // a tolerance and only the object set decides the outcome. Exact float equality
    // reported drifts of a few ULPs as a changed source graph.
    constexpr float Source_Drift_Epsilon = 0.001f;
    constexpr float Source_Drift_Tight_Epsilon = 0.000001f;

    // S2 probe geometry (PRD 0.6 §6.3). The swing is a world-X rotation: X is perpendicular to
    // the vertical skeleton, so swinging the pelvis carries the torso through an arc, while a
    // rotation about the bone's own long axis (Z, used by the archived rounds) spins a witness in
    // place and cannot be judged from a position sample.
    constexpr float Probe_Swing_Radians = 0.6f;
    constexpr uint32_t Probe_Swing_Period_Frames = 320;
    constexpr uint32_t Probe_Report_Interval_Frames = 8;
    // A witness closer than this to the swing axis cannot move whatever the amplitude, so its
    // displacement carries no information about the write (test sheet 2026-10-09, R03).
    constexpr float Probe_Witness_Min_Radius = 0.5f;
    constexpr float Probe_Sample_Epsilon = 1e-6f;
    constexpr uint32_t Probe_Slot_Bytes = 48;
    // How many slots the one-shot raw dump prints. The target skins seen so far hold 31-71 slots and
    // the probe slot can sit anywhere in that range, so the cap is set above the largest skin seen
    // rather than close to it: a dump that omits the slot being sampled answers nothing.
    constexpr uint32_t Probe_Dump_Slot_Limit = 96;

    std::string describe_rows(const float* values)
    {
        return fmt::format("{:.4f} {:.4f} {:.4f} {:.4f} | {:.4f} {:.4f} {:.4f} {:.4f} | {:.4f} {:.4f} {:.4f} {:.4f}",
            values[0], values[1], values[2], values[3], values[4], values[5], values[6], values[7],
            values[8], values[9], values[10], values[11]);
    }

    std::string describe_world(const RE::NiTransform& transform)
    {
        const float values[12] = {
            transform.rotate.entry[0][0], transform.rotate.entry[0][1], transform.rotate.entry[0][2], transform.translate.x,
            transform.rotate.entry[1][0], transform.rotate.entry[1][1], transform.rotate.entry[1][2], transform.translate.y,
            transform.rotate.entry[2][0], transform.rotate.entry[2][1], transform.rotate.entry[2][2], transform.translate.z
        };
        return describe_rows(values);
    }

    // allocatedSize is a byte size in this engine, so a skin's slot count comes from its skin data;
    // the byte size only serves as an upper bound.
    uint32_t skin_bone_count(const RE::NiSkinInstance& skin)
    {
        return std::min({ skin.skinData->GetBoneCount(), skin.numMatrices, skin.allocatedSize / Probe_Slot_Bytes });
    }

    bool skin_owns_bone(const RE::NiSkinInstance& skin, const RE::NiAVObject* bone)
    {
        const uint32_t count = skin_bone_count(skin);
        for (uint32_t i = 0; i < count; ++i)
            if (skin.bones[i] == bone)
                return true;
        return false;
    }

    bool slot_values_changed(const float* lhs, const float* rhs)
    {
        for (int component = 0; component < 12; ++component)
            if (std::abs(lhs[component] - rhs[component]) > Probe_Sample_Epsilon)
                return true;
        return false;
    }

    struct GraphInventory
    {
        std::vector<RE::NiAVObject*> objects;
        std::vector<RE::NiPointer<RE::NiAVObject>> keep_alive;
        std::unordered_set<RE::NiAVObject*> nodes;
        std::unordered_set<RE::NiSkinInstance*> skins;
        std::unordered_set<RE::BSShaderProperty*> properties;
        std::unordered_set<const void*> mutable_buffers;
        std::unordered_set<const RE::NiTransform*> bone_transforms;
        std::unordered_set<RE::BSRenderPass*> passes;
        size_t geometries;
        size_t dynamic_geometries;
    };

    // Recompute a subtree's world transforms by composing a world-space delta onto the captured
    // worlds: apply_world_delta_downward lives in snapshot_transform.h, shared with the idle driver.

    bool transform_drifted(const RE::NiTransform& lhs, const RE::NiTransform& rhs, float epsilon)
    {
        if (std::abs(lhs.scale - rhs.scale) > epsilon)
            return true;
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 3; ++column)
                if (std::abs(lhs.rotate.entry[row][column] - rhs.rotate.entry[row][column]) > epsilon)
                    return true;
        return lhs.translate.GetDistance(rhs.translate) > epsilon;
    }

    bool bound_drifted(const RE::NiBound& lhs, const RE::NiBound& rhs, float epsilon)
    {
        return lhs.center.GetDistance(rhs.center) > epsilon || std::abs(lhs.radius - rhs.radius) > epsilon;
    }

    void record_passes(GraphInventory& inventory, RE::BSShaderProperty* property)
    {
        const auto record = [&](RE::BSRenderPass* pass)
        {
            while (pass && inventory.passes.size() < Max_Graph_Objects)
            {
                if (!inventory.passes.insert(pass).second)
                    break;
                pass = pass->next;
            }
        };
        record(property->renderPassList.head);
        record(property->debugRenderPassList.head);
        if (RE::BSLightingShaderProperty* lighting = netimmerse_cast<RE::BSLightingShaderProperty*>(property))
        {
            for (const RE::BSShaderProperty::RenderPassArray& queue : lighting->arrayQueue)
                record(queue.head);
            record(lighting->shadowMapOrMaskPasses.head);
            record(lighting->occlusionPasses.head);
            record(lighting->volumetricShadowUtilityPasses.head);
            record(lighting->depthPass);
        }
    }

    bool inventory_graph(RE::NiAVObject* root, GraphInventory& out)
    {
        out.geometries = 0;
        out.dynamic_geometries = 0;
        std::vector<RE::NiAVObject*> pending{ root };
        while (!pending.empty())
        {
            RE::NiAVObject* object = pending.back();
            pending.pop_back();
            if (!out.nodes.insert(object).second || out.nodes.size() > Max_Graph_Objects)
                return false;
            out.objects.push_back(object);
            out.keep_alive.emplace_back(object);
            out.bone_transforms.insert(&object->world);
            if (RE::NiNode* node = object->AsNode())
            {
                for (const RE::NiPointer<RE::NiAVObject>& child : node->children)
                    if (child)
                        pending.push_back(child.get());
            }
            if (RE::BSGeometry* geometry = object->AsGeometry())
            {
                ++out.geometries;
                const RE::BSGeometry::GEOMETRY_RUNTIME_DATA& runtime = geometry->GetGeometryRuntimeData();
                if (RE::BSShaderProperty* property = runtime.shaderProperty.get())
                {
                    out.properties.insert(property);
                    if (property->lightData)
                        out.mutable_buffers.insert(property->lightData);
                    record_passes(out, property);
                }
                if (RE::NiSkinInstance* skin = runtime.skinInstance.get())
                {
                    out.skins.insert(skin);
                    for (const void* buffer : { static_cast<const void*>(skin->bones), static_cast<const void*>(skin->boneWorldTransforms), static_cast<const void*>(skin->boneMatrices), static_cast<const void*>(skin->prevBoneMatrices), static_cast<const void*>(skin->skinToWorldWorldToSkinMatrix) })
                        if (buffer)
                            out.mutable_buffers.insert(buffer);
                }
                if (RE::BSDynamicTriShape* dynamic = object->AsDynamicTriShape())
                {
                    ++out.dynamic_geometries;
                    if (dynamic->GetDynamicTrishapeRuntimeData().dynamicData)
                        out.mutable_buffers.insert(dynamic->GetDynamicTrishapeRuntimeData().dynamicData);
                }
            }
        }
        return true;
    }

    struct SourceDiff
    {
        bool root_same;
        bool parent_same;
        bool same_object_set;
        bool local_drift;
        size_t world_tight;
        size_t world_loose;
        size_t bound_loose;
        size_t skin_bones;
    };

    SourceDiff diff_source_graph(RE::NiAVObject* root, const GraphInventory& before, const std::vector<RE::NiTransform>& worlds, const std::vector<RE::NiTransform>& locals,
        const std::vector<RE::NiBound>& bounds, RE::NiNode* parent, bool root_still_current)
    {
        SourceDiff diff{ root_still_current, root->parent == parent, true, false, 0, 0, 0, 0 };
        GraphInventory after{};
        if (!inventory_graph(root, after) || after.objects.size() != before.objects.size() || after.geometries != before.geometries)
            diff.same_object_set = false;
        else
            for (size_t i = 0; i < before.objects.size(); ++i)
                if (before.objects[i] != after.objects[i])
                {
                    diff.same_object_set = false;
                    break;
                }
        for (size_t i = 0; i < before.objects.size(); ++i)
        {
            RE::NiAVObject* object = before.objects[i];
            if (transform_drifted(object->local, locals[i], Source_Drift_Tight_Epsilon))
                diff.local_drift = true;
            if (transform_drifted(object->world, worlds[i], Source_Drift_Tight_Epsilon))
                ++diff.world_tight;
            if (transform_drifted(object->world, worlds[i], Source_Drift_Epsilon))
                ++diff.world_loose;
            if (bound_drifted(object->worldBound, bounds[i], Source_Drift_Epsilon))
                ++diff.bound_loose;
        }
        for (RE::NiSkinInstance* skin : before.skins)
        {
            if (!skin->skinData)
                continue;
            const uint32_t count = std::min(skin->allocatedSize, skin->skinData->GetBoneCount());
            for (uint32_t i = 0; i < count; ++i)
                if (skin->bones && skin->bones[i] && before.bone_transforms.contains(&skin->bones[i]->world))
                    ++diff.skin_bones;
        }
        logger::info("SCOPY SOURCE-DIFF capture-diff root-same={} parent-same={} object-set-same={} local-drift={} world-tight={} world-loose={} bound-loose={} objects={} skin-bones={}",
            diff.root_same, diff.parent_same, diff.same_object_set, diff.local_drift, diff.world_tight, diff.world_loose, diff.bound_loose, before.objects.size(), diff.skin_bones);
        return diff;
    }

    bool audit_copy(const GraphInventory& source, const GraphInventory& copy)
    {
        size_t shared_nodes = 0;
        size_t shared_skins = 0;
        size_t shared_properties = 0;
        size_t shared_buffers = 0;
        size_t shared_passes = 0;
        size_t external_roots = 0;
        size_t external_bones = 0;
        size_t external_transforms = 0;
        size_t external_pass_geometry = 0;
        size_t external_fade_nodes = 0;
        size_t unbound_slots = 0;
        size_t invalid_arrays = 0;
        for (RE::NiAVObject* object : copy.objects)
            shared_nodes += source.nodes.contains(object) ? 1 : 0;
        for (RE::BSShaderProperty* property : copy.properties)
        {
            shared_properties += source.properties.contains(property) ? 1 : 0;
            if (property->fadeNode && !copy.nodes.contains(property->fadeNode))
                ++external_fade_nodes;
        }
        for (const void* buffer : copy.mutable_buffers)
            shared_buffers += source.mutable_buffers.contains(buffer) ? 1 : 0;
        for (RE::BSRenderPass* pass : copy.passes)
        {
            shared_passes += source.passes.contains(pass) ? 1 : 0;
            if (pass->geometry && !copy.nodes.contains(pass->geometry))
                ++external_pass_geometry;
        }

        for (RE::NiSkinInstance* skin : copy.skins)
        {
            if (source.skins.contains(skin))
            {
                ++shared_skins;
                continue;
            }
            if (!skin->rootParent || !copy.nodes.contains(skin->rootParent))
                ++external_roots;
            if (!skin->skinData || skin->numMatrices > skin->allocatedSize)
            {
                ++invalid_arrays;
                continue;
            }
            const uint32_t count = std::min(skin->allocatedSize, skin->skinData->GetBoneCount());
            if (skin->bones && !source.mutable_buffers.contains(skin->bones))
            {
                for (uint32_t i = 0; i < count; ++i)
                {
                    if (!skin->bones[i])
                        ++unbound_slots;
                    else if (!copy.nodes.contains(skin->bones[i]))
                        ++external_bones;
                }
            }
            else if (!skin->bones)
                unbound_slots += count;
            if (skin->boneWorldTransforms && !source.mutable_buffers.contains(skin->boneWorldTransforms))
            {
                for (uint32_t i = 0; i < count; ++i)
                    if (skin->boneWorldTransforms[i] && !copy.bone_transforms.contains(skin->boneWorldTransforms[i]))
                        ++external_transforms;
            }
        }
        logger::info("SCOPY AUDIT shared-nodes={} shared-skins={} shared-properties={} shared-buffers={} shared-passes={} external-roots={} external-bones={} external-transforms={} external-pass-geometry={} external-fade-nodes={} invalid-arrays={} unbound-slots={}",
            shared_nodes, shared_skins, shared_properties, shared_buffers, shared_passes, external_roots, external_bones, external_transforms, external_pass_geometry, external_fade_nodes, invalid_arrays, unbound_slots);
        logger::info("SCOPY CENSUS source-nodes={} copy-nodes={} source-geoms={} copy-geoms={} source-skins={} copy-skins={} source-dynamic={} copy-dynamic={}",
            source.nodes.size(), copy.nodes.size(), source.geometries, copy.geometries, source.skins.size(), copy.skins.size(), source.dynamic_geometries, copy.dynamic_geometries);
        return shared_nodes == 0 && shared_skins == 0 && shared_properties == 0 && shared_buffers == 0 && shared_passes == 0 &&
            external_roots == 0 && external_bones == 0 && external_transforms == 0 && external_pass_geometry == 0 && external_fade_nodes == 0 && invalid_arrays == 0 &&
            source.nodes.size() == copy.nodes.size() && source.geometries == copy.geometries && copy.geometries > 0;
    }
}

SceneGraphCopy& SceneGraphCopy::instance()
{
    static SceneGraphCopy s_instance;
    return s_instance;
}

SceneGraphCopy::SceneGraphCopy() : m_command(Command::e_none), m_generation(0), m_seen_generation(0), m_capture_id(0), m_draw_enabled(false), m_frame(0), m_cap_logged(false), m_anim_probe_enabled(false), m_probe_bone_cache(nullptr), m_probe_target_geometry(nullptr), m_probe_witness(nullptr), m_probe_witness_radius(0.0f), m_probe_subtree_nodes(0), m_probe_affected_geometries(0), m_probe_skinned_geometries(0), m_probe_geometry_bones(0), m_probe_bone_world_pose{}, m_probe_bind_rotate{}, m_probe_bind_valid(false), m_previous_bone_rotate{}, m_previous_bone_valid(false), m_probe_dump_done(false), m_trace_frame(0), m_trace_capture(0), m_trace_bone_before{}, m_trace_bone_after{}, m_trace_witness_before{}, m_trace_witness_after{}, m_trace_slot_pre{}, m_trace_slot_submit{}, m_trace_slot_final{}, m_trace_drawn(0), m_trace_submit_sampled(false), m_trace_complete(false), m_probe_move_min(-1.0f), m_probe_move_max(0.0f), m_probe_samples(0), m_probe_invalid_logged(false), m_probe_frame_report(0) {}
SceneGraphCopy::~SceneGraphCopy()
{
    // Never destroy a parked graph, not even at unload.
    m_parked.clear();
}

void SceneGraphCopy::retire(std::unique_ptr<CharacterClone> snapshot)
{
    if (!snapshot)
        return;
    // The allocation is intentionally not owned by anything that can destroy it: the
    // snapshot itself and the graph it holds stay alive for the rest of the process.
    m_parked.push_back(snapshot.release());
    log_parked();
    if (m_parked.size() <= Parked_Graph_Cap)
        return;
    if (!m_cap_logged)
    {
        logger::warn("SCOPY PARK cap {} reached; the oldest graph stays parked (no destruction attempted)", Parked_Graph_Cap);
        m_cap_logged = true;
    }
    // Erasing the oldest pointer drops only the bookkeeping entry; its graph stays
    // allocated on purpose, because no teardown path has proved safe.
    m_parked.erase(m_parked.begin());
    log_parked();
}

void SceneGraphCopy::log_parked() const
{
    logger::info("SCOPY PARK snapshots held alive for the process lifetime: {}", m_parked.size());
}

void SceneGraphCopy::request(Command command)
{
    m_command.store(command, std::memory_order_release);
}

void SceneGraphCopy::reset_for_load()
{
    m_command.store(Command::e_none, std::memory_order_release);
    m_generation.fetch_add(1, std::memory_order_acq_rel);
}

void SceneGraphCopy::process_requests()
{
    ++m_frame;
    const uint32_t generation = m_generation.load(std::memory_order_acquire);
    RE::UI* ui = RE::UI::GetSingleton();
    if (generation != m_seen_generation || (ui && ui->IsMenuOpen(RE::MainMenu::MENU_NAME)))
    {
        if (m_snapshot)
            logger::info("SCOPY RELEASE reason=session-boundary capture={}", m_capture_id);
        retire(std::move(m_snapshot));
        reset_probe_target();
        m_idle.reset();
        m_draw_enabled = false;
        m_seen_generation = generation;
        m_command.store(Command::e_none, std::memory_order_release);
        return;
    }
    const Command command = m_command.exchange(Command::e_none, std::memory_order_acq_rel);
    if (command == Command::e_none)
        return;
    if (command == Command::e_release)
    {
        retire(std::move(m_snapshot));
        reset_probe_target();
        m_idle.reset();
        m_draw_enabled = false;
        logger::info("SCOPY RELEASE reason=F4 capture={}", m_capture_id);
        return;
    }
    if (!ui || !ui->GameIsPaused() || !PanelMonitor::instance().is_menu_open() || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME))
    {
        logger::warn("SCOPY REQUEST rejected: open a paused InventoryMenu first");
        return;
    }
    switch (command)
    {
    case Command::e_capture:
        capture();
        break;
    case Command::e_toggle_draw:
        if (m_snapshot)
        {
            m_draw_enabled = !m_draw_enabled;
            logger::info("SCOPY DRAW-TOGGLE enabled={} capture={}", m_draw_enabled, m_capture_id);
        }
        else
            logger::warn("SCOPY DRAW-TOGGLE rejected: no accepted snapshot (press F7 and inspect AUDIT)");
        break;
    case Command::e_rotate:
        if (m_snapshot)
            m_snapshot->rotate_snapshot();
        break;
    case Command::e_anim_probe:
        toggle_animation_probe();
        break;
    case Command::e_idle_toggle:
        toggle_idle();
        break;
    default:
        break;
    }
}

void SceneGraphCopy::toggle_animation_probe()
{
    if (!m_snapshot || !m_snapshot->graph())
    {
        logger::warn("SCOPY ANIM probe rejected: capture a copy first (F7)");
        return;
    }
    m_anim_probe_enabled = !m_anim_probe_enabled;
    reset_probe_target();
    m_probe_move_min = -1.0f;
    m_probe_move_max = 0.0f;
    m_probe_samples = 0;
    m_probe_invalid_logged = false;
    m_probe_frame_report = 0;
    if (m_anim_probe_enabled)
        logger::info("SCOPY ANIM probe enabled: node transforms only, applied after the studio's base placement and before pass preparation; the skin matrix buffer is read-only");
    else
        logger::info("SCOPY ANIM probe disabled; the copy keeps its captured pose");
}

void SceneGraphCopy::reset_probe_target()
{
    m_probe_bone_cache = nullptr;
    m_probe_target_geometry = nullptr;
    m_probe_witness = nullptr;
    m_probe_witness_radius = 0.0f;
    m_probe_subtree_nodes = 0;
    m_probe_affected_geometries = 0;
    m_probe_skinned_geometries = 0;
    m_probe_geometry_bones = 0;
    m_probe_geometry.clear();
    m_probe_root.clear();
    m_probe_parent_chain.clear();
    m_probe_skin_report.clear();
    m_probe_bind_rotate = RE::NiMatrix3{};
    m_probe_bind_valid = false;
    m_previous_bone_rotate = RE::NiMatrix3{};
    m_previous_bone_valid = false;
    m_probe_dump_done = false;
    m_trace_pass_geometry.clear();
    m_trace_complete = false;
    m_trace_submit_sampled = false;
}

void SceneGraphCopy::toggle_idle()
{
    m_idle.set_enabled(!m_idle.enabled());
    logger::info("SCOPY IDLE {} (F6); the next frame's base placement restores the captured pose first, so nothing accumulates",
        m_idle.enabled() ? "enabled" : "disabled");
}

void SceneGraphCopy::animate_at_base_pose(CharacterClone& clone)
{
    if (m_snapshot.get() != &clone)
        return;
    RE::NiAVObject* const root = clone.graph();
    if (!root || !m_idle.enabled())
        return;
    // While F2 is armed the probe is the figure's owner: it rebuilds its bone from the captured pose
    // every frame, so an idle underneath it would only fight the measurement.
    if (m_anim_probe_enabled)
        return;
    m_idle.advance(*root);
}

void SceneGraphCopy::select_probe_target(RE::NiAVObject& root)
{
    // Candidate bones come from EVERY skin's bones[] array: a bone picked by walking the node tree
    // can belong to the copy's other skeleton, and taking only the first skin found makes the
    // target depend on traversal order — the CBBE run picked the body skin's pelvis while the UBE
    // run picked the face skin's spine, so the two rounds swung different joints.
    std::unordered_set<RE::NiAVObject*> skin_bones;
    std::vector<std::pair<RE::BSGeometry*, RE::NiSkinInstance*>> skinned_geometries;
    RE::BSVisit::TraverseScenegraphGeometries(&root, [&](RE::BSGeometry* geometry)
    {
        RE::NiSkinInstance* skin = geometry->GetGeometryRuntimeData().skinInstance.get();
        if (!skin || !skin->skinData || !skin->bones || !skin->numMatrices)
            return RE::BSVisit::BSVisitControl::kContinue;
        skinned_geometries.emplace_back(geometry, skin);
        const uint32_t count = skin_bone_count(*skin);
        for (uint32_t i = 0; i < count; ++i)
            if (skin->bones[i])
                skin_bones.insert(skin->bones[i]);
        return RE::BSVisit::BSVisitControl::kContinue;
    });
    // The joint whose subtree covers the most nodes, so the swing is visible on screen.
    size_t best_reach = 0;
    for (RE::NiAVObject* bone : skin_bones)
    {
        size_t reach = 0;
        RE::BSVisit::TraverseScenegraphObjects(bone, [&](RE::NiAVObject*)
        {
            ++reach;
            return RE::BSVisit::BSVisitControl::kContinue;
        });
        if (reach > best_reach)
        {
            best_reach = reach;
            m_probe_bone_cache = bone;
        }
    }
    if (!m_probe_bone_cache)
        return;

    // The target geometry is the skin with the most bones that samples the chosen joint, so the
    // slot read at T1-T3 comes from a body-sized draw rather than a 17-bone face skin.
    RE::NiSkinInstance* chosen_skin = nullptr;
    uint32_t chosen_bones = 0;
    m_probe_target_geometry = nullptr;
    for (const auto& [geometry, skin] : skinned_geometries)
    {
        const uint32_t count = skin_bone_count(*skin);
        if (count <= chosen_bones || !skin_owns_bone(*skin, m_probe_bone_cache))
            continue;
        chosen_bones = count;
        chosen_skin = skin;
        m_probe_target_geometry = geometry;
    }
    if (!m_probe_target_geometry || !chosen_skin)
    {
        logger::warn("SCOPY ANIM probe target bone='{}' is in no skin: no slot to sample and no pass to match", m_probe_bone_cache->name.c_str());
        return;
    }
    m_probe_geometry_bones = chosen_bones;
    m_probe_subtree_nodes = static_cast<uint32_t>(best_reach);
    m_probe_bone_world_pose = m_probe_bone_cache->world;
    m_probe_geometry = m_probe_target_geometry->name.c_str() ? m_probe_target_geometry->name.c_str() : "?";
    m_probe_root = chosen_skin->rootParent && chosen_skin->rootParent->name.c_str() ? chosen_skin->rootParent->name.c_str() : "?";

    // The slot's bind transform is static skin data and one of the quantities the sampled buffer may
    // hold, so it is captured here for the classifier.
    m_probe_bind_valid = false;
    for (uint32_t i = 0; i < chosen_bones; ++i)
    {
        if (chosen_skin->bones[i] != m_probe_bone_cache)
            continue;
        m_probe_bind_rotate = chosen_skin->skinData->GetBoneDataSkinToBone(i).rotate;
        m_probe_bind_valid = true;
        break;
    }

    // How much of the figure the swing moves at all: the geometries whose skin samples a bone
    // inside the swung subtree. This separates "the write reached the node tree" from "the write
    // reached the drawn surface", which is what the two-body difference turned on.
    std::unordered_set<RE::NiAVObject*> subtree;
    RE::BSVisit::TraverseScenegraphObjects(m_probe_bone_cache, [&](RE::NiAVObject* object)
    {
        subtree.insert(object);
        return RE::BSVisit::BSVisitControl::kContinue;
    });
    uint32_t affected = 0;
    for (const auto& [geometry, skin] : skinned_geometries)
    {
        const uint32_t count = skin_bone_count(*skin);
        for (uint32_t i = 0; i < count; ++i)
            if (skin->bones[i] && subtree.contains(skin->bones[i]))
            {
                ++affected;
                break;
            }
    }
    m_probe_affected_geometries = affected;
    m_probe_skinned_geometries = static_cast<uint32_t>(skinned_geometries.size());

    // The joint's ancestry names the pivot the swing turns about; two bodies that pick different
    // joints cannot otherwise be told apart from two bodies that behave differently.
    std::string chain;
    for (RE::NiAVObject* node = m_probe_bone_cache->parent; node; node = node->parent)
    {
        if (chain.size() > 256)
        {
            chain += "…";
            break;
        }
        chain += node->name.c_str() ? node->name.c_str() : "?";
        if (node->parent)
            chain += " < ";
    }
    m_probe_parent_chain = std::move(chain);

    // Witness: the descendant furthest from the world-X swing axis through the bone origin. A
    // witness on the axis cannot move however large the swing is, so its displacement cannot
    // judge the write (test sheet 2026-10-09, R03); an off-axis one travels 2 r sin(theta/2).
    const RE::NiPoint3 pivot = m_probe_bone_world_pose.translate;
    float best_radius = -1.0f;
    RE::BSVisit::TraverseScenegraphObjects(m_probe_bone_cache, [&](RE::NiAVObject* object)
    {
        if (object == m_probe_bone_cache)
            return RE::BSVisit::BSVisitControl::kContinue;
        const float radius = swing_radius_about_x(object->world.translate, pivot);
        if (radius > best_radius)
        {
            best_radius = radius;
            m_probe_witness = object;
            m_probe_witness_radius = radius;
        }
        return RE::BSVisit::BSVisitControl::kContinue;
    });

    // Skinning reads boneWorldTransforms[i], not bones[i]->world. If that pointer array still
    // names the SOURCE actor's transforms, writing the copy's bones can never move the copy: the
    // mesh would read the world actor's pose instead, which also explains a figure that stays
    // frozen while the copy's own bones change.
    m_probe_skin_report.clear();
    uint32_t skins_reported = 0;
    RE::BSVisit::TraverseScenegraphGeometries(&root, [&](RE::BSGeometry* geometry)
    {
        RE::NiSkinInstance* skin = geometry->GetGeometryRuntimeData().skinInstance.get();
        if (!skin || !skin->bones || !skin->boneWorldTransforms || !skin->skinData || skins_reported >= 4)
            return RE::BSVisit::BSVisitControl::kContinue;
        const uint32_t count = std::min(skin->allocatedSize, skin->skinData->GetBoneCount());
        uint32_t direct = 0;
        for (uint32_t i = 0; i < count; ++i)
            if (skin->boneWorldTransforms[i] == &skin->bones[i]->world)
                ++direct;
        ++skins_reported;
        m_probe_skin_report += fmt::format(" [{}: {}/{} direct]", geometry->name.c_str() ? geometry->name.c_str() : "?", direct, count);
        return RE::BSVisit::BSVisitControl::kContinue;
    });

    const RE::TESObjectREFR* source_ref = static_cast<const RE::TESObjectREFR*>(root.GetUserData());
    logger::info("SCOPY ANIM target bone='{}' parent-chain='{}' candidates={} subtree-nodes={} skin='{}' bones={} root='{}' affected-geoms={}/{} witness='{}' witness-radius={:.2f} swing=world-X {:.3f}rad period={}f controllers={} verdict={} skins:{}",
        m_probe_bone_cache->name.c_str(), m_probe_parent_chain.c_str(), skin_bones.size(), m_probe_subtree_nodes,
        m_probe_geometry, m_probe_geometry_bones, m_probe_root,
        m_probe_affected_geometries, m_probe_skinned_geometries,
        m_probe_witness && m_probe_witness->name.c_str() ? m_probe_witness->name.c_str() : "<none>", m_probe_witness_radius,
        Probe_Swing_Radians, Probe_Swing_Period_Frames,
        root.GetControllers() ? "present" : "none",
        source_ref ? "copy-carries-userData-to-source" : "copy-is-graph-invisible", m_probe_skin_report);
}

void SceneGraphCopy::probe_at_base_pose(CharacterClone& clone)
{
    m_trace_submit_sampled = false;
    m_trace_complete = false;
    if (!m_anim_probe_enabled || m_snapshot.get() != &clone)
        return;
    RE::NiAVObject* root = clone.graph();
    if (!root)
        return;
    if (!m_probe_bone_cache)
    {
        select_probe_target(*root);
        if (!m_probe_bone_cache)
            return;
    }

    // T0: this frame's baseline, recorded after the studio's base placement and before any write,
    // so the reader can tell a write that never happened from one an engine stage overwrote.
    m_trace_frame = m_frame;
    m_trace_capture = m_capture_id;
    m_trace_pass_geometry.clear();
    m_trace_bone_before = m_probe_bone_cache->world;
    m_trace_witness_before = m_probe_witness ? m_probe_witness->world.translate : m_trace_bone_before.translate;
    m_trace_slot_pre = ProbeSlotSample{};
    sample_probe_slot(m_trace_slot_pre);
    // T1: the controlled transform. It is rebuilt every frame from the pose captured at arm time,
    // so repeated frames cannot accumulate, and it runs after pose() restored the captured worlds
    // — the order the archived rounds lacked (PRD 0.6 §6.3).
    const float phase = static_cast<float>(m_frame % Probe_Swing_Period_Frames) / static_cast<float>(Probe_Swing_Period_Frames) * 6.2831853f;
    const float swing = std::sin(phase) * Probe_Swing_Radians;
    RE::NiMatrix3 swing_rotation;
    swing_rotation.MakeXRotation(swing);
    apply_world_delta_downward(m_probe_bone_cache, swing_delta_about_pivot(swing_rotation, m_probe_bone_world_pose.translate), 0);
    root->UpdateWorldBound();

    m_trace_bone_after = m_probe_bone_cache->world;
    m_trace_witness_after = m_probe_witness ? m_probe_witness->world.translate : m_trace_bone_after.translate;

    // Node transforms only: the skin matrix buffer is sampled read-only at T1-T3 and never written
    // in this round. Writing it needs the layout, the coordinate space and the upload path
    // established first, and the read-back that once "classified" it compared the slot against the
    // value the same code had just written into it. write_bone_matrix keeps that gated step.
}

void SceneGraphCopy::probe_at_pass_submit(CharacterClone& clone, RE::BSRenderPass* pass)
{
    // T2: the pass that submits the target geometry, sampled immediately before it is drawn —
    // the last CPU-side view of the slot before the GPU work, and the point where a base
    // placement or an engine preparation step would show up as an overwritten transform.
    if (!m_anim_probe_enabled || m_trace_submit_sampled || m_snapshot.get() != &clone)
        return;
    if (!pass || !m_probe_target_geometry || pass->geometry != m_probe_target_geometry)
        return;
    m_trace_submit_sampled = true;
    m_trace_pass_geometry = pass->geometry->name.c_str() ? pass->geometry->name.c_str() : "?";
    m_trace_slot_submit = ProbeSlotSample{};
    sample_probe_slot(m_trace_slot_submit);
}

void SceneGraphCopy::probe_at_draw_done(CharacterClone& clone, uint32_t drawn)
{
    // T3: what the engine's own buffer update left behind once the draw ran.
    if (!m_anim_probe_enabled || m_snapshot.get() != &clone)
        return;
    m_trace_drawn = drawn;
    m_trace_slot_final = ProbeSlotSample{};
    sample_probe_slot(m_trace_slot_final);
    m_trace_complete = true;

    // Once per F2 arm, while the copy is up and its buffers are live.
    if (!m_probe_dump_done)
    {
        dump_probe_skin();
        m_probe_dump_done = true;
    }

    if (m_probe_frame_report == 0)
        m_probe_frame_report = m_frame + Probe_Report_Interval_Frames;
    else if (static_cast<int32_t>(m_frame - m_probe_frame_report) >= 0)
    {
        report_probe_trace();
        m_probe_frame_report = m_frame + Probe_Report_Interval_Frames;
    }

    // After the report, so this frame's swing is the NEXT frame's "previous" candidate rather than
    // a duplicate of the current one: a slot holding last frame's value is what a lagged engine
    // update would look like, and the classifier has to be able to see it.
    m_previous_bone_rotate = m_trace_bone_after.rotate;
    m_previous_bone_valid = true;
}

bool SceneGraphCopy::sample_probe_slot(ProbeSlotSample& sample) const
{
    if (!m_probe_bone_cache || !m_probe_target_geometry)
        return false;
    RE::NiSkinInstance* skin = m_probe_target_geometry->GetGeometryRuntimeData().skinInstance.get();
    if (!skin || !skin->skinData || !skin->bones || !skin->boneMatrices)
        return false;
    const uint32_t count = skin_bone_count(*skin);
    for (uint32_t i = 0; i < count; ++i)
    {
        if (skin->bones[i] != m_probe_bone_cache)
            continue;
        // The slot index is found rather than assumed: slot 0 was assumed to be the probe bone in
        // an earlier round and was not.
        sample.sampled = true;
        sample.slot = i;
        sample.num_matrices = skin->numMatrices;
        sample.frame_id = skin->frameID;
        sample.buffer = skin->boneMatrices;
        const float* const values = static_cast<const float*>(skin->boneMatrices) + static_cast<size_t>(i) * 12;
        for (int component = 0; component < 12; ++component)
            sample.values[component] = values[component];
        if (skin->prevBoneMatrices)
        {
            sample.previous_sampled = true;
            sample.previous_buffer = skin->prevBoneMatrices;
            const float* const previous_values = static_cast<const float*>(skin->prevBoneMatrices) + static_cast<size_t>(i) * 12;
            for (int component = 0; component < 12; ++component)
                sample.previous_values[component] = previous_values[component];
        }
        return true;
    }
    return false;
}

void SceneGraphCopy::dump_probe_skin()
{
    if (!m_probe_target_geometry)
        return;
    RE::NiSkinInstance* skin = m_probe_target_geometry->GetGeometryRuntimeData().skinInstance.get();
    if (!skin || !skin->skinData || !skin->bones || !skin->boneMatrices)
        return;
    const uint32_t slots = skin_bone_count(*skin);
    const uint32_t count = std::min(slots, Probe_Dump_Slot_Limit);
    logger::info("SCOPY TDUMP geometry='{}' root='{}' slots={} printed={}", m_probe_geometry, m_probe_root, slots, count);
    for (uint32_t i = 0; i < count; ++i)
    {
        RE::NiAVObject* const bone = skin->bones[i];
        const float* const matrix = static_cast<const float*>(skin->boneMatrices) + static_cast<size_t>(i) * 12;
        const float* const previous = skin->prevBoneMatrices
            ? static_cast<const float*>(skin->prevBoneMatrices) + static_cast<size_t>(i) * 12
            : nullptr;
        // The bone's own world transform sits next to the buffer's raw contents on the same line:
        // the layout, and whether slot i is bones[i] at all, can then be read off directly instead
        // of being inferred from distances that all missed by about 2.0.
        logger::info("SCOPY TSLOT i={} bone='{}' world=({}) matrix=({}) previous=({})",
            i,
            bone && bone->name.c_str() ? bone->name.c_str() : "<none>",
            bone ? describe_world(bone->world) : std::string("unavailable"),
            describe_rows(matrix),
            previous ? describe_rows(previous) : std::string("unavailable"));
    }
}

std::string SceneGraphCopy::classify_probe_slot(const float* values) const
{
    // Candidates come from engine-side node and bind data only — never from a value this code wrote,
    // which is what made the earlier classifications circular.
    struct Candidate
    {
        const char* name;
        RE::NiMatrix3 rotate;
    };
    std::vector<Candidate> candidates;
    candidates.push_back({ "bone-world", m_trace_bone_after.rotate });
    candidates.push_back({ "bone-captured", m_trace_bone_before.rotate });
    candidates.push_back({ "bone-world-T", m_trace_bone_after.rotate.Transpose() });
    candidates.push_back({ "bone-captured-T", m_trace_bone_before.rotate.Transpose() });
    if (m_previous_bone_valid)
        candidates.push_back({ "bone-prev-frame", m_previous_bone_rotate });
    if (m_probe_bind_valid)
    {
        candidates.push_back({ "bind", m_probe_bind_rotate });
        candidates.push_back({ "bind-T", m_probe_bind_rotate.Transpose() });
        candidates.push_back({ "world-x-bind", m_trace_bone_after.rotate * m_probe_bind_rotate });
        candidates.push_back({ "bind-x-world", m_probe_bind_rotate * m_trace_bone_after.rotate });
        candidates.push_back({ "captured-x-bind", m_trace_bone_before.rotate * m_probe_bind_rotate });
    }

    const char* best = "none";
    const char* second = "none";
    float best_distance = 1.0e30f;
    float second_distance = 1.0e30f;
    for (const Candidate& candidate : candidates)
    {
        const float distance = rotation_distance(values, candidate.rotate);
        if (distance < best_distance)
        {
            second_distance = best_distance;
            second = best;
            best_distance = distance;
            best = candidate.name;
        }
        else if (distance < second_distance)
        {
            second_distance = distance;
            second = candidate.name;
        }
    }
    return fmt::format("{}({:.2f}) next={}({:.2f})", best, best_distance, second, second_distance);
}

void SceneGraphCopy::report_probe_trace()
{
    if (!m_trace_complete || !m_probe_bone_cache)
        return;

    // Sampled over many frames: a constant range means the witness never followed the swing, which
    // one frame cannot distinguish from a small amplitude.
    const float moved = (m_trace_witness_after - m_trace_witness_before).Length();
    m_probe_move_min = m_probe_move_min < 0.0f ? moved : std::min(m_probe_move_min, moved);
    m_probe_move_max = std::max(m_probe_move_max, moved);
    ++m_probe_samples;

    // A witness on the swing axis cannot move whatever the amplitude, so this run's displacement
    // says nothing about the write; only the orientation change remains judgeable (test sheet
    // 2026-10-09, R03).
    const bool witness_usable = m_probe_witness_radius >= Probe_Witness_Min_Radius;
    const bool probe_effective = witness_usable && m_probe_move_max > 0.01f;
    if (m_probe_samples > 20 && !probe_effective && !m_probe_invalid_logged)
    {
        logger::error("SCOPY ANIM probe INVALID: witness='{}' radius={:.2f} moved-range=[{:.2f},{:.2f}]; without radius about the swing axis the displacement cannot judge the write",
            m_probe_witness && m_probe_witness->name.c_str() ? m_probe_witness->name.c_str() : "<none>", m_probe_witness_radius, m_probe_move_min, m_probe_move_max);
        m_probe_invalid_logged = true;
    }

    const RE::NiTransform& before = m_trace_bone_before;
    const RE::NiTransform& after = m_trace_bone_after;
    const float orientation_delta = rotation_angle_degrees(before.rotate, after.rotate);

    // Which of this frame's two node rotations the slot's rotation block is closer to. Observation
    // only: it cannot identify what the buffer stores, and it never compares the slot against a
    // value this code wrote.
    const auto relation = [&before, &after](const ProbeSlotSample& sample) -> const char*
    {
        if (!sample.sampled)
            return "unavailable";
        const float to_captured = rotation_distance(sample.values, before.rotate);
        const float to_swung = rotation_distance(sample.values, after.rotate);
        if (std::abs(to_captured - to_swung) <= 0.01f)
            return "neither";
        return to_swung < to_captured ? "swung" : "captured";
    };
    const bool submit_changed = m_trace_slot_pre.sampled && m_trace_slot_submit.sampled &&
        slot_values_changed(m_trace_slot_pre.values, m_trace_slot_submit.values);
    const bool final_changed = m_trace_slot_submit.sampled && m_trace_slot_final.sampled &&
        slot_values_changed(m_trace_slot_submit.values, m_trace_slot_final.values);

    logger::info("SCOPY T0 frame={} capture={} bone='{}' skin='{}' root='{}' bone-origin=({:.1f},{:.1f},{:.1f}) bone-row1=({:.3f},{:.3f},{:.3f}) witness='{}' witness-radius={:.2f} subtree-nodes={}",
        m_trace_frame, m_trace_capture, m_probe_bone_cache->name.c_str(), m_probe_geometry, m_probe_root,
        before.translate.x, before.translate.y, before.translate.z,
        before.rotate.entry[1][0], before.rotate.entry[1][1], before.rotate.entry[1][2],
        m_probe_witness && m_probe_witness->name.c_str() ? m_probe_witness->name.c_str() : "<none>", m_probe_witness_radius, m_probe_subtree_nodes);
    logger::info("SCOPY T1 frame={} bone-row1=({:.3f},{:.3f},{:.3f}) orientation-delta-deg={:.1f} witness-moved={:.2f} moved-range=[{:.2f},{:.2f}] slot={} buffer-pre={} pre-rel={} probe-effective={}",
        m_trace_frame, after.rotate.entry[1][0], after.rotate.entry[1][1], after.rotate.entry[1][2],
        orientation_delta, moved, m_probe_move_min, m_probe_move_max,
        m_trace_slot_pre.sampled ? m_trace_slot_pre.slot : 0u,
        m_trace_slot_pre.sampled ? "sampled" : "unavailable", relation(m_trace_slot_pre), probe_effective);
    logger::info("SCOPY T2 frame={} pass-geometry='{}' numMatrices={} frameID={} buffer-submit={} changed-since-T1={} submit-rel={}",
        m_trace_frame, m_trace_submit_sampled ? m_trace_pass_geometry.c_str() : "not-submitted", m_trace_slot_submit.num_matrices, m_trace_slot_submit.frame_id,
        m_trace_slot_submit.sampled ? "sampled" : "unavailable", submit_changed, relation(m_trace_slot_submit));
    logger::info("SCOPY T3 frame={} drawn={} buffer-after-draw={} changed-since-T2={} final-rel={} node-row1=({:.3f},{:.3f},{:.3f})",
        m_trace_frame, m_trace_drawn, m_trace_slot_final.sampled ? "sampled" : "unavailable", final_changed, relation(m_trace_slot_final),
        m_probe_bone_cache->world.rotate.entry[1][0], m_probe_bone_cache->world.rotate.entry[1][1], m_probe_bone_cache->world.rotate.entry[1][2]);

    // What the sampled slot actually resembles, against engine-side candidates. The pair labels
    // above can only say "closer to one of my two guesses"; this says which of ten quantities the
    // content is nearest to, and how near, so "the buffer holds something else" stops looking like
    // "the buffer holds nothing".
    if (m_trace_slot_pre.sampled)
        logger::info("SCOPY T1c frame={} buffer=current slot={} closest={} | buffer=previous slot={} closest={}",
            m_trace_frame, m_trace_slot_pre.slot, classify_probe_slot(m_trace_slot_pre.values),
            m_trace_slot_pre.slot, m_trace_slot_pre.previous_sampled ? classify_probe_slot(m_trace_slot_pre.previous_values) : std::string("unsampled"));
    if (m_trace_slot_final.sampled)
        logger::info("SCOPY T3c frame={} buffer=current slot={} closest={} | buffer=previous slot={} closest={}",
            m_trace_frame, m_trace_slot_final.slot, classify_probe_slot(m_trace_slot_final.values),
            m_trace_slot_final.slot, m_trace_slot_final.previous_sampled ? classify_probe_slot(m_trace_slot_final.previous_values) : std::string("unsampled"));
}

CharacterClone* SceneGraphCopy::drawable() const
{
    return m_draw_enabled ? m_snapshot.get() : nullptr;
}

void SceneGraphCopy::capture()
{
    m_draw_enabled = false;
    retire(std::move(m_snapshot));
    // Re-select the target on the graph this capture produces: the cached one now points into the
    // parked copy the panel no longer draws. The idle's joints are the same kind of per-graph state.
    reset_probe_target();
    m_idle.reset();
    ++m_capture_id;
    RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
    RE::NiPointer<RE::NiAVObject> source(player ? player->Get3D(false) : nullptr);
    if (!source || !source->AsNode() || !std::isfinite(source->world.scale) || std::abs(source->world.scale) < 0.0001f)
    {
        logger::warn("SCOPY FAILED capture={} reason=no-valid-third-person-root", m_capture_id);
        return;
    }
    GraphInventory source_inventory{};
    if (!inventory_graph(source.get(), source_inventory))
    {
        logger::error("SCOPY FAILED capture={} reason=source-cycle-or-object-limit", m_capture_id);
        return;
    }
    std::vector<RE::NiTransform> source_worlds;
    std::vector<RE::NiTransform> source_locals;
    std::vector<RE::NiBound> source_bounds;
    for (RE::NiAVObject* object : source_inventory.objects)
    {
        source_worlds.push_back(object->world);
        source_locals.push_back(object->local);
        source_bounds.push_back(object->worldBound);
    }
    RE::NiNode* const source_parent = source->parent;
    logger::info("SCOPY BEGIN capture={} actor={:08x} source={} parent={} nodes={} geoms={} api=NiObject::Clone id=68835/70187", m_capture_id, player->GetFormID(),
        static_cast<void*>(source.get()), static_cast<void*>(source_parent), source_inventory.nodes.size(), source_inventory.geometries);

    RE::NiPointer<RE::NiObject> copied(source->Clone());
    logger::info("SCOPY CLONE-RETURN capture={} copy={}", m_capture_id, static_cast<void*>(copied.get()));
    RE::NiNode* root = copied ? copied->AsNode() : nullptr;
    if (!root || root == source.get() || root->parent)
    {
        logger::error("SCOPY FAILED capture={} reason=null-aliased-or-parented-root", m_capture_id);
        return;
    }
    GraphInventory copy_inventory{};
    if (!inventory_graph(root, copy_inventory))
    {
        logger::error("SCOPY FAILED capture={} reason=copy-cycle-or-object-limit", m_capture_id);
        return;
    }
    const SourceDiff diff = diff_source_graph(source.get(), source_inventory, source_worlds, source_locals, source_bounds, source_parent, player->Get3D(false) == source.get());
    if (!diff.same_object_set)
    {
        logger::error("SCOPY BLOCKED capture={} reason=source-object-set-differs (no pose or draw was performed)", m_capture_id);
        return;
    }
    const bool accepted = audit_copy(source_inventory, copy_inventory);
    if (!accepted)
    {
        logger::error("SCOPY BLOCKED capture={} reason=isolation-audit (no pose or draw was performed)", m_capture_id);
        return;
    }
    for (size_t i = 0; i < source_inventory.objects.size(); ++i)
    {
        RE::NiAVObject* original = source_inventory.objects[i];
        RE::NiAVObject* target = copy_inventory.objects[i];
        if (original->name != target->name || std::strcmp(original->GetRTTI()->GetName(), target->GetRTTI()->GetName()) != 0)
        {
            logger::error("SCOPY BLOCKED capture={} reason=structural-correspondence index={} source-name='{}' copy-name='{}'", m_capture_id, i, original->name.c_str(), target->name.c_str());
            return;
        }
    }
    size_t removed_controllers = 0;
    size_t removed_collisions = 0;
    for (size_t i = 0; i < copy_inventory.objects.size(); ++i)
    {
        RE::NiAVObject* object = copy_inventory.objects[i];
        object->world = source_worlds[i];
        object->previousWorld = object->world;
        object->worldBound = source_bounds[i];
        removed_controllers += object->controllers ? 1 : 0;
        removed_collisions += object->collisionObject ? 1 : 0;
        object->controllers = nullptr;
        object->collisionObject = nullptr;
        object->SetUserData(nullptr);
    }
    for (RE::BSShaderProperty* property : copy_inventory.properties)
    {
        removed_controllers += property->controllers ? 1 : 0;
        property->controllers = nullptr;
    }
    // Snapshot the native clone's captured worlds without re-running facegen, Havok or SMP.
    m_snapshot = std::make_unique<CharacterClone>(RE::NiPointer<RE::NiAVObject>(root));
    logger::info("SCOPY READY capture={} controllers-removed={} collisions-removed={} draw=off actor-created=false F8=draw F3=rotate F4=release", m_capture_id, removed_controllers, removed_collisions);
    for (RE::NiAVObject* object : copy_inventory.objects)
        if (object->AsGeometry())
            logger::info("SCOPY GEOMETRY capture={} name='{}' type='{}'", m_capture_id, object->name.c_str() ? object->name.c_str() : "", object->GetRTTI()->GetName());
}

PLUGIN_NAMESPACE_END
