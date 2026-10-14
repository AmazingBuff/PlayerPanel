//
// Created by AmazingBuff on 2026/10/13.
//

#include "character/puppet_draw.h"

#include "character/final_pose_apply.h"
#include "character/final_pose_probe.h"
#include "character/figure_audit.h"
#include "character/puppet_driver.h"
#include "character/snapshot_transform.h"
#include "panel/panel.h"

#include "render/evidence.h"
#include "render/studio/figure_passes.h"
#include "render/studio/light.h"

#include "RE/A/Actor.h"
#include "RE/B/BSGeometry.h"
#include "RE/B/BShkbAnimationGraph.h"
#include "RE/B/BSVisit.h"
#include "RE/N/NiAVObject.h"
#include "RE/N/NiCamera.h"
#include "RE/N/NiNode.h"
#include "RE/U/UI3DSceneManager.h"

#include <cmath>

// winnt.h defines macros of these names, which would rewrite the REX constants used below as
// `REX::W32::0x1000`; every Windows header this file needs is already parsed at this point.
#undef MEM_COMMIT
#undef PAGE_NOACCESS

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // The studio constants the copy route measured (character_clone.cpp); R01 adopts them unchanged
    // so the first comparison against the copy's framing has one variable - the root - not four.
    constexpr float Studio_Figure_Scale = 0.35f;
    constexpr float Studio_Depth = 485.0f;
    constexpr float Studio_Facing_Z_Rad = 0.0f;

    // The pose provenance the A4/A5 pair samples. Skeleton order, names as the tree carries them;
    // missing is reported, never skipped silently.
    constexpr std::string_view A4_Bones[] = {
        "NPC Head [Head]",
        "NPC Spine1 [Spn1]",
        "NPC Spine2 [Spn2]",
        "NPC R Hand [RHand]",
        "NPC L Hand [LHand]",
    };

    // Per-frame summary noise gate: a working draw repeats itself, so the full A4/A5 and restore
    // reconciliation print on this cadence - and always when something is off (zero passes, a
    // restore mismatch), because that is exactly when a reader is looking.
    constexpr std::uint32_t Log_Period_Frames = 120;
    constexpr float Restore_Mismatch = 0.001f;

    bool readable(void const* address, std::size_t bytes)
    {
        if (!address || bytes == 0)
            return false;
        REX::W32::MEMORY_BASIC_INFORMATION info{};
        if (REX::W32::VirtualQuery(address, &info, sizeof(info)) == 0)
            return false;
        if (info.state != REX::W32::MEM_COMMIT || info.protect == REX::W32::PAGE_NOACCESS)
            return false;
        const auto* const begin = static_cast<const std::uint8_t*>(address);
        const auto* const end = static_cast<const std::uint8_t*>(info.baseAddress) + info.regionSize;
        return begin + bytes <= end;
    }

    struct BoneSample
    {
        bool present = false;
        RE::NiMatrix3 rotate;
        RE::NiPoint3 translate;
    };

    // One A-phase sample of the fixed bone set, resolved in the CURRENT tree (no pointer survives
    // the frame - HKX43). The bridge map is built once per draw by the caller.
    BoneSample sample_bone(std::string_view a_name, std::unordered_map<std::string_view, RE::NiAVObject*> const& a_by_name)
    {
        BoneSample sample;
        const auto found = a_by_name.find(a_name);
        if (found == a_by_name.end() || !found->second || !readable(found->second, sizeof(RE::NiAVObject)))
            return sample;
        sample.present = true;
        sample.rotate = found->second->local.rotate;
        sample.translate = found->second->local.translate;
        return sample;
    }

    void report_phase(char const* a_label, std::uint32_t a_frame, std::vector<BoneSample> const& a_samples)
    {
        std::uint32_t missing = 0;
        std::string detail;
        for (std::size_t i = 0; i < a_samples.size() && i < std::size(A4_Bones); ++i)
        {
            if (!a_samples[i].present)
            {
                ++missing;
                continue;
            }
            const RE::NiMatrix3& r = a_samples[i].rotate;
            detail += fmt::format(" {}='({:.3f},{:.3f},{:.3f})'",
                A4_Bones[i], r.entry[0][0], r.entry[1][1], r.entry[2][2]);
        }
        logger::info("SCOPY R01 {} frame={} present={} missing={}{}",
            a_label, a_frame, a_samples.size() - missing, missing, detail);
    }

    float phase_max_rotation(std::vector<BoneSample> const& a_before, std::vector<BoneSample> const& a_after)
    {
        float worst = 0.0f;
        const std::size_t count = std::min(a_before.size(), a_after.size());
        for (std::size_t i = 0; i < count; ++i)
        {
            if (!a_before[i].present || !a_after[i].present)
                continue;
            worst = std::max(worst, rotation_angle_degrees(a_before[i].rotate, a_after[i].rotate));
        }
        return worst;
    }

    // The engine's skin cache is keyed by a render-frame id: if the matrices were already computed
    // for THIS frame - before the studio placement and the applied pose were written - SetupGeometry
    // keeps them and the skinned body is drawn where those older bone worlds were (the parked
    // position, i.e. invisible), while a rigid attachment like a weapon follows its node and stays
    // visible. That is exactly the intermittent "only the axe" frame (AFP9h: 16-geometry and
    // 48-geometry frames alike, full figure and lone axe alternating within one session).
    //
    // Invalidating the id makes the engine recompute from the bone worlds this frame actually wrote;
    // nothing is written to `boneMatrices` (that cache stays entirely engine-owned, invariant 1).
    void invalidate_skin_cache(RE::NiAVObject& a_root)
    {
        constexpr std::uint32_t Invalid_Frame = 0xFFFFFFFFu;
        RE::BSVisit::TraverseScenegraphGeometries(&a_root, [](RE::BSGeometry* a_geometry)
        {
            const RE::BSGeometry::GEOMETRY_RUNTIME_DATA& data = a_geometry->GetGeometryRuntimeData();
            if (RE::NiSkinInstance* const skin = data.skinInstance.get())
                skin->frameID = Invalid_Frame;
            return RE::BSVisit::BSVisitControl::kContinue;
        });
    }

    // What this draw is actually consuming: how many geometries the tree carries, how many are
    // skinned, and - for the first skinned one - whether the engine's skin state belongs to THIS
    // render frame and where its first bone and bound sit. A body drawn from another frame's matrices
    // is exactly the failure this line makes visible: the rigid axe follows the node transforms, the
    // skinned body does not.
    void report_skin_census(RE::NiAVObject& a_root, std::uint32_t a_frame, std::uint32_t a_passes)
    {
        std::uint32_t geometries = 0;
        std::uint32_t skinned = 0;
        std::uint32_t with_property = 0;
        RE::BSGeometry* first = nullptr;
        RE::BSVisit::TraverseScenegraphGeometries(&a_root, [&](RE::BSGeometry* a_geometry)
        {
            ++geometries;
            const RE::BSGeometry::GEOMETRY_RUNTIME_DATA& data = a_geometry->GetGeometryRuntimeData();
            if (data.shaderProperty)
                ++with_property;
            const RE::NiSkinInstance* const skin = data.skinInstance.get();
            if (skin && skin->rootParent)
            {
                ++skinned;
                if (!first)
                    first = a_geometry;
            }
            return RE::BSVisit::BSVisitControl::kContinue;
        });

        RE::BSGraphics::State* const state = RE::BSGraphics::State::GetSingleton();
        const std::uint32_t render_frame = state ? state->GetFrameCount() : 0;
        if (!first)
        {
            logger::info("SCOPY R01 census frame={} render-frame={} geoms={} skinned={} property={} passes={} | no skinned geometry",
                a_frame, render_frame, geometries, skinned, with_property, a_passes);
            return;
        }

        const RE::NiSkinInstance* const skin = first->GetGeometryRuntimeData().skinInstance.get();
        RE::NiAVObject* const bone0 = skin && skin->bones ? skin->bones[0] : nullptr;
        const bool bone0_ok = bone0 && readable(bone0, sizeof(RE::NiAVObject));
        const RE::NiPoint3 bone0_world = bone0_ok ? bone0->world.translate : RE::NiPoint3{};
        logger::info("SCOPY R01 census frame={} render-frame={} geoms={} skinned={} property={} passes={} | first='{}' skin-frameID={} num-matrices={} alloc={} bone0='{}' bone0-world-t=({:.1f},{:.1f},{:.1f}) bound-c=({:.1f},{:.1f},{:.1f})",
            a_frame, render_frame, geometries, skinned, with_property, a_passes,
            first->name.c_str(), skin ? skin->frameID : 0, skin ? skin->numMatrices : 0, skin ? skin->allocatedSize : 0,
            bone0_ok ? bone0->name.c_str() : "?",
            bone0_world.x, bone0_world.y, bone0_world.z,
            first->worldBound.center.x, first->worldBound.center.y, first->worldBound.center.z);
    }
}

