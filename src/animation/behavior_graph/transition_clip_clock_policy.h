//
// C-G5-TRANS v7 (2026-11-20): clip clock offsets and the frozen/advancing classifier.
//
// The offsets are the AE 1.6.1170 layout audited in
// docs/diagnostics/g5-clip-clock-20261120/README.md: Generate copies clip->localTime
// (+0xF0) into hkaAnimationControl::localTime (+0x10) and both control samplers take
// their time argument from +0x10, so the probe field is the sampling clock.
//
#pragma once

#include <cstddef>
#include <cstdint>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    inline constexpr std::size_t Clip_Local_Time_Offset = 0xF0;
    inline constexpr std::size_t Clip_Time_Offset = 0xF4;
    inline constexpr std::size_t Clip_Crop_Start_Offset = 0x58;
    inline constexpr std::size_t Clip_Crop_End_Offset = 0x5C;
    inline constexpr std::size_t Clip_Playback_Speed_Offset = 0x64;
    inline constexpr std::size_t Clip_Mode_Offset = 0x72;
    inline constexpr std::size_t Clip_Control_Offset = 0x88;
    inline constexpr std::size_t Clip_At_End_Offset = 0x104;

    inline constexpr std::size_t Control_Local_Time_Offset = 0x10;
    inline constexpr std::size_t Control_Weight_Offset = 0x14;
    inline constexpr std::size_t Control_Track_Weights_Size_Offset = 0x20;
    inline constexpr std::size_t Control_Playback_Speed_Offset = 0x5C;
    inline constexpr std::size_t Control_Crop_Start_Offset = 0x9C;
    inline constexpr std::size_t Control_Crop_End_Offset = 0xA0;

    inline constexpr std::uint8_t Clip_Mode_User_Controlled = 2;

    // What the clip's own clock did between two samples of the same (segment, phase).
    enum class ClipClockAdvance : std::uint8_t
    {
        e_unknown,          // no previous sample for this phase, or the state was unreadable
        e_advancing,        // clip->time (+0xF4) changed
        e_frozen_at_end,    // time frozen with atEnd set: Update skips the localTime write (0xACDF8A)
        e_frozen_no_step,   // time frozen and not at end: the clip saw no timestep
    };

    [[nodiscard]] constexpr bool clip_clock_mode_is_user_controlled(std::uint8_t a_mode) noexcept
    {
        return a_mode == Clip_Mode_User_Controlled;
    }

    [[nodiscard]] constexpr ClipClockAdvance clip_clock_advance(std::uint32_t a_previous_time_bits,
        std::uint32_t a_time_bits, bool a_at_end, bool a_have_previous) noexcept
    {
        if (!a_have_previous)
            return ClipClockAdvance::e_unknown;
        if (a_time_bits != a_previous_time_bits)
            return ClipClockAdvance::e_advancing;
        return a_at_end ? ClipClockAdvance::e_frozen_at_end : ClipClockAdvance::e_frozen_no_step;
    }

    [[nodiscard]] constexpr char const* clip_clock_advance_name(ClipClockAdvance a_advance) noexcept
    {
        switch (a_advance)
        {
            case ClipClockAdvance::e_unknown: return "unknown";
            case ClipClockAdvance::e_advancing: return "advancing";
            case ClipClockAdvance::e_frozen_at_end: return "frozen-at-end";
            case ClipClockAdvance::e_frozen_no_step: return "frozen-no-step";
        }
        return "unknown";
    }
}
PLUGIN_NAMESPACE_END
