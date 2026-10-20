//
// Created by AmazingBuff on 2026/10/08.
//

#include "character/scene_graph.h"


#include "character/figure_audit.h"
#include "character/held_equipment.h"
#include "character/snapshot_transform.h"

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

    // Recompute a subtree's world transforms by composing a world-space delta onto the captured
    // worlds are copied without advancing source controllers.

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

void SceneGraph::retire_graph(RE::NiPointer<RE::NiAVObject> graph)
{
    if (!graph) return;
    // Only bookkeeping is bounded. These retained references outlive every SceneGraph instance.
    static std::vector<RE::NiPointer<RE::NiAVObject>*> s_parked_graphs;
    static bool s_cap_logged = false;
    static constexpr size_t s_parked_graph_cap = 12;
    s_parked_graphs.push_back(std::make_unique<RE::NiPointer<RE::NiAVObject>>(std::move(graph)).release());
    logger::info("SCOPY PARK actor={:08x} snapshots held alive for the process lifetime: {}", source_id(), s_parked_graphs.size());
    if (s_parked_graphs.size() > s_parked_graph_cap)
    {
        if (!s_cap_logged) logger::warn("SCOPY PARK cap reached; oldest graph remains allocated");
        s_cap_logged = true;
        s_parked_graphs.erase(s_parked_graphs.begin());
    }
}

RE::NiPointer<RE::NiAVObject> SceneGraph::capture_graph(RE::Actor& actor)
{
    // Re-select the target on the graph this capture produces: the cached one now points into the
    // parked copy the panel no longer draws. The idle's joints are the same kind of per-graph state.
    ++m_capture_id;
    RE::Actor* const player = &actor;
    RE::NiPointer<RE::NiAVObject> source(actor.Get3D(false));
    if (!source || !source->AsNode() || !std::isfinite(source->world.scale) || std::abs(source->world.scale) < 0.0001f)
    {
        logger::warn("SCOPY FAILED capture={} reason=no-valid-third-person-root", m_capture_id);
        return {};
    }
    GraphInventory source_inventory{};
    if (!inventory_graph(source.get(), source_inventory))
    {
        logger::error("SCOPY FAILED capture={} reason=source-cycle-or-object-limit", m_capture_id);
        return {};
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
        return {};
    }
    GraphInventory copy_inventory{};
    if (!inventory_graph(root, copy_inventory))
    {
        logger::error("SCOPY FAILED capture={} reason=copy-cycle-or-object-limit", m_capture_id);
        return {};
    }
    const SourceDiff diff = diff_source_graph(source.get(), source_inventory, source_worlds, source_locals, source_bounds, source_parent, player->Get3D(false) == source.get());
    if (!diff.same_object_set)
    {
        logger::error("SCOPY BLOCKED capture={} reason=source-object-set-differs (no pose or draw was performed)", m_capture_id);
        return {};
    }
    const bool accepted = audit_copy(source_inventory, copy_inventory);
    if (!accepted)
    {
        logger::error("SCOPY BLOCKED capture={} reason=isolation-audit (no pose or draw was performed)", m_capture_id);
        return {};
    }
    for (size_t i = 0; i < source_inventory.objects.size(); ++i)
    {
        RE::NiAVObject* original = source_inventory.objects[i];
        RE::NiAVObject* target = copy_inventory.objects[i];
        if (original->name != target->name || std::strcmp(original->GetRTTI()->GetName(), target->GetRTTI()->GetName()) != 0)
        {
            logger::error("SCOPY BLOCKED capture={} reason=structural-correspondence index={} source-name='{}' copy-name='{}'", m_capture_id, i, original->name.c_str(), target->name.c_str());
            return {};
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
    hold_copy_equipment(actor, source_inventory.objects, copy_inventory.objects);
    logger::info("SCOPY READY actor={:08x} capture={} controllers-removed={} collisions-removed={}",
        actor.GetFormID(), m_capture_id, removed_controllers, removed_collisions);
    for (RE::NiAVObject* object : copy_inventory.objects)
        if (object->AsGeometry())
            logger::info("SCOPY GEOMETRY capture={} name='{}' type='{}'", m_capture_id, object->name.c_str() ? object->name.c_str() : "", object->GetRTTI()->GetName());
    return RE::NiPointer<RE::NiAVObject>(root);
}

PLUGIN_NAMESPACE_END
