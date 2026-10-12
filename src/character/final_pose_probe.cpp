//
// Created by AmazingBuff on 2026/10/13.
//

#include "character/final_pose_probe.h"

#include "character/puppet_driver.h"
#include "character/snapshot_transform.h"

#include "RE/A/Actor.h"
#include "RE/B/BSAnimationGraphManager.h"
#include "RE/B/BSFadeNode.h"
#include "RE/B/BSShaderAccumulator.h"
#include "RE/B/BShkbAnimationGraph.h"
#include "RE/B/BSVisit.h"
#include "RE/H/hkaSkeleton.h"
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

    // A copy of one pose sample as plain numbers. It reads the same three SSE quads `read_pose` in
    // animation_source.h reads, but with memcpy: this one is pointed at engine memory whose offset
    // and alignment are exactly what is being validated, and a misaligned vector load would fault
    // inside the probe that exists to turn a wrong assumption into a printed number.
    PoseSample copy_pose(RE::hkQsTransform const& a_pose)
    {
        float raw[12];
        std::memcpy(raw, &a_pose, sizeof(raw));

        PoseSample sample{};
        sample.position[0] = raw[0];
        sample.position[1] = raw[1];
        sample.position[2] = raw[2];
        sample.quaternion[0] = raw[4];
        sample.quaternion[1] = raw[5];
        sample.quaternion[2] = raw[6];
        sample.quaternion[3] = raw[7];
        sample.scale = raw[8];
        return sample;
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

    // The character's third-person graph: the one whose holder is that character and which has a root
    // node. Same selection the animation-source round established, reused rather than re-guessed.
    RE::BShkbAnimationGraph* third_person_graph(RE::TESObjectREFR& a_subject)
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

    // Every node of the graph's own tree by exact name. The engine's boneNodes table does not list
    // every bone the animation skeleton names - the alignment round measured 37/116 shared order and
    // 109/116 reachable names - so a name bridge is the only way to ask the skeleton order its
    // question at all.
    std::unordered_map<std::string_view, RE::NiAVObject*> nodes_by_name(RE::NiAVObject& a_root)
    {
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
    if (a_view.num_data <= 0 || a_view.num_data > Max_Pose_Count)
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

void FinalPoseProbe::tick(bool a_paused)
{
    if (!m_mark_requested.exchange(false, std::memory_order_acq_rel))
        return;

    ++m_marks;
    PuppetDriver& driver = PuppetDriver::instance();
    logger::info("SCOPY FP mark={} paused={} tick={} manual-ticks={}",
        m_marks, a_paused ? 1 : 0, driver.tick_enabled() ? "on" : "off", driver.manual_ticks());

    // The camera chain first: it is shared by both subjects, and it is what the display side has to
    // adapt once the Actor may no longer be translated into frame.
    report_camera();

    // The puppet first: it is the Actor the route would draw. The player follows as the reference -
    // the engine drives that one's graph and nodes itself, so its calibration says what the correct
    // answer looks like on this machine and skeleton, while the puppet's says what the parked Actor
    // actually delivers.
    subject("puppet", driver.actor(), m_puppet);
    subject("player", RE::PlayerCharacter::GetSingleton(), m_player);
}

void FinalPoseProbe::subject(char const* a_label, RE::TESObjectREFR* a_subject, SubjectState& a_state)
{
    if (!a_subject || !readable(a_subject, sizeof(RE::TESObjectREFR)))
    {
        logger::info("SCOPY FP subject={} UNAVAILABLE reason=no-actor", a_label);
        return;
    }

    RE::BShkbAnimationGraph* const graph = third_person_graph(*a_subject);
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

    logger::info("SCOPY FP census subject={} form={:08x} graph=0x{:x} bone-nodes={} bound={} pose-local={} num-anim-bones={} tracks-in-lod={} lod={} skeleton='{}' skeleton-bones={}",
        a_label,
        a_subject->GetFormID(),
        reinterpret_cast<std::uintptr_t>(graph),
        graph->boneNodes.size(),
        bound,
        character ? character->numPoseLocal : -1,
        static_cast<std::uint32_t>(graph->numAnimBones),
        character ? character->numTracksInLOD : -1,
        character ? character->currentLOD : -1,
        skeleton && skeleton->name.c_str() ? skeleton->name.c_str() : "",
        skeleton ? skeleton->bones.size() : 0);

    report_figure(a_label, *a_subject);

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

    const Calibration calibration = calibrate(*graph, a_state, a_label);
    const Candidate& best = calibration.candidate;
    if (best.slot < 0)
    {
        logger::info("SCOPY FP verdict subject={} match=0 reason={} nodes-moved={}",
            a_label, calibration.decision.reason, a_state.nodes_moved);
        return;
    }
    logger::info("SCOPY FP verdict subject={} match={} reason={} best={}/slot{} within-1deg={}/{} ratio={:.2f} max-rot={:.3f}deg max-pos={:.3f} named={} nodes-moved={}",
        a_label,
        calibration.decision.match ? 1 : 0,
        calibration.decision.reason,
        best.order, best.slot, best.score.within, best.score.compared, best.score.ratio(),
        best.score.max_rotation, best.score.max_position, best.named, a_state.nodes_moved);
}

FinalPoseProbe::Calibration FinalPoseProbe::calibrate(RE::BShkbAnimationGraph& a_graph, SubjectState const& a_state,
    char const* a_label)
{
    Calibration calibration;

    // The two candidate tables, built once per mark. Index alignment is the whole point: entry i is
    // the node pose index i would drive under that order, and an index without a node is marked
    // absent rather than dropped, so a sparse table cannot shift the comparison onto wrong bones.
    std::vector<NodeLocal> by_bone_nodes(a_graph.boneNodes.size());
    for (std::int32_t i = 0; i < static_cast<std::int32_t>(a_graph.boneNodes.size()); ++i)
    {
        RE::NiNode* const node = a_graph.boneNodes[i].node;
        if (!node || !readable(node, sizeof(RE::NiAVObject)))
            continue;
        by_bone_nodes[static_cast<std::size_t>(i)].rotate = node->local.rotate;
        by_bone_nodes[static_cast<std::size_t>(i)].translate = node->local.translate;
        by_bone_nodes[static_cast<std::size_t>(i)].present = true;
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

    std::vector<NodeLocal> by_skeleton(skeleton ? static_cast<std::size_t>(skeleton->bones.size()) : 0);
    std::uint32_t named = 0;
    for (std::int32_t i = 0; i < static_cast<std::int32_t>(by_skeleton.size()); ++i)
    {
        const char* const name = skeleton->bones[i].name.c_str();
        if (!name || !*name)
            continue;
        const auto found = by_name.find(std::string_view(name));
        if (found == by_name.end())
            continue;
        RE::NiAVObject* const node = found->second;
        if (!node || !readable(node, sizeof(RE::NiAVObject)))
            continue;
        const std::size_t index = static_cast<std::size_t>(i);
        by_skeleton[index].rotate = node->local.rotate;
        by_skeleton[index].translate = node->local.translate;
        by_skeleton[index].present = true;
        ++named;
    }

    std::vector<Candidate> candidates;
    for (int order = 0; order < 2; ++order)
    {
        const bool skeleton_order = order == 1;
        const char* const order_name = skeleton_order ? "skeleton" : "bone-nodes";
        const std::vector<NodeLocal>& table = skeleton_order ? by_skeleton : by_bone_nodes;

        for (int slot = 0; slot < Output_Slots; ++slot)
        {
            const std::vector<PoseSample>& pose = a_state.pose[slot];
            if (pose.empty() || table.empty())
                continue;

            Candidate candidate;
            candidate.order = order_name;
            candidate.slot = slot;
            candidate.sampled = static_cast<std::uint32_t>(pose.size());
            candidate.named = skeleton_order ? named : 0;
            candidate.score = score_pose_order(pose, table, Pose_Match_Degrees);

            if (skeleton_order)
            {
                logger::info("SCOPY FP order={} slot={} subject={} sampled={} bones={} named={} compared={} within-1deg={} ratio={:.2f} max-rot={:.3f}deg max-pos={:.3f}",
                    order_name, slot, a_label, candidate.sampled, table.size(), candidate.named, candidate.score.compared,
                    candidate.score.within, candidate.score.ratio(), candidate.score.max_rotation, candidate.score.max_position);
            }
            else
            {
                logger::info("SCOPY FP order={} slot={} subject={} sampled={} bones={} compared={} within-1deg={} ratio={:.2f} max-rot={:.3f}deg max-pos={:.3f}",
                    order_name, slot, a_label, candidate.sampled, table.size(), candidate.score.compared,
                    candidate.score.within, candidate.score.ratio(), candidate.score.max_rotation, candidate.score.max_position);
            }

            candidates.push_back(candidate);
        }
    }

    // The decision the application stage will take on exactly these numbers, reached by the same call
    // it will make - so this log says whether that stage would be allowed to write, not just how the
    // candidates scored.
    std::vector<OrderScore> scores;
    scores.reserve(candidates.size());
    for (Candidate const& candidate : candidates)
        scores.push_back(candidate.score);
    calibration.decision = decide_pose_order(scores);
    if (!candidates.empty())
        calibration.candidate = candidates[calibration.decision.best];

    return calibration;
}

PLUGIN_NAMESPACE_END
