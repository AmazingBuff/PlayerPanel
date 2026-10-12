//
// Created by AmazingBuff on 2026/10/13.
//

#include "character/final_pose_probe.h"

#include "character/final_pose_apply.h"
#include "character/puppet_driver.h"
#include "character/snapshot_transform.h"

#include "RE/A/Actor.h"
#include "RE/B/BSAnimationGraphManager.h"
#include "RE/B/BSFadeNode.h"
#include "RE/B/BSGeometry.h"
#include "RE/B/BSShaderAccumulator.h"
#include "RE/B/BSShaderProperty.h"
#include "RE/B/BShkbAnimationGraph.h"
#include "RE/B/BSVisit.h"
#include "RE/H/hkaSkeleton.h"
#include "RE/H/hkbBehaviorGraph.h"
#include "RE/H/hkbCharacter.h"
#include "RE/H/hkbCharacterSetup.h"
#include "RE/M/Main.h"
#include "RE/N/NiCamera.h"
#include "RE/P/PlayerCharacter.h"
#include "RE/T/TESObjectREFR.h"
#include "RE/U/UI3DSceneManager.h"

#include <cmath>
#include <cstring>

// winnt.h defines macros of these names, which would rewrite the REX constants used below as
// `REX::W32::0x1000`; every Windows header this file needs is already parsed at this point.
#undef MEM_COMMIT
#undef PAGE_NOACCESS

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // The graph keeps two output buffers so the engine can interpolate between two updates; which of
    // them is the one the current frame's pose came from is one of the questions this probe answers.
    constexpr int Output_Slots = 2;
    // hkbGeneratorOutput::Tracks is a master header followed by one TrackHeader per standard track;
    // the pose is standard track 2 (RE::hkbGeneratorOutput::StandardTracks::kPose).
    constexpr std::size_t Track_Master_Header_Size = 0x10;
    constexpr std::size_t Track_Header_Size = 0x10;
    constexpr std::size_t Max_Track_Count = 64;
    constexpr std::int32_t Max_Pose_Count = 2048;
    // The bar for "this index reproduces that bone", taken from the HKX28 protocol rather than tuned
    // after seeing a result (R00: the tolerances are fixed before the package is handed over).
    constexpr float Match_Rotation_Degrees = 1.0f;
    constexpr float Moved_Rotation_Degrees = 0.01f;
    constexpr float Moved_Position = 0.01f;

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

    // A copy of one pose sample as plain numbers. The shared helper owns the layout and the
    // unaligned-read reasoning; this name is what the sampling code here calls it by.
    PoseSample copy_pose(RE::hkQsTransform const& a_pose)
    {
        return read_pose_unaligned(a_pose);
    }

    float quat_angle_degrees(PoseSample const& a_first, PoseSample const& a_second)
    {
        // q and -q are the same rotation, so the absolute dot is the one that measures the turn.
        const float dot = std::abs(
            a_first.quaternion[0] * a_second.quaternion[0] +
            a_first.quaternion[1] * a_second.quaternion[1] +
            a_first.quaternion[2] * a_second.quaternion[2] +
            a_first.quaternion[3] * a_second.quaternion[3]);
        return 2.0f * std::acos(std::clamp(dot, 0.0f, 1.0f)) * 57.2957795131f;
    }

    float pose_distance(PoseSample const& a_first, PoseSample const& a_second)
    {
        const float dx = a_first.position[0] - a_second.position[0];
        const float dy = a_first.position[1] - a_second.position[1];
        const float dz = a_first.position[2] - a_second.position[2];
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    // Which standard tracks a buffer currently carries, as a compact list. The pose is what this
    // round needs; world-from-model (0) and extracted motion (1) are what the display side will need
    // later for root motion, and they cost one read of headers the caller has already validated.
    std::string live_tracks(RE::hkbGeneratorOutput const& a_output)
    {
        RE::hkbGeneratorOutput::Tracks const* const tracks = a_output.tracks;
        if (!readable(tracks, Track_Master_Header_Size + Track_Header_Size))
            return "-";

        const std::int32_t count = tracks->masterHeader.numTracks;
        if (count <= 0 || static_cast<std::size_t>(count) > Max_Track_Count ||
            !readable(tracks, Track_Master_Header_Size + Track_Header_Size * static_cast<std::size_t>(count)))
            return "-";

        std::string live;
        for (std::int32_t i = 0; i < count; ++i)
        {
            if (tracks->trackHeaders[i].onFraction <= 0.0f)
                continue;
            if (!live.empty())
                live += ",";
            live += fmt::format("{}", i);
        }
        return live.empty() ? "-" : live;
    }

    // The character's third-person graph and the tree's name table moved to the shared helpers: they
    // are declared with the other shared readers in this header, so the application stage selects the
    // same graph and bridges the same names this reads.

    // The camera chain the studio draw actually renders through, read-only. The pass submission
    // (`BSBatchRenderer::SetupAndDrawPass`) takes no camera and writes no projection, so the view the
    // figure is projected with is whatever engine state was left in place when the batch runs - and
    // that has to be known before the display side can be designed at all: the route that must not
    // translate the Actor per frame has to move this camera instead. The accumulator records the
    // camera it was started with (`NiAccumulator::camera`), which is the decisive identity: if it is
    // the UI3D camera, then that camera is the one to adapt.
    void report_camera()
    {
        const RE::UI3DSceneManager* const ui3d = RE::UI3DSceneManager::GetSingleton();
        if (!ui3d || !readable(ui3d, sizeof(RE::UI3DSceneManager)))
        {
            logger::info("SCOPY FP camera UNAVAILABLE reason=no-ui3d-manager");
            return;
        }

        RE::NiCamera* const ui3d_camera = ui3d->camera.get();
        RE::BSShaderAccumulator* const accumulator = ui3d->unk10.get();
        const RE::NiCamera* accumulator_camera = nullptr;
        if (accumulator && readable(accumulator, sizeof(RE::NiAccumulator)))
            accumulator_camera = accumulator->camera;
        RE::NiCamera* const world_camera = RE::Main::WorldRootCamera();

        logger::info("SCOPY FP camera ui3d=0x{:x} ui3d-camera=0x{:x} accumulator=0x{:x} accumulator-camera=0x{:x} world-camera=0x{:x} accumulator-is-ui3d={} accumulator-is-world={} ui3d-is-world={}",
            reinterpret_cast<std::uintptr_t>(ui3d),
            reinterpret_cast<std::uintptr_t>(ui3d_camera),
            reinterpret_cast<std::uintptr_t>(accumulator),
            reinterpret_cast<std::uintptr_t>(accumulator_camera),
            reinterpret_cast<std::uintptr_t>(world_camera),
            accumulator_camera && accumulator_camera == ui3d_camera ? 1 : 0,
            accumulator_camera && accumulator_camera == world_camera ? 1 : 0,
            ui3d_camera && ui3d_camera == world_camera ? 1 : 0);

        if (!ui3d_camera || !readable(ui3d_camera, sizeof(RE::NiCamera)))
        {
            logger::info("SCOPY FP camera ui3d-camera UNAVAILABLE reason=unreadable");
            return;
        }

        const RE::NiCamera::RUNTIME_DATA& flat = ui3d_camera->GetRuntimeData();
        const RE::NiCamera::RUNTIME_DATA2& projection = ui3d_camera->GetRuntimeData2();
        // The view axis is row 2 of worldToCam - the quantity the studio's own anchor is built from.
        const RE::NiPoint3 view{ flat.worldToCam[2][0], flat.worldToCam[2][1], flat.worldToCam[2][2] };

        logger::info("SCOPY FP camera ui3d-camera world-t=({:.1f},{:.1f},{:.1f}) view=({:.4f},{:.4f},{:.4f}) w2c-t=({:.1f},{:.1f},{:.1f}) near={:.2f} far={:.1f} l={:.1f} r={:.1f} t={:.1f} b={:.1f} ortho={} cached-pos=({:.1f},{:.1f},{:.1f}) cached-rot-col2=({:.4f},{:.4f},{:.4f}) ui3d-frustum l={:.1f} r={:.1f} t={:.1f} b={:.1f} near={:.2f} far={:.1f}",
            ui3d_camera->world.translate.x, ui3d_camera->world.translate.y, ui3d_camera->world.translate.z,
            view.x, view.y, view.z,
            flat.worldToCam[3][0], flat.worldToCam[3][1], flat.worldToCam[3][2],
            projection.viewFrustum.fNear, projection.viewFrustum.fFar,
            projection.viewFrustum.fLeft, projection.viewFrustum.fRight,
            projection.viewFrustum.fTop, projection.viewFrustum.fBottom,
            projection.viewFrustum.bOrtho ? 1 : 0,
            ui3d->cachedCameraPos.x, ui3d->cachedCameraPos.y, ui3d->cachedCameraPos.z,
            ui3d->cachedCameraRot.entry[0][2], ui3d->cachedCameraRot.entry[1][2], ui3d->cachedCameraRot.entry[2][2],
            ui3d->viewFrustum.fLeft, ui3d->viewFrustum.fRight, ui3d->viewFrustum.fTop, ui3d->viewFrustum.fBottom,
            ui3d->viewFrustum.fNear, ui3d->viewFrustum.fFar);
    }

    // What the display side needs about the figure itself: where its own root sits in the world, how
    // far along the camera's view axis that is, and how far off the axis - the two numbers a
    // camera-adapting framing would have to drive to zero - plus the fade, because a fade of zero
    // makes the engine skip the node entirely.
    void report_figure(char const* a_label, RE::TESObjectREFR& a_subject)
    {
        RE::NiAVObject* const root = a_subject.Get3D(false);
        if (!root || !readable(root, sizeof(RE::NiAVObject)))
        {
            logger::info("SCOPY FP figure subject={} UNAVAILABLE reason=no-3d", a_label);
            return;
        }

        RE::NiPoint3 view{ 0.0f, 0.0f, 0.0f };
        RE::NiPoint3 eye{ 0.0f, 0.0f, 0.0f };
        if (const RE::UI3DSceneManager* const ui3d = RE::UI3DSceneManager::GetSingleton())
        {
            if (RE::NiCamera* const camera = ui3d->camera.get(); camera && readable(camera, sizeof(RE::NiCamera)))
            {
                const RE::NiCamera::RUNTIME_DATA& flat = camera->GetRuntimeData();
                view = RE::NiPoint3{ flat.worldToCam[2][0], flat.worldToCam[2][1], flat.worldToCam[2][2] };
                eye = camera->world.translate;
            }
        }

        const RE::NiPoint3 position = root->world.translate;
        const float depth = position.Dot(view);
        const float lateral = (position - view * depth).Length();
        const RE::BSFadeNode* const fade = root->AsFadeNode();

        logger::info("SCOPY FP figure subject={} root=0x{:x} world-t=({:.1f},{:.1f},{:.1f}) view-depth={:.1f} lateral={:.1f} eye-distance={:.1f} fade={:.3f} bound-center=({:.1f},{:.1f},{:.1f}) bound-radius={:.1f}",
            a_label,
            reinterpret_cast<std::uintptr_t>(root),
            position.x, position.y, position.z,
            depth, lateral, (position - eye).Length(),
            fade ? fade->currentFade : -1.0f,
            root->worldBound.center.x, root->worldBound.center.y, root->worldBound.center.z,
            root->worldBound.radius);
    }

    // The nodes a skinned draw actually consumes: the palette `NiSkinInstance::bones` points at. R03
    // and R04 need to tell an outlier that blocks from one that does not - a bone a target mesh reads
    // cannot be skipped, while a helper branch or a cloth chain the panel does not draw can - and an
    // unbound palette (a figure the engine has never rendered, runs 49-52's shape) says the
    // classification is unavailable rather than that nothing is necessary.
    struct SkinPalette
    {
        std::unordered_set<RE::NiAVObject const*> nodes;
        std::uint32_t skinned = 0;
        std::uint32_t bound = 0;    // skins whose palette carried at least one node
        std::uint32_t entries = 0;
    };

    SkinPalette skin_palette(RE::NiAVObject& a_root)
    {
        constexpr std::uint32_t Max_Palette_Entries = 1024;

        SkinPalette palette;
        RE::BSVisit::TraverseScenegraphGeometries(&a_root, [&](RE::BSGeometry* a_geometry)
        {
            if (!a_geometry || !readable(a_geometry, sizeof(RE::BSGeometry)))
                return RE::BSVisit::BSVisitControl::kContinue;

            const RE::NiSkinInstance* const skin = a_geometry->GetGeometryRuntimeData().skinInstance.get();
            if (!skin || !skin->rootParent || !readable(skin, sizeof(RE::NiSkinInstance)))
                return RE::BSVisit::BSVisitControl::kContinue;
            ++palette.skinned;

            const std::uint32_t declared = skin->allocatedSize;
            RE::NiAVObject* const* const bones = skin->bones;
            if (!bones || declared == 0 || declared > Max_Palette_Entries ||
                !readable(bones, sizeof(RE::NiAVObject*) * static_cast<std::size_t>(declared)))
                return RE::BSVisit::BSVisitControl::kContinue;

            bool bound = false;
            for (std::uint32_t i = 0; i < declared; ++i)
            {
                RE::NiAVObject* const node = bones[i];
                if (!node)
                    continue;
                bound = true;
                ++palette.entries;
                palette.nodes.insert(node);
            }
            if (bound)
                ++palette.bound;
            return RE::BSVisit::BSVisitControl::kContinue;
        });
        return palette;
    }

    // What the chosen candidate's outliers mean for a draw: how many of them a skinned mesh reads.
    void report_necessity(char const* a_label, RE::TESObjectREFR& a_subject,
        std::vector<RE::NiAVObject*> const& a_outliers)
    {
        if (a_outliers.empty())
            return;

        RE::NiAVObject* const root = a_subject.Get3D(false);
        if (!root || !readable(root, sizeof(RE::NiAVObject)))
        {
            logger::info("SCOPY FP necessary subject={} UNAVAILABLE reason=no-3d", a_label);
            return;
        }

        const SkinPalette palette = skin_palette(*root);
        std::uint32_t necessary = 0;
        std::uint32_t helper = 0;
        for (RE::NiAVObject* const node : a_outliers)
        {
            if (node && palette.nodes.contains(node))
                ++necessary;
            else
                ++helper;
        }

        logger::info("SCOPY FP necessary subject={} outliers={} necessary={} helper={} palettes={}/{} entries={} verdict={}",
            a_label, a_outliers.size(), necessary, helper, palette.bound, palette.skinned, palette.entries,
            palette.bound != 0 ? "classified" : "unbound-palette");
    }

    // R01's other prerequisite, measured before anything is drawn: the Actor's own tree has to carry
    // geometries that can produce passes at all. `rendererData` is the device-side shape the engine
    // creates lazily, and the round that hand-drew a figure the paused world had never rendered found
    // every pass dead on exactly that field (runs 49-52), so it is counted here instead of being
    // discovered at draw time. Read-only - the class check goes through the typed shader property,
    // and nothing generates or submits a pass.
    void report_geometry(char const* a_label, RE::TESObjectREFR& a_subject)
    {
        RE::NiAVObject* const root = a_subject.Get3D(false);
        if (!root || !readable(root, sizeof(RE::NiAVObject)))
        {
            logger::info("SCOPY FP geoms subject={} UNAVAILABLE reason=no-3d", a_label);
            return;
        }

        std::uint32_t geometries = 0;
        std::uint32_t skinned = 0;
        std::uint32_t with_property = 0;
        std::uint32_t without_property = 0;
        std::uint32_t effect_shader = 0;
        std::uint32_t renderer_data = 0;
        std::uint32_t skinned_with_renderer_data = 0;

        RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* a_geometry)
        {
            if (!a_geometry || !readable(a_geometry, sizeof(RE::BSGeometry)))
                return RE::BSVisit::BSVisitControl::kContinue;
            ++geometries;

            const RE::BSGeometry::GEOMETRY_RUNTIME_DATA& data = a_geometry->GetGeometryRuntimeData();
            const RE::NiSkinInstance* const skin = data.skinInstance.get();
            const bool is_skinned = skin && skin->rootParent;
            if (is_skinned)
                ++skinned;

            const RE::BSShaderProperty* const property = data.shaderProperty.get();
            if (property && readable(property, sizeof(RE::BSShaderProperty)))
            {
                ++with_property;
                const RE::NiRTTI* const rtti = property->GetRTTI();
                const char* const name = rtti ? rtti->GetName() : nullptr;
                if (name && std::strstr(name, "EffectShader") != nullptr)
                    ++effect_shader;
            }
            else
            {
                ++without_property;
            }

            if (data.rendererData)
            {
                ++renderer_data;
                if (is_skinned)
                    ++skinned_with_renderer_data;
            }
            return RE::BSVisit::BSVisitControl::kContinue;
        });

        logger::info("SCOPY FP geoms subject={} root=0x{:x} geometries={} skinned={} property={} none={} effect={} renderer-data={} skinned-with-rd={}",
            a_label, reinterpret_cast<std::uintptr_t>(root), geometries, skinned, with_property, without_property,
            effect_shader, renderer_data, skinned_with_renderer_data);
    }
}

