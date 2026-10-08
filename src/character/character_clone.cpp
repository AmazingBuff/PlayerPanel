//
// Created by AmazingBuff on 2026/10/5.
//

#include "character_clone.h"
#include "character/snapshot_transform.h"

#include "render/studio/light.h"

PLUGIN_NAMESPACE_BEGIN

namespace
{
    constexpr char Clone_Postfix[] = "_Clone";
    // Assembly pacing, not engine timing: the detach gate is the bind-bone
    // count, the frame counters only debounce and expire.
    constexpr uint32_t Clone_Assemble_Wait_Frames = 3;
    // 20 s: the render gate waits for the player to look somewhere that
    // culls the clone IN — its timing depends on where the camera points
    // after a load, so it needs more headroom than the attach signals.
    constexpr uint32_t Clone_Assemble_Limit_Frames = 1200;
    constexpr uint32_t Clone_Assemble_Warn_Frames = 120;
    // Settle window after dressing: equipping REMOVES the replaced body-part
    // geometries immediately while the armor nifs attach ASYNCHRONOUSLY over
    // the next frames (2026-10-07 23:56 log: detached 14 ms after dressed,
    // geoms 12→8 — the four replaced body pieces gone, armor never mounted
    // because data3D was already severed). Detaching on the frame where all
    // CURRENT geometries are bound therefore races the armor load and loses
    // almost every time. Require the geometry census to hold steady across
    // this window before the detach — new armor pieces change it.
    constexpr uint32_t Clone_Settle_Wait_Frames = 30;
    constexpr uint32_t Clone_Settle_Stable_Frames = 20;
    constexpr uint32_t Clone_Settle_Limit_Frames = 600;
    constexpr uint32_t Clone_Settle_Warn_Frames = 120;
    
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

    // Flash guard: while the shell waits at the player its fade node is
    // pinned to a near-invisible alpha — NOT zero. A zeroed BSFadeNode is
    // skipped by the engine's renderer entirely, which would starve the
    // rendered gate (numMatrices never written) and stall the detach on
    // some loads. 0.05 keeps the draw alive (numMatrices written) while
    // staying practically invisible next to the player body. 0x130
    // currentFade is the CLib-named field; 0x128/0x12C are handled as
    // target/rate per the v6.70 read.
    void suppress_fade(RE::NiAVObject* graph)
    {
        if (RE::BSFadeNode* fade = graph->AsFadeNode())
        {
            fade->unk128 = 0.05f;
            fade->unk12C = 0.0f;
            fade->currentFade = 0.05f;
        }
    }

    void restore_fade(RE::NiAVObject* graph)
    {
        if (RE::BSFadeNode* fade = graph->AsFadeNode())
        {
            fade->unk128 = 1.0f;
            fade->unk12C = 0.0f;
            fade->currentFade = 1.0f;
        }
    }

    struct AssemblyStats
    {
        std::uint32_t geometries;
        std::uint32_t skinned;
        std::uint32_t roots;
        std::uint32_t num_matrices;
        std::uint32_t bone_count;
        std::uint32_t alloc_slots;
        std::uint32_t bound;
        std::uint32_t bound_nodes;
        std::uint32_t skin_data_null;
        std::uint32_t bones_array_null;
    };

