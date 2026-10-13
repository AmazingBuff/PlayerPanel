//
// Created by AmazingBuff on 2026/10/13.
//

#include "render/evidence.h"

#include <cstdio>
#include <cstring>

PLUGIN_NAMESPACE_BEGIN

namespace
{
    void write_tga(std::filesystem::path const& a_file, std::uint32_t a_width, std::uint32_t a_height,
        std::vector<std::uint8_t> const& a_bgra)
    {
        std::FILE* out = _wfopen(a_file.c_str(), L"wb");
        if (!out)
            return;
        std::uint8_t header[18] = {};
        header[2] = 2;   // uncompressed true-color
        header[12] = static_cast<std::uint8_t>(a_width & 0xFF);
        header[13] = static_cast<std::uint8_t>((a_width >> 8) & 0xFF);
        header[14] = static_cast<std::uint8_t>(a_height & 0xFF);
        header[15] = static_cast<std::uint8_t>((a_height >> 8) & 0xFF);
        header[16] = 32;
        header[17] = 0x20;   // top-down origin
        std::fwrite(header, 1, sizeof(header), out);
        std::fwrite(a_bgra.data(), 1, a_bgra.size(), out);
        std::fclose(out);
    }
}

void dump_studio_target(RenderTarget const& a_target, char const* a_tag)
{
    RE::BSGraphics::Renderer* const renderer = RE::BSGraphics::Renderer::GetSingleton();
    if (!renderer || !a_target.texture() || !a_target.rtv())
        return;
    const RE::BSGraphics::RendererData& runtime = renderer->GetRuntimeData();
    if (!runtime.context || !runtime.forwarder)
        return;

    REX::W32::D3D11_TEXTURE2D_DESC desc{};
    a_target.texture()->GetDesc(&desc);
    REX::W32::D3D11_TEXTURE2D_DESC staging_desc = desc;
    staging_desc.usage = REX::W32::D3D11_USAGE_STAGING;
    staging_desc.bindFlags = 0;
    staging_desc.cpuAccessFlags = REX::W32::D3D11_CPU_ACCESS_READ;
    staging_desc.miscFlags = 0;
    staging_desc.mipLevels = 1;
    staging_desc.arraySize = 1;

    REX::W32::ID3D11Texture2D* staging = nullptr;
    if (!REX::W32::SUCCESS(runtime.forwarder->CreateTexture2D(&staging_desc, nullptr, &staging)) || !staging)
        return;
    runtime.context->CopyResource(staging, a_target.texture());

    REX::W32::D3D11_MAPPED_SUBRESOURCE mapped{};
    if (!REX::W32::SUCCESS(runtime.context->Map(staging, 0, REX::W32::D3D11_MAP_READ, 0, &mapped)))
    {
        staging->Release();
        return;
    }

    // The studio target is R8G8B8A8_UNORM (the call-site composite format), so each texel is already
    // four readable bytes; the TGA wants BGRA.
    std::vector<std::uint8_t> bgra;
    bgra.reserve(static_cast<std::size_t>(desc.width) * desc.height * 4);
    for (std::uint32_t y = 0; y < desc.height; ++y)
    {
        auto const* row = static_cast<std::uint8_t const*>(mapped.data) + static_cast<std::size_t>(y) * mapped.rowPitch;
        for (std::uint32_t x = 0; x < desc.width; ++x)
        {
            bgra.push_back(row[static_cast<std::size_t>(x) * 4 + 2]);
            bgra.push_back(row[static_cast<std::size_t>(x) * 4 + 1]);
            bgra.push_back(row[static_cast<std::size_t>(x) * 4 + 0]);
            bgra.push_back(255);
        }
    }
    runtime.context->Unmap(staging, 0);
    staging->Release();

    const std::filesystem::path dir = SKSE::log::log_directory().value_or(std::filesystem::path(".")) / "CharacterPanel";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    for (std::uint32_t index = 0; index < 1000; ++index)
    {
        const std::filesystem::path file = dir / fmt::format("{}-{:03}.tga", a_tag ? a_tag : "studio", index);
        if (std::filesystem::exists(file))
            continue;
        write_tga(file, desc.width, desc.height, bgra);
        logger::info("Studio dump written: {} ({}x{})", file.string(), desc.width, desc.height);
        return;
    }
    logger::warn("Studio dump directory is full; TGA not written");
}

PLUGIN_NAMESPACE_END
