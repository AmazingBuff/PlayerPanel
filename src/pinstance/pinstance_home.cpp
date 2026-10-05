//
// P's menu-scene home: whitelist arming and the stage-2b graph relocation
// under CP_StudioHome (attach/relocate/release/verify). (2026-10-05 src migration: split out of tools/m0_proto, behavior byte-identical)
//

#include "pinstance/pinstance.h"

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // Stage-2b: the dedicated home node under menuObjects[0] — the light
    // rig's host, which has kept our foreign subtree alive across every
    // menu open/close since v6.47 (runs 81-93, U2's optimistic evidence).
    constexpr std::string_view Home_Node_Name = "CP_StudioHome";
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
PLUGIN_NAMESPACE_END
