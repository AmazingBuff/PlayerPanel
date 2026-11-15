//
// C-G5-CAL K2-C4 control B, plan A (2026-11-14).
//
// This file is the immutable registration written before any native control
// output was read. It names the authored resource, its byte identity, the
// spline structure that the runtime carrier must expose, and the fixed sample
// times. The truth bits live in docs/diagnostics/g5-cal-control-b-20261114 /
// mt_idle_female_truth.txt and were produced offline by HavokLib (GPL-3, local
// tool only, never linked into this repository).
//
// Scope: a single-layer, fixed-time sample of one authored clip. It does not
// model graph blending, IK, physics, OAR conditions or randomness, and it does
// not produce tau_p/tau_r/tau_s/epsilon_t.
#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // Runtime loose override actually loaded for the female player. This is NOT
    // the 30,944-byte Skyrim - Animations.bsa male file audited earlier.
    inline constexpr char Control_Sample_Resource[] =
        "meshes\\actors\\character\\animations\\female\\mt_idle.hkx";
    inline constexpr std::uint64_t Control_Sample_Resource_Bytes = 113920;
    inline constexpr char Control_Sample_Resource_Sha256[] =
        "0EF8248CE2C50A2251FA592F7C65E75E75B5E4DC55E9C654436543180BCD73C7";

    // hkaAnimation header (all float fields are compared by raw bits).
    inline constexpr std::uint32_t Control_Sample_Duration_Bits = 0x41C82222u;  // 25.0166664
    inline constexpr std::uint32_t Control_Sample_Track_Count = 99;
    inline constexpr std::uint32_t Control_Sample_Float_Track_Count = 0;

    // hkaSplineCompressedAnimation header.
    inline constexpr std::uint32_t Control_Sample_Num_Frames = 1501;
    inline constexpr std::uint32_t Control_Sample_Num_Blocks = 6;
    inline constexpr std::uint32_t Control_Sample_Max_Frames_Per_Block = 256;
    inline constexpr std::uint32_t Control_Sample_Block_Duration_Bits = 0x40881736u;       // 4.25283337
    inline constexpr std::uint32_t Control_Sample_Block_Inverse_Duration_Bits = 0x3E70C7D9u;  // 0.235137358
    inline constexpr std::uint32_t Control_Sample_Frame_Duration_Bits = 0x3C889FD6u;        // 0.0166777782

    // Authored compressed payload (hkArray<uint8_t> data).
    inline constexpr std::uint32_t Control_Sample_Data_Bytes = 82400;
    inline constexpr std::uint64_t Control_Sample_Data_Fnv1a64 = 0xF56EE1500EB5A3D0ull;
    inline constexpr std::uint64_t Control_Sample_Block_Offsets_Fnv1a64 = 0xF17EBD9ADA8CF785ull;

    // Authored hkaAnimationBinding: identity 0..98, normal blend hint.
    inline constexpr std::uint32_t Control_Sample_Binding_Map_Count = 99;
    inline constexpr std::uint64_t Control_Sample_Binding_Map_Fnv1a64 = 0x077954859409395Eull;
    inline constexpr std::uint32_t Control_Sample_Blend_Hint = 0;

    // Fixed sample times, lowest to highest, as raw binary32 bits so the native
    // side and the offline tool cannot disagree about the decimal value.
    inline constexpr std::uint32_t Control_Sample_Times[] = {
        0x00000000u,  // 0
        0x3C888889u,  // 1/60
        0x3F800000u,  // 1
        0x40800000u,  // 4
        0x40A00000u,  // 5
        0x41080000u,  // 8.5
        0x41480000u,  // 12.5
        0x41880000u,  // 17
        0x41C80000u,  // 25
    };
    inline constexpr std::size_t Control_Sample_Time_Count =
        sizeof(Control_Sample_Times) / sizeof(Control_Sample_Times[0]);

    // FNV-1a over time-major, track-minor (10 float32 each: t.x t.y t.z r.x r.y
    // r.z r.w s.x s.y s.z), little-endian bytes. The offline truth file prints
    // the same value as BITS_FNV1A64.
    inline constexpr std::uint64_t Control_Sample_Truth_Bits_Fnv1a64 = 0x8E2E1D12B040807Dull;

    // ---- C-G5-CAL2 control grid (docs/diagnostics/g5-cal2-20261115 §9) ----
    //
    // 121 predetermined grid times: the baseline t=0 plus t=k/60 for k=1..120,
    // generated once by float64 division rounded to float32 and pasted verbatim
    // below; the runtime never computes times and the offline truth tool
    // receives exactly these bits. [1] equals the registered nine-time 1/60
    // bit-for-bit, [60] is 1, [120] is 2.
    inline constexpr std::size_t Control_Grid_Time_Count = 121;
    inline constexpr std::size_t Control_Grid_Generation_Count = 120;
    inline constexpr std::uint32_t Control_Grid_Times[] = {
        0x00000000u, 0x3C888889u, 0x3D088889u,  // baseline t=0, k=1, k=2
        0x3D4CCCCDu, 0x3D888889u, 0x3DAAAAABu,  // k=3..5
        0x3DCCCCCDu, 0x3DEEEEEFu, 0x3E088889u,  // k=6..8
        0x3E19999Au, 0x3E2AAAABu, 0x3E3BBBBCu,  // k=9..11
        0x3E4CCCCDu, 0x3E5DDDDEu, 0x3E6EEEEFu,  // k=12..14
        0x3E800000u, 0x3E888889u, 0x3E911111u,  // k=15..17
        0x3E99999Au, 0x3EA22222u, 0x3EAAAAABu,  // k=18..20
        0x3EB33333u, 0x3EBBBBBCu, 0x3EC44444u,  // k=21..23
        0x3ECCCCCDu, 0x3ED55555u, 0x3EDDDDDEu,  // k=24..26
        0x3EE66666u, 0x3EEEEEEFu, 0x3EF77777u,  // k=27..29
        0x3F000000u, 0x3F044444u, 0x3F088889u,  // k=30..32
        0x3F0CCCCDu, 0x3F111111u, 0x3F155555u,  // k=33..35
        0x3F19999Au, 0x3F1DDDDEu, 0x3F222222u,  // k=36..38
        0x3F266666u, 0x3F2AAAABu, 0x3F2EEEEFu,  // k=39..41
        0x3F333333u, 0x3F377777u, 0x3F3BBBBCu,  // k=42..44
        0x3F400000u, 0x3F444444u, 0x3F488889u,  // k=45..47
        0x3F4CCCCDu, 0x3F511111u, 0x3F555555u,  // k=48..50
        0x3F59999Au, 0x3F5DDDDEu, 0x3F622222u,  // k=51..53
        0x3F666666u, 0x3F6AAAABu, 0x3F6EEEEFu,  // k=54..56
        0x3F733333u, 0x3F777777u, 0x3F7BBBBCu,  // k=57..59
        0x3F800000u, 0x3F822222u, 0x3F844444u,  // k=60..62
        0x3F866666u, 0x3F888889u, 0x3F8AAAABu,  // k=63..65
        0x3F8CCCCDu, 0x3F8EEEEFu, 0x3F911111u,  // k=66..68
        0x3F933333u, 0x3F955555u, 0x3F977777u,  // k=69..71
        0x3F99999Au, 0x3F9BBBBCu, 0x3F9DDDDEu,  // k=72..74
        0x3FA00000u, 0x3FA22222u, 0x3FA44444u,  // k=75..77
        0x3FA66666u, 0x3FA88889u, 0x3FAAAAABu,  // k=78..80
        0x3FACCCCDu, 0x3FAEEEEFu, 0x3FB11111u,  // k=81..83
        0x3FB33333u, 0x3FB55555u, 0x3FB77777u,  // k=84..86
        0x3FB9999Au, 0x3FBBBBBCu, 0x3FBDDDDEu,  // k=87..89
        0x3FC00000u, 0x3FC22222u, 0x3FC44444u,  // k=90..92
        0x3FC66666u, 0x3FC88889u, 0x3FCAAAABu,  // k=93..95
        0x3FCCCCCDu, 0x3FCEEEEFu, 0x3FD11111u,  // k=96..98
        0x3FD33333u, 0x3FD55555u, 0x3FD77777u,  // k=99..101
        0x3FD9999Au, 0x3FDBBBBCu, 0x3FDDDDDEu,  // k=102..104
        0x3FE00000u, 0x3FE22222u, 0x3FE44444u,  // k=105..107
        0x3FE66666u, 0x3FE88889u, 0x3FEAAAABu,  // k=108..110
        0x3FECCCCDu, 0x3FEEEEEFu, 0x3FF11111u,  // k=111..113
        0x3FF33333u, 0x3FF55555u, 0x3FF77777u,  // k=114..116
        0x3FF9999Au, 0x3FFBBBBCu, 0x3FFDDDDEu,  // k=117..119
        0x40000000u,                            // k=120 (t=2.0)
    };
    static_assert(Control_Grid_Time_Count ==
        sizeof(Control_Grid_Times) / sizeof(Control_Grid_Times[0]));
    static_assert(Control_Grid_Times[1] == Control_Sample_Times[1],
        "grid 1/60 must match the registered nine-time table bit-for-bit");
    static_assert(std::bit_cast<float>(Control_Grid_Times[60]) == 1.0f);
    static_assert(std::bit_cast<float>(Control_Grid_Times[120]) == 2.0f);

    // CGRD v1 artifact layout (docs/diagnostics/g5-cal2-20261115 §9.3).
    inline constexpr std::size_t Control_Grid_Header_Size = 112;
    inline constexpr std::size_t Control_Grid_Record_Size = 4004;
    inline constexpr std::size_t Control_Grid_Artifact_Size =
        Control_Grid_Header_Size + Control_Grid_Time_Count * Control_Grid_Record_Size;
    static_assert(Control_Grid_Artifact_Size == 484596);

    [[nodiscard]] inline std::uint64_t control_sample_fnv1a64(void const* a_data, std::size_t a_size)
    {
        auto const* const bytes = static_cast<std::uint8_t const*>(a_data);
        std::uint64_t hash = 0xcbf29ce484222325ull;
        for (std::size_t i = 0; i < a_size; ++i)
        {
            hash ^= bytes[i];
            hash *= 0x100000001b3ull;
        }
        return hash;
    }
}
PLUGIN_NAMESPACE_END
