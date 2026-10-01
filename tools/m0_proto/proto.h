//
// Created by AmazingBuff on 2026/09/28.
//

#pragma once

#include <atomic>
#include <cstdint>

namespace CharacterPanelProto
{
    [[nodiscard]] bool supported_runtime() noexcept;

    class Proto final
    {
    public:
        static Proto& instance();

        // kDataLoaded, gated on the exact AE 1.6.1170 runtime.
        void install();

        // F6 one-shot: the next DrawInterfaceStart call is bracketed for pass
        // redirection. While the bracket is open, the three
        // RenderPassImmediately call-site hooks log every pass and replay
        // menu-scene BSLightingShader passes into a private offscreen target;
        // the original calls still run, so the visible menu frame is
        // untouched. The target is read back to a TGA when the bracket
        // closes.
        void arm();

        // Render thread; consumes the armed flag for exactly one frame.
        bool take_armed();

        // Installs the DrawInterfaceStart call-site hook (private trampoline).
        bool install_hook();

        // Installs the three RenderPassImmediately call-site hooks (private
        // trampoline).
        bool install_pass_hooks();

    private:
        Proto() = default;

        bool m_installed{ false };
        std::atomic<bool> m_armed{ false };
    };
}