PuppetDraw& PuppetDraw::instance()
{
    static PuppetDraw s_instance;
    return s_instance;
}

void PuppetDraw::toggle()
{
    const bool next = !m_enabled.load(std::memory_order_acquire);
    m_enabled.store(next, std::memory_order_release);
    m_fallback_warned = false;
    logger::info("SCOPY R01 direct-draw {} ({})",
        next ? "on" : "off",
        next ? "the panel draws the puppet's own 3D; A3 physics and FaceGen stay out of scope"
             : "the panel draws the detached copy again");
}

void PuppetDraw::begin_frame()
{
    if (m_framed_root)
        return;   // a previous frame failed to reach end_frame(); it restores its own state below
    if (!m_enabled.load(std::memory_order_acquire) || !PanelMonitor::instance().is_menu_open())
        return;

    RE::Actor* const puppet = PuppetDriver::instance().actor();
    if (!puppet || !readable(puppet, sizeof(RE::Actor)))
        return;

    RE::NiAVObject* const root = puppet->Get3D(false);
    if (!root || !readable(root, sizeof(RE::NiAVObject)))
        return;

    RE::UI3DSceneManager* const ui3d = RE::UI3DSceneManager::GetSingleton();
    if (!ui3d || !readable(ui3d, sizeof(RE::UI3DSceneManager)))
        return;
    RE::NiCamera* const camera = ui3d->camera.get();
    if (!camera || !readable(camera, sizeof(RE::NiCamera)))
        return;
    const auto& w2c = camera->GetRuntimeData().worldToCam;
    const RE::NiPoint3 anchor{ w2c[2][0] * Studio_Depth, w2c[2][1] * Studio_Depth, w2c[2][2] * Studio_Depth };

    // The framing measures from data THIS frame owns. `figure_skinned_bound` reads geometry
    // worldBounds, which are a cache the engine refreshes on its own schedule - measured still
    // carrying the pre-park world position while the bones had already moved to the anchor, which
    // throws the figure thousands of units along the stale axis (AFP9g: small, dark, off-centre).
    // The cached bound is still logged beside it, because the difference is the finding.
    root->UpdateWorldBound();
    const FigureBound cached = figure_skinned_bound(*root);
    FigureBound bound = figure_skeleton_bound(*root);
    // The palette is the live source and is used whenever it has anything to say, but it is EMPTY
    // until the engine has set a skin up - and the engine does that when the geometry is drawn, which
    // is the very thing this path is trying to do. Measured 2026-10-14: the shell arrived with 44
    // skinned geometries and a zero allocation on every one, so the palette bound was invalid on every
    // F6 frame, `begin_frame` framed nothing, `draw` fell back with `reason=not-framed` and the panel
    // stayed empty. The tree's own node cloud is the fallback; once a draw has allocated the palette
    // the live bound takes over again by itself.
    char const* source = "palette";
    if (!bound.valid)
    {
        bound = figure_node_cloud_bound(*root);
        source = "nodes";
    }
    if (!bound.valid)
        return;

    static std::uint32_t s_frames = 0;
    const bool verbose = ++s_frames % Log_Period_Frames == 1;

    m_framed_root = root;
    m_saved_local = root->local;
    m_saved_world = root->world;
    m_saved_previous = root->previousWorld;
    m_saved_bound = root->worldBound;

    // The placement is written as the root's LOCAL, and the derived worlds are forced for this frame.
    // This actor's subtree is re-derived from `local` every frame, so a world-only write is wiped at
    // the next derivation - while `local` alone would only take effect in a later frame, i.e. after
    // the skin state this frame's draw consumes has been computed. Both are written here, and the
    // whole thing runs before the puppet's graph update (see the caller) for that reason.
    const RE::NiTransform delta = figure_world_delta(m_saved_world, bound.center, anchor, Studio_Figure_Scale, Studio_Facing_Z_Rad);
    m_applied_world = delta * m_saved_world;
    m_applied_local = m_saved_local;
    if (root->parent && readable(root->parent, sizeof(RE::NiAVObject)))
        m_applied_local = root->parent->world.Invert() * m_applied_world;
    else
        m_applied_local = m_applied_world;
    root->local = m_applied_local;
    root->world = m_applied_world;
    root->previousWorld = m_applied_world;
    recompute_subtree_worlds(*root, 0);

    // The framing assertion - and the line whose absence let AFP9c ship a figure drawn outside the
    // frustum: the delta must put the measured bound center exactly on the anchor.
    const RE::NiPoint3 framed_center = delta * bound.center;
    const float center_d = (framed_center - anchor).Length();
    if (verbose || center_d > 0.5f)
    {
        logger::info("SCOPY R01 frame frame={} bound-from={} center-d={:.4f} world-t-before=({:.1f},{:.1f},{:.1f}) world-t-after=({:.1f},{:.1f},{:.1f}) anchor=({:.1f},{:.1f},{:.1f}) | bound c=({:.1f},{:.1f},{:.1f}) r={:.1f} cached-bound c=({:.1f},{:.1f},{:.1f}) r={:.1f}",
            s_frames, source, center_d,
            m_saved_world.translate.x, m_saved_world.translate.y, m_saved_world.translate.z,
            root->world.translate.x, root->world.translate.y, root->world.translate.z,
            anchor.x, anchor.y, anchor.z,
            bound.center.x, bound.center.y, bound.center.z, bound.radius,
            cached.valid ? cached.center.x : 0.0f, cached.valid ? cached.center.y : 0.0f,
            cached.valid ? cached.center.z : 0.0f, cached.radius);
    }
}

