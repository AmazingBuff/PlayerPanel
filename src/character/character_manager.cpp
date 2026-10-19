//
// Created by AmazingBuff on 2026/10/6.
//
#include "character/character_manager.h"
#include "character/equipment_mirror.h"
#include "character/figure_audit.h"
#include "panel/panel_monitor.h"
#include "oar/query_policy.h"
#include "RE/S/ScriptEventSourceHolder.h"
#if CHARACTER_PANEL_SCENE_COPY_EXPERIMENT
#include "character/scene_graph_copy.h"
#endif
PLUGIN_NAMESPACE_BEGIN
CharacterManager& CharacterManager::instance() { static CharacterManager s_instance; return s_instance; }

void CharacterManager::install()
{
    if (!m_installed)
        if (RE::ScriptEventSourceHolder* events = RE::ScriptEventSourceHolder::GetSingleton())
        {
            events->AddEventSink(&m_equip_sink);
            m_installed = true;
            logger::info("CharacterManager equipment sink installed");
        }
}

void CharacterManager::create_clones(std::vector<RE::Actor*> const& actors)
{
    for (RE::Actor* actor : actors)
    {
        if (!actor || !actor->Get3D(false)) continue;
        const RE::FormID id = actor->GetFormID();
        std::lock_guard lock(m_clones_mutex);
        if (!m_character_clones.contains(id))
        {
            m_character_clones.emplace(id, std::make_shared<CharacterClone>(actor, CHARACTER_PANEL_SCENE_COPY_EXPERIMENT != 0));
            std::lock_guard event_lock(m_events_mutex);
            m_revisions.try_emplace(id, 0);
            logger::info("CharacterManager watch actor={:08x}", id);
        }
    }
}

std::shared_ptr<CharacterClone> CharacterManager::get_clone(RE::Actor* actor)
{
    if (!actor) return nullptr;
    std::lock_guard lock(m_clones_mutex);
    const auto found = m_character_clones.find(actor->GetFormID());
    return found != m_character_clones.end() ? found->second : nullptr;
}

std::uint64_t CharacterManager::equipment_revision(RE::Actor const& actor) const
{
    std::lock_guard lock(m_events_mutex);
    const auto found = m_revisions.find(actor.GetFormID());
    return found != m_revisions.end() ? found->second : 0;
}

RE::BSEventNotifyControl CharacterManager::EquipSink::ProcessEvent(RE::TESEquipEvent const* event,
    RE::BSTEventSource<RE::TESEquipEvent>*) noexcept
{
    if (event && event->actor)
    {
        CharacterManager& manager = instance();
        std::lock_guard lock(manager.m_events_mutex);
        const RE::FormID id = event->actor->GetFormID();
        if (auto found = manager.m_revisions.find(id); found != manager.m_revisions.end())
        {
            ++found->second;
            logger::info("CharacterManager equipment actor={:08x} revision={} equipped={}", id, found->second, event->equipped);
        }
    }
    return RE::BSEventNotifyControl::kContinue;
}

void CharacterManager::update(bool paused)
{
    if (m_clear_requested.exchange(false, std::memory_order_acq_rel)) release_clones();
    RE::UI* const ui = RE::UI::GetSingleton();
    if (ui && ui->IsMenuOpen(RE::MainMenu::MENU_NAME))
    {
        if (!m_character_clones.empty()) release_clones();
        return;
    }
    create_clones({ RE::PlayerCharacter::GetSingleton() });
    const bool open = PanelMonitor::instance().is_menu_open();
    const std::uint64_t menu_revision = PanelMonitor::instance().revision();
    const bool opening = open && (!m_was_open || menu_revision != m_menu_revision);
    const bool usable = open && paused && ui && !ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME);
    std::vector<std::shared_ptr<CharacterClone>> clones;
    {
        std::lock_guard lock(m_clones_mutex);
        for (auto const& [id, clone] : m_character_clones) { (void)id; clones.push_back(clone); }
    }
#if CHARACTER_PANEL_SCENE_COPY_EXPERIMENT
    SceneGraphCopy& copier = SceneGraphCopy::instance();
    copier.process_boundary(usable);
    const auto command = copier.take_command();
#endif
    for (auto const& clone : clones)
    {
        auto const& monitor = clone->monitor();
        const auto source = monitor.source.get();
        if (!source)
        {
            clone->close_session();
            {
                std::lock_guard lock(m_clones_mutex);
                std::lock_guard event_lock(m_events_mutex);
                std::erase_if(m_character_clones, [&](auto const& entry)
                {
                    if (entry.second != clone) return false;
                    m_revisions.erase(entry.first);
                    return true;
                });
            }
            continue;
        }
        if (!source->Get3D(false))
        {
            clone->close_session();
            (void)clone->update_source_graph(*source);
            continue;
        }
        const std::uint64_t revision = equipment_revision(*source);
        clone->update_monitor(*source, revision, paused, open, opening, m_was_open && !open, usable);
        if (!usable) continue;
#if CHARACTER_PANEL_SCENE_COPY_EXPERIMENT
        const bool manual_capture = source->IsPlayerRef() && command == SceneGraphCopy::Command::e_capture;
        if (source->IsPlayerRef() && command == SceneGraphCopy::Command::e_toggle_draw) clone->toggle_draw();
        if (clone->capture_due(*source, revision, manual_capture))
        {
            RE::NiPointer<RE::NiAVObject> graph = copier.capture(*source);
            const bool accepted = graph != nullptr;
            if (accepted) clone->replace_graph(std::move(graph));
            clone->capture_result(accepted, opening);
        }
#endif
        if (clone->graph()) clone->animation_controller().retarget(*clone->graph());
        clone->animation_controller().poll();
    }
    m_was_open = open;
    m_menu_revision = menu_revision;
}

void CharacterManager::clear_clones()
{
    m_clear_requested.store(true, std::memory_order_release);
}

void CharacterManager::release_clones()
{
    std::lock_guard lock(m_clones_mutex);
    for (auto const& [id, clone] : m_character_clones)
    {
        logger::info("CharacterManager lifetime boundary actor={:08x}", id);
        clone->close_session();
    }
    m_character_clones.clear();
    {
        std::lock_guard event_lock(m_events_mutex);
        m_revisions.clear();
    }
    oar::QuerySession::invalidate_projects();
    m_was_open = false;
    m_menu_revision = 0;
}
PLUGIN_NAMESPACE_END
