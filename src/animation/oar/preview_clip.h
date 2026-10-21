//
// Created by AmazingBuff on 2026/10/16.
//

#pragma once

#include "RE/H/hkbClipGenerator.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <string>

PLUGIN_NAMESPACE_BEGIN
namespace oar
{
    class PreviewContext
    {
    public:
        PreviewContext(RE::hkbCharacter* character, RE::hkbBehaviorGraph* behavior)
        {
            std::memcpy(m_storage.data(), &character, sizeof(character));
            std::memcpy(m_storage.data() + 8, &behavior, sizeof(behavior));
        }
        [[nodiscard]] RE::hkbContext const& get() const
        {
            return *reinterpret_cast<RE::hkbContext const*>(m_storage.data());
        }
    private:
        alignas(16) std::array<std::byte, sizeof(RE::hkbContext)> m_storage{};
    };

    class PreviewClip
    {
    public:
        PreviewClip() = default;
        PreviewClip(PreviewClip const&) = delete;
        PreviewClip& operator=(PreviewClip const&) = delete;

        void initialize(std::uint16_t index, std::string name, bool loop)
        {
            initialize_native(RE::hkbClipGenerator::VTABLE[0].address(), index, std::move(name), loop);
        }

        void initialize_native(std::uintptr_t vtable, std::uint16_t index, std::string name, bool loop)
        {
            m_name = std::move(name);
            m_storage.fill(std::byte{});
            write(0, vtable);
            write(0x38, m_name.c_str());
            write(0x40, static_cast<std::uint16_t>(0xFFFF));
            write(0x48, m_name.c_str());
            write(0x64, 1.0f);
            write(0x70, index);
            write(0x72, static_cast<std::uint8_t>(loop ? 1 : 0));
            write(0x105, true);
        }

        [[nodiscard]] RE::hkbClipGenerator* get() { return reinterpret_cast<RE::hkbClipGenerator*>(m_storage.data()); }
        [[nodiscard]] std::uintptr_t user_data() const
        {
            std::uintptr_t value;
            std::memcpy(&value, m_storage.data() + 0x30, sizeof(value));
            return value;
        }

    private:
        template <class T>
        void write(std::size_t offset, T value) { std::memcpy(m_storage.data() + offset, &value, sizeof(value)); }

        // Same carrier contract as OAR's FakeClipGenerator: only the loader and audited conditions
        // consume this native layout; it is never registered, activated, updated or sent events.
        alignas(16) std::array<std::byte, sizeof(RE::hkbClipGenerator)> m_storage{};
        std::string m_name;
    };
}
PLUGIN_NAMESPACE_END