void PuppetDraw::end_frame()
{
    RE::NiAVObject* const root = m_framed_root;
    m_framed_root = nullptr;
    if (!root || !readable(root, sizeof(RE::NiAVObject)))
        return;

    root->local = m_saved_local;
    root->world = m_saved_world;
    root->previousWorld = m_saved_previous;
    recompute_subtree_worlds(*root, 0);
    root->worldBound = m_saved_bound;

    const RE::NiPoint3 world_delta = root->world.translate - m_saved_world.translate;
    const float world_rotation = rotation_angle_degrees(m_saved_world.rotate, root->world.rotate);
    const bool restored = world_delta.Length() <= Restore_Mismatch && world_rotation <= Restore_Mismatch &&
        std::abs(root->world.scale - m_saved_world.scale) <= Restore_Mismatch;
    static std::uint32_t s_restores = 0;
    if (!restored || ++s_restores % Log_Period_Frames == 1)
    {
        logger::info("SCOPY R01 restore frame={} world-d=({:.6f},{:.6f},{:.6f}) world-rot={:.6f}deg scale={:.6f}/{:.6f} ok={}",
            s_restores, world_delta.x, world_delta.y, world_delta.z, world_rotation,
            root->world.scale, m_saved_world.scale, restored ? 1 : 0);
    }
}

