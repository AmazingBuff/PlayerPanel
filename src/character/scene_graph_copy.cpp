//
// Created by AmazingBuff on 2026/10/08.
//

#include "character/scene_graph_copy.h"

#include "character/character_clone.h"
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

SceneGraphCopy::SceneGraphCopy() : m_command(Command::e_none), m_generation(0), m_seen_generation(0), m_capture_id(0), m_draw_enabled(false), m_frame(0), m_cap_logged(false) {}
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
    default:
        break;
    }
}

CharacterClone* SceneGraphCopy::drawable() const
{
    return m_draw_enabled ? m_snapshot.get() : nullptr;
}

void SceneGraphCopy::capture()
{
    m_draw_enabled = false;
    retire(std::move(m_snapshot));
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
