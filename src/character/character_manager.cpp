//
// Created by AmazingBuff on 2026/10/6.
//

#include "character_manager.h"

#include "REX/W32/KERNEL32.h"

PLUGIN_NAMESPACE_BEGIN

CharacterManager& CharacterManager::instance()
{
    static CharacterManager s_instance;
    return s_instance;
}

void CharacterManager::create_clones(const std::vector<RE::Actor*>& actors)
{
    // Spawning runs synchronously in the render bracket (the call site):
    // the SKSE task queue was proven to flush on a BSJobs worker thread
    // (log 2026-10-07 21:05: task thread 18840 vs render bracket 20412 vs
    // main 7676), and reference creation on a worker thread is the race the
    // 2026-10-07 18:01 crash grew from. Spawn+dress in the render bracket
    // went through 100+ m0 rounds unharmed.
    static bool s_thread_probed = false;
    if (!s_thread_probed)
    {
        s_thread_probed = true;
        logger::info("Clone spawn runs on render-bracket thread {}", REX::W32::GetCurrentThreadId());
    }

    for (RE::Actor* actor : actors)
    {
        if (actor && actor->Get3D())
        {
            std::lock_guard<std::mutex> lock(m_clones_mutex);
            if (!m_character_clones.contains(actor))
            {
                std::shared_ptr<CharacterClone> clone = std::make_shared<CharacterClone>(actor);
                m_character_clones.emplace(actor, clone);
            }
        }
    }
}

std::shared_ptr<CharacterClone> CharacterManager::get_clone(RE::Actor* actor)
{
    std::lock_guard<std::mutex> lock(m_clones_mutex);
    auto it = m_character_clones.find(actor);
    if (it != m_character_clones.end())
        return it->second;
    return nullptr;
}

void CharacterManager::clear_clones()
{
    // Called from the SKSE message handler (main thread): synchronous clear.
    // A detached graph is solely owned by its CharacterClone and freed here;
    // a not-yet-detached shell is engine-managed and reclaimed by the load.
    std::lock_guard<std::mutex> lock(m_clones_mutex);
    m_character_clones.clear();
}

CharacterManager::CharacterManager()
{

}

CharacterManager::~CharacterManager()
{

}




PLUGIN_NAMESPACE_END