bool PuppetDraw::draw(RE::UI3DSceneManager const& a_ui3d, CommonStates const& a_states, RenderTarget& a_target)
{
    static std::uint32_t s_frames = 0;
    const bool verbose = ++s_frames % Log_Period_Frames == 1;

    // The toggle's off state is normal operation, not a fault: it returns quietly. Everything else
    // is a real refusal and is reported once per enablement.
    const auto fallback = [this](char const* a_reason, bool a_warn)
    {
        if (a_warn && !m_fallback_warned)
        {
            m_fallback_warned = true;
            logger::warn("SCOPY R01 fallback to the copy reason={}", a_reason);
        }
        return false;
    };

    if (!m_enabled.load(std::memory_order_acquire))
        return fallback("disabled", false);

    // Without this frame's placement there is nothing to draw here: begin_frame() refuses for the
    // same reasons this path would, and the copy below serves.
    if (!m_framed_root)
        return fallback("not-framed", true);

    RE::Actor* const puppet = PuppetDriver::instance().actor();
    if (!puppet || !readable(puppet, sizeof(RE::Actor)))
        return fallback("no-puppet", true);

    RE::BShkbAnimationGraph* const graph = find_third_person_graph(*puppet);
    RE::NiAVObject* const root = puppet->Get3D(false);
    if (!graph || !root || !readable(graph, sizeof(RE::BShkbAnimationGraph)) || !readable(root, sizeof(RE::NiAVObject)))
        return fallback("no-3d-or-graph", true);

    RE::BSGraphics::Renderer* renderer = RE::BSGraphics::Renderer::GetSingleton();
    if (!renderer)
        return fallback("no-renderer", true);
    const RE::BSGraphics::RendererData& runtime = renderer->GetRuntimeData();
    if (!runtime.context || !runtime.forwarder)
        return fallback("no-context", true);

    // R01 identity line: what is about to be drawn, and whether it is the tree the application
    // stage writes. Re-proven whenever the root or the graph changes (reload, re-assembly).
    if (root != m_logged_root || graph != m_logged_graph)
    {
        const FinalPoseApply& apply = FinalPoseApply::instance();
        logger::info("SCOPY R01 identity actor={:08x} root=0x{:x} graph=0x{:x} generation={} mapping={} verified={}",
            puppet->GetFormID(),
            reinterpret_cast<std::uintptr_t>(root),
            reinterpret_cast<std::uintptr_t>(graph),
            apply.generation(), apply.mapping(), apply.verified() ? 1 : 0);
        m_logged_root = root;
        m_logged_graph = graph;
    }

    static bool s_a3_logged = false;
    if (!s_a3_logged)
    {
        s_a3_logged = true;
        logger::info("SCOPY R01 A3 physics=skipped reason=first-round-physics-off");
    }

    // The bridge once per draw: A4 and A5 resolve the same names in the same tree.
    std::unordered_map<std::string_view, RE::NiAVObject*> by_name = nodes_by_name(*root);

    RE::NiCamera* const camera = a_ui3d.camera.get();
    if (!camera || !readable(camera, sizeof(RE::NiCamera)))
        return fallback("no-ui3d-camera", true);
    const auto& w2c = camera->GetRuntimeData().worldToCam;
    const RE::NiPoint3 anchor{ w2c[2][0] * Studio_Depth, w2c[2][1] * Studio_Depth, w2c[2][2] * Studio_Depth };

    // The placement itself was written by begin_frame(), before this frame's graph update: the skin
    // state this draw consumes is computed during that update, so a transform applied here would
    // reach rigid attachments only (AFP9e: the axe followed, the skinned body did not).
    std::vector<BoneSample> before_pass;
    before_pass.reserve(std::size(A4_Bones));
    for (const std::string_view name : A4_Bones)
        before_pass.push_back(sample_bone(name, by_name));

    // A4: the pose the application stage wrote, still on the nodes right before the passes go out.
    if (verbose)
        report_phase("A4", s_frames, before_pass);

    invalidate_skin_cache(*root);

    StudioLight::instance().place(anchor);

    const FigurePassRequest request{
        .root = root,
        .accumulator = a_ui3d.unk10.get(),
        .context = runtime.context,
        .target = &a_target,
        .states = &a_states,
        .light_slots = StudioLight::instance().lights(),
        .light_count = Studio_Light_Count,
        .check_render_target = verbose,
    };
    const FigurePassStats stats = draw_figure_passes(request);

    // Every draw-window exit parks the rig out of gameplay space (the ledger shells would otherwise
    // light the world from the anchor), the same contract the copy route keeps.
    StudioLight::instance().park();

    std::vector<BoneSample> after_draw;
    after_draw.reserve(std::size(A4_Bones));
    for (const std::string_view name : A4_Bones)
        after_draw.push_back(sample_bone(name, by_name));

    const float a5_rotation = phase_max_rotation(before_pass, after_draw);
    if (verbose)
    {
        report_phase("A5", s_frames, after_draw);
        logger::info("SCOPY R01 A5vsA4 frame={} max-rot={:.4f}deg passes={} rd-null={} lights={} om-match={}",
            s_frames, a5_rotation, stats.drawn, stats.renderer_data_null, stats.inject_lights ? 1 : 0,
            stats.render_target_checked ? (stats.render_target_matched ? 1 : 0) : -1);
        if (stats.render_target_checked)
            logger::info("SCOPY R01 OM bound=0x{:x} studio=0x{:x}",
                reinterpret_cast<std::uintptr_t>(stats.render_target_bound),
                reinterpret_cast<std::uintptr_t>(a_target.rtv()));
        report_skin_census(*root, s_frames, stats.drawn);
    }

    // The image itself: the first drawn frame and every 300 after it (capped), so a round can be read
    // off the TGA instead of a frame capture - the judgement that has cost the most round trips here.
    if (m_dumps < Max_Dumps && --m_dump_countdown <= 0)
    {
        ++m_dumps;
        m_dump_countdown = Dump_Period_Frames;
        dump_studio_target(a_target, "r01-panel");
    }

    if (stats.drawn == 0)
        return fallback("no-passes", true);

    return true;
}

PLUGIN_NAMESPACE_END
