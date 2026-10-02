//
// Created by AmazingBuff on 2026/09/28.
//

#pragma once

#include <atomic>
#include <cstdint>
#include <string_view>

namespace CharacterPanelProto
{
    [[nodiscard]] bool supported_runtime() noexcept;

    class Proto final
    {
    public:
        static Proto& instance();

        // kDataLoaded, gated on the exact AE 1.6.1170 runtime.
        void install();

        // F7 (F6 is bound in the user's game): toggle the panel. While open,
        // EVERY DrawInterfaceStart frame is bracketed — menu-scene
        // BSLightingShader passes are replayed into the persistent studio
        // target AFTER the original call (v2.5 order), so the visible frame
        // is untouched and the target stays current every menu frame.
        void toggle_panel();

        // F8: one-shot debug readback of the studio target to a TGA
        // (synchronous GPU readback — one frame hitch, not part of the
        // steady-state path). Only honored while the panel is open; consumed
        // by the render thread when the current bracket closes.
        void request_dump();

        // Session invalidation (FR-06): loading a save or starting a new game
        // closes the panel. The render thread releases the studio target at
        // its next non-bracketed DrawInterfaceStart.
        void close_panel(std::string_view reason);

        // Stage-2 toggle hook: the panel toggle path also drives the
        // independent display instance P (spawn on open, despawn on close).
        void toggle_p_instance();

        // Render thread: whether this DrawInterfaceStart frame is bracketed.
        bool panel_frame_active();

        // Render thread: consume a pending F8 dump request.
        bool take_dump();

        // Render thread: consume a pending studio-target release request.
        bool take_release_pending();

        // Render thread: consume a pending dump-at-close request (v3.1: one
        // evidence TGA is written from the studio target just before the
        // release destroys it — runs 17/18 showed the dump key is naturally
        // pressed AFTER closing, so closing itself must produce the
        // evidence).
        bool take_dump_on_close();

        // Monotonic per-open counter; lets the render thread detect a fresh
        // panel open (counter/throttle reset, failed-creation retry).
        std::uint32_t panel_generation();

        // Installs the DrawInterfaceStart call-site hook (private trampoline).
        bool install_hook();

        // Installs the three RenderPassImmediately call-site hooks (private
        // trampoline).
        bool install_pass_hooks();

    private:
        Proto() = default;

        bool m_installed{ false };
        bool m_capture_ready{ false };
        std::atomic<bool> m_panel_open{ false };
        std::atomic<bool> m_release_pending{ false };
        std::atomic<bool> m_dump_requested{ false };
        std::atomic<bool> m_dump_on_close{ false };
        std::atomic<std::uint32_t> m_panel_generation{ 0 };
    };
}
