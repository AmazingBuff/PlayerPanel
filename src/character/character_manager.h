//
// Created by AmazingBuff on 2026/10/6.
//
#pragma once
#include "character/character_clone.h"
#include "RE/T/TESEquipEvent.h"
PLUGIN_NAMESPACE_BEGIN
class CharacterManager
{
public:
    static CharacterManager& instance();
    void install();
    void create_clones(std::vector<RE::Actor*> const& actors);
    std::shared_ptr<CharacterClone> get_clone(RE::Actor* actor);
    [[nodiscard]] std::uint64_t equipment_revision(RE::Actor const& actor) const;
    void update(bool paused);
    void clear_clones();
private:
    class EquipSink final : public RE::BSTEventSink<RE::TESEquipEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(RE::TESEquipEvent const*,
            RE::BSTEventSource<RE::TESEquipEvent>*) noexcept override;
    };
    void release_clones();
    std::atomic_bool m_clear_requested{ false };
    CharacterManager() = default;
    ~CharacterManager() = default;
    std::unordered_map<RE::FormID, std::shared_ptr<CharacterClone>> m_character_clones;
    mutable std::mutex m_clones_mutex;
    mutable std::mutex m_events_mutex;
    std::unordered_map<RE::FormID, std::uint64_t> m_revisions;
    EquipSink m_equip_sink;
    bool m_installed = false;
    bool m_was_open = false;
    std::uint64_t m_menu_revision = 0;
};
PLUGIN_NAMESPACE_END