std::unordered_map<std::string_view, RE::NiAVObject*> nodes_by_name(RE::NiAVObject& a_root)
{
    // The engine's boneNodes table does not list every bone the animation skeleton names - the
    // alignment round measured 37/116 shared order and 109/116 reachable names - so a name bridge is
    // the only way to ask the skeleton order its question at all.
    std::unordered_map<std::string_view, RE::NiAVObject*> by_name;
    RE::BSVisit::TraverseScenegraphObjects(&a_root, [&](RE::NiAVObject* a_object)
    {
        if (a_object && readable(a_object, sizeof(RE::NiAVObject)))
        {
            const char* const name = a_object->name.c_str();
            if (name && *name)
                by_name.try_emplace(std::string_view(name), a_object);
        }
        return RE::BSVisit::BSVisitControl::kContinue;
    });
    return by_name;
}

RE::BShkbAnimationGraph* find_third_person_graph(RE::TESObjectREFR& a_subject)
{
    RE::BSAnimationGraphManagerPtr manager;
    if (!a_subject.GetAnimationGraphManager(manager) || !manager)
        return nullptr;
    for (RE::BSTSmartPointer<RE::BShkbAnimationGraph> const& holder : manager->graphs)
    {
        RE::BShkbAnimationGraph* const graph = holder.get();
        if (graph && graph->holder && graph->rootNode &&
            static_cast<RE::TESObjectREFR*>(graph->holder) == &a_subject)
            return graph;
    }
    return nullptr;
}

