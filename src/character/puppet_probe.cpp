//
// Created by AmazingBuff on 2026/10/11.
//

#include "character/puppet_probe.h"

#if CHARACTER_PANEL_S2_PROBE

#include "character/animation_source.h"

#include "RE/A/Actor.h"
#include "RE/A/ActorEquipManager.h"
#include "RE/B/BSAnimationUpdateData.h"
#include "RE/B/BShkbAnimationGraph.h"
#include "RE/B/BSFadeNode.h"
#include "RE/I/IAnimationGraphManagerHolder.h"
#include "RE/P/PlayerCharacter.h"
#include "RE/T/TESObjectREFR.h"

#include <algorithm>
#include <cmath>

// winnt.h defines macros of these names, which would rewrite the REX constants used below as
// `REX::W32::0x1000`; every Windows header this file needs is already parsed at this point.
#undef MEM_COMMIT
#undef PAGE_NOACCESS

PLUGIN_NAMESPACE_BEGIN

namespace
{
    constexpr std::uint32_t Grace_Frames = 240;            // full assembly window at the player
    constexpr std::uint32_t Digest_Interval_Ticks = 30;    // one clip digest per ~half second
    constexpr float Max_Manual_Dt = 0.1f;                  // clamp: a stalled frame must not jump the graph

    // The flash guard, same recipe as character_clone.cpp: the fade node is pinned near-invisible
    // (not zero - a zeroed BSFadeNode is skipped by the culler and the skin would never bind)
    // for the whole probe, because the puppet stands at the player the entire session.
    void suppress_fade(RE::NiAVObject* graph)
    {
        if (RE::BSFadeNode* const fade = graph ? graph->AsFadeNode() : nullptr)
        {
            fade->unk128 = 0.05f;
            fade->unk12C = 0.0f;
            fade->currentFade = 0.05f;
        }
    }

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

    // The clip digest line: name, class, registered index and the generator's own clock. The
    // localTime here is the engine's per-clip state; whether it moves under OUR dt is question 2.
    std::string clip_digest(const ActiveClipEntry& entry)
    {
        float local_time = -1.0f;
        float speed = 0.0f;
        std::uint16_t binding_index = 0;
        if (readable(entry.generator, sizeof(RE::hkbClipGenerator)))
        {
            local_time = entry.generator->localTime;
            speed = entry.generator->playbackSpeed;
            binding_index = entry.generator->animationBindingIndex;
        }
        char const* const kind = entry.kind == ClipClass::e_idle ? "idle" : (entry.kind == ClipClass::e_equip ? "equip" : "other");
        return fmt::format("{}#{}({} t={:.2f}s v={:.2f})", entry.name, binding_index, kind, local_time, speed);
    }

    bool looks_replaced(std::string_view name)
    {
        // OAR replacement paths are long, carry OAR's own folder depth and mostly live under
        // its meshes root; vanilla engine names are short "Animations\..." paths. The probe only
        // OBSERVES which shape the running names take - the A/B stage is what proves sampling.
        return name.size() > 64 && name.find("OAR") != std::string_view::npos;
    }
}

PuppetProbe& PuppetProbe::instance()
{
    static PuppetProbe s_instance;
    return s_instance;
}

void PuppetProbe::request_advance()
{
    m_requested.fetch_add(1, std::memory_order_release);
}

void PuppetProbe::tick()
{
    if (m_requested.exchange(0, std::memory_order_acq_rel) != 0)
    {
        switch (m_stage)
        {
        case Stage::e_off:
            spawn();
            break;
        case Stage::e_alive:
            start_manual_tick();
            break;
        case Stage::e_manual:
            equip_test();
            break;
        case Stage::e_equipped:
            sample_ab();
            break;
        case Stage::e_sampled:
        case Stage::e_dead:
            kill("f6-final-press");
            break;
        }
    }

    RE::Actor* const puppet = actor();
    if (!puppet)
    {
        // A load boundary may have reclaimed the puppet mid-probe; fail soft and reset.
        if (m_stage != Stage::e_off && m_stage != Stage::e_dead)
        {
            logger::warn("SCOPY PUPPET reset reason=puppet-lost (load boundary or engine cleanup); press F6 to start over");
            m_stage = Stage::e_off;
            m_puppet = nullptr;
            m_puppet_form_id = 0;
        }
        return;
    }

    RE::UI* const ui = RE::UI::GetSingleton();
    const bool paused = ui && ui->GameIsPaused();
    const bool manual_wanted = m_stage >= Stage::e_manual;
    const bool grace_left = m_grace_frames < Grace_Frames;

    // While the game runs the engine ticks the puppet itself; the digests then show whether the
    // parked-at-player puppet animates at all (question 1's second half). While the panel pauses
    // the world, OUR clock takes over - that is question 2.
    if (manual_wanted && paused)
    {
        const auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - m_last_tick).count();
        m_last_tick = now;
        dt = std::clamp(dt, 0.0f, Max_Manual_Dt);

        RE::BSAnimationUpdateData update{};
        update.deltaTime = dt;
        update.refr = puppet;
        puppet->UpdateAnimationGraphManager(update);
        ++m_ticks;
        if (!grace_left)
            suppress_fade(puppet->Get3D(false));
        if (m_ticks % Digest_Interval_Ticks == 0)
        {
            ++m_digests;
            logger::info("SCOPY PUPPET digest n={} manual-ticks={} paused={} dt={:.4f} clips={}",
                m_digests, m_ticks, paused ? 1 : 0, dt, log_clips("manual"));
        }
    }
    else
    {
        // Unpaused (or pre-manual): the engine's own update drives the puppet. Suppress the fade
        // during grace so the shell never shows, then keep it pinned while it stands here.
        ++m_grace_frames;
        suppress_fade(puppet->Get3D(false));
        if (m_grace_frames == Grace_Frames)
            logger::info("SCOPY PUPPET grace complete frames={} (assembly window over; shell stays pinned at the player)", Grace_Frames);
        if (m_grace_frames % (Digest_Interval_Ticks * 4) == 0)
        {
            ++m_digests;
            logger::info("SCOPY PUPPET digest n={} engine-phase paused={} clips={}", m_digests, paused ? 1 : 0, log_clips("engine"));
        }
    }
}

