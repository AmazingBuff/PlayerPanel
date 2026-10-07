//
// Created by AmazingBuff on 2026/10/5.
//

#include "character_clone.h"

#include "render/studio/light.h"

PLUGIN_NAMESPACE_BEGIN

namespace
{
    constexpr char Clone_Postfix[] = "_Clone";
    constexpr float Park_Depth_Below_Player = 8000.0f;
    constexpr char Character_Node_Name[] = "Clone Node";
    
    constexpr RE::BGSBipedObjectForm::BipedObjectSlot Body_Slots[] =
    {
        RE::BGSBipedObjectForm::BipedObjectSlot::kBody,     RE::BGSBipedObjectForm::BipedObjectSlot::kHead,                 RE::BGSBipedObjectForm::BipedObjectSlot::kHands,
        RE::BGSBipedObjectForm::BipedObjectSlot::kForearms, RE::BGSBipedObjectForm::BipedObjectSlot::kAmulet,               RE::BGSBipedObjectForm::BipedObjectSlot::kRing,
        RE::BGSBipedObjectForm::BipedObjectSlot::kFeet,     RE::BGSBipedObjectForm::BipedObjectSlot::kCalves,               RE::BGSBipedObjectForm::BipedObjectSlot::kTail,
        RE::BGSBipedObjectForm::BipedObjectSlot::kLongHair, RE::BGSBipedObjectForm::BipedObjectSlot::kCirclet,              RE::BGSBipedObjectForm::BipedObjectSlot::kEars,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModMouth, RE::BGSBipedObjectForm::BipedObjectSlot::kModNeck,              RE::BGSBipedObjectForm::BipedObjectSlot::kModChestPrimary,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModChestSecondary, RE::BGSBipedObjectForm::BipedObjectSlot::kModShoulder, RE::BGSBipedObjectForm::BipedObjectSlot::kModArmLeft,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModArmRight, RE::BGSBipedObjectForm::BipedObjectSlot::kModLegRight,       RE::BGSBipedObjectForm::BipedObjectSlot::kModLegLeft,
        RE::BGSBipedObjectForm::BipedObjectSlot::kModFaceJewelry,
    };
    
    void mirror_worn_equipment(RE::Actor* clone, RE::Actor* actor)
    {
        RE::InventoryChanges* changes = actor->GetInventoryChanges(true);
        RE::ActorEquipManager* equip_manager = RE::ActorEquipManager::GetSingleton();
        if (!changes || !changes->entryList || !equip_manager)
        {
            logger::warn("Clone dressing unavailable (changes={} equip={})", static_cast<void*>(changes), static_cast<void*>(equip_manager));
            return;
        }

        std::uint32_t added = 0;
        for (const RE::InventoryEntryData* entry : *changes->entryList)
        {
            if (!entry || !entry->IsWorn())
                continue;
            RE::TESBoundObject* object = entry->object;
            if (!object)
                continue;

            RE::BGSBipedObjectForm* biped = object->As<RE::BGSBipedObjectForm>();
            if (biped && std::ranges::any_of(Body_Slots, [biped](RE::BGSBipedObjectForm::BipedObjectSlot slot) { return biped->HasPartOf(slot); }))
            {
                clone->AddObjectToContainer(object, nullptr, 1, nullptr);
                equip_manager->EquipObject(
                    clone,
                    object,
                    nullptr,
                    1,
                    nullptr,
                    false,  // queueEquip: applied in this call
                    true,   // forceEquip: the clone has no AI to choose
                    false,  // playSounds: silent
                    true);  // applyNow: dressed immediately
                ++added;
            }
        }
    }
    
    
    constexpr float Studio_Figure_Scale = 0.35f;
    constexpr float Studio_Facing_Z_Rad = 0.0f;
    constexpr float Studio_Standoff = 40.0f;
    constexpr float Studio_Depth = 485.0f;

