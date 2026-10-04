// Stage-2 work package: independent display instance P. See the header and
// docs/stage2-p-instance-plan.md. Route 3 (run-31 correction) after two
// falsifications:
//
// - Route 1 (v5): generic engine deep copy of the player graph returns no
//   usable node (F5).
// - Route 2 (v5.1-v5.3): clone actor built and dressed correctly, but
//   moving the graph anywhere never produced passes — the menu culler
//   ignores foreign graphs attached under menuObjects (runs 29/30: attach
//   succeeded, zero p passes; the menu scene collects geometry through the
//   culler's private queue, capture-report addendum 2), and a detached
//   graph belongs to no culler at all.
// - Route 3 (this): P stays a LIVE WORLD ACTOR. The world culler is the
//   one chain that demonstrably emits its passes; those passes reach the
//   same three RenderPassImmediately call sites, where the v5.3 whitelist
//   (root ancestry + panel-open gate) picks them out of the world stream
//   and replays them into the studio target. The actor is parked ~8k units
//   below the player — inside the frustum far plane (20480) so culling
//   keeps collecting it, far below any gameplay pitch so no camera sees
//   it. The world-stream passthrough still draws P where the world would
//   show it (nowhere a camera points); hiding the world-side pixels is
//   follow-up work, scoped by PRD 0.5 to the menu-frame contract.
// - PACING (run-28 finding): SKSE's task interface drains queued tasks
//   within the SAME game frame, so the state machine is paced by the
//   RENDER thread — every DrawInterfaceStart queues at most ONE game-thread
//   step, making "frames" real rendered frames.
// - Lifecycle (F8): killing the clone actor is deferred past the grace
//   window when a despawn races it; the build times out on real-frame
//   counts. No scene-graph surgery happens on this route, so the run-29/30
//   retire-list machinery is gone.

