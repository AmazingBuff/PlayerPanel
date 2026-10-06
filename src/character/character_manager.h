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

    // a clone need two frames to get graph and apply to ui
    void create_clones(const std::vector<RE::Actor*>& actors, const RE::NiPointer<RE::NiNode>& node);

    std::shared_ptr<CharacterClone> get_clone(RE::Actor* actor);

    void clear_clones();
private:
    CharacterManager();
    ~CharacterManager();

private:
    std::unordered_map<RE::Actor*, std::shared_ptr<CharacterClone>> m_character_clones;
    std::unordered_set<RE::Actor*> m_generated_clones;
    std::atomic_bool m_step_queue;
};

PLUGIN_NAMESPACE_END