    // Bind-bone census over skin roots. Three different layers live here:
    // numMatrices = per-frame render activity (SetupGeometry writes it);
    // bone_count = NiSkinData capacity (nif data, static); bound_nodes =
    // skin->bones[i] pointers actually filled — the skin-to-skeleton
    // BINDING state. Armor attach is async (a first-load head part can
    // outwait any fixed frame window), and an unbound skin renders its
    // vertices at model-space origin — "at the feet".
    AssemblyStats measure_assembly(RE::NiAVObject* root)
    {
        AssemblyStats out{ .geometries = 0, .skinned = 0, .roots = 0, .num_matrices = 0, .bone_count = 0, .bound = 0, .bound_nodes = 0, .skin_data_null = 0, .bones_array_null = 0 };
        std::unordered_set<const RE::NiAVObject*> seen_roots;
        RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* geometry)
        {
            ++out.geometries;
            const RE::NiSkinInstance* skin = geometry->GetGeometryRuntimeData().skinInstance.get();
            if (!skin || !skin->rootParent)
                return RE::BSVisit::BSVisitControl::kContinue;
            ++out.skinned;
            if (seen_roots.insert(skin->rootParent).second)
                ++out.roots;
            if (!skin->skinData)
            {
                ++out.skin_data_null;
                return RE::BSVisit::BSVisitControl::kContinue;
            }
            out.num_matrices += skin->numMatrices;
            const std::uint32_t cap = skin->skinData->GetBoneCount();
            out.bone_count += cap;
            // The engine allocates the instance bone-pointer array on demand;
            // "fully bound" is measured against the ALLOCATED slots, not the
            // declared capacity (a 3BA skin measured 47 slots vs 131 declared,
            // bound and rendering fine — waiting for 131 cost 600 frames).
            const std::uint32_t n = std::min(cap, skin->allocatedSize);
            out.alloc_slots += n;
            out.bound += std::min(skin->numMatrices, cap);
            if (!skin->bones)
            {
                ++out.bones_array_null;
                return RE::BSVisit::BSVisitControl::kContinue;
            }
            for (std::uint32_t i = 0; i < n; ++i)
                if (skin->bones[i])
                    ++out.bound_nodes;
            return RE::BSVisit::BSVisitControl::kContinue;
        });
        return out;
    }

    // Geom name census for the detach-time forensics log: the settle gate
    // above needs its one-shot evidence line to be decidable from the log
    // alone (which 8 of 12 survived, which armor mounted).
    void log_geometry_names(RE::NiAVObject* root)
    {
        std::uint32_t count = 0;
        std::string names;
        RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* geometry)
        {
            const char* n = geometry->name.c_str();
            names.append("  ");
            names.append(n && *n ? n : "(null)");
            ++count;
            return RE::BSVisit::BSVisitControl::kContinue;
        });
        logger::info("Clone geometry census ({}):{}", count, names);
    }

    // Body and physics mods name their skeleton-driven collision helpers after these
    // tokens; they carry a skinInstance but are not part of the visible figure.
    bool is_collision_helper(const char* name)
    {
        if (!name || !name[0])
            return false;
        constexpr std::string_view tokens[] = { "Collision", "VirtualGround", "3BCA_", "Stopper" };
        const std::string_view view{ name };
        for (const std::string_view token : tokens)
            if (view.find(token) != std::string_view::npos)
                return true;
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

    void reset_root_to_bind_pose(RE::NiAVObject* object, bool& map_logged, std::uint32_t& map_stall)
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
                if (!bone)
                    continue;
                // Head/neck bones stay at their engine-driven pose. The
                // facegen tree is parented to the HEAD BONE on CBBE (follows
                // the T-pose reset) but to the SKELETON ROOT on UBE (bind
                // data in absolute skeleton coordinates) — resetting the
                // head bones moves them without moving the UBE facegen tree,
                // tearing the head off the body. The head pose freezes at
                // whatever the engine last drove; that is an FR-03 concern.
                if (const char* bone_name = bone->name.c_str(); bone_name && (std::strstr(bone_name, "[Head]") != nullptr || std::strstr(bone_name, "Neck") != nullptr))
                    continue;
                group->bones.insert_or_assign(bone, data->GetBoneDataSkinToBone(i).Invert());
            }
            return RE::BSVisit::BSVisitControl::kContinue;
        });

        for (const auto& group : groups)
        {
            // SKIP the facegen group. Its root (BSFaceGenNiNodeSkinned) hosts
            // the hair-physics bones (hdtSSE) and its bind data references
            // skeleton bones living OUTSIDE its subtree (Spine2, Head — CBBE
            // measures exactly 2, both unreachable from the facegen walk, so
            // this reset was always a no-op there). On UBE, ~207 hair-physics
            // bones DO live in the subtree and were reset against a frame
            // their bind data doesn't match, fighting the SMP simulation —
            // the head-body separation. The head model itself follows the
            // skeleton bones (Head/Spine2 are reset by the skeleton group),
            // so skipping changes nothing on CBBE and hands the physics bones
            // back to SMP on UBE.
            const RE::NiRTTI* root_rtti = group.root->GetRTTI();
            if (root_rtti && std::strstr(root_rtti->GetName(), "BSFaceGenNiNode") != nullptr)
                continue;

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

        // One-shot PER-CLONE map dump: the skin binding is suspected of
        // index-order divergence on runtime-built skins (UBE facegen — head
        // vertices sampled a low-body bone's matrix). The first poses run
        // BEFORE SetupGeometry writes numMatrices, when these maps are
        // legitimately empty — dump only once populated (or after a bounded
        // stall). State lives on the clone (per-instance, not static): a
        // CBBE session must not consume the UBE session's dump.
        if (!map_logged)
        {
            std::size_t total = 0;
            for (const auto& group : groups)
                total += group.bones.size();
            const bool stalled = ++map_stall > 600;
            if (total > 0 || stalled)
            {
                map_logged = true;
                if (stalled && total == 0)
                    logger::warn("T-pose maps stayed empty for 600 frames; dumping anyway");
                for (const auto& group : groups)
                {
                    logger::info(
                        "T-pose group root='{}' bones={}",
                        group.root->name.c_str() ? group.root->name.c_str() : "(null)",
                        group.bones.size());
                    std::uint32_t k = 0;
                    for (const auto& [bone, world] : group.bones)
                    {
                        if (k >= 10)
                            break;
                        logger::info(
                            "  bind[{}] bone='{}' world=({:.1f},{:.1f},{:.1f})",
                            k,
                            bone->name.c_str() ? bone->name.c_str() : "(null)",
                            world.translate.x, world.translate.y, world.translate.z);
                        ++k;
                    }
                }
            }
        }
    }

    // Per-pass studio lighting (v6.56 slot convention: sceneLights[0] is the
    // engine's ambient slot, point lights start at [1]). The light array is
    // re-fetched from the ShadowSceneNode ledger by StudioLight::refresh()
    // every frame, so the pointers handed to the pass are ledger-fresh for
    // the whole window — the run 55/56 copied-pointer crash class. The
    // engine's light tick keeps rewriting foreign shells (run 88 pinned
    // lodDimmer back to 0 every frame), so the shells are re-patched inside
    // the window, right before their pass draws.
    // The facegen head (BSFaceGenNiNode skinned tree) is attached to the
    // skeleton ASYNCHRONOUSLY by the engine — detaching before it lands
    // means the engine never finishes the job and the panel figure has no
    // head. Wait for it (bounded); it is an attach-layer signal, so no
    // deadlock risk.
    bool facegen_attached(RE::NiAVObject* root)
    {
        bool found = false;
        RE::BSVisit::TraverseScenegraphObjects(root, [&](RE::NiAVObject* object)
        {
            if (const RE::NiRTTI* rtti = object->GetRTTI(); rtti && std::strstr(rtti->GetName(), "BSFaceGenNiNode") != nullptr)
            {
                found = true;
                return RE::BSVisit::BSVisitControl::kStop;
            }
            return RE::BSVisit::BSVisitControl::kContinue;
        });
        return found;
    }

    void submit_pass(RE::BSRenderPass* pass, bool inject_lights, RE::BSLight** studio_lights)
    {
        RE::BSLight** saved_scene_lights = nullptr;
        std::uint8_t saved_num_lights = 0;
        std::uint8_t saved_shadow_lights = 0;
        if (inject_lights)
        {
            saved_scene_lights = pass->sceneLights;
            saved_num_lights = pass->numLights;
            saved_shadow_lights = pass->numShadowLights;

            pass->numLights = static_cast<std::uint8_t>(Studio_Light_Count);
            pass->numShadowLights = 0;
            pass->sceneLights = studio_lights;

            for (std::size_t i = 0; i < Studio_Light_Count; ++i)
            {
                if (RE::BSLight* shell = studio_lights[i])
                {
                    shell->lodDimmer = 1.0f;
                    shell->luminance = 1.0f;
                    shell->frustrumCull = 0;
                }
            }
        }

        RE::BSBatchRenderer::SetupAndDrawPass(pass, pass->passEnum, (pass->passEnum & 0x40) != 0, 0x200);

        if (inject_lights)
        {
            pass->sceneLights = saved_scene_lights;
            pass->numLights = saved_num_lights;
            pass->numShadowLights = saved_shadow_lights;
        }
    }
}

