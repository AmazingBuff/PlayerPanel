//
// Created by AmazingBuff on 2026/10/11.
//

#pragma once

#include "character/animation_source.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

PLUGIN_NAMESPACE_BEGIN

// One session-owned Actor supplies graph decisions; the renderer still draws a detached copy.
// Birth requires the running world. Paused stepping uses panel time and never advances other Actors.
class PuppetDriver
{
public:
    static PuppetDriver& instance();

    void request_spawn();
    void request_release();

    void release(char const* reason);

    void tick(bool paused);

    std::string drive(RE::NiAVObject& copy_root);


    [[nodiscard]] static bool is_owned_form(std::uint32_t form_id);

    [[nodiscard]] RE::Actor* actor() const;
    [[nodiscard]] bool tick_enabled() const { return m_tick_enabled.load(std::memory_order_acquire); }

    void set_tick_enabled(bool a_enabled);
    void request_step();

    void set_force_generate(bool a_enabled);
    [[nodiscard]] bool force_generate() const { return m_force_generate.load(std::memory_order_acquire); }
    [[nodiscard]] std::uint32_t manual_ticks() const { return m_manual_ticks; }

private:
    PuppetDriver() = default;

    void spawn();
    void park(char const* why);

    void poll_equipment(RE::Actor& puppet);

    std::atomic_bool m_spawn_requested{ false };
    std::atomic_bool m_release_requested{ false };

    std::atomic_bool m_tick_enabled{ true };

    std::atomic_bool m_step_requested{ false };

    std::atomic_bool m_force_generate{ false };
    RE::NiPointer<RE::TESObjectREFR> m_puppet;
    std::uint32_t m_puppet_form_id = 0;
    std::uint32_t m_grace_frames = 0;
    std::uint32_t m_repark_frames = 0;
    std::uint32_t m_manual_ticks = 0;

    bool m_data_logged = false;
    std::uint32_t m_stall_frames = 0;
    bool m_stall_logged = false;

    std::uint32_t m_right_form = 0;
    std::uint32_t m_left_form = 0;
    std::uint32_t m_equip_reactions = 0;
    std::chrono::steady_clock::time_point m_last_tick{};

    RE::hkbClipGenerator const* m_current_generator = nullptr;
    ClipPlayback m_playback;
    std::string m_mapping;
};

PLUGIN_NAMESPACE_END
