//
// Created by AmazingBuff on 2026/09/21.
//

#include "frame_hook.h"

#include "present_hook.h"

PLUGIN_NAMESPACE_BEGIN

FrameHook& FrameHook::instance()
{
    static FrameHook s_instance;
    return s_instance;
}

FrameHook::FrameHook() :
    m_frame_pending(false),
    m_listeners{},
    m_listener_count(0),
    m_present_listeners{},
    m_present_listener_count(0),
    m_installed(false)
{
}

bool FrameHook::install(Listener a_listener)
{
    if (a_listener && !has_listener(a_listener))
    {
        if (m_listener_count < Max_Frame_Listener_Count)
            m_listeners[m_listener_count++] = a_listener;
        else
            logger::warn("Frame listener capacity reached; the listener was not registered");
    }

    return ensure_installed();
}

bool FrameHook::install_present(PresentListener a_listener)
{
    if (a_listener && !has_present_listener(a_listener))
    {
        if (m_present_listener_count < Max_Present_Listener_Count)
            m_present_listeners[m_present_listener_count++] = a_listener;
        else
            logger::warn("Present listener capacity reached; the listener was not registered");
    }

    return ensure_installed();
}

bool FrameHook::ensure_installed()
{
    if (m_installed)
        return true;

    if (!PresentHook::instance().install(&FrameHook::on_present))
    {
        logger::warn("Swap chain is not ready; the frame hook is retried on the next game message");
        return false;
    }

    m_installed = true;
    logger::info("Frame hook installed over the swap chain present");
    return true;
}

// Not noexcept: queueing the tick may throw, and PresentHook::present_thunk already guards this call.
void FrameHook::on_present(REX::W32::IDXGISwapChain* a_swap_chain)
{
    FrameHook& self = instance();

    for (std::uint32_t index = 0; index < self.m_present_listener_count; ++index)
    {
        try
        {
            self.m_present_listeners[index](a_swap_chain);
        }
        catch (...)
        {
            try { logger::error("Present listener failed"); } catch (...) {}
        }
    }

    if (self.m_frame_pending.exchange(true, std::memory_order_acq_rel))
        return;

    SKSE::TaskInterface const* const tasks = SKSE::GetTaskInterface();
    if (!tasks)
    {
        self.m_frame_pending.store(false, std::memory_order_release);
        return;
    }

    tasks->AddTask(&FrameHook::dispatch_frame);
}

void FrameHook::dispatch_frame()
{
    FrameHook& self = instance();
    // Released before the tick so a frame that arrives during long listener work queues the next one.
    self.m_frame_pending.store(false, std::memory_order_release);
    self.tick();
}

void FrameHook::tick()
{
    for (std::uint32_t index = 0; index < m_listener_count; ++index)
    {
        try
        {
            m_listeners[index]();
        }
        catch (...)
        {
            try { logger::error("Frame listener failed"); } catch (...) {}
        }
    }
}

bool FrameHook::has_listener(Listener a_listener) const
{
    for (std::uint32_t index = 0; index < m_listener_count; ++index)
    {
        if (m_listeners[index] == a_listener)
            return true;
    }
    return false;
}

bool FrameHook::has_present_listener(PresentListener a_listener) const
{
    for (std::uint32_t index = 0; index < m_present_listener_count; ++index)
    {
        if (m_present_listeners[index] == a_listener)
            return true;
    }
    return false;
}

PLUGIN_NAMESPACE_END