bool read_pose_track(RE::BShkbAnimationGraph& a_graph, int a_slot, OutputTrackView& a_view,
    RE::hkQsTransform const*& a_pose, std::int32_t& a_count)
{
    a_view = OutputTrackView{};
    a_pose = nullptr;
    a_count = 0;

    if (a_slot < 0 || a_slot >= Output_Slots || !readable(&a_graph, sizeof(RE::BShkbAnimationGraph)))
    {
        a_view.reason = "graph-unreadable";
        return false;
    }

    RE::hkbGeneratorOutput* const output = a_graph.generatorOutputs[a_slot];
    a_view.output = output;
    if (!readable(output, sizeof(RE::hkbGeneratorOutput)))
    {
        a_view.reason = "output-unreadable";
        return false;
    }
    a_view.output_readable = true;

    RE::hkbGeneratorOutput::Tracks const* const tracks = output->tracks;
    if (!readable(tracks, Track_Master_Header_Size + Track_Header_Size))
    {
        a_view.reason = "tracks-unreadable";
        return false;
    }
    a_view.tracks_readable = true;
    a_view.num_tracks = tracks->masterHeader.numTracks;

    constexpr auto Pose_Track = static_cast<std::size_t>(RE::hkbGeneratorOutput::StandardTracks::kPose);
    if (a_view.num_tracks <= static_cast<std::int32_t>(Pose_Track))
    {
        a_view.reason = "no-pose-track";
        return false;
    }
    a_view.exists = true;
    if (static_cast<std::size_t>(a_view.num_tracks) > Max_Track_Count ||
        !readable(tracks, Track_Master_Header_Size + Track_Header_Size * static_cast<std::size_t>(a_view.num_tracks)))
    {
        a_view.reason = "track-window-unreadable";
        return false;
    }

    // The typed array is declared with a single element (`Tracks::trackHeaders[1]`), so the window
    // above is what makes indexing past it valid rather than the declaration.
    RE::hkbGeneratorOutput::TrackHeader const* const headers = tracks->trackHeaders;
    RE::hkbGeneratorOutput::TrackHeader const& header = headers[Pose_Track];
    a_view.header_readable = true;
    a_view.num_data = header.numData;
    a_view.capacity = header.capacity;
    a_view.element_size = header.elementSizeBytes;
    a_view.data_offset = header.dataOffset;
    a_view.type = static_cast<std::uint8_t>(header.type);
    a_view.flags = static_cast<std::uint8_t>(header.flags);
    a_view.on_fraction = header.onFraction;
    // The engine's own liveness rule for a track, and the one OAR's preview uses.
    a_view.valid = header.onFraction > 0.0f;

    if (!a_view.valid)
    {
        a_view.reason = "track-off";
        return false;
    }
    // `onFraction` says the track is part of this output; the element count says whether it carries
    // anything. A live track with zero elements is a real state - the graph generated an output with
    // no pose in it (measured on the parked puppet, AFP1) - and it must not read as a bad count.
    if (a_view.num_data == 0)
    {
        a_view.reason = "pose-empty";
        return false;
    }
    if (a_view.num_data < 0 || a_view.num_data > Max_Pose_Count)
    {
        a_view.reason = "pose-count-out-of-range";
        return false;
    }
    if (a_view.element_size != static_cast<std::int16_t>(sizeof(RE::hkQsTransform)))
    {
        // Anything but a full QsTransform element cannot be interpreted as a pose, so it is reported
        // rather than reinterpreted.
        a_view.reason = "element-size";
        return false;
    }

    auto const* const base = reinterpret_cast<std::uint8_t const*>(tracks);
    const auto span = static_cast<std::size_t>(a_view.num_data) * sizeof(RE::hkQsTransform);
    if (!readable(base + a_view.data_offset, span))
    {
        a_view.reason = "pose-data-unreadable";
        return false;
    }

    a_view.data = base + a_view.data_offset;
    a_view.data_readable = true;
    a_view.reason = "ok";
    a_pose = static_cast<RE::hkQsTransform const*>(a_view.data);
    a_count = a_view.num_data;
    return true;
}