CharacterClone::CharacterClone(RE::Actor* actor) : m_clone(nullptr), m_clone_state(CloneState::e_none), m_wait_frames(0), m_dressed(false), m_bind_map_logged(false), m_bind_map_stall(0), m_is_snapshot(false), m_snapshot_angle(0.0f), m_draw_frames(0)
{
    m_settle_last_geoms = 0;
    m_settle_last_skinned = 0;
    m_settle_stable = 0;
    m_geom_names_logged = false;
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

    // PlaceObjectAtMe at the player, and STAY there while assembling — this
    // is the exact v6.70 recipe (run 105: 74 bind bones, no visible clone).
    // The bind-bone census taught us why: NiSkinInstance::bones is filled by
    // the engine's first actual render of the geometry, and fade=0 does not
    // opt out of the culler. Hiding the clone underground or far away (both
    // tried) skips the cull entirely, the skin never binds, and the graph
    // stays boneless forever. Transparent-at-the-player is the only spot
    // that is both invisible and rendered.
    const RE::NiPointer<RE::TESObjectREFR> placed = player->PlaceObjectAtMe(clone_base, false);

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

    // NO dressing here: equipping in the birth frame races the skeleton
    // assembly (armor attach vs skin bind) and the skin never binds — the
    // m0 order (spawn → grace → dressing, runs 23-105) dresses only after
    // the bare skeleton has bound. detach_graph runs the phases.

    m_clone = placed;
    m_clone_state.store(CloneState::e_generated, std::memory_order_release);
    m_wait_frames = 0;
    logger::info("Clone has been created at the player, from {} ({:08x})", actor->GetDisplayFullName(), actor->GetFormID());
}

