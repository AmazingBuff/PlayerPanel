//
// Studio target evidence dump. (2026-10-05 src migration: split out of tools/m0_proto, behavior byte-identical)
//

#include "render/evidence.h"
#include "render/offscreen_target.h"

#include <cmath>
#include <cstdio>
#include <cstring>

PLUGIN_NAMESPACE_BEGIN
namespace
{
    void write_tga(const std::filesystem::path& file, std::uint32_t width, std::uint32_t height,
        const std::vector<std::uint8_t>& bgra)
    {
        std::FILE* out = _wfopen(file.c_str(), L"wb");
        if (!out)
            return;
        std::uint8_t header[18] = {};
        header[2] = 2;  // uncompressed true-color
        header[12] = static_cast<std::uint8_t>(width & 0xFF);
        header[13] = static_cast<std::uint8_t>((width >> 8) & 0xFF);
        header[14] = static_cast<std::uint8_t>(height & 0xFF);
        header[15] = static_cast<std::uint8_t>((height >> 8) & 0xFF);
        header[16] = 32;
        header[17] = 0x20;  // top-down origin
        std::fwrite(header, 1, sizeof(header), out);
        std::fwrite(bgra.data(), 1, bgra.size(), out);
        std::fclose(out);
    }
}

// One-shot GPU -> CPU copy of the studio color target (F7 debug
// path), tone-mapped to an 8-bit BGRA TGA under the SKSE log
// directory when the target is HDR, copied verbatim otherwise (the
// call-site composite is R8G8B8A8_UNORM). Synchronous by design —
// never called on the steady-state path.
void dump_offscreen_to_log_dir()
{
    auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
    OffscreenTarget& target = offscreen_target();
    if (!renderer || !target.color)
        return;
    auto& runtime = renderer->GetRuntimeData();
    if (!runtime.context || !runtime.forwarder)
        return;

    REX::W32::D3D11_TEXTURE2D_DESC desc{};
    target.color->GetDesc(&desc);
    REX::W32::D3D11_TEXTURE2D_DESC staging_desc = desc;
    staging_desc.usage = REX::W32::D3D11_USAGE_STAGING;
    staging_desc.bindFlags = 0;
    staging_desc.cpuAccessFlags = REX::W32::D3D11_CPU_ACCESS_READ;
    staging_desc.miscFlags = 0;
    staging_desc.mipLevels = 1;
    staging_desc.arraySize = 1;
    REX::W32::ID3D11Texture2D* staging = nullptr;
    if (runtime.forwarder->CreateTexture2D(&staging_desc, nullptr, &staging) != 0 || !staging)
        return;
    runtime.context->CopyResource(staging, target.color);

    REX::W32::D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(runtime.context->Map(staging, 0, REX::W32::D3D11_MAP_READ, 0, &mapped)))
    {
        staging->Release();
        return;
    }

    auto decode_r11f = [](std::uint32_t bits) -> float {
        if (bits == 0)
            return 0.0f;
        std::uint32_t const exponent = (bits >> 6) & 0x1F;
        std::uint32_t const mantissa = bits & 0x3F;
        if (exponent == 0x1F)
            return 1e30f;
        if (exponent == 0)
            return std::ldexp(static_cast<float>(mantissa), -14 - 6);
        return std::ldexp(static_cast<float>(mantissa | 0x40), static_cast<int>(exponent) - 15 - 6);
    };
    auto tone = [](float v) {
        v = v / (1.0f + v);
        return static_cast<std::uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    };

    const bool hdr = desc.format == REX::W32::DXGI_FORMAT_R11G11B10_FLOAT;
    std::vector<std::uint8_t> bgra;
    bgra.reserve(static_cast<std::size_t>(desc.width) * desc.height * 4);
    for (std::uint32_t y = 0; y < desc.height; ++y)
    {
        auto const* row = reinterpret_cast<std::uint8_t const*>(mapped.data) + y * mapped.rowPitch;
        for (std::uint32_t x = 0; x < desc.width; ++x)
        {
            std::uint8_t r8 = 0;
            std::uint8_t g8 = 0;
            std::uint8_t b8 = 0;
            if (hdr)
            {
                std::uint32_t packed = 0;
                std::memcpy(&packed, row + static_cast<std::size_t>(x) * 4, sizeof(packed));
                r8 = tone(decode_r11f(packed & 0x7FF));
                g8 = tone(decode_r11f((packed >> 11) & 0x7FF));
                b8 = tone(decode_r11f((packed >> 22) & 0x3FF));
            }
            else
            {
                r8 = row[static_cast<std::size_t>(x) * 4 + 0];
                g8 = row[static_cast<std::size_t>(x) * 4 + 1];
                b8 = row[static_cast<std::size_t>(x) * 4 + 2];
            }
            bgra.push_back(b8);
            bgra.push_back(g8);
            bgra.push_back(r8);
            bgra.push_back(255);
        }
    }
    runtime.context->Unmap(staging, 0);
    staging->Release();

    std::filesystem::path dir =
        SKSE::log::log_directory().value_or(std::filesystem::path(".")) / "CharacterPanel";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    for (std::uint32_t index = 0; index < 1000; ++index)
    {
        std::filesystem::path file = dir / fmt::format("proto-pass-{:03}.tga", index);
        if (std::filesystem::exists(file))
            continue;
        write_tga(file, desc.width, desc.height, bgra);
        logger::info("Proto v3 studio dump written: {}", file.string());
        return;
    }
    logger::warn("Proto v3 dump directory is full; TGA not written");
}
PLUGIN_NAMESPACE_END