void PuppetProbe::spawn()
{
    RE::PlayerCharacter* const player = RE::PlayerCharacter::GetSingleton();
    if (!player)
    {
        logger::warn("SCOPY PUPPET spawn UNAVAILABLE reason=no-player");
        return;
    }
    RE::TESNPC* const source_base = player->GetActorBase();
    RE::TESForm* const duplicate = source_base ? source_base->CreateDuplicateForm(false, nullptr) : nullptr;
    RE::TESNPC* const puppet_base = duplicate ? duplicate->As<RE::TESNPC>() : nullptr;
    if (!puppet_base)
    {
        logger::warn("SCOPY PUPPET spawn UNAVAILABLE reason=duplicate-form-failed");
        return;
    }
    puppet_base->fullName = "CharacterPanel Puppet";
    puppet_base->defaultOutfit = nullptr;

    // v6.70 recipe verbatim: born AT the player (full-LOD assembly; run 104 proved born-at-depth
    // strips the skeleton), fade pinned near-invisible, the pin flags keep it inert while the
    // probe uses it. The probe never parks it - parking is a product-round question.
    const RE::NiPointer<RE::TESObjectREFR> placed = player->PlaceObjectAtMe(puppet_base, false);
    RE::Actor* const puppet = placed ? placed->As<RE::Actor>() : nullptr;
    if (!puppet)
    {
        logger::warn("SCOPY PUPPET spawn UNAVAILABLE reason=place-failed");
        return;
    }
    puppet->GetActorRuntimeData().boolFlags.set(RE::Actor::BOOL_FLAGS::kMovementBlocked);
    puppet->GetActorRuntimeData().boolFlags.set(RE::Actor::BOOL_FLAGS::kAttackingDisabled);
    puppet->GetActorRuntimeData().boolFlags.set(RE::Actor::BOOL_FLAGS::kCastingDisabled);
    puppet->SetActivationBlocked(true);
    puppet->StopCombat();

    m_puppet = placed;
    m_puppet_form_id = puppet->GetFormID();
    m_grace_frames = 0;
    m_ticks = 0;
    m_digests = 0;
    m_replacements_seen.clear();
    m_ab_verdict = "not-taken";
    m_stage = Stage::e_alive;
    logger::info("SCOPY PUPPET stage=alive form={:08x} base={:08x} (question 1: watch the engine-phase digests for clips)",
        m_puppet_form_id, source_base ? source_base->GetFormID() : 0);
}

void PuppetProbe::start_manual_tick()
{
    RE::Actor* const puppet = actor();
    if (!puppet)
        return;
    m_stage = Stage::e_manual;
    m_last_tick = std::chrono::steady_clock::now();
    m_ticks = 0;
    logger::info("SCOPY PUPPET stage=manual (question 2: open the panel; digests now tick on OUR clock while paused)");
}

