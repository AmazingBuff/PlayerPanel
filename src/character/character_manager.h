//
// Created by AmazingBuff on 2026/10/6.
//

#pragma once

#include "character_clone.h"

PLUGIN_NAMESPACE_BEGIN

class CharacterManager
{
public:
    static CharacterManager& instance();

    // Spawns one clone per actor, once per actor. The graph detach runs
    // separately, inside the render bracket (CharacterClone::detach_graph) —
    // it must not happen on the task thread: the render bracket is the only
    // writer serialized with the world renderer job.
    void create_clones(const std::vector<RE::Actor*>& actors);

    std::shared_ptr<CharacterClone> get_clone(RE::Actor* actor);

    void clear_clones();
private:
    enum class ManagerState : uint8_t
    {
        e_none,
        e_create,
        e_clear,
    };
private:
    CharacterManager();
    ~CharacterManager();

private:
    std::unordered_map<RE::Actor*, std::shared_ptr<CharacterClone>> m_character_clones;
    // Guards the map: written from the render bracket (spawn) and the main
    // thread (clear), read from the render bracket (get_clone).
    mutable std::mutex m_clones_mutex;
};

PLUGIN_NAMESPACE_END