FinalPoseProbe& FinalPoseProbe::instance()
{
    static FinalPoseProbe s_instance;
    return s_instance;
}

void FinalPoseProbe::request_mark()
{
    m_mark_requested.store(true, std::memory_order_release);
}

FinalPoseProbe::PreSample FinalPoseProbe::sample_only(RE::TESObjectREFR* a_subject)
{
    PreSample sample;
    if (!a_subject || !readable(a_subject, sizeof(RE::TESObjectREFR)))
        return sample;

    RE::BShkbAnimationGraph* const graph = find_third_person_graph(*a_subject);
    if (!graph || !readable(graph, sizeof(RE::BShkbAnimationGraph)))
        return sample;

    for (int slot = 0; slot < Output_Slots; ++slot)
    {
        OutputTrackView view;
        RE::hkQsTransform const* pose = nullptr;
        std::int32_t count = 0;
        sample.reason[slot] = read_pose_track(*graph, slot, view, pose, count) ? "ok" : view.reason;
        sample.output[slot] = view.output;
        sample.pose_count[slot] = view.num_data;
        if (pose && count > 0)
        {
            sample.pose[slot].reserve(static_cast<std::size_t>(count));
            for (std::int32_t i = 0; i < count; ++i)
                sample.pose[slot].push_back(copy_pose(pose[i]));
        }
    }

    sample.bones.reserve(graph->boneNodes.size());
    for (RE::BShkbAnimationGraph::BoneNodeEntry const& entry : graph->boneNodes)
    {
        RE::NiNode* const node = entry.node;
        BoneState state;
        if (node && readable(node, sizeof(RE::NiAVObject)))
        {
            state.node = node;
            state.rotate = node->local.rotate;
            state.translate = node->local.translate;
            ++sample.bones_present;
        }
        sample.bones.push_back(state);
    }

    // The character's own local pose, as a second source of the same information.
    if (readable(&graph->characterInstance, sizeof(RE::hkbCharacter)))
    {
        const RE::hkbCharacter& character = graph->characterInstance;
        if (character.poseLocal && character.numPoseLocal > 0 &&
            readable(character.poseLocal, static_cast<std::size_t>(character.numPoseLocal) * sizeof(RE::hkQsTransform)))
        {
            sample.pose_local.reserve(static_cast<std::size_t>(character.numPoseLocal));
            for (std::int32_t i = 0; i < character.numPoseLocal; ++i)
                sample.pose_local.push_back(read_pose_unaligned(character.poseLocal[i]));
        }
    }

    sample.valid = true;
    return sample;
}

void FinalPoseProbe::sample_before_tick()
{
    // Not consuming the request here: this is the A0 half of the same mark the tick will report.
    if (!m_mark_requested.load(std::memory_order_acquire))
        return;

    PuppetDriver& driver = PuppetDriver::instance();
    m_puppet_pre = sample_only(driver.actor());
    m_player_pre = sample_only(RE::PlayerCharacter::GetSingleton());

    // The A0 lines: what the two subjects looked like before this frame advanced anything.
    const auto report_pre = [](char const* a_label, PreSample const& a_sample)
    {
        if (!a_sample.valid)
        {
            logger::info("SCOPY FP A0 subject={} UNAVAILABLE reason=no-sample", a_label);
            return;
        }
        logger::info("SCOPY FP A0 subject={} output0=0x{:x} pose0={} reason0={} output1=0x{:x} pose1={} reason1={} bones={}/{} pose-local={}",
            a_label,
            reinterpret_cast<std::uintptr_t>(a_sample.output[0]), a_sample.pose_count[0], a_sample.reason[0],
            reinterpret_cast<std::uintptr_t>(a_sample.output[1]), a_sample.pose_count[1], a_sample.reason[1],
            a_sample.bones_present, a_sample.bones.size(), a_sample.pose_local.size());
    };
    report_pre("puppet", m_puppet_pre);
    report_pre("player", m_player_pre);
}

void FinalPoseProbe::report_frame_delta(char const* a_label, PreSample const& a_pre, SubjectState const& a_now)
{
    if (!a_pre.valid)
        return;

    // What this frame's tick changed, measured on the two samples the same frame produced. The node
    // comparison keeps the identity rule the mark deltas use: only the same node at the same index
    // counts, and a rebuilt tree is reported as rebound instead of as a phantom move.
    float pose_rotation = 0.0f;
    for (int slot = 0; slot < Output_Slots; ++slot)
    {
        const std::size_t comparable = std::min(a_pre.pose[slot].size(), a_now.pose[slot].size());
        for (std::size_t i = 0; i < comparable; ++i)
            pose_rotation = std::max(pose_rotation, quat_angle_degrees(a_pre.pose[slot][i], a_now.pose[slot][i]));
    }

    std::uint32_t nodes_compared = 0;
    std::uint32_t nodes_moved = 0;
    std::uint32_t nodes_rebound = 0;
    float nodes_rotation = 0.0f;
    float nodes_position = 0.0f;
    const std::size_t comparable = std::min(a_pre.bones.size(), a_now.bones.size());
    for (std::size_t i = 0; i < comparable; ++i)
    {
        BoneState const& before = a_pre.bones[i];
        BoneState const& now = a_now.bones[i];
        if (!before.node || !now.node)
            continue;
        if (before.node != now.node)
        {
            ++nodes_rebound;
            continue;
        }
        ++nodes_compared;
        const float rotation = rotation_angle_degrees(before.rotate, now.rotate);
        const float position = (before.translate - now.translate).Length();
        if (rotation > 0.01f || position > 0.01f)
            ++nodes_moved;
        nodes_rotation = std::max(nodes_rotation, rotation);
        nodes_position = std::max(nodes_position, position);
    }

    // The character's own local pose, asked the same question: did this frame move it?
    std::uint32_t local_moved = 0;
    float local_rotation = 0.0f;
    const std::size_t local_comparable = std::min(a_pre.pose_local.size(), a_now.pose_local.size());
    for (std::size_t i = 0; i < local_comparable; ++i)
    {
        const float rotation = quat_angle_degrees(a_pre.pose_local[i], a_now.pose_local[i]);
        if (rotation > 0.01f)
            ++local_moved;
        local_rotation = std::max(local_rotation, rotation);
    }

    logger::info("SCOPY FP A0A1 subject={} pose0={}/{} pose1={}/{} pose-max-rot={:.3f}deg nodes={}/{} nodes-moved={} nodes-max-rot={:.3f}deg nodes-max-pos={:.3f} nodes-rebound={} pose-local={}/{} pose-local-moved={} pose-local-max-rot={:.3f}deg",
        a_label,
        a_pre.pose[0].size(), a_now.pose[0].size(),
        a_pre.pose[1].size(), a_now.pose[1].size(),
        pose_rotation,
        nodes_compared, a_pre.bones.size(),
        nodes_moved, nodes_rotation, nodes_position, nodes_rebound,
        local_comparable, a_pre.pose_local.size(), local_moved, local_rotation);
}