CharacterClone::CharacterClone(RE::NiPointer<RE::NiAVObject> graph) :
    m_clone(nullptr), m_clone_state(CloneState::e_ready), m_graph_object(std::move(graph)),
    m_wait_frames(0), m_dressed(false), m_bind_map_logged(false), m_bind_map_stall(0),
    m_is_snapshot(true), m_snapshot_angle(0.0f), m_draw_frames(0)
{
    restore_fade(m_graph_object.get());
    const size_t pruned = prune_effect_objects(m_graph_object.get());
    m_snapshot_root_world = m_graph_object->world;
    const SkinnedBound bound = measure_skinned_bound(m_graph_object.get());
    m_snapshot_center = bound.valid ? bound.center : m_graph_object->worldBound.center;
    logger::info("SCOPY FRAME skinned-bound valid={} radius={:.1f} center=({:.1f},{:.1f},{:.1f}) graph-bound radius={:.1f} scale={:.2f} pruned-nodes={}",
        bound.valid, bound.radius, m_snapshot_center.x, m_snapshot_center.y, m_snapshot_center.z, m_graph_object->worldBound.radius, Studio_Figure_Scale, pruned);
    RE::BSVisit::TraverseScenegraphObjects(m_graph_object.get(), [&](RE::NiAVObject* object)
    {
        m_snapshot_nodes.push_back({ object, object->world, object->worldBound });
        return RE::BSVisit::BSVisitControl::kContinue;
    });
}

void CharacterClone::rotate_snapshot()
{
    if (m_is_snapshot)
    {
        m_snapshot_angle += 1.57079632679f;
        logger::info("SCOPY ROTATE angle-deg={:.0f}", m_snapshot_angle * 57.2957795131f);
    }
}

CharacterClone::~CharacterClone()
{
#if CHARACTER_PANEL_SCENE_COPY_EXPERIMENT
    // The experiment's snapshot graph is a native NiObject::Clone that no engine teardown
    // path survives: releasing it crashed in capture(), at a clean frame boundary and at
    // the main-menu boundary (crashes 2026-10-08 23-01-11, 23-07-35, 23-12-02), always in
    // BSFadeNode's destructor through the tbb allocator. This class holds the graph's only
    // reference, so leaving m_graph_object untouched keeps it alive and the process
    // reclaims it at exit.
    (void)m_graph_object.get();
#else
    // A detached graph is solely owned here; releasing the pointer frees it.
    // A not-yet-detached graph is still engine-managed through the shell, and
    // m_clone releases the plugin's reference only.
    m_graph_object = nullptr;
#endif
    m_clone_state.store(CloneState::e_none, std::memory_order_release);
}

