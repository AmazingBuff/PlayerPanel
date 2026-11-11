//
// Created by AmazingBuff on 2026/11/11.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/pose_capture_policy.h"

#include <array>
#include <bit>
#include <cstdint>
#include <format>
#include <iostream>
#include <limits>
#include <string_view>

namespace
{
    using test_plugin::animation::behavior_graph::PoseSample;
    using test_plugin::animation::behavior_graph::capture_pose_sample;
    using test_plugin::animation::behavior_graph::format_source_pose_track;

    struct alignas(16) OfflineTransform
    {
        std::array<float, 4> translation;
        std::array<float, 4> rotation;
        std::array<float, 4> scale;
    };

    static_assert(sizeof(OfflineTransform) == 0x30 && alignof(OfflineTransform) == 16);
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);

    void emit_track(std::string_view const case_id, std::string_view const role, OfflineTransform const& transform)
    {
        PoseSample const sample = capture_pose_sample(transform);
        std::array<float, 10> const components{
            sample.translation[0], sample.translation[1], sample.translation[2],
            sample.rotation[0], sample.rotation[1], sample.rotation[2], sample.rotation[3],
            sample.scale[0], sample.scale[1], sample.scale[2]
        };
        std::cout << std::format("{{\"type\":\"track\",\"case\":\"{}\",\"role\":\"{}\",\"track\":0,\"raw_bits\":[", case_id, role);
        for (std::size_t i = 0; i < components.size(); ++i)
            std::cout << std::format("{}\"{:08x}\"", i == 0 ? "" : ",", std::bit_cast<std::uint32_t>(components[i]));
        std::cout << std::format("],\"padding_bits\":[\"{:08x}\",\"{:08x}\"],\"row\":\"{}\"}}\n",
            std::bit_cast<std::uint32_t>(transform.translation[3]), std::bit_cast<std::uint32_t>(transform.scale[3]),
            format_source_pose_track(0, sample));
    }
}

int main()
{
    std::cout << std::format("{{\"type\":\"header\",\"schema\":1,\"fixture\":\"g5-cal-a-analytic-sign-v1\",\"pairs\":2,\"tracks\":4,\"components\":40,\"carrier_bytes\":{},\"carrier_alignment\":{},\"msvc_full_ver\":{},\"stl_version\":{},\"stl_update\":{},\"native\":false}}\n",
        sizeof(OfflineTransform), alignof(OfflineTransform), _MSC_FULL_VER, _MSVC_STL_VERSION, _MSVC_STL_UPDATE);

    OfflineTransform reference{
        { 3.1250003f, -4.2500003f, 12.5000003f, std::bit_cast<float>(0x7fc01234u) },
        { 0.0f, 0.0f, 0.25881904510252074f, 0.9659258262890683f },
        { 0.75000036f, 1.00000036f, 1.25000036f, std::bit_cast<float>(0x7f800000u) }
    };
    OfflineTransform observed = reference;
    observed.translation[1] += 3.0f;
    observed.translation[2] += 4.0f;
    observed.rotation = { 0.0f, 0.0f, 0.7071067811865475f, 0.7071067811865475f };
    observed.scale[2] += 0.125f;
    observed.translation[3] = std::bit_cast<float>(0x7fc05678u);
    observed.scale[3] = std::bit_cast<float>(0xff800000u);
    emit_track("analytic", "reference", reference);
    emit_track("analytic", "observed", observed);

    reference.translation[3] = std::bit_cast<float>(0x7fc09abcu);
    reference.scale[3] = std::bit_cast<float>(0x7f800000u);
    observed = reference;
    for (float& component : observed.rotation)
        component = -component;
    observed.translation[3] = std::bit_cast<float>(0x7fc0def0u);
    observed.scale[3] = std::bit_cast<float>(0xff800000u);
    emit_track("sign", "reference", reference);
    emit_track("sign", "observed", observed);
    std::cout << "{\"type\":\"end\",\"pairs\":2,\"tracks\":4,\"components\":40}\n";
    return std::cout ? 0 : 1;
}
