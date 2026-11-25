//
// C-WORLD v1 (2026-11-25): normal-play Smooth phase sampling.
//
// Contract: docs/diagnostics/world-phase-probe-contract-20261125/README.md
//
// Read-only. Never queries OAR, never calls DAC, never writes a variable or the
// candidate graph. The phase label is a direct mapping of the observed markers,
// not an inference about which OAR variant would win.
//
#include "animation/world_phase_probe.h"

#include "animation/dac/effects.h"
#include "animation/settings.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

PLUGIN_NAMESPACE_BEGIN
namespace animation
{
    namespace
    {
        // One row per 250 ms keeps a long normal-play session bounded while still
        // sampling every idle boundary (the shipped clips run several seconds).
        constexpr auto World_Phase_Sample_Ms = std::chrono::milliseconds(250);
        // Guard rail from the contract's stop rules.
        constexpr std::size_t World_Phase_Max_Rows = 5000;

        // The two file-local effect ids the Start/Loop decision keys on.
        constexpr std::uint32_t Mag_Start_On = 0x801;
        constexpr std::uint32_t Mag_Loop_On = 0x802;

        std::chrono::steady_clock::time_point g_last_sample{};
        std::size_t g_rows = 0;
        bool g_exhausted_reported = false;

        // The report is a comma-joined list of "%02X<value>" tokens, optionally with a
        // trailing '!' when every instance sits inactive. The observed values are six hex
        // digits (243801, 243803, ...) while the ESP records carry 0x801/0x803, so the
        // effect identity is the low three hex digits of the reported value. Compare those
        // rather than the whole value or a string suffix.
        bool marker_active(std::string_view report, std::uint32_t wanted)
        {
            std::size_t begin = 0;
            while (begin <= report.size())
            {
                const std::size_t comma = report.find(',', begin);
                std::string_view token = report.substr(begin,
                    (comma == std::string_view::npos ? report.size() : comma) - begin);
                const bool inactive = token.ends_with('!');
                if (inactive) token.remove_suffix(1);
                std::uint32_t value = 0;
                bool parsed = !token.empty();
                for (char const c : token)
                {
                    std::uint32_t digit = 0;
                    if (c >= '0' && c <= '9') digit = static_cast<std::uint32_t>(c - '0');
                    else if (c >= 'A' && c <= 'F') digit = static_cast<std::uint32_t>(c - 'A' + 10);
                    else if (c >= 'a' && c <= 'f') digit = static_cast<std::uint32_t>(c - 'a' + 10);
                    else { parsed = false; break; }
                    value = (value << 4) | digit;
                }
                if (parsed && (value & 0xFFFu) == wanted)
                    return !inactive;
                if (comma == std::string_view::npos) break;
                begin = comma + 1;
            }
            return false;
        }
    }

    void sample_world_phase(RE::Actor& actor)
    {
        if (!world_phase_probe())
            return;
        const auto now = std::chrono::steady_clock::now();
        if (g_last_sample.time_since_epoch().count() != 0 && now - g_last_sample < World_Phase_Sample_Ms)
            return;
        g_last_sample = now;
        if (g_rows >= World_Phase_Max_Rows)
        {
            if (!g_exhausted_reported)
            {
                g_exhausted_reported = true;
                logger::warn("WORLD PHASE probe exhausted after {} rows; further samples suppressed", g_rows);
            }
            return;
        }
        ++g_rows;

        const std::string markers = dac::smooth_marker_report(actor);
        const bool start_on = marker_active(markers, Mag_Start_On);
        const bool loop_on = marker_active(markers, Mag_Loop_On);
        const char* const phase = start_on && loop_on ? "both" : start_on ? "start" : loop_on ? "loop" : "unknown";
        logger::info("WORLD PHASE actor={:08x} markers='{}' start-on={} loop-on={} phase={} row={}",
            actor.GetFormID(), markers, start_on, loop_on, phase, g_rows);
    }
}
PLUGIN_NAMESPACE_END