    struct SkinnedBound
    {
        RE::NiPoint3 center;
        float radius;
        bool valid;
    };

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
            const auto& wb = geometry->worldBound;
            union_sphere(out, wb.center, wb.radius);
            return RE::BSVisit::BSVisitControl::kContinue;
        });
        return out;
    }

    struct SkinBindGroup
    {
        RE::NiAVObject* root;
        std::unordered_map<const RE::NiAVObject*, RE::NiTransform> bones;
    };

    void reset_bind_downward(RE::NiAVObject* object, const RE::NiTransform& parent_world, const SkinBindGroup& group)
    {
        RE::NiTransform world = parent_world * object->local;
        if (auto it = group.bones.find(object); it != group.bones.end())
        {
            world = it->second;
            object->local = parent_world.Invert() * world;
        }
        if (auto* node = object->AsNode())
        {
            for (auto& child : node->children)
            {
                if (child)
                    reset_bind_downward(child.get(), world, group);
            }
        }

    }

    void reset_root_to_bind_pose(RE::NiAVObject* object)
    {
        std::vector<SkinBindGroup> groups;
        RE::BSVisit::TraverseScenegraphGeometries(object, [&](RE::BSGeometry* geometry)
        {
            const RE::BSGeometry::GEOMETRY_RUNTIME_DATA& rd = geometry->GetGeometryRuntimeData();
            const RE::NiSkinInstance* skin = rd.skinInstance.get();
            if (!skin || !skin->bones || !skin->skinData || !skin->rootParent)
                return RE::BSVisit::BSVisitControl::kContinue;
            RE::NiAVObject* root = skin->rootParent;
            auto group = std::ranges::find_if(groups, [root](const SkinBindGroup& g) { return g.root == root; });
            if (group == groups.end())
            {
                groups.emplace_back(root);
                group = groups.end() - 1;
            }
            const RE::NiSkinData* data = skin->skinData.get();
            const std::uint32_t count = std::min(skin->numMatrices, data->GetBoneCount());
            for (std::uint32_t i = 0; i < count; ++i)
            {
                RE::NiAVObject* bone = skin->bones[i];
                if (bone)
                    group->bones.insert_or_assign(bone, data->GetBoneDataSkinToBone(i).Invert());
            }
            return RE::BSVisit::BSVisitControl::kContinue;
        });

        for (const auto& group : groups)
        {
            RE::NiTransform identity;
            if (RE::NiNode* node = group.root->AsNode())
            {
                for (RE::NiPointer<RE::NiAVObject>& child : node->children)
                {
                    if (child)
                        reset_bind_downward(child.get(), identity, group);
                }
            }
        }
    }

    void submit_pass(RE::BSRenderPass* pass)
    {
        // save for restore
        RE::BSLight** const saved_scene_lights = pass->sceneLights;
        const std::uint8_t saved_num_lights = pass->numLights;
        const std::uint8_t saved_shadow_lights = pass->numShadowLights;

        pass->numLights = Studio_Light_Count;
        pass->numShadowLights = 0;
        pass->sceneLights = StudioLight::instance().lights();

        RE::BSBatchRenderer::SetupAndDrawPass(pass, pass->passEnum, (pass->passEnum & 0x40) != 0, 0x200);

        pass->sceneLights = saved_scene_lights;
        pass->numLights = saved_num_lights;
        pass->numShadowLights = saved_shadow_lights;
    }
}