void FinalPoseProbe::tick(bool a_paused)
{
    if (!m_mark_requested.exchange(false, std::memory_order_acq_rel))
        return;

    ++m_marks;
    PuppetDriver& driver = PuppetDriver::instance();
    logger::info("SCOPY FP mark={} paused={} tick={} manual-ticks={} placement={}",
        m_marks, a_paused ? 1 : 0, driver.tick_enabled() ? "on" : "off", driver.manual_ticks(),
        PuppetDriver::placement_name(driver.placement()));

    // The camera chain first: it is shared by both subjects, and it is what the display side has to
    // adapt once the Actor may no longer be translated into frame.
    report_camera();

    // The puppet first: it is the Actor the route would draw. The player follows as the reference -
    // the engine drives that one's graph and nodes itself, so its calibration says what the correct
    // answer looks like on this machine and skeleton, while the puppet's says what the parked Actor
    // actually delivers.
    subject("puppet", driver.actor(), m_puppet);
    subject("player", RE::PlayerCharacter::GetSingleton(), m_player);

    // The A0 -> A1 pair, then the physics stage this round does not have: writing `skipped` is what
    // keeps the six observation points complete instead of silently absent (R00's A3).
    report_frame_delta("puppet", m_puppet_pre, m_puppet);
    report_frame_delta("player", m_player_pre, m_player);
    logger::info("SCOPY FP A3 physics=skipped reason=first-round-physics-off");

    // A2: what the application stage did on the frame this mark describes. The stage owns the numbers;
    // the mark is where a reader looks for them, so they are printed together with the rest.
    const FinalPoseApply& apply = FinalPoseApply::instance();
    const FinalPoseApply::Stats& apply_stats = apply.stats();
    logger::info("SCOPY FP A2 drive={} apply={} mapping={} verified={} generation={} written={} unreachable={} frames={} verifications={} max-rot={:.3f}deg max-pos={:.3f} blocked={}",
        FinalPoseApply::drive_name(apply.drive()),
        apply.apply_enabled() ? "on" : "off",
        apply.mapping(),
        apply.verified() ? 1 : 0,
        apply.generation(),
        apply_stats.written, apply_stats.unreachable, apply_stats.frames, apply_stats.verifications,
        apply_stats.max_rotation, apply_stats.max_position,
        apply.blocked());

    m_puppet_pre = PreSample{};
    m_player_pre = PreSample{};
}

void FinalPoseProbe::report_state(char const* a_label, RE::BShkbAnimationGraph const& a_graph, RE::hkbCharacter const* a_character)
{
    constexpr std::size_t raw_row = 16;

    RE::hkbBehaviorGraph* const behavior = a_graph.behaviorGraph;
    const bool behavior_readable = behavior && readable(behavior, sizeof(RE::hkbBehaviorGraph));

    logger::info("SCOPY FP state subject={} bhv=0x{:x} bhv-readable={} is-active={} is-linked={} update-active-nodes={} state-changed={} static-nodes={} interm-outputs={} active-nodes=0x{:x} node-clones={} root-generator=0x{:x} user-data=0x{:x} anim-bones={} interp=({:.3f},{:.3f}) fade-controllers={} do-foot-ik={} holder=0x{:x} root-node=0x{:x}",
        a_label,
        reinterpret_cast<std::uintptr_t>(behavior),
        behavior_readable ? 1 : 0,
        behavior_readable ? static_cast<std::int32_t>(behavior->isActive) : -1,
        behavior_readable ? static_cast<std::int32_t>(behavior->isLinked) : -1,
        behavior_readable ? static_cast<std::int32_t>(behavior->updateActiveNodes) : -1,
        behavior_readable ? static_cast<std::int32_t>(behavior->stateOrTransitionChanged) : -1,
        behavior_readable ? static_cast<std::int32_t>(behavior->numStaticNodes) : -1,
        behavior_readable ? behavior->numIntermediateOutputs : -1,
        reinterpret_cast<std::uintptr_t>(behavior_readable ? behavior->activeNodes : nullptr),
        behavior_readable && behavior->activeNodes && readable(behavior->activeNodes, sizeof(RE::NodeList))
            ? static_cast<std::uint32_t>(behavior->activeNodes->size())
            : 0u,
        reinterpret_cast<std::uintptr_t>(behavior_readable ? behavior->rootGenerator.get() : nullptr),
        static_cast<std::uintptr_t>(behavior_readable ? behavior->userData : 0),
        static_cast<std::uint32_t>(a_graph.numAnimBones),
        a_graph.interpolationAmounts[0],
        a_graph.interpolationAmounts[1],
        static_cast<std::uint32_t>(a_graph.fadeControllers.size()),
        static_cast<std::uint32_t>(a_graph.doFootIK),
        reinterpret_cast<std::uintptr_t>(a_graph.holder),
        reinterpret_cast<std::uintptr_t>(a_graph.rootNode));

    // The bytes no source has named yet, printed rather than guessed at: the graph's flag tail (the
    // two unknown words and six unknown bytes after numAnimBones), the behaviour graph's lifecycle
    // tail, and the character's LOD fields with the padding that sits beside them.
    auto const raw = [a_label, raw_row](char const* what, void const* base, std::size_t offset, std::size_t size)
    {
        auto const* const start = static_cast<std::uint8_t const*>(base) + offset;
        if (!readable(start, size))
        {
            logger::info("SCOPY FP raw subject={} what={} off=0x{:x} UNAVAILABLE reason=unreadable", a_label, what, offset);
            return;
        }

        for (std::size_t row = 0; row < size; row += raw_row)
        {
            const std::size_t count = std::min(raw_row, size - row);
            std::string bytes;
            for (std::size_t index = 0; index < count; ++index)
                bytes += fmt::format("{:02x} ", start[row + index]);
            logger::info("SCOPY FP raw subject={} what={} off=0x{:x} {}", a_label, what, offset + row, bytes);
        }
    };

    raw("anim-graph", &a_graph, 0x238, 0x18);
    if (behavior_readable)
        raw("behavior-graph", behavior, 0x118, 0x18);
    if (a_character && readable(a_character, sizeof(RE::hkbCharacter)))
        raw("character", a_character, 0x18, 0x18);
}

