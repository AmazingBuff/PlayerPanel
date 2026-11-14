//
// C-G5-CAL K2-C4 control B policy test (engine-free).
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/control_sample_policy.h"

#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>

using namespace test_plugin::animation::behavior_graph;

namespace
{
    void schedule_is_the_registered_nine_points()
    {
        assert(Control_Sample_Time_Count == 9);
        assert(Control_Sample_Track_Count == 99);
        assert(Control_Sample_Time_Count * Control_Sample_Track_Count * 10 == 8910);
        assert(Control_Sample_Times[0] == 0x00000000u);
        assert(Control_Sample_Times[1] == 0x3C888889u);
        assert(Control_Sample_Times[8] == 0x41C80000u);
        float previous = -1.0f;
        for (std::uint32_t const bits : Control_Sample_Times)
        {
            float const value = std::bit_cast<float>(bits);
            assert(value > previous);
            previous = value;
        }
        assert(std::bit_cast<float>(Control_Sample_Times[1]) == 1.0f / 60.0f);
    }

    void resource_registration_is_well_formed()
    {
        assert(std::strlen(Control_Sample_Resource_Sha256) == 64);
        for (char const* p = Control_Sample_Resource_Sha256; *p; ++p)
            assert((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F'));
        assert(Control_Sample_Resource_Bytes == 113920);
        assert(Control_Sample_Data_Bytes == 82400);
        assert(Control_Sample_Data_Fnv1a64 == 0xF56EE1500EB5A3D0ull);
        assert(Control_Sample_Block_Offsets_Fnv1a64 == 0xF17EBD9ADA8CF785ull);
        assert(Control_Sample_Binding_Map_Fnv1a64 == 0x077954859409395Eull);
        assert(Control_Sample_Truth_Bits_Fnv1a64 == 0x8E2E1D12B040807Dull);
        assert(Control_Sample_Frame_Duration_Bits == 0x3C889FD6u);
        assert(Control_Sample_Block_Duration_Bits == 0x40881736u);
    }

    void fnv_matches_the_reference_vectors()
    {
        assert(control_sample_fnv1a64(nullptr, 0) == 0xcbf29ce484222325ull);
        char const hello[] = "hello";
        assert(control_sample_fnv1a64(hello, 5) == 0xa430d84680aabd0bull);
        std::uint8_t const bytes[] = { 0x00, 0xff, 0x10, 0x20 };
        assert(control_sample_fnv1a64(bytes, sizeof(bytes)) == 0xd6e9317bf4e57b50ull);
    }

    void structural_relations_hold()
    {
        // duration = 1500 * frameDuration and blockDuration = 255 * frameDuration.
        float const frame_duration = std::bit_cast<float>(Control_Sample_Frame_Duration_Bits);
        float const block_duration = std::bit_cast<float>(Control_Sample_Block_Duration_Bits);
        assert(std::fabs(block_duration - 255.0f * frame_duration) < 1.0e-6f);
        assert(static_cast<std::uint32_t>(Control_Sample_Num_Frames - 1) == 1500);
    }
}

int main()
{
    schedule_is_the_registered_nine_points();
    resource_registration_is_well_formed();
    fnv_matches_the_reference_vectors();
    structural_relations_hold();
    std::cout << "control sample policy: 4 cases passed\n";
    return 0;
}
