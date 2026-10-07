//
// Created by AmazingBuff on 2026/10/6.
//

#include "character_manager.h"

PLUGIN_NAMESPACE_BEGIN

CharacterManager& CharacterManager::instance()
{
    static CharacterManager s_instance;
    return s_instance;
}

void CharacterManager::create_clones(const std::vector<RE::Actor*>& actors, const RE::NiPointer<RE::NiNode>& node)
{
    if (!actors.empty() || !m_generated_clones.empty())
    {
        bool expected = false;
        if (m_step_queue.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            SKSE::GetTaskInterface()->AddTask([this, actors, node]()
            {
                std::vector<RE::Actor*> erased_actors;
                erased_actors.reserve(m_generated_clones.size());
                for (RE::Actor* actor : m_generated_clones)
                {
                    const auto clone = m_character_clones.find(actor);
                    if (clone == m_character_clones.end())
                    {
                        erased_actors.push_back(actor);
                        continue;
                    }

                    if (actor && actor->Get3D() && clone->second->attach_graph(node))
                        erased_actors.push_back(actor);
                }
                
                for (RE::Actor* actor : erased_actors)
                    m_generated_clones.erase(actor);

                // for next frame
                for (RE::Actor* actor : actors)
                {
                    if (actor && actor->Get3D() && !m_character_clones.contains(actor))
                    {
                        std::shared_ptr<CharacterClone> clone = std::make_shared<CharacterClone>(actor);
                        m_generated_clones.emplace(actor);
                        m_character_clones.emplace(actor, clone);
                    }
                }

                m_step_queue.store(false, std::memory_order_release);
            });
        }
    }
}

std::shared_ptr<CharacterClone> CharacterManager::get_clone(RE::Actor* actor)
{
    auto it = m_character_clones.find(actor);
    if (it != m_character_clones.end())
        return it->second;
    return nullptr;
}

void CharacterManager::clear_clones()
{
    SKSE::GetTaskInterface()->AddTask([this]()
    {
        m_character_clones.clear();
        m_generated_clones.clear();
    });
}

CharacterManager::CharacterManager() : m_step_queue(false)
{

}

CharacterManager::~CharacterManager()
{

}



PLUGIN_NAMESPACE_END