void FinalPoseProbe::subject(char const* a_label, RE::TESObjectREFR* a_subject, SubjectState& a_state)
{
    if (!a_subject || !readable(a_subject, sizeof(RE::TESObjectREFR)))
    {
        logger::info("SCOPY FP subject={} UNAVAILABLE reason=no-actor", a_label);
        return;
    }

    RE::BShkbAnimationGraph* const graph = find_third_person_graph(*a_subject);
    if (!graph || !readable(graph, sizeof(RE::BShkbAnimationGraph)))
    {
        logger::info("SCOPY FP subject={} form={:08x} UNAVAILABLE reason=no-third-person-graph",
            a_label, a_subject->GetFormID());
        return;
    }

    RE::hkbCharacter* const character = readable(&graph->characterInstance, sizeof(RE::hkbCharacter))
        ? &graph->characterInstance
        : nullptr;
    const RE::hkaSkeleton* skeleton = nullptr;
    if (character && character->setup && readable(character->setup.get(), sizeof(RE::hkbCharacterSetup)))
    {
        const RE::hkaSkeleton* const candidate = character->setup->animationSkeleton.get();
        if (candidate && readable(candidate, sizeof(RE::hkaSkeleton)))
            skeleton = candidate;
    }

    std::uint32_t bound = 0;
    for (RE::BShkbAnimationGraph::BoneNodeEntry const& entry : graph->boneNodes)
    {
        if (entry.node && readable(entry.node, sizeof(RE::NiAVObject)))
            ++bound;
    }

    // The generation this mark belongs to: a new 3D root or a new graph invalidates every mapping and
    // every pose held across frames, so it is counted rather than left to be noticed (R02/R06).
    RE::NiAVObject* const subject_root = a_subject->Get3D(false);
    if (a_state.root != subject_root || a_state.graph != graph)
    {
        if (a_state.root || a_state.graph)
            ++a_state.generation;
        a_state.root = subject_root;
        a_state.graph = graph;
    }

    logger::info("SCOPY FP census subject={} form={:08x} gen={} graph=0x{:x} root=0x{:x} bone-nodes={} bound={} pose-local={} num-anim-bones={} tracks-in-lod={} lod={} skeleton='{}' skeleton-bones={}",
        a_label,
        a_subject->GetFormID(),
        a_state.generation,
        reinterpret_cast<std::uintptr_t>(graph),
        reinterpret_cast<std::uintptr_t>(subject_root),
        graph->boneNodes.size(),
        bound,
        character ? character->numPoseLocal : -1,
        static_cast<std::uint32_t>(graph->numAnimBones),
        character ? character->numTracksInLOD : -1,
        character ? character->currentLOD : -1,
        skeleton && skeleton->name.c_str() ? skeleton->name.c_str() : "",
        skeleton ? skeleton->bones.size() : 0);

    report_state(a_label, *graph, character);

    report_figure(a_label, *a_subject);
    report_geometry(a_label, *a_subject);

    for (int slot = 0; slot < Output_Slots; ++slot)
    {
        RE::hkbGeneratorOutput* const output = graph->generatorOutputs[slot];
        const std::string on_tracks = output && readable(output, sizeof(RE::hkbGeneratorOutput))
            ? live_tracks(*output)
            : "-";

        OutputTrackView view;
        RE::hkQsTransform const* pose = nullptr;
        std::int32_t count = 0;
        const bool pose_readable = read_pose_track(*graph, slot, view, pose, count);

        logger::info("SCOPY FP slot={} subject={} out=0x{:x} tracks={} on-tracks=[{}] exists={} valid={} type={} flags={} num-data={} cap={} elem={} off={} on={:.3f} data=0x{:x} readable={} reason={}",
            slot, a_label,
            reinterpret_cast<std::uintptr_t>(view.output),
            view.num_tracks,
            on_tracks,
            view.exists ? 1 : 0,
            view.valid ? 1 : 0,
            view.type,
            view.flags,
            view.num_data,
            view.capacity,
            view.element_size,
            view.data_offset,
            view.on_fraction,
            reinterpret_cast<std::uintptr_t>(view.data),
            view.data_readable ? 1 : 0,
            view.reason);

        // The copy this mark owns: plain numbers read once, so the delta below compares two samples
        // this probe holds rather than two reads of a buffer the engine may still be writing.
        std::vector<PoseSample> current;
        if (pose_readable)
        {
            current.reserve(static_cast<std::size_t>(count));
            for (std::int32_t i = 0; i < count; ++i)
                current.push_back(copy_pose(pose[i]));
        }

        const std::vector<PoseSample>& previous = a_state.pose[slot];
        if (previous.empty())
        {
            logger::info("SCOPY FP delta slot={} subject={} prev=0 now={} first=1 readable={}",
                slot, a_label, current.size(), pose_readable ? 1 : 0);
        }
        else
        {
            const std::size_t comparable = std::min(previous.size(), current.size());
            std::uint32_t changed = 0;
            float max_rotation = 0.0f;
            float max_position = 0.0f;
            for (std::size_t i = 0; i < comparable; ++i)
            {
                const float rotation = quat_angle_degrees(previous[i], current[i]);
                const float position = pose_distance(previous[i], current[i]);
                if (rotation > Moved_Rotation_Degrees || position > Moved_Position)
                    ++changed;
                max_rotation = std::max(max_rotation, rotation);
                max_position = std::max(max_position, position);
            }
            logger::info("SCOPY FP delta slot={} subject={} prev={} now={} compared={} changed={} max-rot={:.3f}deg max-pos={:.3f} vs-mark={}",
                slot, a_label, previous.size(), current.size(), comparable, changed, max_rotation, max_position, m_marks - 1);
        }

        a_state.pose[slot] = std::move(current);
    }

    // The node locals as the engine left them. Every pointer is re-resolved from the live graph on
    // each mark and only its identity is kept: HKX43's dangling read came from holding a node across
    // frames, and a rebuilt tree must show up as `rebound`, not as a phantom move or a fault.
    std::vector<BoneState> bones;
    bones.reserve(graph->boneNodes.size());
    std::uint32_t missing = 0;
    for (RE::BShkbAnimationGraph::BoneNodeEntry const& entry : graph->boneNodes)
    {
        RE::NiNode* const node = entry.node;
        if (!node || !readable(node, sizeof(RE::NiAVObject)))
        {
            bones.push_back(BoneState{});
            ++missing;
            continue;
        }
        BoneState state;
        state.node = node;
        state.rotate = node->local.rotate;
        state.translate = node->local.translate;
        bones.push_back(state);
    }

    std::uint32_t moved = 0;
    if (a_state.bones.empty())
    {
        logger::info("SCOPY FP nodes subject={} bones={} missing={} first=1", a_label, bones.size(), missing);
    }
    else
    {
        std::uint32_t compared = 0;
        std::uint32_t rebound = 0;
        std::uint32_t gone = 0;
        float max_rotation = 0.0f;
        float max_position = 0.0f;
        const std::size_t comparable = std::min(a_state.bones.size(), bones.size());
        for (std::size_t i = 0; i < comparable; ++i)
        {
            BoneState const& before = a_state.bones[i];
            BoneState const& now = bones[i];
            if (!before.node || !now.node)
            {
                ++gone;
                continue;
            }
            if (before.node != now.node)
            {
                ++rebound;
                continue;
            }
            ++compared;
            const float rotation = rotation_angle_degrees(before.rotate, now.rotate);
            const float position = (now.translate - before.translate).Length();
            if (rotation > Moved_Rotation_Degrees || position > Moved_Position)
                ++moved;
            max_rotation = std::max(max_rotation, rotation);
            max_position = std::max(max_position, position);
        }
        logger::info("SCOPY FP nodes subject={} bones={} compared={} rebound={} gone={} missing={} changed={} max-rot={:.3f}deg max-pos={:.3f} vs-mark={}",
            a_label, bones.size(), compared, rebound, gone, missing, moved, max_rotation, max_position, m_marks - 1);
    }
    a_state.nodes_moved = moved;
    a_state.bones = std::move(bones);

    // The character's own local pose, alongside the graph's output pose: the paused puppet has no
    // output at all, and whether this one still moves under our own clock is the difference between
    // "the route needs another source" and "the engine stopped evaluating".
    //
    // Its movement is also measured ACROSS marks, not only within a frame: the 2026-10-13 23:03 session
    // measured 29 of 116 entries moving by at most 0.079 degrees inside one paused frame on BOTH
    // subjects - and the player is never manually ticked, so that alone cannot say whether our tick is
    // what drives it. A window of hundreds of ticks can.
    std::vector<PoseSample> fresh_local;
    if (character && character->poseLocal && character->numPoseLocal > 0 &&
        readable(character->poseLocal, static_cast<std::size_t>(character->numPoseLocal) * sizeof(RE::hkQsTransform)))
    {
        fresh_local.reserve(static_cast<std::size_t>(character->numPoseLocal));
        for (std::int32_t i = 0; i < character->numPoseLocal; ++i)
            fresh_local.push_back(read_pose_unaligned(character->poseLocal[i]));
    }

    if (!fresh_local.empty())
    {
        float local_rotation = 0.0f;
        float local_position = 0.0f;
        std::uint32_t local_moved = 0;
        const std::size_t count = std::min(fresh_local.size(), a_state.pose_local.size());
        for (std::size_t i = 0; i < count; ++i)
        {
            const float rotation = quat_angle_degrees(a_state.pose_local[i], fresh_local[i]);
            const float position = pose_distance(a_state.pose_local[i], fresh_local[i]);
            if (rotation > 0.01f || position > 0.01f)
                ++local_moved;
            local_rotation = std::max(local_rotation, rotation);
            local_position = std::max(local_position, position);
        }
        if (!a_state.pose_local.empty())
        {
            logger::info("SCOPY FP pose-local subject={} entries={} moved={} max-rot={:.3f}deg max-pos={:.3f} vs-mark={}",
                a_label, fresh_local.size(), local_moved, local_rotation, local_position, m_marks - 1);
        }
        a_state.pose_local = std::move(fresh_local);
    }

    const Calibration calibration = calibrate(*graph, a_state, a_label);
    const Candidate& best = calibration.candidate;
    if (best.slot < 0)
    {
        logger::info("SCOPY FP verdict subject={} match=0 reason={} nodes-moved={}",
            a_label, calibration.decision.reason, a_state.nodes_moved);
        return;
    }
    logger::info("SCOPY FP verdict subject={} match={} reason={} best={}/slot{} within-1deg={}/{} ratio={:.2f} max-rot={:.3f}deg max-pos={:.3f} max-scale={:.4f} world-within-1deg={}/{} named={} nodes-moved={}",
        a_label,
        calibration.decision.match ? 1 : 0,
        calibration.decision.reason,
        best.order, best.slot, best.score.within, best.score.compared, best.score.ratio(),
        best.score.max_rotation, best.score.max_position, best.score.max_scale_deviation,
        best.world.within, best.world.compared, best.named, a_state.nodes_moved);

    // Which of the bones nothing reproduced are ones a draw would actually read.
    report_necessity(a_label, *a_subject, calibration.outlier_nodes);
}

