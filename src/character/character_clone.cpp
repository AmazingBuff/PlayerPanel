//
// Created by AmazingBuff on 2026/10/5.
//

#include "character_clone.h"
#include "character/equipment_mirror.h"
#include "character/figure_audit.h"
#include "character/snapshot_transform.h"

#include "render/studio/figure_passes.h"
#include "render/studio/light.h"

#if CHARACTER_PANEL_SCENE_COPY_EXPERIMENT
#include "character/scene_graph_copy.h"
#endif

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
    
    void mirror_worn_equipment(RE::Actor* clone, RE::Actor* actor)
    {
        // The worn-item walk moved to the shared mirror (equipment_mirror.h): the puppet dresses from
        // the same rule, and the two routes must not keep two answers to "what is the player wearing".
        if (!clone || !actor)
            return;
        const std::uint32_t added = equipment_equip_all_worn(*clone, *actor);
        if (added == 0)
            logger::warn("Clone dressing found no worn body item to mirror (equip={})", static_cast<void*>(RE::ActorEquipManager::GetSingleton()));
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

    // The bind-bone census (struct + walk) lives in figure_audit.h: the puppet's assembly gate asks
    // the same three-layer question - render activity, nif capacity, binding state - and the AFP9
    // crash proved a second copy of a palette walk is a second chance to get the memory rule wrong.

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
    // The rule itself lives in figure_audit.h (figure_is_collision_helper) - the puppet's framing
    // asks the same question, and two token lists would drift.
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

    // The per-pass submission this file used to own now lives in render/studio/figure_passes.cpp,
    // shared with the same-Actor route; what stayed here is only what is specific to a detached graph.
}
CharacterClone::CharacterClone(RE::Actor* actor) : m_clone(nullptr), m_clone_state(CloneState::e_none), m_wait_frames(0), m_dressed(false), m_bind_map_logged(false), m_bind_map_stall(0), m_is_snapshot(false), m_draw_frames(0)
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
    clone_base->sleepOutfit = nullptr;
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
    m_source = actor;
    m_clone_state.store(CloneState::e_generated, std::memory_order_release);
    m_wait_frames = 0;
    logger::info("Clone has been created at the player, from {} ({:08x})", actor->GetDisplayFullName(), actor->GetFormID());
}

CharacterClone::CharacterClone(RE::NiPointer<RE::NiAVObject> graph) :
    m_clone(nullptr), m_clone_state(CloneState::e_ready), m_graph_object(std::move(graph)),
    m_wait_frames(0), m_dressed(false), m_bind_map_logged(false), m_bind_map_stall(0),
    m_is_snapshot(true), m_draw_frames(0)
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

