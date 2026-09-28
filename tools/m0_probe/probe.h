//
// Created by AmazingBuff on 2026/09/28.
//

#pragma once

#include <cstdint>

namespace CharacterPanelProbe
{
    [[nodiscard]] bool supported_runtime() noexcept;

    enum class CaptureKind : std::uint8_t
    {
        kCodeAndMenu,
        kMenuOnly
    };

    class Probe final
    {
    public:
        static Probe& instance();

        void install();
        void capture(CaptureKind kind) noexcept;

    private:
        Probe() = default;

        bool m_installed{ false };
        std::uint32_t m_capture_index{ 0 };
    };
}