FinalPoseProbe::Calibration FinalPoseProbe::calibrate(RE::BShkbAnimationGraph& a_graph, SubjectState const& a_state,
    char const* a_label)
{
    Calibration calibration;

    // One candidate index order as data: the node's local and world transform per index, plus the
    // names, so an outlier can be reported by the bone it belongs to instead of by a number. Index
    // alignment is the whole point: entry i is the node pose index i would drive under that order, and
    // an index without a node is marked absent rather than dropped, so a sparse table cannot shift
    // the comparison onto the wrong bones.
    struct CandidateTable
    {
        std::vector<TransformSample> locals;
        std::vector<TransformSample> worlds;
        std::vector<std::string> names;
        std::vector<RE::NiAVObject*> nodes;   // the node each index resolved to, for the palette question
        std::uint32_t named = 0;   // skeleton order only: bones that resolved to a node by exact name
    };

    const auto read_node = [](RE::NiAVObject const* a_node, TransformSample& a_local, TransformSample& a_world)
    {
        if (!a_node || !readable(a_node, sizeof(RE::NiAVObject)))
            return false;
        a_local.rotate = a_node->local.rotate;
        a_local.translate = a_node->local.translate;
        a_local.scale = a_node->local.scale;
        a_local.present = true;
        a_world.rotate = a_node->world.rotate;
        a_world.translate = a_node->world.translate;
        a_world.scale = a_node->world.scale;
        a_world.present = true;
        return true;
    };

    CandidateTable by_bone_nodes;
    by_bone_nodes.locals.resize(a_graph.boneNodes.size());
    by_bone_nodes.worlds.resize(a_graph.boneNodes.size());
    by_bone_nodes.names.resize(a_graph.boneNodes.size());
    by_bone_nodes.nodes.resize(a_graph.boneNodes.size());
    for (std::int32_t i = 0; i < static_cast<std::int32_t>(a_graph.boneNodes.size()); ++i)
    {
        const std::size_t index = static_cast<std::size_t>(i);
        RE::NiNode* const node = a_graph.boneNodes[i].node;
        read_node(node, by_bone_nodes.locals[index], by_bone_nodes.worlds[index]);
        if (node && readable(node, sizeof(RE::NiAVObject)))
        {
            by_bone_nodes.nodes[index] = node;
            const char* const name = node->name.c_str();
            if (name && *name)
                by_bone_nodes.names[index] = name;
        }
    }

    const RE::hkaSkeleton* skeleton = nullptr;
    if (readable(&a_graph.characterInstance, sizeof(RE::hkbCharacter)))
    {
        const RE::hkbCharacter& character = a_graph.characterInstance;
        if (character.setup && readable(character.setup.get(), sizeof(RE::hkbCharacterSetup)))
        {
            const RE::hkaSkeleton* const candidate = character.setup->animationSkeleton.get();
            if (candidate && readable(candidate, sizeof(RE::hkaSkeleton)))
                skeleton = candidate;
        }
    }

    std::unordered_map<std::string_view, RE::NiAVObject*> by_name;
    if (skeleton && a_graph.rootNode && readable(a_graph.rootNode, sizeof(RE::NiAVObject)))
        by_name = nodes_by_name(*a_graph.rootNode);

    CandidateTable by_skeleton;
    if (skeleton)
    {
        const std::size_t bones = static_cast<std::size_t>(skeleton->bones.size());
        by_skeleton.locals.resize(bones);
        by_skeleton.worlds.resize(bones);
        by_skeleton.names.resize(bones);
        by_skeleton.nodes.resize(bones);
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(bones); ++i)
        {
            const std::size_t index = static_cast<std::size_t>(i);
            const char* const name = skeleton->bones[i].name.c_str();
            if (name && *name)
                by_skeleton.names[index] = name;

            const auto found = name && *name ? by_name.find(std::string_view(name)) : by_name.end();
            if (found == by_name.end())
                continue;
            if (read_node(found->second, by_skeleton.locals[index], by_skeleton.worlds[index]))
            {
                by_skeleton.nodes[index] = found->second;
                ++by_skeleton.named;
            }
        }
    }

    // The names of the bones a candidate did not reproduce: R03 asks which ones, because a helper
    // branch can be skipped while a target mesh's own bone cannot.
    const auto name_outliers = [](OrderScore const& a_score, std::vector<std::string> const& a_names)
    {
        std::string out;
        for (std::uint32_t index : a_score.outliers)
        {
            if (!out.empty())
                out += ",";
            out += "'";
            out += index < a_names.size() && !a_names[index].empty() ? a_names[index] : "?";
            out += "'";
        }
        return out.empty() ? std::string("-") : out;
    };

    // The character's own local pose, scored the same way the graph's output pose is: it is the second
    // source the paused panel could read, and this line says whether it is a whole pose at all and in
    // which order it is indexed (HKX2 measured `poseLocal` as boneNodes-ordered, unlike this graph
    // output). A source that is only micro-drift would score near zero here and must not be mistaken
    // for a pose.
    if (!a_state.pose_local.empty() && (skeleton || !by_bone_nodes.locals.empty()))
    {
        const OrderScore local_by_bones = score_pose_order(a_state.pose_local, by_bone_nodes.locals, Pose_Match_Degrees);
        const OrderScore local_by_skeleton = score_pose_order(a_state.pose_local, by_skeleton.locals, Pose_Match_Degrees);
        logger::info("SCOPY FP pose-local order subject={} entries={} bone-nodes within-1deg={}/{} ratio={:.2f} | skeleton within-1deg={}/{} ratio={:.2f}",
            a_label, a_state.pose_local.size(),
            local_by_bones.within, local_by_bones.compared, local_by_bones.ratio(),
            local_by_skeleton.within, local_by_skeleton.compared, local_by_skeleton.ratio());
    }

    std::vector<Candidate> candidates;
    for (int order = 0; order < 2; ++order)
    {
        const bool skeleton_order = order == 1;
        const char* const order_name = skeleton_order ? "skeleton" : "bone-nodes";
        const CandidateTable& table = skeleton_order ? by_skeleton : by_bone_nodes;

        for (int slot = 0; slot < Output_Slots; ++slot)
        {
            const std::vector<PoseSample>& pose = a_state.pose[slot];
            if (pose.empty() || table.locals.empty())
                continue;

            Candidate candidate;
            candidate.order = order_name;
            candidate.slot = slot;
            candidate.sampled = static_cast<std::uint32_t>(pose.size());
            candidate.named = table.named;
            candidate.score = score_pose_order(pose, table.locals, Pose_Match_Degrees);

            // The world space, only where a real parent table exists to compose with: the animation
            // skeleton's own. The pose is expected to reproduce whole bone chains, so a mismatch here
            // says the two hierarchies disagree even though every local matched.
            if (skeleton_order && skeleton && !skeleton->parentIndices.empty() &&
                readable(skeleton->parentIndices.data(), sizeof(std::int16_t) * static_cast<std::size_t>(skeleton->parentIndices.size())))
            {
                std::vector<TransformSample> expected_locals(pose.size());
                pose_locals(pose, expected_locals);
                std::vector<TransformSample> expected_worlds(expected_locals.size());
                compose_worlds(expected_locals,
                    { skeleton->parentIndices.data(), static_cast<std::size_t>(skeleton->parentIndices.size()) },
                    expected_worlds);
                // The composed worlds start at the root bone; the node worlds carry the Actor's own
                // placement on top. Align the chains before scoring, or every bone is convicted of an
                // offset that belongs to the world, not to the pose.
                std::vector<TransformSample> aligned(expected_worlds.size());
                align_worlds_to_root(expected_worlds,
                    { skeleton->parentIndices.data(), static_cast<std::size_t>(skeleton->parentIndices.size()) },
                    table.worlds, aligned);
                candidate.world = score_transforms(aligned, table.worlds, Pose_Match_Degrees);
            }

            if (skeleton_order)
            {
                logger::info("SCOPY FP order={} slot={} subject={} sampled={} bones={} named={} compared={} within-1deg={} beyond={} ratio={:.2f} max-rot={:.3f}deg max-pos={:.3f} max-scale={:.4f} outliers={}",
                    order_name, slot, a_label, candidate.sampled, table.locals.size(), candidate.named,
                    candidate.score.compared, candidate.score.within, candidate.score.beyond, candidate.score.ratio(),
                    candidate.score.max_rotation, candidate.score.max_position, candidate.score.max_scale_deviation,
                    name_outliers(candidate.score, table.names));
                logger::info("SCOPY FP world order={} slot={} subject={} compared={} within-1deg={} beyond={} ratio={:.2f} max-rot={:.3f}deg max-pos={:.3f} max-scale={:.4f} outliers={}",
                    order_name, slot, a_label, candidate.world.compared, candidate.world.within, candidate.world.beyond,
                    candidate.world.ratio(), candidate.world.max_rotation, candidate.world.max_position,
                    candidate.world.max_scale_deviation, name_outliers(candidate.world, table.names));
            }
            else
            {
                logger::info("SCOPY FP order={} slot={} subject={} sampled={} bones={} compared={} within-1deg={} beyond={} ratio={:.2f} max-rot={:.3f}deg max-pos={:.3f} max-scale={:.4f} outliers={}",
                    order_name, slot, a_label, candidate.sampled, table.locals.size(), candidate.score.compared,
                    candidate.score.within, candidate.score.beyond, candidate.score.ratio(),
                    candidate.score.max_rotation, candidate.score.max_position, candidate.score.max_scale_deviation,
                    name_outliers(candidate.score, table.names));
            }

            candidates.push_back(candidate);
        }
    }

    // The decision the application stage will take on exactly these numbers, reached by the same call
    // it will make - so this log says whether that stage would be allowed to write, not just how the
    // candidates scored. It gates on the local space, which is what a write would have to reproduce.
    std::vector<OrderScore> scores;
    scores.reserve(candidates.size());
    for (Candidate const& candidate : candidates)
        scores.push_back(candidate.score);
    calibration.decision = decide_pose_order(scores);
    if (!candidates.empty())
    {
        calibration.candidate = candidates[calibration.decision.best];

        // The chosen candidate's outlier nodes, by index: the palette question below asks which of
        // them a skinned mesh reads, and that depends on which order was chosen.
        const CandidateTable& chosen = std::string_view(calibration.candidate.order) == "skeleton"
            ? by_skeleton
            : by_bone_nodes;
        calibration.outlier_nodes.reserve(calibration.candidate.score.outliers.size());
        for (std::uint32_t index : calibration.candidate.score.outliers)
        {
            if (static_cast<std::size_t>(index) < chosen.nodes.size() && chosen.nodes[static_cast<std::size_t>(index)])
                calibration.outlier_nodes.push_back(chosen.nodes[static_cast<std::size_t>(index)]);
        }
    }

    return calibration;
}

PLUGIN_NAMESPACE_END
