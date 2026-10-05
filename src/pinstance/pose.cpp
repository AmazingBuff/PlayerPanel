//
// The studio pose: fixed view-axis anchor, T-pose re-derivation from the
// skin data, forced cascade, and skinned-bound centering. (2026-10-05 src migration: split out of tools/m0_proto, behavior byte-identical)
//

#include "pinstance/pinstance.h"

#include <cmath>
#include <unordered_map>

PLUGIN_NAMESPACE_BEGIN

namespace
{
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

    // Run 36: how far along the UI3D camera's view direction P stands
    // from the camera position (game units). The item preview lives in
    // the same space; round 1 calibrates so the whole body fits.
    constexpr float Studio_Standoff = 40.0f;

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
PLUGIN_NAMESPACE_END
