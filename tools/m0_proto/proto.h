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

        // F6 one-shot: the next DrawInterfaceStart call runs with the UI3D
        // secondary accumulator swapped in as the engine's current
        // accumulator, then restores and dumps the offscreen result.
        void arm();

        // Render thread; consumes the armed flag for exactly one frame.
        bool take_armed();

        // Installs the DrawInterfaceStart call-site hook (private trampoline).
        bool install_hook();

    private:
        Proto() = default;

        bool m_installed{ false };
        std::atomic<bool> m_armed{ false };
    };
}
