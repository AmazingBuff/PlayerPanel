#pragma once

#include "render/dx11/d3d11_util.h"

#include <array>
#include <initializer_list>
#include <vector>

namespace render_test
{
    namespace w32 = REX::W32;
    using PLUGIN_NAMESPACE::RenderTarget;
    using PLUGIN_NAMESPACE::RenderTargetOptions;

    struct Device
    {
        w32::ComPtr<w32::ID3D11Device> device;
        w32::ComPtr<w32::ID3D11DeviceContext> context;
        bool init()
        {
            return w32::SUCCESS(w32::D3D11CreateDevice(nullptr, w32::D3D_DRIVER_TYPE_WARP,
                nullptr, 0, nullptr, 0, 7, device.GetAddressOf(), nullptr, context.GetAddressOf()));
        }
    };

    template <class T>
    w32::ComPtr<w32::ID3D11Buffer> constant(Device const& gpu, T const& data)
    {
        static_assert(sizeof(T) % 16 == 0);
        w32::ComPtr<w32::ID3D11Buffer> result;
        w32::D3D11_BUFFER_DESC desc{ .byteWidth = sizeof(T), .usage = w32::D3D11_USAGE_DEFAULT,
            .bindFlags = w32::D3D11_BIND_CONSTANT_BUFFER };
        w32::D3D11_SUBRESOURCE_DATA initial{ .sysMem = &data };
        gpu.device->CreateBuffer(&desc, &initial, result.GetAddressOf());
        return result;
    }

    inline std::vector<std::array<float, 4>> read_pixels(Device const& gpu, RenderTarget const& source)
    {
        w32::D3D11_TEXTURE2D_DESC desc{};
        source.texture()->GetDesc(&desc);
        if (desc.format != w32::DXGI_FORMAT_R32G32B32A32_FLOAT)
            return {};
        desc.usage = w32::D3D11_USAGE_STAGING;
        desc.bindFlags = 0;
        desc.cpuAccessFlags = w32::D3D11_CPU_ACCESS_READ;
        w32::ComPtr<w32::ID3D11Texture2D> staging;
        if (!w32::SUCCESS(gpu.device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf())))
            return {};
        gpu.context->CopyResource(staging.Get(), source.texture());
        w32::D3D11_MAPPED_SUBRESOURCE mapped{};
        if (!w32::SUCCESS(gpu.context->Map(staging.Get(), 0, w32::D3D11_MAP_READ, 0, &mapped)))
            return {};
        std::vector<std::array<float, 4>> result;
        for (std::uint32_t y = 0; y < desc.height; ++y)
        {
            auto const* row = reinterpret_cast<std::array<float, 4> const*>(
                static_cast<std::byte const*>(mapped.data) + y * mapped.rowPitch);
            result.insert(result.end(), row, row + desc.width);
        }
        gpu.context->Unmap(staging.Get(), 0);
        return result;
    }

    [[nodiscard]] bool native_sss(Device const& gpu, std::filesystem::path const& root);
    [[nodiscard]] bool native_material(Device const& gpu, std::filesystem::path const& root);
    w32::ComPtr<w32::ID3D11ComputeShader> compile_native_shader(Device const& gpu, std::filesystem::path const& root,
        wchar_t const* file, std::initializer_list<char const*> defines = {});
}