#include "proto_pinstance.h"
#include "proto.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace CharacterPanelProto
{
    namespace
    {
        namespace logger = SKSE::log;

        // Spike-verified grace: the engine finishes the clone's secondary
        // bases and AI process asynchronously; virtuals before that crash
        // (call [rax+0x38] evidence). Same 60-frame window as the spike.
        constexpr std::uint32_t Grace_Frames = 60;
        // After dressing, wait for the biped 3D to appear, then hold the
        // arm for this many more frames (skeleton settle).
        constexpr std::uint32_t Settle_Frames = 15;
        // Run 65 (v6.32): recalibrated for the ASPECT-CORRECT composite
        // window (the panel's aspect 0.51 vs the target's 1.78 — the
        // sampled horizontal slice is (MaxX-MinX)/(MaxY-MinY) = 0.286 of
        // the target width). At the previous 0.70 the T-pose arm span
        // (≈ the body height) overflowed that slice ~2×; 0.35 fits the
        // whole body with correct world proportions (~50% of the panel
        // height — the T-pose's 1:1 silhouette in a 0.51-aspect panel is
        // width-limited; M1 poses with arms down will fill taller).
        constexpr float Studio_Figure_Scale = 0.35f;
        // Run 68 (v6.35): the facing flips BACK to 0. Run 67's back-lighting
        // at Rz(0) was the LIGHT rig's doing (the v6.33 rig positions did
        // not reach the shader); with the rig verified working (run 68:
        // front-lit), the Rz(π) figure presented its back to the player —
        // so Rz(0) is the facing-the-camera orientation.
        constexpr float Studio_Facing_Z_Rad = 0.0f;
        // Hard cap on the whole build in REAL frames; past it the attempt
        // fails and the panel stays on the item preview (sticky until the
        // next open). ~20 s at 60 fps.
        constexpr std::uint32_t Build_Timeout_Frames = 1200;
        // Run 36: how far along the UI3D camera's view direction P stands
        // from the camera position (game units). The item preview lives in
        // the same space; round 1 calibrates so the whole body fits.
        constexpr float Studio_Standoff = 40.0f;
        // v6.24 residue sweep: the clone is a placed reference and SAVES
        // with the game — a save made while it existed reloads it next
        // session as an unmanaged, un-pinned, VISIBLE player duplicate
        // (run 57: the "3BA" actor the user talked to). Cadence of the
        // player-cell sweep on the game-thread pump.
        constexpr std::uint32_t Residue_Scan_Interval_Frames = 300;
        // v6.24: marker base name for clones this plugin places — saved
        // clones match it precisely; legacy saves match the player's name.
        constexpr std::string_view Clone_Base_Name = "CharacterPanel_Clone";
        // v6.25: geometric isolation. Run 58 proved the AI-side levers
        // (activation flag, bool flags) don't hold on this actor — the user
        // still activated and talked to OUR pinned clone. Distance is the
        // lever that cannot fail: interaction/dialogue/collision reach is
        // a few hundred units, so parking P 8000 units below the player
        // (the run-31 route-3 configuration) makes it unreachable while
        // every perceptible channel stays dead. Stage-2b: the park's
        // remaining job is the BUILD WINDOW only (place → grace → dress →
        // relocate, ~1.5 s) — at relocation the shell is deleted, so the
        // world holds no clone afterwards (v6.64; the ghost layer that used
        // to hide the parked double is retired with it). The studio path
        // re-poses the root's LOCAL transform at draw time and never reads
        // the park.
        constexpr float Park_Depth_Below_Player = 8000.0f;
        // Stage-2b: the dedicated home node under menuObjects[0] — the light
        // rig's host, which has kept our foreign subtree alive across every
        // menu open/close since v6.47 (runs 81-93, U2's optimistic evidence).
        constexpr std::string_view Home_Node_Name = "CP_StudioHome";

        // v6.25: re-applied every live frame (the engine's update chain
        // re-derives a live actor's transform, so a one-shot park does not
        // stick — the same reason route 3 re-parked per tick, run 31).
        void park_below_player(RE::Actor* a_clone)
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!a_clone || !player)
                return;
            const RE::NiPoint3 pos = player->GetPosition();
            // Actor's override carries the character-controller update flag
            // (true = warp the controller with the ref).
            a_clone->SetPosition(RE::NiPoint3{ pos.x, pos.y, pos.z - Park_Depth_Below_Player }, true);
        }

        // v6.30: alignment reference = the SKINNED body only. The root's
        // world bound includes attached props (bows/quivers), whose offset
        // dragged the center up/back and left only the legs in frame
        // (run 63). Union the skinned geometries' world bounds instead —
        // that is the visual body the user wants framed.
        struct SkinnedBound
        {
            RE::NiPoint3 center{ 0.0f, 0.0f, 0.0f };
            float radius{ 0.0f };
            bool valid{ false };
        };

        void union_sphere(SkinnedBound& a_out, const RE::NiPoint3& a_center, float a_radius)
        {
            if (!a_out.valid)
            {
                a_out.center = a_center;
                a_out.radius = a_radius;
                a_out.valid = true;
                return;
            }
            const RE::NiPoint3 d = a_center - a_out.center;
            const float dist = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
            if (a_out.radius >= a_radius + dist)
                return;  // fully contained
            if (a_radius >= a_out.radius + dist)
            {
                a_out.center = a_center;
                a_out.radius = a_radius;
                return;
            }
            const float merged = (dist + a_out.radius + a_radius) * 0.5f;
            const float t = dist > 1e-6f ? (merged - a_out.radius) / dist : 0.0f;
            a_out.center = a_out.center + d * t;
            a_out.radius = merged;
        }

        SkinnedBound measure_skinned_bound(RE::NiAVObject* a_root)
        {
            SkinnedBound out;
            RE::BSVisit::TraverseScenegraphGeometries(a_root, [&](RE::BSGeometry* a_geometry) {
                if (!a_geometry->GetGeometryRuntimeData().skinInstance)
                    return RE::BSVisit::BSVisitControl::kContinue;
                const auto& wb = a_geometry->worldBound;
                union_sphere(out, wb.center, wb.radius);
                return RE::BSVisit::BSVisitControl::kContinue;
            });
            return out;
        }

        // v6.23: studio T-pose. Each bone's BIND world in the skin-root
        // (rootParent) space is the inverse of the skin data's skinToBone
        // transform; a top-down walk can set every bone's LOCAL to
        // reproduce the bind worlds (local = parentWorld⁻¹ ∘ bindWorld).
        // Non-bone nodes keep their locals and recompose accordingly. The
        // anim graph may re-drive the bones between frames (invisible in
        // the world since v6.22); the studio re-applies the reset every
        // frame it draws, so the skin matrices (frameID recompute) always
        // read the bind pose.
        //
        // Run 57 corrections: bind worlds are relative to THEIR OWN skin
        // root — a single shared map across several rootParents reset
        // bones in the wrong frame (the log's "74 bind bones over 3 skin
        // root(s)" — the skeleton folded, head at the ground). Group by
        // root; walk each group only in its own frame. The skin root
        // itself is the reference frame: never reset its own local
        // (clobbering it wiped the rig's base transform) and start
        // accumulating at identity from its children.
        struct SkinBindGroup
        {
            RE::NiAVObject* root{ nullptr };
            std::unordered_map<const RE::NiAVObject*, RE::NiTransform> bones;
        };

        void reset_bind_downward(RE::NiAVObject* a_node, const RE::NiTransform& a_parent_world,
            const SkinBindGroup& a_group)
        {
            RE::NiTransform world = a_parent_world * a_node->local;
            if (auto it = a_group.bones.find(a_node); it != a_group.bones.end())
            {
                world = it->second;
                a_node->local = a_parent_world.Invert() * world;
            }
            if (auto* node = a_node->AsNode())
                for (auto& child : node->children)
                    if (child)
                        reset_bind_downward(child.get(), world, a_group);
        }

        void reset_root_to_bind_pose(RE::NiAVObject* a_root)
        {            std::vector<SkinBindGroup> groups;
            RE::BSVisit::TraverseScenegraphGeometries(a_root, [&](RE::BSGeometry* a_geometry) {
                const auto& rd = a_geometry->GetGeometryRuntimeData();
                const RE::NiSkinInstance* skin = rd.skinInstance.get();
                if (!skin || !skin->bones || !skin->skinData || !skin->rootParent)
                    return RE::BSVisit::BSVisitControl::kContinue;
                RE::NiAVObject* root = skin->rootParent;
                auto group = std::find_if(groups.begin(), groups.end(),
                    [root](const SkinBindGroup& g) { return g.root == root; });
                if (group == groups.end())
                {
                    groups.push_back({ root, {} });
                    group = groups.end() - 1;
                }
                const auto* data = skin->skinData.get();
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
                if (auto* node = group.root->AsNode())
                    for (auto& child : node->children)
                        if (child)
                            reset_bind_downward(child.get(), identity, group);
            }
            static bool logged = false;
            if (!logged)
            {
                logged = true;
                std::size_t total = 0;
                for (const auto& group : groups)
                    total += group.bones.size();
                logger::info("Proto P studio T-pose: {} bind bones over {} skin root(s)", total,
                    groups.size());
            }
        }
    }

    void PInstance::pose_for_studio()
    {
        // Run 38: pose the P root DIRECTLY in the studio camera's frame at
        // draw time. Moving the actor (SetPosition every tick) proved
        // unreliable — the engine re-derives the 3D root's world transform
        // from its own update chain, and the run-38 VS dump still showed
        // world gameplay coordinates. Drawing happens after the engine's
        // scene update, so setting the root's LOCAL transform here (its
        // parent is the world cell root, so local == world target) and
        // cascading an Update re-poses the whole skeleton for the skin
        // matrices that SetupGeometry re-reads this same window.
        //
        // Run 39: the camera-derived pose landed the vertices at
        // SV_Position (353, 1087, -20) — near the camera but still outside
        // the clip volume, meaning the camera's world rotate column did
        // not match the actual studio view transform. Calibration now uses
        // the ONLY known-good reference: the highlighted item preview's
        // world transform under menuObjects[1] — the manager poses it in
        // exactly the space the studio camera projects correctly. P stands
        // a body-height in FRONT of the item's position (toward the item's
        // facing camera side), scaled to the item's preview scale so the
        // full body fits the same framing. When no item geometry exists
        // the pose falls back to identity near the origin.
        RE::NiAVObject* root =
            reinterpret_cast<RE::NiAVObject*>(m_active_root.load(std::memory_order_acquire));
        if (!root)
            return;

        // v6.28: FIXED studio anchor. The v6.3-v6.27 anchor was the
        // selected item's world translate — the manager re-poses each item
        // model, so the FIGURE moved whenever the selection changed (run
        // 61). The studio is its own place now: the eye sits at the world
        // origin (run 36: worldToCam translation ~0, re-confirmed by the
        // w2c_t dump field) and the view direction is worldToCam row 3, so
        // the anchor is simply Studio_Depth along the view axis. Its NDC
        // is (0,0), and the composite squeezes the WHOLE studio target
        // into the panel rect — the view-axis point lands exactly at the
        // PANEL CENTER, independent of any item. Depth 485 is the
        // run-55-proven in-bounds distance (the item preview's own depth).
        constexpr float Studio_Depth = 485.0f;
        RE::NiPoint3 anchor{ 0.0f, 0.0f, 0.0f };
        float w2c_translation = 0.0f;
        if (auto* ui3d = RE::UI3DSceneManager::GetSingleton())
        {
            if (auto* cam = ui3d->camera.get())
            {
                const auto& w2c = cam->GetRuntimeData().worldToCam;
                anchor = RE::NiPoint3{ w2c[2][0] * Studio_Depth, w2c[2][1] * Studio_Depth,
                    w2c[2][2] * Studio_Depth };
                w2c_translation = w2c[2][3];
            }
        }
        m_studio_anchor = anchor;
        // The item preview hangs at the anchor with its local +Y pointing
        // at the studio camera (the manager's convention).
        //
        // Run 53 (v6.18): park P AT THE ITEM ANCHOR, not on a derived view
        // axis. Two reasons converged:
        // 1. The v6.7-6.10 near-plane parking relies on the worldToCam
        //    eye-point derivation, whose translation convention has a
        //    KNOWN unresolved drift (run 46: eye flips 485.1 -> -15.0
        //    within a session). The anchor needs no derivation at all —
        //    it is read straight from the scene.
        // 2. Run 40 already proved the anchor pose projects in-bounds
        //    (NDC x/w 0.22 y/w 0.28), and run 52's RenderDoc shot shows
        //    the figure fully formed in the DEPTH buffer while the RT is
        //    near-black — the PS lighting terms collapse because P's
        //    passes carry DUNGEON (world) lights thousands of units from
        //    the menu-space fragments. The menu lights are positioned for
        //    the item preview — parking P exactly at the anchor puts the
        //    fragments where those lights work (the draw side overrides
        //    the pass lights with the menu lights in v6.18).
        RE::NiPoint3 target = anchor;
        root->local.translate = target;
        // Run 59 (v6.26): facing. The old Rz(180°) was calibrated for the
        // GRAPH-driven pose (run 55), whose body orientation came from the
        // animation state; the T-pose skeleton's AUTHORED facing is the
        // opposite — with 180° the figure presented its back (run 58
        // screenshot). Identity faces the studio camera.
        root->local.rotate.SetEulerAnglesXYZ(0.0f, 0.0f, Studio_Facing_Z_Rad);
        // v6.26 framing, measure pass: body radius at scale 1 (the T-pose
        // is deterministic, so this is a stable input for the size solve).
        root->local.scale = 1.0f;
        // v6.23: T-pose. The anim graph's last-driven pose is neither
        // deterministic nor non-spontaneous; the skeleton's bind pose is
        // both. Re-derived from the skin data and re-applied every draw so
        // the skin matrices always read it — and the v6.21 bound centering
        // now measures a fixed, predictable silhouette.
        reset_root_to_bind_pose(root);
        // Run 41: Update(kDirty) alone is NOT enough — the selective-update
        // flags (the clone carries animation controllers) short-circuit the
        // cascade, so the BONE world transforms stayed at the engine's
        // world-space pose: the run-40/41 SV z/w=0.971 is exactly the
        // gameplay distance from the studio camera origin to the dungeon —
        // the skinned vertices were still projected from the OLD bone
        // positions. UpdateDownwardPass forces the FULL transform cascade
        // over every child regardless of flags, which is what the skin
        // matrix re-read (frameID path) consumes.
        RE::NiUpdateData update_data{ 0.0f, RE::NiUpdateData::Flag::kDirty };
        root->UpdateDownwardPass(update_data, 0);
        root->UpdateWorldBound();
        // v6.30: alignment reference = the SKINNED body only. The root's
        // world bound includes attached props (bows/quivers), whose offset
        // dragged the center up/back and left only the legs in frame
        // (run 63).
        const SkinnedBound measure_pass = measure_skinned_bound(root);
        const float body_radius = measure_pass.radius;

        // v6.30 framing: the scale is the DIRECTLY calibrated constant
        // Studio_Figure_Scale (0.70 — see its comment); the formula chain
        // (v6.26-v6.29) is retired after two of its three inputs (the
        // camera-node transform, the viewFrustum) proved unreliable.
        const float figure_scale = Studio_Figure_Scale;
        root->local.scale = figure_scale;
        root->UpdateDownwardPass(update_data, 0);
        root->UpdateWorldBound();
        // Run 56: DYNAMIC CENTERING. The run-55 render showed the figure
        // rising from the anchor (feet) — scaled up, the head left the
        // frame. v6.30: the center is the SKINNED body's bound (props
        // excluded — see measure_skinned_bound); shifting the root by
        // (anchor − center) lands the body center exactly on the anchor —
        // the one position whose projection is proven in-frame
        // (runs 40/53/55). Then re-cascade so the skin matrices read the
        // final pose.
        const SkinnedBound centered = measure_skinned_bound(root);
        const RE::NiPoint3 center_shift{ anchor.x - centered.center.x,
            anchor.y - centered.center.y, anchor.z - centered.center.z };
        root->local.translate.x += center_shift.x;
        root->local.translate.y += center_shift.y;
        root->local.translate.z += center_shift.z;
        root->UpdateDownwardPass(update_data, 0);
        root->UpdateWorldBound();
        // Run 55/56: per-open calibration dump — fixed anchor, skinned
        // body bound and the scale, before and after centering. w2c_t is
        // the eye-at-origin guard (run 36/62: expected ~-15; if it drifts
        // the fixed-anchor depth needs revisiting).
        static thread_local std::uint32_t s_pose_logs = 0;
        if (s_pose_logs++ < 6)
        {
            logger::info(
                "Proto v6.30 studio pose: anchor=({:.1f},{:.1f},{:.1f}) depth={:.1f} w2c_t={:.1f} "
                "skinned_body_r={:.1f} figure_scale={:.3f} centered r={:.1f} c=({:.1f},{:.1f},{:.1f})",
                anchor.x, anchor.y, anchor.z, Studio_Depth, w2c_translation, body_radius,
                root->local.scale, centered.radius, centered.center.x, centered.center.y,
                centered.center.z);
        }
    }

    namespace
    {

        // Body-worn mirroring in the order SKSE's EquipItemEx uses (spike
        // verified: AddObjectToContainer first, then ActorEquipManager;
        // AddWornItem without container membership crashed in
        // ExtraDataList::GetEnchantment). Only biped body slots.
        void mirror_worn_equipment(RE::Actor* clone, RE::PlayerCharacter* player)
        {
            RE::InventoryChanges* changes = player->GetInventoryChanges(true);
            RE::ActorEquipManager* equip_manager = RE::ActorEquipManager::GetSingleton();
            if (!changes || !changes->entryList || !equip_manager)
            {
                logger::warn("Proto P dressing unavailable (changes={} equip={})", static_cast<void*>(changes),
                    static_cast<void*>(equip_manager));
                return;
            }

            using BipedSlot = RE::BGSBipedObjectForm::BipedObjectSlot;
            constexpr BipedSlot Body_Slots[] = {
                BipedSlot::kBody,     BipedSlot::kHead,     BipedSlot::kHands,
                BipedSlot::kForearms, BipedSlot::kAmulet,   BipedSlot::kRing,
                BipedSlot::kFeet,     BipedSlot::kCalves,   BipedSlot::kTail,
                BipedSlot::kLongHair, BipedSlot::kCirclet,  BipedSlot::kEars,
                BipedSlot::kModMouth, BipedSlot::kModNeck,  BipedSlot::kModChestPrimary,
                BipedSlot::kModChestSecondary, BipedSlot::kModShoulder, BipedSlot::kModArmLeft,
                BipedSlot::kModArmRight, BipedSlot::kModLegRight, BipedSlot::kModLegLeft,
                BipedSlot::kModFaceJewelry,
            };

            std::uint32_t added = 0;
            for (RE::InventoryEntryData* entry : *changes->entryList)
            {
                if (!entry)
                    continue;
                RE::TESBoundObject* object = entry->object;  // GetObject is macro-clashed by Windows.h
                if (!object || !entry->IsWorn())
                    continue;
                RE::BGSBipedObjectForm* biped = object->As<RE::BGSBipedObjectForm>();
                if (!biped)
                    continue;
                const bool body_worn = std::any_of(std::begin(Body_Slots), std::end(Body_Slots),
                    [biped](BipedSlot slot) { return biped->HasPartOf(slot); });
                if (!body_worn)
                    continue;

                clone->AddObjectToContainer(object, nullptr, 1, nullptr);
                equip_manager->EquipObject(clone, object, nullptr, 1, nullptr,
                    false,  // a_queueEquip: applied in this call
                    true,   // a_forceEquip: the clone has no AI to choose
                    false,  // a_playSounds: silent
                    true);  // a_applyNow: dressed immediately
                ++added;
            }
            logger::info("Proto P body-worn items mirrored: {}", added);
        }
    }

    PInstance& PInstance::instance()
    {
        static PInstance s_instance;
        return s_instance;
    }

    void PInstance::set_world_ready(bool a_ready)
    {
        m_world_ready.store(a_ready, std::memory_order_release);
    }

    void PInstance::pump()
    {
        // Render thread, once per rendered frame: queue ONE game-thread
        // state-machine step. Runs in every non-None state — kAttached
        // included, whose step re-parks P under the player every frame
        // (run 31: a static park point left the frustum when the player
        // turned, culling P and silencing its passes).
        const State state = m_state.load(std::memory_order_acquire);
        if (state == State::kNone && !m_world_ready.load(std::memory_order_acquire))
            return;

        bool expected = false;
        if (m_step_queued.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            SKSE::GetTaskInterface()->AddTask([this]() {
                m_step_queued.store(false, std::memory_order_release);
                // v6.24: residue sweep first — a stale clone from a previous
                // session's save may sit exactly where the new one spawns.
                sweep_stale_clones();
                // v6.34: the MAIN MENU is not a preview context — close the
                // panel the moment it opens. The clone-3D liveness check
                // below fires a frame LATE (the world unload trails the menu
                // load), and run 67 showed exactly one menu frame still
                // compositing the panel. IsMenuOpen is checked while the
                // instance is alive; after the close the pump early-outs
                // (kNone + world not ready) and never queues again until
                // the next session arms it.
                if (m_state.load(std::memory_order_acquire) != State::kNone)
                {
                    auto* ui = RE::UI::GetSingleton();
                    if (ui && ui->IsMenuOpen(RE::MainMenu::MENU_NAME))
                    {
                        PInstance::instance().despawn();
                        CharacterPanelProto::Proto::instance().close_panel("main menu");
                        logger::info("Proto P panel closed: main menu open");
                    }
                }
                // v6.32: quit/menu-transition liveness check. Pre-relocation
                // the graph is engine-owned: a lost 3D means the world
                // unloaded and the whitelisted root dangles — disarm before
                // the render pass (run 65 crash: TraverseScenegraphGeometries
                // on the stale root). Stage-2b: once the graph is relocated
                // it is owned by this instance (NiPointer + holder slot) and
                // the shell is already DELETED (v6.64) — the clone ref's
                // fate is irrelevant there, so the Get3D/ref probe runs only
                // in the pre-relocation build window, and the menu-home
                // hosting is verified instead (U2). The pump runs on the
                // game thread at DrawInterfaceStart — BEFORE this frame's
                // render work — so any disarm here leads the studio draw by
                // a frame.
                if (m_state.load(std::memory_order_acquire) == State::kAttached)
                {
                    const bool homed =
                        m_home.load(std::memory_order_acquire) == HomeState::kMenuHome;
                    if (!homed)
                    {
                        auto* live = m_clone.get().get() ? m_clone.get()->As<RE::Actor>() : nullptr;
                        if (!live || !live->Get3D(false))
                        {
                            logger::warn("Proto P clone graph lost (world unloaded?); disarming before "
                                         "the render pass");
                            // v6.33: the world is gone — that is ALSO no valid
                            // preview context (FR-06): close the panel itself,
                            // otherwise the main menu keeps compositing the last
                            // studio image (run 65 screenshot). The pump runs at
                            // DrawInterfaceStart entry, so the very next
                            // panel_frame_active() read takes the non-bracketed
                            // path and the release consumes this same frame.
                            PInstance::instance().despawn();
                            CharacterPanelProto::Proto::instance().close_panel("world unloaded");
                        }
                    }
                    else
                    {
                        verify_home();
                    }
                }
                // Run 49 (v6.15): with no P alive, every idle frame retries
                // the auto-spawn ONCE the world is unpaused and actually
                // rendering a player — the clone's geometries only get their
                // device buffers (rendererData) when the world renderer
                // processes them, which never happens paused in a menu.
                if (m_state.load(std::memory_order_acquire) == State::kNone)
                {
                    auto* ui = RE::UI::GetSingleton();
                    auto* player = RE::PlayerCharacter::GetSingleton();
                    if (ui && !ui->GameIsPaused() && player && player->Get3D(false))
                    {
                        spawn();
                        return;
                    }
                }
                tick();
            });
        }
    }

    void PInstance::spawn()
    {
        if (m_state.load(std::memory_order_acquire) != State::kNone)
            return;

        RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->GetActorBase() || !player->GetSequencer())
        {
            logger::warn("Proto P spawn ignored: player or AI process not ready");
            return;
        }
        RE::TESNPC* player_base = player->GetActorBase();
        RE::TESForm* duplicate = player_base->CreateDuplicateForm(false, nullptr);
        auto* clone_base = duplicate ? duplicate->As<RE::TESNPC>() : nullptr;
        if (!clone_base)
        {
            logger::warn("Proto P spawn failed: CreateDuplicateForm (plan F5 successor)");
            return;
        }
        clone_base->faceNPC = player_base;
        // v6.24: marker name — the base is runtime-created and dies with
        // the session unless saved; a saved clone carries this name into
        // the next session, where the residue sweep matches it precisely
        // (legacy saves without the marker fall back to the player's name).
        clone_base->fullName = RE::BSFixedString(Clone_Base_Name);

        // v6.70 (run 104): born-at-depth is FALSIFIED — CreateReferenceAt
        // Location at the park depth assembled a DEGRADED graph (the engine
        // picks character LOD by reference distance at load time): skin
        // instances with zero bind matrices ("0 bind bones" vs the 74 of
        // the v6.23 era), parts rendered at raw model space, mirrored items
        // invisible, per-cycle nondeterminism. The pre-2b evidence (place
        // at the player, then park deep) stays authoritative: the reference
        // must sit near the player while the graph assembles; parking deep
        // AFTER assembly never downgrades it. The v6.68 flash is therefore
        // fixed at the VISIBILITY layer, not the placement layer: the
        // grace node park (v6.38) plus the grace fade guard (v6.69, below)
        // make every ordering-losing frame render fully transparent, and a
        // transparent, displaced graph gives the activation raycast nothing
        // to hit.
        RE::NiPointer<RE::TESObjectREFR> placed = player->PlaceObjectAtMe(clone_base, false);
        auto* clone = placed ? placed->As<RE::Actor>() : nullptr;
        if (!clone)
        {
            logger::warn("Proto P spawn failed: PlaceObjectAtMe (plan F5 successor)");
            return;
        }
        m_clone = RE::ObjectRefHandle(clone);
        m_frames_since_place = 0;
        m_dressed_frames = 0;
        m_total_frames = 0;
        // Stage-2b: a fresh instance starts parked again — any previous
        // home went away with its despawn.
        m_home.store(HomeState::kWorldParked, std::memory_order_release);
        m_home_graph = nullptr;
        m_home_node = nullptr;
        m_state.store(State::kWaitingGrace, std::memory_order_release);
        logger::info("Proto P clone placed at the player (PlaceObjectAtMe; high-model assembly), "
                     "grace node park + fade guard hold it invisible");
    }

    void PInstance::tick()
    {
        // Run 38: the actor-level re-park (SetPosition every frame) is
        // RETIRED — the engine's update chain re-derives the 3D root's
        // world transform from its own bookkeeping, and the run-38 VS dump
        // proved the studio draw still saw world gameplay coordinates. The
        // authoritative pose now happens at draw time inside the studio OM
        // window (pose_for_studio), on the root's LOCAL transform. The tick
        // keeps walking only the build/kill state machine.
        //
        // Run 49 (v6.15): the build must NEVER advance while the game is
        // paused — dressed armor meshes only get their device buffers from
        // real unpaused world rendering, so a build that runs inside the
        // inventory produces a whitelist with zero renderer-initialized
        // geometries (the run-49 rd=9 failure). Stall until the world is
        // live again; the build timeout only counts live frames.
        if (auto* ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused())
            return;

        const State state = m_state.load(std::memory_order_acquire);
        switch (state)
        {
            case State::kWaitingGrace:
            case State::kWaiting3D:
            case State::kKillPending:
                break;
            case State::kAttached:
                // v6.25: keep P parked 8k below the player every live frame
                // (run-31 configuration) — unreachable beats behavior flags
                // (run 58: the flags didn't hold). The studio never reads
                // the park. Stage-2b: this branch runs at most once — the
                // FIRST kAttached tick relocates the graph AND deletes the
                // shell (v6.64); the park only matters for the fail-open
                // path (stuck-parked shells keep needing the re-park).
                if (auto* parked = m_clone.get().get() ? m_clone.get()->As<RE::Actor>() : nullptr)
                    park_below_player(parked);
                // Stage-2b: move the graph to its menu-scene home. No init
                // gate — run 95 proved the engine's SetupAndDrawPass
                // (call_site_original) lazy-initializes the device buffers,
                // so there is nothing to wait for.
                try_relocate();
                return;
            default:
                return;  // kNone: nothing to walk
        }

        ++m_total_frames;
        if (m_total_frames > Build_Timeout_Frames)
        {
            logger::warn("Proto P build timeout ({} real frames); killing the clone (plan F8)", m_total_frames);
            m_state.store(State::kNone, std::memory_order_release);
            kill_actor();
            return;
        }

        RE::Actor* clone = m_clone.get().get() ? m_clone.get()->As<RE::Actor>() : nullptr;
        if (!clone)
        {
            // FR-06 invalidation destroyed the placed ref before the build
            // finished; the panel close path has already logged its reason.
            // Run 54: this was a SILENT kNone transition — the prime
            // suspect for run 53's steady-state draws vanishing without a
            // single log line. Make it visible.
            logger::warn("Proto P build aborted: the placed clone ref is gone (state was {})",
                state == State::kWaitingGrace   ? "kWaitingGrace"
                    : state == State::kWaiting3D ? "kWaiting3D"
                                                 : "kKillPending");
            m_state.store(State::kNone, std::memory_order_release);
            return;
        }

        if (state == State::kKillPending)
        {
            // Despawn raced the grace window; now that it is over, virtuals
            // are safe (spike evidence) and the kill can run.
            kill_actor();
            m_state.store(State::kNone, std::memory_order_release);
            return;
        }

        if (state == State::kWaitingGrace)
        {
            ++m_frames_since_place;
            // v6.38 (user report: the clone flashed at the player for a few
            // frames after a save load). During the grace window the actor
            // must not be touched via virtuals (the spike's [rax+0x38]
            // crashes — secondary bases/AI process are still async), but
            // the 3D GRAPH is plain NiAVObject data: as soon as it exists,
            // shifting its root node's LOCAL translate needs no virtual
            // dispatch and is a frame-accurate fix — the clone never shows
            // at the player at all. park_below_player (actor SetPosition)
            // still runs at grace end as the engine-authoritative park; the
            // engine's update chain re-derives a live actor's transform,
            // which is exactly why this node-level shift is re-applied
            // every grace tick.
            // v6.38 (user report: the clone flashed at the player for a few
            // frames after a save load). During the grace window the actor
            // must not be touched via virtuals (the spike's [rax+0x38]
            // crashes — secondary bases/AI process are still async), so
            // Get3D() (virtual Get3D2) is off-limits too. The 3D graph
            // pointer is plain data though: LOADED_REF_DATA::data3D (0x68).
            // As soon as the graph exists, shifting its root's LOCAL
            // translate needs no virtual dispatch and is frame-accurate —
            // the clone never shows at the player at all.
            // park_below_player (actor SetPosition) still runs at grace end
            // as the engine-authoritative park; the engine's update chain
            // re-derives a live actor's transform, which is exactly why
            // this node-level shift is re-applied every grace tick.
            if (auto* loaded = clone->loadedData; loaded && loaded->data3D)
            {
                auto* early = loaded->data3D.get();
                auto* player_refr = RE::PlayerCharacter::GetSingleton();
                auto* player_loaded = player_refr ? player_refr->loadedData : nullptr;
                if (early && player_loaded && player_loaded->data3D)
                {
                    const RE::NiPoint3 down = player_loaded->data3D.get()->world.translate;
                    early->local.translate = RE::NiPoint3{ down.x, down.y,
                        down.z - Park_Depth_Below_Player };
                    RE::NiUpdateData early_update{ 0.0f, RE::NiUpdateData::Flag::kDirty };
                    early->UpdateDownwardPass(early_update, 0);
                }
                // v6.69/70: fade guard — the PRIMARY flash fix. A freshly
                // placed character ramps in through its BSFadeNode, and an
                // ordering-losing frame renders the graph before the shift
                // above lands (the user's transparent, interactive double).
                // Pin the fade trio (target/rate/current) to zero each grace
                // tick: plain floats, and AsFadeNode dispatches on the
                // fully-constructed node (UpdateDownwardPass above is the
                // same safety class). Whatever frames the node park loses,
                // this pin still renders them fully transparent, and the
                // ramp cannot climb while the target sits at zero. Restored
                // at grace end, before the dress adds new geometry.
                if (auto* fade = early->AsFadeNode())
                {
                    auto& fade_rt = fade->GetRuntimeData();
                    fade_rt.unk128 = 0.0f;
                    fade_rt.unk12C = 0.0f;
                    fade_rt.currentFade = 0.0f;
                }
            }
            if (m_frames_since_place < Grace_Frames)
                return;  // the next pump queues the next step
            // v6.69: restore the fade the grace guard pinned to zero —
            // target=1, rate=0, current=1: fully opaque and no further ramp.
            // The world never draws the parked graph visibly, but the studio
            // must see it opaque; dress geometry attaches after this point.
            if (auto* loaded = clone->loadedData; loaded && loaded->data3D)
                if (auto* fade = loaded->data3D.get()->AsFadeNode())
                {
                    auto& fade_rt = fade->GetRuntimeData();
                    fade_rt.unk128 = 1.0f;
                    fade_rt.unk12C = 0.0f;
                    fade_rt.currentFade = 1.0f;
                }
            // Grace over (spike order): dress from the player's worn set and
            // park IN FRONT OF the camera, just outside its typical pitch.
            // Route 3 (run 31 correction): the graph is NEVER detached from
            // the world — the menu culler ignores attached foreign graphs
            // (run 29/30: attach succeeded, zero passes) while the WORLD
            // culler still sees the parked actor and emits its passes into
            // the world stream, which the replay whitelist (v5.3) catches.
            // The park must therefore stay INSIDE the frustum far plane
            // (camera far = 20480) but outside ordinary view directions:
            // ~8k units below the player is culled by nothing, visible to
            // no gameplay camera pitched less than ~60 degrees down, and
            // its geometry cost is one character.
            clone->GetActorRuntimeData().boolFlags.set(RE::Actor::BOOL_FLAGS::kMovementBlocked);
            // v6.23 behavior pinning (plan §0af), v6.24 correction: the
            // clone must be behavior-inert in the world — no interaction or
            // dialogue (the E prompt), no hostile acts. SetCollision is
            // RETIRED: TESObjectREFR::SetCollision only edits the record
            // flags (load-time), zero runtime effect — run 57 proved it.
            // Runtime collision/ghost handling is re-evaluated after the
            // residue sweep (§0ag): the actor the user met was most likely
            // a stale clone loaded from a previous session's save, not this
            // pinned one — the sweep log names every deletion.
            clone->GetActorRuntimeData().boolFlags.set(RE::Actor::BOOL_FLAGS::kAttackingDisabled);
            clone->GetActorRuntimeData().boolFlags.set(RE::Actor::BOOL_FLAGS::kCastingDisabled);
            clone->SetActivationBlocked(true);
            clone->StopCombat();
            // v6.25: the primary imperceptibility lever is GEOMETRIC — out
            // of reach, out of dialogue, out of collision (see
            // Park_Depth_Below_Player). The flags above stay as secondary
            // cover; run 58 proved they don't hold on their own.
            park_below_player(clone);
            logger::info("Proto P pinned (ref=0x{:X}): no-move/attack/cast flags, activation blocked, "
                         "parked 8k below the player",
                clone->GetFormID());
            mirror_worn_equipment(clone, RE::PlayerCharacter::GetSingleton());
            m_dressed_frames = 0;
            m_state.store(State::kWaiting3D, std::memory_order_release);
            logger::info("Proto P clone dressed; waiting for the biped 3D (park follows)");
            return;
        }

        // kWaiting3D: the biped graph must exist after dressing, then settle
        // for a few frames before we promote it to the whitelist.
        ++m_dressed_frames;
        RE::NiAVObject* player3d = clone->Get3D(false);
        if (!player3d || m_dressed_frames < Settle_Frames)
            return;  // the next pump queues the next step

        attach_graph();
    }

    // Run 51 (v6.16): the run-50 session showed 0/N device buffers at arm
    // time despite unpaused world rendering — and the v6.15 attempt to
    // Disable() the clone crashed the next draw (RIP=0: disabling an actor
    // destroys its 3D graph, the whitelisted root dangled). The graph now
    // stays alive and enabled.
    //
    // The renderer-init heartbeat that used to live here was RETIRED after
    // run 94: the rendererData/vertexBuffer census reports 0 even for the
    // player's own graph (run 54's falsification, repeated verbatim) — and
    // run 95 then answered U1 outright: the engine's SetupAndDrawPass
    // lazy-initializes the device buffers, so no init probing is needed at
    // all.

    // Route 3 (run 31): P stays a WORLD actor. The menu culler ignored the
    // attached graph (runs 29/30) and a detached graph has no culler at all;
    // the WORLD culler is the only chain that ever emitted P passes. The
    // "attach" step registers the graph on the whitelist; the pose happens
    // at draw time in the UI3D camera frame (run 38), and the world-side
    // passthrough suppression (v5.3) keeps the double out of the world view.
    void PInstance::attach_graph()
    {
        RE::Actor* clone = m_clone.get().get() ? m_clone.get()->As<RE::Actor>() : nullptr;
        if (!clone)
        {
            logger::warn("Proto P attach lost its subject (clone=null)");
            m_state.store(State::kNone, std::memory_order_release);
            return;
        }
        RE::NiAVObject* graph = clone->Get3D(false);
        if (!graph)
        {
            logger::warn("Proto P attach: biped 3D vanished");
            m_state.store(State::kNone, std::memory_order_release);
            return;
        }

        // Re-entrant safety: drop a stale whitelist mark before promoting
        // the new root.
        m_active_root.store(0, std::memory_order_relaxed);

        // Run 38: no actor-level park at all — the authoritative pose is
        // draw-time (pose_for_studio on the root's local transform); any
        // SetPosition here is overridden by the engine's own update chain.

        m_active_root.store(reinterpret_cast<std::uintptr_t>(graph), std::memory_order_release);
        m_state.store(State::kAttached, std::memory_order_release);
        logger::info("Proto P whitelist armed: root={} (draw-time pose in the UI3D camera frame; no "
                     "dependence on a highlighted item)",
            static_cast<void*>(graph));

        // Run 49/50 (v6.15): renderer-init census at arm time — a zero here
        // is the run-49 rd=9 signature (the manual draw rejects every pass
        // until device buffers exist). Informational only: the manual draw
        // needs no re-arm, the rd gate starts passing the moment the world
        // renderer creates the buffers.
        //
        // Run 51 (v6.16): the v6.15 Disable() here CRASHED the next draw
        // (crash-2026-10-02-20-59-23: pose_for_studio line 169, RIP=0 null
        // vtable — disabling an actor DESTROYS its 3D graph, so the
        // whitelisted root dangled). Stage-2b resolves it structurally: the
        // graph is re-homed under CP_StudioHome with a strong NiPointer and
        // data3D is severed BEFORE the shell is ever disabled (v6.63/64),
        // so there is nothing left for Disable to destroy.
        //
        // Informational reading only: this census is the falsified run-54
        // instrument — it reads 0 even for a fully initialized graph (run
        // 94: P=0/14 player=0/23 while the studio submitted 14/14 every
        // frame). Kept as a reference line, never as a gate.
        std::size_t skinned = 0;
        std::size_t initialized = 0;
        RE::BSVisit::TraverseScenegraphGeometries(graph, [&](RE::BSGeometry* geometry) {
            auto& geom_rt = geometry->GetGeometryRuntimeData();
            if (!geom_rt.skinInstance)
                return RE::BSVisit::BSVisitControl::kContinue;
            ++skinned;
            if (geom_rt.rendererData && geom_rt.rendererData->vertexBuffer)
                ++initialized;
            return RE::BSVisit::BSVisitControl::kContinue;
        });
        logger::info("Proto P renderer init check: {}/{} skinned geometries have device buffers "
                     "(heartbeat keeps probing until full)",
            initialized, skinned);
    }

    // Stage-2b round 1: move the whitelisted graph out of the world into a
    // private home under the menu scene, on the FIRST unpaused kAttached
    // tick — no init gate at all (U1 answered by run 95): the v6.60 census
    // gate was the falsified run-54 instrument (never fired), and the
    // v6.61 ghost-warm gate never armed because world-pass flow is
    // view/culling dependent (run 95: zero world P passes arrived ALL
    // session). Yet the studio drew that same never-world-rendered graph
    // 411 times (submitted=14/14, figure on screen): the engine's
    // SetupAndDrawPass — which is exactly what call_site_original invokes —
    // initializes the device buffers itself. There is nothing to wait for.
    // The shell stays parked and pinned exactly as before, and the graph
    // object address is unchanged, so the whitelist, the discovery logs and
    // the pass recipes all survive untouched.
    void PInstance::try_relocate()
    {
        if (m_home.load(std::memory_order_acquire) != HomeState::kWorldParked)
            return;
        if (m_state.load(std::memory_order_acquire) != State::kAttached)
            return;
        RE::NiAVObject* graph = root();
        if (!graph)
            return;
        relocate_home(graph);
    }

    void PInstance::relocate_home(RE::NiAVObject* a_graph)
    {
        RE::NiNode* old_parent = a_graph->parent;
        auto* ui3d = RE::UI3DSceneManager::GetSingleton();
        RE::NiNode* host = ui3d ? ui3d->menuObjects[0].get() : nullptr;
        if (!old_parent || !host)
        {
            logger::warn("Proto P relocation unavailable (old_parent={} host={}): the graph stays parked",
                static_cast<void*>(old_parent), static_cast<void*>(host));
            m_home.store(HomeState::kStuckParked, std::memory_order_release);
            return;
        }

        // Render-thread latch: the studio draw skips frames inside this
        // window (stage-2b plan §3-6).
        m_relocating.store(true, std::memory_order_release);

        auto* home = RE::NiNode::Create();
        if (!home)
        {
            m_relocating.store(false, std::memory_order_release);
            m_home.store(HomeState::kStuckParked, std::memory_order_release);
            logger::warn("Proto P home node creation failed; the graph stays parked");
            return;
        }
        home->name = Home_Node_Name;

        // v6.63: SEVER the engine's claim on the graph first. Run 96: with
        // the shell still alive, its 3D bookkeeping (fed by the per-tick
        // warp park) re-parented the graph back into the world EVERY frame
        // — 707 verify_home/re-home rounds in one session, a per-frame
        // scene-graph tug-of-war. Nulling data3D (plain data at the same
        // offset the v6.38 grace code already touches, no virtuals) removes
        // the graph from that bookkeeping: the engine holds no reference to
        // fight over, and the shell becomes a normal far-away 3D-less actor
        // (a state the engine maintains natively for unloaded NPCs). It
        // also makes the round-2 shell kill safe by construction — Disable
        // has no 3D left to destroy (the run-51 mechanism).
        if (auto* clone = m_clone.get().get() ? m_clone.get()->As<RE::Actor>() : nullptr)
            if (auto* loaded = clone->loadedData)
                loaded->data3D = nullptr;

        // The surgery is three pointer moves — the graph object and
        // everything inside it (skeleton, skins, properties, device
        // buffers) stays identical:
        // 1. host our holder under the menu scene (the rig's proven host);
        host->AttachChild(home);
        // 2. detach from the world cell — the world culler loses the
        //    subtree right here (engine-native: the same move a 3D unload
        //    runs), then
        old_parent->DetachChild(a_graph);
        // 3. re-home under the holder.
        home->AttachChild(a_graph);
        // Strong ownership: from here the engine holds no parent link to
        // the graph; this NiPointer plus the holder's child slot keep it
        // alive — the run-51 dangling-whitelist crash class is structurally
        // gone, and kill_actor (Disable) can no longer free the graph.
        m_home_graph.reset(a_graph);
        m_home_node.reset(home);

        // Forced cascade: the graph carries selective-update flags that
        // short-circuit plain Update (run 41) — UpdateDownwardPass is what
        // pose_for_studio uses every draw anyway.
        RE::NiUpdateData update_data{ 0.0f, RE::NiUpdateData::Flag::kDirty };
        home->UpdateDownwardPass(update_data, 0);

        m_home.store(HomeState::kMenuHome, std::memory_order_release);
        m_relocating.store(false, std::memory_order_release);

        // U5 probe: the pose math writes the root LOCAL in accumulator
        // space assuming an identity parent chain — the same assumption the
        // run-93-verified light rig makes about this host. If home->world
        // is not identity the figure will shift; plan §5-H5 has the
        // parentWorld⁻¹ fix.
        const auto& hw = home->world;
        logger::info(
            "Proto P home: graph relocated under CP_StudioHome — root=0x{:X} (unchanged) "
            "old_parent=[{}] host=menuObjects[0] home_world=({:.2f},{:.2f},{:.2f}) scale={:.3f}",
            reinterpret_cast<std::uintptr_t>(a_graph),
            old_parent->name.c_str() ? old_parent->name.c_str() : "(null)",
            hw.translate.x, hw.translate.y, hw.translate.z, hw.scale);

        // v6.64 (round 2): the shell dies NOW. data3D was severed above, so
        // Disable has no 3D to destroy (the run-51 mechanism is gone) and
        // the graph survives on our NiPointer + the holder slot. From this
        // tick on the world holds no clone — the v6.22 ghost layer that hid
        // the parked double is retired with it (build-window residual: the
        // parked, node-level-shifted double is world-rendered during the
        // ~1.5 s grace+settle and only reachable to a camera pitched steeply
        // down; FR-06 becomes structural the moment the shell dies).
        kill_actor();
        logger::info("Proto P round 2: shell actor deleted at relocation; the world holds no clone");
    }

    void PInstance::release_home()
    {
        // Called with the whitelist already disarmed (despawn order), so no
        // render-thread traversal can be in flight on this graph — the
        // relocating latch is not needed here.
        if (m_home.exchange(HomeState::kWorldParked, std::memory_order_acq_rel) != HomeState::kMenuHome)
            return;
        RE::NiAVObject* graph = m_home_graph.get();
        RE::NiNode* home = m_home_node.get();
        if (graph && home && graph->parent == home)
            home->DetachChild(graph);  // releases the holder's ref
        m_home_graph = nullptr;        // releases our ref — the last one
        m_home_node = nullptr;         // the holder frees with it
        logger::info("Proto P home released: graph detached from CP_StudioHome and freed");
    }

    // Stage-2b (U2, game thread): the menu scene must keep hosting the
    // relocated graph. loadedModels persistence and the light rig's 13-run
    // tenancy (runs 81-93) say it does; if the engine ever strips the
    // attachment, re-home from our NiPointer — the graph object survives.
    void PInstance::verify_home()
    {
        RE::NiAVObject* graph = m_home_graph.get();
        RE::NiNode* home = m_home_node.get();
        if (!graph || !home)
            return;
        if (graph->parent == home)
            return;
        // v6.63: name where the graph went — the re-parenting fight (run
        // 96) should be gone with the data3D sever; if this fires, the
        // parent identity says who took it (null = stripped, a node = the
        // engine re-attached it somewhere).
        logger::warn(
            "Proto P home parent was detached (parent={} name=[{}]); re-homed under CP_StudioHome",
            static_cast<void*>(graph->parent),
            graph->parent && graph->parent->name.c_str() ? graph->parent->name.c_str() : "(null)");
        home->AttachChild(graph);
        RE::NiUpdateData update_data{ 0.0f, RE::NiUpdateData::Flag::kDirty };
        home->UpdateDownwardPass(update_data, 0);
        logger::warn(
            "Proto P home parent was detached (engine stripped the host slot?); re-homed under "
            "CP_StudioHome");
    }

    // Stage-2b (U2, plan §3-7): the per-open hosting check with its verdict
    // log — called from the panel-open paths on the game thread.
    void PInstance::note_panel_open()
    {
        if (m_state.load(std::memory_order_acquire) != State::kAttached)
            return;
        if (m_home.load(std::memory_order_acquire) != HomeState::kMenuHome)
            return;
        RE::NiAVObject* graph = m_home_graph.get();
        RE::NiNode* home = m_home_node.get();
        if (!graph || !home)
            return;
        if (graph->parent == home)
        {
            logger::info("Proto P home parent ok (per-open check)");
            return;
        }
        verify_home();  // logs the re-home warning
    }

    void PInstance::kill_actor()
    {
        if (RE::TESObjectREFR* clone = m_clone.get().get())
        {
            if (auto* actor = clone->As<RE::Actor>())
            {
                actor->Disable();
                actor->SetDelete(true);
            }
        }
        m_clone = RE::ObjectRefHandle{};
        logger::info("Proto P clone actor killed");
    }

    void PInstance::despawn()
    {
        const State state = m_state.exchange(State::kNone, std::memory_order_acq_rel);
        if (state == State::kNone)
            return;

        // Route 3: the whitelist mark is the only render-side registration;
        // dropping it stops all replays. The graph itself stays owned by the
        // actor until the shell is deleted — no scene-graph surgery, no
        // retire list this route.
        if (const std::uintptr_t root = m_active_root.exchange(0, std::memory_order_acq_rel); root)
            logger::info("Proto P whitelist disarmed (root=0x{:X})", root);

        if (state == State::kAttached || state == State::kWaiting3D)
        {
            // Either long past its grace window or safely parked without a
            // graph to dress: the shell kill is safe now. Stage-2b order:
            // kill FIRST (the engine releases its refs — loadedData->data3D
            // etc.), THEN drop the home; our NiPointer and the holder's
            // child slot kept the graph alive across the kill (refcounts,
            // not raw addresses — the run-51 lesson applied in the safe
            // direction), and the last release frees it here on the game
            // thread with the whitelist already disarmed.
            kill_actor();
            release_home();
        }
        else
        {
            // kWaitingGrace: the state machine must finish the kill once
            // virtuals are safe (F8) — revive it in kKillPending.
            m_state.store(State::kKillPending, std::memory_order_release);
            logger::info("Proto P despawn during grace; kill deferred to the state machine");
        }
    }

    void PInstance::drain_retired() {}

    void PInstance::sweep_stale_clones()
    {
        if (++m_frames_until_residue_scan < Residue_Scan_Interval_Frames)
            return;
        m_frames_until_residue_scan = 0;

        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* cell = player ? player->GetParentCell() : nullptr;
        const RE::TESNPC* player_base = player ? player->GetActorBase() : nullptr;
        // Only touch references while the world is live (same gate as the
        // spawn path) — mid-load cell contents are transient.
        if (!cell || !player_base || !player->Get3D(false))
            return;

        const RE::NiPointer<RE::TESObjectREFR> self = m_clone.get();
        const char* player_name_c = player_base->GetFullName();
        const std::string_view player_name = player_name_c ? player_name_c : "";
        std::uint32_t deleted = 0;
        for (const auto& ref : cell->GetRuntimeData().references)
        {
            auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
            if (!actor || actor->IsPlayerRef())
                continue;
            if (self && actor == self.get())
                continue;  // our live, pinned clone
            // v6.65: refs already deleted/disabled are inert — our own
            // killed shell lingers in the cell's reference list until the
            // engine purges it (SetDelete does not remove it immediately),
            // and re-sweeping it every cadence was pure log noise (run 98:
            // 4 deletions of the same ref 0xFF0063BD). Legacy stale clones
            // load ENABLED, so they are still matched.
            if (actor->IsDisabled() || actor->IsDeleted())
                continue;
            auto* base = actor->GetActorBase();
            if (!base || base->GetFormID() < 0xFF000000)
                continue;  // runtime-created bases only — mod content is untouchable
            const char* name = base->GetFullName();
            const bool marked = name && Clone_Base_Name == name;
            const bool legacy = name && !player_name.empty() && player_name == name;
            if (!marked && !legacy)
                continue;
            actor->Disable();
            actor->SetDelete(true);
            ++deleted;
            logger::warn("Proto P residue sweep: deleted stale clone ref=0x{:X} base=0x{:X} name=[{}]",
                actor->GetFormID(), base->GetFormID(), name ? name : "");
        }
        if (deleted > 0)
            logger::info("Proto P residue sweep: {} stale clone(s) removed from the player cell",
                deleted);
    }

    bool PInstance::is_p_descendant(const RE::BSGeometry* geometry) const
    {
        const std::uintptr_t root = m_active_root.load(std::memory_order_acquire);
        if (!root || !geometry)
            return false;
        const RE::NiAVObject* node = geometry;
        for (std::size_t depth = 0; node && depth < 32; ++depth)
        {
            if (reinterpret_cast<std::uintptr_t>(node) == root)
                return true;
            node = node->parent;
        }
        return false;
    }

    bool PInstance::is_p_geometry(const RE::BSGeometry* geometry) const
    {
        // Run 31: skinned pieces only. Handheld props (Scb quiver, Torch,
        // bows, weapons — run 31 log) are static meshes on bone attach
        // nodes; their draws use the node's world transform, which in the
        // studio replay filled the frame with giant blobs. Skinned
        // geometries (body, armor, hair, face) deform through the skeleton
        // and are exactly the character the panel must show.
        if (!geometry || !geometry->GetGeometryRuntimeData().skinInstance)
            return false;
        return is_p_descendant(geometry);
    }
}
