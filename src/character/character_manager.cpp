//
// Created by AmazingBuff on 2026/10/6.
//
#include "character/character_manager.h"
#include "panel/panel_monitor.h"
#include "oar/query_policy.h"
#include "RE/S/ScriptEventSourceHolder.h"
PLUGIN_NAMESPACE_BEGIN
// Native resources are released only by serialized lifetime updates, never CRT teardown.
CharacterManager& CharacterManager::instance() { static CharacterManager* s_instance = new CharacterManager; return *s_instance; }

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

void CharacterManager::register_actors(std::vector<RE::Actor*> const& actors)
{
    for (RE::Actor* actor : actors)
    {
        if (!actor) continue;
        const RE::FormID id = actor->GetFormID();
        std::lock_guard lock(m_registry_mutex);
        if (!m_scene_graphs.contains(id))
        {
            m_scene_graphs.emplace(id, std::make_shared<SceneGraph>(*actor));
            std::lock_guard event_lock(m_events_mutex);
            m_revisions.try_emplace(id, 0);
            logger::info("CharacterManager watch actor={:08x}", id);
        }
    }
}

std::shared_ptr<SceneGraph> CharacterManager::get_scene_graph(RE::Actor* actor)
{
    if (!actor) return nullptr;
    std::lock_guard lock(m_registry_mutex);
    const auto found = m_scene_graphs.find(actor->GetFormID());
    return found != m_scene_graphs.end() ? found->second : nullptr;
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
    if (m_clear_requested.exchange(false, std::memory_order_acq_rel)) release_previews();
    RE::UI* const ui = RE::UI::GetSingleton();
    if (ui && ui->IsMenuOpen(RE::MainMenu::MENU_NAME))
    {
        if (!m_scene_graphs.empty()) release_previews();
        return;
    }
    if (ui && ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) return;
    register_actors({ RE::PlayerCharacter::GetSingleton() });
    const bool open = PanelMonitor::instance().is_menu_open();
    const std::uint64_t menu_revision = PanelMonitor::instance().revision();
    const bool opening = open && (!m_was_open || menu_revision != m_menu_revision);
    const bool usable = open && paused && ui && !ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME);
    const PreviewFrame frame{ paused, open, opening, m_was_open && !open, usable };
    std::vector<std::pair<RE::FormID, std::shared_ptr<SceneGraph>>> previews;
    {
        std::lock_guard lock(m_registry_mutex);
        for (auto const& entry : m_scene_graphs) previews.push_back(entry);
    }
    for (auto const& [id, graph] : previews)
    {
        std::uint64_t revision = 0;
        {
            std::lock_guard lock(m_events_mutex);
            if (auto found = m_revisions.find(id); found != m_revisions.end()) revision = found->second;
        }
        if (!graph->update(frame, revision))
        {
            std::lock_guard lock(m_registry_mutex);
            std::lock_guard event_lock(m_events_mutex);
            m_scene_graphs.erase(id);
            m_revisions.erase(id);
        }
    }
    m_was_open = open;
    m_menu_revision = menu_revision;
}

void CharacterManager::clear_previews()
{
    m_clear_requested.store(true, std::memory_order_release);
}

void CharacterManager::release_previews()
{
    std::lock_guard lock(m_registry_mutex);
    for (auto const& [id, graph] : m_scene_graphs)
    {
        (void)graph;
        logger::info("CharacterManager lifetime boundary actor={:08x}", id);
    }
    m_scene_graphs.clear();
    {
        std::lock_guard event_lock(m_events_mutex);
        m_revisions.clear();
    }
    oar::QuerySession::invalidate_projects();
    m_was_open = false;
    m_menu_revision = 0;
}
PLUGIN_NAMESPACE_END