CharacterClone::CharacterClone(RE::Actor* actor) : m_wait_frames(0), m_light_warned(false), m_clone(nullptr), m_clone_state(CloneState::e_none)
{
    RE::TESNPC* source_base = actor ? actor->GetActorBase() : nullptr;
    RE::TESForm* duplicate = source_base ? source_base->CreateDuplicateForm(false, nullptr) : nullptr;
    RE::TESNPC* clone_base = duplicate ? duplicate->As<RE::TESNPC>() : nullptr;
    if (!clone_base)
    {
        logger::warn("Clone spawn failed: {} ({:08x})", actor ? actor->GetDisplayFullName() : nullptr, actor ? actor->GetFormID() : 0);
        return;
    }
    std::string name(actor->GetDisplayFullName());
    name.append(Clone_Postfix);
    clone_base->fullName = name;

    // remove all objects by cloning
    clone_base->defaultOutfit = nullptr;
    static_cast<RE::TESContainer*>(clone_base)->ClearDataComponent();

    RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
    RE::NiPoint3 position = player->GetPosition();
    position.z -= Park_Depth_Below_Player;
    const RE::TESObjectREFRPtr placed = RE::TESDataHandler::GetSingleton()->CreateReferenceAtLocation(clone_base, position, player->GetAngle(), player->GetParentCell(), player->GetWorldspace(), nullptr, nullptr, RE::ObjectRefHandle(), false, true).get();

    RE::Actor* clone = placed ? placed->As<RE::Actor>() : nullptr;
    if (!clone)
    {
        logger::warn("Clone spawn failed: {} ({:08x})", actor->GetDisplayFullName(), actor->GetFormID());
        return;
    }

    clone->GetActorRuntimeData().boolFlags.set(RE::Actor::BOOL_FLAGS::kMovementBlocked);
    clone->GetActorRuntimeData().boolFlags.set(RE::Actor::BOOL_FLAGS::kAttackingDisabled);
    clone->GetActorRuntimeData().boolFlags.set(RE::Actor::BOOL_FLAGS::kCastingDisabled);
    clone->SetActivationBlocked(true);
    clone->StopCombat();

    mirror_worn_equipment(clone, player);

    m_clone = placed;
    m_clone_state.store(CloneState::e_generated, std::memory_order_release);
    logger::info("Clone has been created at the player, from {} ({:08x})", actor->GetDisplayFullName(), actor->GetFormID());
}

CharacterClone::~CharacterClone()
{
    RE::NiAVObject* graph = m_graph_object.get();
    RE::NiNode* character_node = m_character_node.get();
    if (graph && character_node && graph->parent == character_node)
        character_node->DetachChild(graph);
    m_character_node = nullptr;
    m_graph_object = nullptr;
    m_clone_state.store(CloneState::e_none, std::memory_order_release);
}

bool CharacterClone::attach_graph(const RE::NiPointer<RE::NiNode>& host)
{
    if (m_clone_state.load(std::memory_order_acquire) == CloneState::e_graph)
        return true;

    RE::Actor* clone = m_clone ? m_clone->As<RE::Actor>() : nullptr;
    if (!clone)
    {
        logger::warn("Clone attach lost its subject (clone=null)");
        m_clone_state.store(CloneState::e_none, std::memory_order_release);
        return false;
    }
    RE::NiAVObject* graph = clone->Get3D(false);
    if (!graph)
    {
        logger::warn("Clone attach: biped 3D vanished");
        m_clone_state.store(CloneState::e_generated, std::memory_order_release);
        return false;
    }

    RE::NiNode* old_parent = graph->parent;
    if (!old_parent || !host)
    {
        logger::warn("Clone relocation unavailable (old_parent={} host={}): the graph stays parked", static_cast<void*>(old_parent), static_cast<void*>(host.get()));
        m_clone_state.store(CloneState::e_generated, std::memory_order_release);
        return false;
    }

    RE::NiNode* character_node = RE::NiNode::Create();
    if (!character_node)
    {
        logger::warn("Clone home node creation failed; the graph stays parked");
        m_clone_state.store(CloneState::e_generated, std::memory_order_release);
        return false;
    }
    character_node->name = Character_Node_Name;

    host->AttachChild(character_node);
    old_parent->DetachChild(graph);
    character_node->AttachChild(graph);

    if (RE::LOADED_REF_DATA* loaded = clone->loadedData)
        loaded->data3D = nullptr;

    RE::NiUpdateData update_data{
        .time = 0.0f,
        .flags = RE::NiUpdateData::Flag::kDirty
    };
    character_node->UpdateDownwardPass(update_data, 0);

    m_character_node.reset(character_node);
    m_graph_object.reset(graph);
    m_clone_state.store(CloneState::e_graph, std::memory_order_release);

    logger::info("Clone {} ({:08x}) has been attached to graph", clone->GetDisplayFullName(), clone->GetFormID());

    // kill actor
    clone->Disable();
    clone->SetDelete(true);

    m_clone = nullptr;

    return true;
}

