#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include <SimpleMath.h>
#include "REX/W32/D3D11.h"

PLUGIN_NAMESPACE_BEGIN
namespace community_shaders
{
    using Vector4 = DirectX::SimpleMath::Vector4;
    using Matrix = DirectX::SimpleMath::Matrix;
    // CS 1.9.1 PDB layouts plus Common/SharedData.hlsli and Common/FrameBuffer.hlsli.
    // No STL objects from the upstream classes are mirrored or constructed here.
    struct alignas(16) SharedData
    {
        Vector4 water[25]{};
        Vector4 light_direction{}, light_color{}, sun_direction{}, sun_color{};
        Vector4 masser_direction{}, masser_color{}, secunda_direction{}, secunda_color{};
        Vector4 camera{}, dimensions{};
        float timer{};
        std::uint32_t frame{}, active_frame{}, interior = 1;
        std::uint32_t directional_shadows{}, map_menu{}, hide_sky = 1;
        float mip_bias{};
        float water_height = -std::numeric_limits<float>::max();
        float pad[3]{};
        Vector4 ambient_r{}, ambient_g{}, ambient_b{}, hdr{};
    };
    static_assert(sizeof(SharedData) == 0x2A0);
    static_assert(offsetof(SharedData, camera) == 0x210);
    static_assert(offsetof(SharedData, ambient_r) == 0x260);

    struct alignas(16) FrameData
    {
        Matrix view{}, projection{}, view_projection{}, unjittered{}, previous_unjittered{};
        Matrix projection_unjittered{}, projection_unjittered_inverse{}, view_inverse{};
        Matrix view_projection_inverse{}, projection_inverse{};
        Vector4 position{}, previous_position{}, params{};
        Vector4 resolution{ 1, 1, 1, 1 }, resolution_inverse{ 1, 1, 1, 1 };
    };
    static_assert(sizeof(FrameData) == 0x2D0);
    static_assert(offsetof(FrameData, position) == 0x280);

    struct alignas(16) SssData
    {
        Vector4 kernels[42]{};
        Vector4 base_profile{}, human_profile{};
        float fov{};
        std::uint32_t samples{}, scatter_mode{}, pad{};
        Vector4 mean_free_path_base{}, mean_free_path_human{};
    };
    static_assert(sizeof(SssData) == 0x2F0);
    static_assert(offsetof(SssData, fov) == 0x2C0);

    struct FeatureData
    {
        std::array<std::byte, 0x20> grass, materials, cubemaps;
        std::array<std::byte, 0x30> terrain_shadows;
        std::array<std::byte, 0x20> lights;
        std::array<std::byte, 0xC0> wetness;
        std::array<std::byte, 0x90> skylighting;
        std::array<std::byte, 0x10> clouds;
        std::array<std::byte, 0x20> lod;
        std::array<std::byte, 0x50> hair;
        std::array<std::byte, 0x10> terrain_variation;
        std::array<std::byte, 0x30> ibl;
        std::array<std::byte, 0x10> translucency;
        std::array<std::uint32_t, 28> linear_lighting;
        std::array<std::byte, 0x80> effects;
        std::array<std::byte, 0x10> terrain_blending;
        std::array<std::byte, 0xC0> fog;
        std::array<std::byte, 0x10> pbr;
        std::array<std::byte, 0x70> skin;
    };
    static_assert(sizeof(FeatureData) == 0x510);
    static_assert(offsetof(FeatureData, lights) == 0x90);
    static_assert(offsetof(FeatureData, linear_lighting) == 0x2D0);
    static_assert(offsetof(FeatureData, skin) == 0x4A0);

    // A native ConstantBuffer wrapper holds one COM pointer followed by its creation descriptor.
    // The view borrows the original wrapper until exit; it owns each substitute GPU allocation.
    struct NativeBuffer
    {
        REX::W32::ID3D11Buffer* resource{};
        REX::W32::D3D11_BUFFER_DESC desc{};
    };
    static_assert(sizeof(NativeBuffer) == 0x20);

    struct alignas(16) EncodingData
    {
        std::uint32_t width, height, linear_lighting, pad{};
    };
    static_assert(sizeof(EncodingData) == 16);
}
PLUGIN_NAMESPACE_END