CharacterClone::~CharacterClone()
{
#if CHARACTER_PANEL_SCENE_COPY_EXPERIMENT
    // Native snapshot teardown remains unsafe. SceneGraphCopy parks the owning wrapper
    // without destroying it; this branch alone does not prevent automatic NiPointer destruction.
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

    const FigureAssembly stats = graph ? figure_measure_assembly(*graph) : FigureAssembly{};

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
        mirror_worn_equipment(clone, m_source);
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

    RE::NiUpdateData update_data{
        0.0f,
        RE::NiUpdateData::Flag::kDirty
    };

    if (m_is_snapshot)
    {
        RE::NiTransform placement;
        placement.translate = anchor;
        placement.rotate.SetEulerAnglesXYZ(0.0f, 0.0f, 0.0f);
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
        if (m_recentre_pending)
        {
            // The idle now drives the joints, so the body's measured bound is the pose the panel
            // will actually show — re-center once against the anchor (oar-idle2 problem 1: a
            // figure captured mid-jump kept the jump's crouched center and sat too low).
            // UpdateDownwardPass cascades the whole tree correctly; writing node worlds by hand
            // here (a first version did) shattered every non-animated descendant — skin matrices
            // collapsed and the figure shrank into a white silhouette (oar-idle3 hotfix).
            m_graph_object->UpdateDownwardPass(update_data, 0);
            m_graph_object->UpdateWorldBound();
            const SkinnedBound posed = measure_skinned_bound(m_graph_object.get());
            if (posed.valid)
            {
                m_graph_object->local.translate.x += anchor.x - posed.center.x;
                m_graph_object->local.translate.y += anchor.y - posed.center.y;
                m_graph_object->local.translate.z += anchor.z - posed.center.z;
                m_graph_object->UpdateDownwardPass(update_data, 0);
                m_graph_object->UpdateWorldBound();
                logger::info("SCOPY FRAME recentre offset=({:.1f},{:.1f},{:.1f}) radius={:.1f}",
                    anchor.x - posed.center.x, anchor.y - posed.center.y, anchor.z - posed.center.z, posed.radius);
            }
            m_recentre_pending = false;
        }
        return;
    }

    m_graph_object->local.translate = anchor;
    m_graph_object->local.rotate.SetEulerAnglesXYZ(0.0f, 0.0f, Studio_Facing_Z_Rad);
    m_graph_object->local.scale = 1.0f;

    reset_root_to_bind_pose(m_graph_object.get(), m_bind_map_logged, m_bind_map_stall);

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

    RE::BSGraphics::Renderer* renderer = RE::BSGraphics::Renderer::GetSingleton();
    const RE::BSGraphics::RendererData& runtime = renderer->GetRuntimeData();
    if (!runtime.context || !runtime.forwarder)
        return false;

    pose();

#if CHARACTER_PANEL_SCENE_COPY_EXPERIMENT
    // Restore placement before applying animation; the draw must retain the resulting worlds.
    SceneGraphCopy::instance().animate_at_base_pose(*this);
#endif

    // Pull the detached light rig to the anchor and cascade it: the shader
    // reads NiLight::world.translate (v6.37) and this window is the only
    // place the cascade runs. Must precede the pass generation.
    StudioLight::instance().place(m_anchor);

    // The batch itself is shared with the same-Actor route (render/studio/figure_passes.h); what
    // stays here is what only this route knows: the graph it detached, its anchor and placement.
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

    // One-shot per clone: read back what the context held after the batch. When
    // the shadow state says DIRTY_RENDERTARGET, SetupAndDrawPass re-applies the
    // engine's ledger target mid-batch (v6.45), which empties the studio target
    // without any log line of its own — this decides "rerouted" from
    // "drew-nothing" in one look.
    if (stats.render_target_checked)
    {
        m_om_check_logged = true;
        logger::info("Clone draw OM check: bound={} studio={} match={}",
            stats.render_target_bound, static_cast<const void*>(render_target.rtv()), stats.render_target_matched);
    }

    // Per-frame summary plus the skin census: after the detach, the engine's
    // SetupGeometry writes numMatrices/boneMatrices every drawn frame — the
    // census watching `bound` climb (0 → ~74) is the binding-has-run proof.
    if (!m_is_snapshot || ++m_draw_frames % 120 == 1)
    {
        const FigureAssembly census = p_root ? figure_measure_assembly(*p_root) : FigureAssembly{};
        logger::info(
            "Clone draw: passes={} rd-null={} lights={} | geoms={} skinned={} roots={} num-mat={} bone-count={} bound={}",
            stats.drawn, stats.renderer_data_null, stats.inject_lights ? Studio_Light_Count : 0,
            census.geometries, census.skinned, census.roots, census.num_matrices, census.bone_count, census.bound);
        if (m_is_snapshot)
        {
            // `animation_driver` lives in the scene-copy class, which only exists in that
            // configuration: naming it unconditionally is what broke the default (Actor-route) build.
#if CHARACTER_PANEL_SCENE_COPY_EXPERIMENT
            logger::info("SCOPY DRAW frame={} passes={} lights={} animation-driver={} physics-driver=none",
                m_draw_frames, stats.drawn, stats.inject_lights, SceneGraphCopy::instance().animation_driver());
#else
            logger::info("Clone draw frame={} passes={} lights={} physics-driver=none",
                m_draw_frames, stats.drawn, stats.inject_lights);
#endif
        }
    }

    if (stats.drawn == 0)
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