void CharacterClone::pose()
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

    m_graph_object->local.translate = anchor;
    m_graph_object->local.rotate.SetEulerAnglesXYZ(0.0f, 0.0f, Studio_Facing_Z_Rad);
    m_graph_object->local.scale = 1.0f;

    reset_root_to_bind_pose(m_graph_object.get());

    RE::NiUpdateData update_data{
        0.0f, 
        RE::NiUpdateData::Flag::kDirty
    };
    m_graph_object->UpdateDownwardPass(update_data, 0);
    m_graph_object->UpdateWorldBound();

    m_graph_object->local.scale = Studio_Figure_Scale;
    m_graph_object->UpdateDownwardPass(update_data, 0);
    m_graph_object->UpdateWorldBound();

    const SkinnedBound centered = measure_skinned_bound(m_graph_object.get());

    m_graph_object->local.translate.x += anchor.x - centered.center.x;
    m_graph_object->local.translate.y += anchor.y - centered.center.y;
    m_graph_object->local.translate.z += anchor.z - centered.center.z;

    m_graph_object->UpdateDownwardPass(update_data, 0);
    m_graph_object->UpdateWorldBound();
}

void CharacterClone::draw(const RE::UI3DSceneManager* ui3d, const CommonStates& states, const RenderTarget& render_target)
{
    RE::NiAVObject* p_root = m_graph_object.get();
    const RE::NiPointer<RE::BSShaderAccumulator>& accumulator = ui3d->unk10;
    if (!p_root || !ui3d || !ui3d->camera || !accumulator)
        return;

    RE::NiUpdateData update_data{
        .time = 0.0f,
        .flags = RE::NiUpdateData::Flag::kDirty
    };

    const RE::NiPointer<RE::NiNode> studio_light_node = StudioLight::instance().light_node();
    RE::BSLight** lights = StudioLight::instance().lights();

    // move to studio position
    studio_light_node->local.translate = m_anchor;
    studio_light_node->UpdateDownwardPass(update_data, 0);

    RE::NiPoint3 saved_light_pos[Studio_Light_Count] = {};
    RE::NiNode* saved_node_parent[Studio_Light_Count] = {};
    bool node_mutated[Studio_Light_Count] = {};


    const auto& w2c = ui3d->camera->GetRuntimeData().worldToCam;

    RE::NiPoint3 right{ w2c[0][0], w2c[0][1], w2c[0][2] };
    RE::NiPoint3 up{ w2c[1][0], w2c[1][1], w2c[1][2] };
    RE::NiPoint3 forward{ w2c[2][0], w2c[2][1], w2c[2][2] };

    const float right_len = right.Length();
    const float up_len = up.Length();
    const float forward_len = forward.Length();
    if (right_len > 1e-6f)
        right *= 1.0f / right_len;
    if (up_len > 1e-6f)
        up *= 1.0f / up_len;
    if (forward_len > 1e-6f)
        forward *= 1.0f / forward_len;

    for (std::uint32_t i = 0; i < Studio_Light_Count; ++i)
    {
        RE::BSLight* light = lights[i];
        saved_light_pos[i] = light->worldTranslate;

        const float spread = (static_cast<float>(i) - (Studio_Light_Count - 1) * 0.5f) * 45.0f;
        const RE::NiPoint3 light_target = m_anchor - forward * 70.0f + up * 50.0f + right * spread;
        light->worldTranslate = light_target;  // culler copy — kept for free

        if (const RE::NiPointer<RE::NiLight>& ni_light = light->light; ni_light)
        {
            const RE::NiPoint3 local_offset{ right * spread + up * 50.0f - forward * 70.0f };
            ni_light->local.translate = local_offset;
            if (ni_light->parent)
            {
                saved_node_parent[i] = ni_light->parent;
                ni_light->parent->UpdateDownwardPass(update_data, 0);
                node_mutated[i] = true;
            }
        }
    }

    RE::BSGraphics::Renderer* renderer = RE::BSGraphics::Renderer::GetSingleton();
    const RE::BSGraphics::RendererData& runtime = renderer->GetRuntimeData();
    if (!runtime.context || !runtime.forwarder)
        return;

    //RE::BSGraphics::RendererShadowState::GetSingleton()->GetRuntimeData().stateUpdateFlags.reset(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);


    pose();

    REX::W32::D3D11_VIEWPORT viewport{
        .topLeftX = 0.0f,
        .topLeftY = 0.0f,
        .width = static_cast<float>(render_target.width()),
        .height = static_cast<float>(render_target.height()),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    runtime.context->RSSetViewports(1, &viewport);


    REX::W32::ID3D11RenderTargetView* const rtv = render_target.rtv();

    runtime.context->OMSetRenderTargets(1, &rtv, render_target.dsv());
    runtime.context->OMSetDepthStencilState(states.depth_default(), 0);

    constexpr float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    runtime.context->ClearRenderTargetView(rtv, clear_color);
    runtime.context->ClearDepthStencilView(render_target.dsv(), REX::W32::D3D11_CLEAR_DEPTH | REX::W32::D3D11_CLEAR_STENCIL, 1.0f, 0);

    runtime.context->OMSetBlendState(states.opaque(), nullptr, 0xFFFFFFFF);
    runtime.context->RSSetState(states.cull_none());

    RE::BSGraphics::RendererShadowState::GetSingleton()->GetRuntimeData().stateUpdateFlags.reset(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);

    std::uint32_t drawn = 0;
    RE::BSVisit::TraverseScenegraphGeometries(p_root, [&](RE::BSGeometry* geometry)
    {
        const RE::BSGeometry::GEOMETRY_RUNTIME_DATA& geom_rt = geometry->GetGeometryRuntimeData();
        if (!geom_rt.shaderProperty)
            return RE::BSVisit::BSVisitControl::kContinue;

        if (const RE::BSShaderProperty::RenderPassArray* pass_array = geom_rt.shaderProperty->GetRenderPasses(geometry, std::to_underlying(RE::BSShaderAccumulator::RENDER_MODE::kNormal), accumulator.get()))
        {
            for (RE::BSRenderPass* pass = pass_array->head; pass; pass = pass->next)
            {
                if (pass->geometry != geometry || !pass->shader)
                    continue;
                submit_pass(pass);
                ++drawn;
            }
        }
        return RE::BSVisit::BSVisitControl::kContinue;
    });

    if (drawn == 0)
    {
        logger::warn("Clone draw: no render pass to draw");
        return;
    }

    // move back to unreachable position
    RE::BSGraphics::RendererShadowState::GetSingleton()->GetRuntimeData().stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);

    for (std::uint32_t i = 0; i < Studio_Light_Count; ++i)
    {
        lights[i]->worldTranslate = saved_light_pos[i];

        if (node_mutated[i] && saved_node_parent[i])
            saved_node_parent[i]->UpdateDownwardPass(update_data, 0);
    }

    studio_light_node->local.translate = {0.0f, 0.0f, Studio_Light_Pos_Z};
    studio_light_node->UpdateDownwardPass(update_data, 0);
}

PLUGIN_NAMESPACE_END
