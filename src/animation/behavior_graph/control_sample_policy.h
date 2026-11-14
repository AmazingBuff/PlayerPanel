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
