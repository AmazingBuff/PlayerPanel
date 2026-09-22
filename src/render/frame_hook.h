//
// Created by AmazingBuff on 2026/09/21.
//

#pragma once

#include <REX/W32/D3D11.h>
#include <atomic>
#include <cstdint>

PLUGIN_NAMESPACE_BEGIN

// Upper bounds on simultaneously registered listeners; further registration is refused and logged
// instead of growing the arrays. Enough for the modules that need a frame tick today.
inline constexpr std::uint32_t Max_Frame_Listener_Count = 4;
inline constexpr std::uint32_t Max_Present_Listener_Count = 2;

// Turns the swap chain present hook into a frame tick. The present callback runs on a render thread,
// so it invokes the present listeners there directly - that is where D3D work belongs - and then
// marks the frame and queues one task so the game-thread listeners are invoked from SKSE's task
// queue. Installing is idempotent: the present hook is written once and a later install only
// registers its listener.
class FrameHook
{
public:
    using Listener = void (*)();
    using PresentListener = void (*)(REX::W32::IDXGISwapChain*);

    static FrameHook& instance();
    FrameHook(FrameHook const&) = delete;
    FrameHook& operator=(FrameHook const&) = delete;

    // Registers a game-thread listener and ensures the present hook is installed.
    bool install(Listener a_listener);
    // Registers a render-thread listener that runs inside the present callback. It must not block
    // and must return without submitting any draw when it has nothing to draw.
    bool install_present(PresentListener a_listener);

private:
    FrameHook();
    ~FrameHook() = default;

    static void on_present(REX::W32::IDXGISwapChain* a_swap_chain);
    static void dispatch_frame();
    bool ensure_installed();
    [[nodiscard]] bool has_listener(Listener a_listener) const;
    [[nodiscard]] bool has_present_listener(PresentListener a_listener) const;
    void tick();

private:
    std::atomic<bool> m_frame_pending;
    Listener m_listeners[Max_Frame_Listener_Count];
    std::uint32_t m_listener_count;
    PresentListener m_present_listeners[Max_Present_Listener_Count];
    std::uint32_t m_present_listener_count;
    bool m_installed;
};

PLUGIN_NAMESPACE_END