void PuppetProbe::equip_test()
{
    RE::Actor* const puppet = actor();
    RE::PlayerCharacter* const player = RE::PlayerCharacter::GetSingleton();
    if (!puppet || !player)
    {
        logger::warn("SCOPY PUPPET equip UNAVAILABLE reason=subject-lost");
        return;
    }
    // Mirror the player's worn state onto the puppet (ActorEquipManager::EquipObject, the recipe
    // the retired Actor route validated): body slots for the OAR conditions, then both hands -
    // the right hand is the equip-clip trigger under test.
    RE::ActorEquipManager* const equip_manager = RE::ActorEquipManager::GetSingleton();
    RE::InventoryChanges* const changes = player->GetInventoryChanges(true);
    if (!equip_manager || !changes || !changes->entryList)
    {
        logger::warn("SCOPY PUPPET equip UNAVAILABLE reason=no-equip-manager");
        return;
    }
    std::uint32_t equipped = 0;
    for (const RE::InventoryEntryData* const entry : *changes->entryList)
    {
        if (!entry || !entry->IsWorn() || !entry->object)
            continue;
        puppet->AddObjectToContainer(entry->object, nullptr, 1, nullptr);
        equip_manager->EquipObject(puppet, entry->object, nullptr, 1, nullptr, false, true, false, true);
        ++equipped;
    }
    RE::InventoryEntryData* const hands = player->GetEquippedEntryData(false);
    RE::InventoryEntryData* const left = player->GetEquippedEntryData(true);
    for (const RE::InventoryEntryData* const entry : { hands, left })
    {
        if (!entry || !entry->object)
            continue;
        puppet->AddObjectToContainer(entry->object, nullptr, 1, nullptr);
        equip_manager->EquipObject(puppet, entry->object, nullptr, 1, nullptr, false, true, false, true);
        ++equipped;
    }
    m_stage = Stage::e_equipped;
    logger::info("SCOPY PUPPET stage=equipped items={} (question 4: equip clips should activate in the paused panel; digests watch for them)", equipped);
}

void PuppetProbe::sample_ab()
{
    RE::Actor* const puppet = actor();
    if (!puppet)
    {
        logger::warn("SCOPY PUPPET ab UNAVAILABLE reason=subject-lost");
        return;
    }
    RE::NiAVObject* const root = puppet->Get3D(false);
    if (!root)
    {
        logger::warn("SCOPY PUPPET ab UNAVAILABLE reason=no-3d");
        return;
    }
    // The HKX28 chain, parameterized by the puppet: select graph -> resolve the running clip's
    // engine-loaded binding -> SamplePartialTracks -> sweep against the puppet's own poseLocal.
    // The verdict lines land in the SCOPY ANIM play ground-truth block; this stage wraps them.
    logger::info("SCOPY PUPPET ab begin form={:08x} (question 5: SCOPY ANIM play ground-truth verdict below is the answer)", m_puppet_form_id);
    const RE::hkaAnimation* instance_animation = nullptr;
    const RE::hkaAnimationBinding* loaded_binding = nullptr;
    float played_local_time = -1.0f;
    report_animation_instance(*puppet, *root, *root, instance_animation, loaded_binding, played_local_time);
    ClipPlayback playback;
    if (prepare_animation_clip(*puppet, *root, *root, playback, instance_animation, loaded_binding, played_local_time))
        m_ab_verdict = playback.valid() ? "see-ground-truth-lines" : "no-clip-resolved";
    else
        m_ab_verdict = "prepare-failed";
    m_stage = Stage::e_sampled;
    logger::info("SCOPY PUPPET ab end verdict={} driver={}", m_ab_verdict, playback.valid() ? playback.name : std::string("none"));
}

void PuppetProbe::kill(char const* reason)
{
    RE::Actor* const puppet = actor();
    if (puppet)
    {
        puppet->StopCombat();
        puppet->SetDelete(true);
    }
    m_puppet = nullptr;
    m_puppet_form_id = 0;
    m_stage = Stage::e_off;
    std::string replacements;
    for (const std::string& name : m_replacements_seen)
        replacements += name + "; ";
    logger::info("SCOPY PUPPET stage=off reason={} ticks={} digests={} ab-verdict='{}' oar-shaped-names=[{}]",
        reason, m_ticks, m_digests, m_ab_verdict, replacements);
}

RE::Actor* PuppetProbe::actor() const
{
    if (!m_puppet || m_stage == Stage::e_off || m_stage == Stage::e_dead)
        return nullptr;
    RE::Actor* const puppet = m_puppet->As<RE::Actor>();
    return puppet && readable(puppet, sizeof(RE::Actor)) ? puppet : nullptr;
}

std::string PuppetProbe::log_clips(char const* phase)
{
    (void)phase;
    RE::Actor* const puppet = actor();
    if (!puppet)
        return "0 [(subject-lost)]";
    std::vector<ActiveClipEntry> clips;
    collect_running_clip_generators(*puppet, clips);
    std::string line;
    for (size_t i = 0; i < clips.size() && i < 8; ++i)
    {
        line += fmt::format("{}{}", line.empty() ? "" : "; ", clip_digest(clips[i]));
        if (looks_replaced(clips[i].name) &&
            std::ranges::find(m_replacements_seen, clips[i].name) == m_replacements_seen.end())
            m_replacements_seen.push_back(clips[i].name);
    }
    if (clips.size() > 8)
        line += fmt::format("; +{} more", clips.size() - 8);
    return fmt::format("{} [{}]", clips.size(), line);
}

PLUGIN_NAMESPACE_END

#endif