bool CharacterClone::detach_graph()
{
    if (m_clone_state.load(std::memory_order_acquire) == CloneState::e_ready)
        return true;
    if (m_clone_state.load(std::memory_order_acquire) == CloneState::e_discarded)
        return false;

    RE::Actor* clone = m_clone ? m_clone->As<RE::Actor>() : nullptr;
    if (!clone)
    {
        logger::warn("Clone detach lost its subject (clone=null)");
        m_clone_state.store(CloneState::e_none, std::memory_order_release);
        return false;
    }

    // Take the graph straight from loadedData: it exists as soon as the
    // skeleton is built, so the fade guard engages on the very first frame
    // instead of waiting for a readiness gate. The clone stays AT the player
    // and transparent while assembling — the culler must keep rendering it
    // or the skin never binds (see the ctor comment).
    RE::LOADED_REF_DATA* loaded = clone->loadedData;
    RE::NiAVObject* graph = loaded ? loaded->data3D.get() : nullptr;
    if (!graph)
        return false;  // skeleton still building — quiet retry next frame

    // born-invisible for the whole assembling window
    suppress_fade(graph);

    ++m_wait_frames;

    const AssemblyStats stats = measure_assembly(graph);

    if (!m_dressed)
    {
        // The skin binding happens through the engine's own SetupGeometry
        // inside the studio's SetupAndDrawPass — i.e. AFTER the detach (the
        // m0 "74 bind bones" were always measured post-detach). Gating the
        // detach on `bound` deadlocks the binding behind the very pipeline
        // that performs it. The census below is an observation log only.
        //
        // The facegen head, however, is attached to the skeleton
        // ASYNCHRONOUSLY — detaching before it lands means the engine never
        // finishes the job and the figure is headless. Wait for it
        // (bounded); it is an attach-layer signal, no deadlock risk.
        const bool facegen = facegen_attached(graph);
        // numMatrices > 0 = the engine has rendered this clone at least once
        // (SetupGeometry writes it) — the boneMatrices buffer is initialized.
        // Detaching before that leaves the body rendering from an
        // uninitialized matrix buffer on the next loads (invisible body).
        // The clone sits at the player and is culled-in, so this is a
        // guaranteed event — but its timing depends on where the player
        // looks after a load, hence the bounded fallback.
        const bool rendered = stats.num_matrices > 0;
        if (m_wait_frames % Clone_Assemble_Warn_Frames == 1)
            logger::info(
                "Clone graph census: geoms={} skinned={} roots={} num-matrices={} bone-count={} bound={} skin-data-null={} facegen={} rendered={}",
                stats.geometries, stats.skinned, stats.roots, stats.num_matrices, stats.bone_count, stats.bound, stats.skin_data_null, facegen, rendered);
        const bool ready = facegen && rendered;
        if (!ready && m_wait_frames > Clone_Assemble_Limit_Frames)
            logger::warn(
                "Clone detach: assembly incomplete after {} frames (facegen={} rendered={}); detaching anyway",
                m_wait_frames, facegen, rendered);
        else if (!ready)
            return false;
        if (m_wait_frames < Clone_Assemble_Wait_Frames)
            return false;

        // Phase 2: dress while the shell is still alive (the m0 order:
        // spawn → grace → dressing). Equipping in the birth frame raced the
        // skeleton assembly.
        mirror_worn_equipment(clone, RE::PlayerCharacter::GetSingleton());
        m_dressed = true;
        m_wait_frames = 0;
        logger::info(
            "Clone dressed pre-detach: geoms={} skinned={} roots={} num-matrices={} bone-count={} bound={}",
            stats.geometries, stats.skinned, stats.roots, stats.num_matrices, stats.bone_count, stats.bound);
        return false;
    }

    // Phase 3: wait until every skinned piece is actually BOUND to the
    // skeleton AND the geometry set has SETTLED. Equipping removes the
    // replaced body-part geometries immediately while the armor nifs mount
    // asynchronously over the following frames (2026-10-07 23:56: every
    // detach ran 14 ms after dressing, geoms 12→8 — the four replaced
    // pieces were gone and the armor never arrived because data3D was
    // severed right away). "All current geometries bound" is trivially
    // true on the naked subset during that race, so the bind census alone
    // cannot gate the detach — the settle window holds the geometry census
    // steady across it instead. Bounded fallback detaches anyway.
    const bool fully_bound = stats.skin_data_null == 0 && stats.bones_array_null == 0 && stats.alloc_slots > 0 && stats.bound_nodes >= stats.alloc_slots;
    if (stats.geometries != m_settle_last_geoms || stats.skinned != m_settle_last_skinned)
    {
        if (m_settle_last_geoms != 0 || m_settle_last_skinned != 0)
            logger::info(
                "Clone dressing geometry change: geoms={}→{} skinned={}→{} (settle counter reset)",
                m_settle_last_geoms, stats.geometries, m_settle_last_skinned, stats.skinned);
        m_settle_last_geoms = stats.geometries;
        m_settle_last_skinned = stats.skinned;
        m_settle_stable = 0;
    }
    else
        ++m_settle_stable;

    const bool settled = m_wait_frames >= Clone_Settle_Wait_Frames && m_settle_stable >= Clone_Settle_Stable_Frames;
    if ((!fully_bound || !settled) && m_wait_frames <= Clone_Settle_Limit_Frames)
    {
        if (m_wait_frames % Clone_Settle_Warn_Frames == 1)
            logger::info(
                "Clone dressing census: geoms={} skinned={} roots={} num-matrices={} bone-count={} bound-nodes={} skin-data-null={} bones-array-null={} stable={} (waiting for full bind + settle)",
                stats.geometries, stats.skinned, stats.roots, stats.num_matrices, stats.bone_count, stats.bound_nodes, stats.skin_data_null, stats.bones_array_null, m_settle_stable);
        return false;
    }
    if (!fully_bound)
        logger::warn(
            "Clone detach: dressing never fully bound after {} frames (bound {}/{} bone slots); detaching anyway",
            m_wait_frames, stats.bound_nodes, stats.bone_count);
    else if (!settled)
        logger::warn(
            "Clone detach: geometry set never settled after {} frames (stable={}); detaching anyway",
            m_wait_frames, m_settle_stable);

    // One-shot forensics: the final geometry set at detach time, so a bad
    // round is decidable from the log alone (which pieces survived, which
    // armor mounted).
    if (!m_geom_names_logged)
    {
        m_geom_names_logged = true;
        log_geometry_names(graph);
    }

    RE::NiNode* old_parent = graph->parent;
    if (!old_parent)
    {
        logger::warn("Clone detach: graph has no world parent to detach from");
        m_clone_state.store(CloneState::e_generated, std::memory_order_release);
        return false;
    }

    // Sever the engine's claim BEFORE the detach: with data3D live the
    // shell's 3D bookkeeping re-parents the graph back into the world every
    // tick (run 96: 707 re-home rounds per session); nulled, the engine
    // holds no reference to fight over, and the shell kill below loses the
    // run-51 "Disable destroys the 3D" mechanism constructively.
    loaded->data3D = nullptr;

    // Three pointer moves in the m0 relocate order, minus the re-home: the
    // graph is never hosted under a menu scene root — it becomes solely
    // owned by this object (see class comment for why hosting is fatal).
    old_parent->DetachChild(graph);

    restore_fade(graph);  // plugin-owned now; the studio needs it visible

    RE::NiUpdateData update_data{
        .time = 0.0f,
        .flags = RE::NiUpdateData::Flag::kDirty
    };
    graph->UpdateDownwardPass(update_data, 0);

    m_graph_object.reset(graph);
    m_clone_state.store(CloneState::e_ready, std::memory_order_release);

    logger::info(
        "Clone {} ({:08x}) detached from the world graph: geoms={} skinned={} roots={} num-matrices={} bone-count={} bound={}",
        clone->GetDisplayFullName(), clone->GetFormID(), stats.geometries, stats.skinned, stats.roots, stats.num_matrices, stats.bone_count, stats.bound);

    // Park the shell DISABLED but ALIVE — SetDelete is the killer: its actor
    // teardown (biped dismantle) strips the worn-armor skins out of the
    // detached graph, leaving head+hands on some loads (2026-10-07 23:48-50,
    // four geometries dropped at the detach). A disabled actor with data3D
    // severed is a state the engine maintains natively: no 3D re-assembly,
    // no tick, invisible (m0 kept live shells through 100+ rounds). The
    // shell reference is released at the next save-load boundary together
    // with the clone.
    clone->Disable();

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

    if (m_is_snapshot)
    {
        RE::NiTransform placement;
        placement.translate = anchor;
        placement.rotate.SetEulerAnglesXYZ(0.0f, 0.0f, m_snapshot_angle);
        placement.scale = Studio_Figure_Scale;
        const RE::NiTransform delta = snapshot_delta(m_snapshot_root_world, placement, m_snapshot_center, anchor);
        m_graph_object->local = delta * m_snapshot_root_world;

        // Preserve captured SMP world poses; do not run copied controllers or Actor callbacks.
        for (const SnapshotNode& node : m_snapshot_nodes)
        {
            node.object->world = delta * node.world;
            node.object->previousWorld = node.object->world;
            node.object->worldBound.center = delta * node.bound.center;
            node.object->worldBound.radius = std::abs(delta.scale) * node.bound.radius;
        }
        return;
    }

    m_graph_object->local.translate = anchor;
    m_graph_object->local.rotate.SetEulerAnglesXYZ(0.0f, 0.0f, Studio_Facing_Z_Rad);
    m_graph_object->local.scale = 1.0f;

    reset_root_to_bind_pose(m_graph_object.get(), m_bind_map_logged, m_bind_map_stall);

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

bool CharacterClone::draw(const RE::UI3DSceneManager* ui3d, const CommonStates& states, const RenderTarget& render_target)
{
    if (!detach_graph())
        return false;

    RE::NiAVObject* p_root = m_graph_object.get();
    if (!p_root || !ui3d || !ui3d->camera || !ui3d->unk10)
        return false;
    const RE::NiPointer<RE::BSShaderAccumulator>& accumulator = ui3d->unk10;

    RE::BSGraphics::Renderer* renderer = RE::BSGraphics::Renderer::GetSingleton();
    const RE::BSGraphics::RendererData& runtime = renderer->GetRuntimeData();
    if (!runtime.context || !runtime.forwarder)
        return false;

    RE::BSGraphics::RendererShadowState::GetSingleton()->GetRuntimeData().stateUpdateFlags.reset(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);


    pose();

    // Pull the detached light rig to the anchor and cascade it: the shader
    // reads NiLight::world.translate (v6.37) and this window is the only
    // place the cascade runs. Must precede the pass generation.
    StudioLight::instance().place(m_anchor);

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


    // Per-pass injection engages only when every studio slot is ledger-served;
    // otherwise the passes draw with whatever the accumulator carries.
    RE::BSLight** studio_lights = StudioLight::instance().lights();
    const bool inject_lights = studio_lights && studio_lights[0] && studio_lights[1] && studio_lights[2];

    std::uint32_t drawn = 0;
    std::uint32_t renderer_data_null = 0;
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
                // Diagnostics only — never gate the draw on rendererData: the
                // engine's SetupAndDrawPass lazily creates the device buffers
                // (run 95), and a gate here would remove the only caller that
                // could ever initialize them (run 52).
                if (!geom_rt.rendererData)
                    ++renderer_data_null;
                submit_pass(pass, inject_lights, studio_lights);
                ++drawn;
            }
        }
        return RE::BSVisit::BSVisitControl::kContinue;
    });

    // Per-frame summary plus the skin census: after the detach, the engine's
    // SetupGeometry writes numMatrices/boneMatrices every drawn frame — the
    // census watching `bound` climb (0 → ~74) is the binding-has-run proof.
    if (!m_is_snapshot || ++m_draw_frames % 120 == 1)
    {
        const AssemblyStats census = measure_assembly(p_root);
        logger::info(
            "Clone draw: passes={} rd-null={} lights={} | geoms={} skinned={} roots={} num-mat={} bone-count={} bound={}",
            drawn, renderer_data_null, inject_lights ? Studio_Light_Count : 0,
            census.geometries, census.skinned, census.roots, census.num_matrices, census.bone_count, census.bound);
        if (m_is_snapshot)
            logger::info("SCOPY DRAW frame={} passes={} lights={} animation-driver=none physics-driver=none", m_draw_frames, drawn, inject_lights);
    }

    RE::BSGraphics::RendererShadowState::GetSingleton()->GetRuntimeData().stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);

    if (drawn == 0)
    {
        logger::warn("Clone draw: no render pass to draw");
        StudioLight::instance().park();
        return false;
    }

    // Every draw-window exit parks the rig far out of gameplay space — the
    // ledger shells would otherwise keep lighting the world from the anchor
    // (the 2026-10-07 21:38 light-leak screenshot).
    StudioLight::instance().park();
    return true;
}

PLUGIN_NAMESPACE_END
