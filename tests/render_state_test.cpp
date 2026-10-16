#include "render_fixture.h"
#include "render/community_shaders/gpu_data.h"
#include "render/community_shaders/binary_profile.h"
#include "render/shader_sources.h"

#include <cmath>
#include <cstdio>

namespace
{
    using namespace render_test;
    namespace cs = PLUGIN_NAMESPACE::community_shaders;

    bool readable_target(Device const& gpu)
    {
        RenderTarget target(RenderTargetOptions{ w32::DXGI_FORMAT_R16G16B16A16_FLOAT,
            w32::DXGI_FORMAT_D32_FLOAT, true, true });
        RenderTarget color_only(w32::DXGI_FORMAT_R8G8B8A8_UNORM, w32::DXGI_FORMAT_UNKNOWN);
        if (!target.init(gpu.device.Get(), 7, 3) || !color_only.init(gpu.device.Get(), 7, 3) ||
            !target.matches(gpu.device.Get(), 7, 3) || !color_only.matches(gpu.device.Get(), 7, 3))
            return false;
        auto* original = color_only.texture();
        if (!color_only.init(gpu.device.Get(), 7, 3) || original != color_only.texture())
            return false;
        w32::D3D11_SHADER_RESOURCE_VIEW_DESC input{};
        w32::D3D11_DEPTH_STENCIL_VIEW_DESC depth{}, read_only{};
        target.depth_srv()->GetDesc(&input);
        target.dsv()->GetDesc(&depth);
        target.read_only_dsv()->GetDesc(&read_only);
        auto* output = target.rtv();
        auto* sampled_depth = target.depth_srv();
        gpu.context->OMSetRenderTargets(1, &output, target.read_only_dsv());
        gpu.context->PSSetShaderResources(17, 1, &sampled_depth);
        w32::ComPtr<w32::ID3D11ShaderResourceView> bound;
        gpu.context->PSGetShaderResources(17, 1, bound.GetAddressOf());
        gpu.context->ClearState();
        const bool valid = bound.Get() == sampled_depth && target.uav() &&
            input.format == w32::DXGI_FORMAT_R32_FLOAT && depth.format == w32::DXGI_FORMAT_D32_FLOAT &&
            read_only.flags == w32::D3D11_DSV_READ_ONLY_DEPTH;
        target.release();
        return valid && !target.matches(gpu.device.Get(), 7, 3) && !target.uav() && !target.depth_srv() &&
            target.init(gpu.device.Get(), 11, 5) && target.width() == 11 && target.height() == 5;
    }

    bool high_slots_and_compute(Device const& gpu)
    {
        gpu.context->ClearState();
        RenderTarget output(RenderTargetOptions{ w32::DXGI_FORMAT_R32G32B32A32_FLOAT,
            w32::DXGI_FORMAT_UNKNOWN, true });
        RenderTarget source(w32::DXGI_FORMAT_R32G32B32A32_FLOAT, w32::DXGI_FORMAT_UNKNOWN);
        RenderTarget scratch(RenderTargetOptions{ w32::DXGI_FORMAT_R32G32B32A32_FLOAT,
            w32::DXGI_FORMAT_UNKNOWN, true });
        if (!output.init(gpu.device.Get(), 8, 8) || !source.init(gpu.device.Get(), 8, 8) ||
            !scratch.init(gpu.device.Get(), 8, 8))
            return false;
        auto* target = output.rtv();
        auto* srv = source.srv();
        auto* uav = scratch.uav();
        gpu.context->OMSetRenderTargets(1, &target, nullptr);
        gpu.context->VSSetShaderResources(127, 1, &srv);
        gpu.context->PSSetShaderResources(71, 1, &srv);
        gpu.context->CSSetShaderResources(37, 1, &srv);
        gpu.context->CSSetUnorderedAccessViews(7, 1, &uav, nullptr);
        w32::ComPtr<w32::ID3D11ComputeShader> shader;
        if (!PLUGIN_NAMESPACE::create_compute_shader(gpu.device.Get(),
            "[numthreads(1,1,1)] void main(uint3 id:SV_DispatchThreadID) {}", "main", "capture fixture", shader.GetAddressOf()))
            return false;
        gpu.context->CSSetShader(shader.Get(), nullptr, 0);
        w32::D3D11_SAMPLER_DESC desc{};
        desc.filter = w32::D3D11_FILTER_MIN_MAG_MIP_POINT;
        desc.addressU = desc.addressV = desc.addressW = w32::D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.maxLOD = w32::D3D11_FLOAT32_MAX;
        w32::ComPtr<w32::ID3D11SamplerState> sampler;
        if (!w32::SUCCESS(gpu.device->CreateSamplerState(&desc, sampler.GetAddressOf())))
            return false;
        gpu.context->PSSetSamplers(15, 1, sampler.GetAddressOf());
        gpu.context->VSSetSamplers(15, 1, sampler.GetAddressOf());
        gpu.context->CSSetSamplers(15, 1, sampler.GetAddressOf());
        std::array<float, 1024> data{};
        auto buffer = constant(gpu, data);
        w32::ComPtr<w32::ID3D11DeviceContext1> context1;
        if (!buffer.Get() || !w32::SUCCESS(gpu.context->QueryInterface(w32::IID_ID3D11DeviceContext1,
            reinterpret_cast<void**>(context1.GetAddressOf()))))
            return false;
        const std::uint32_t first = 16, count = 32;
        context1->CSSetConstantBuffers1(13, 1, buffer.GetAddressOf(), &first, &count);
        {
            PLUGIN_NAMESPACE::D3D11StateCapture capture(gpu.context.Get());
            gpu.context->ClearState();
            // This would evict the original RTV during restoration if preview UAVs were not unbound first.
            auto* conflicting = output.uav();
            gpu.context->CSSetUnorderedAccessViews(0, 1, &conflicting, nullptr);
        }
        w32::ComPtr<w32::ID3D11ShaderResourceView> vs, ps, compute;
        w32::ComPtr<w32::ID3D11RenderTargetView> restored_target;
        w32::ComPtr<w32::ID3D11UnorderedAccessView> restored_uav, cleared_uav;
        w32::ComPtr<w32::ID3D11ComputeShader> restored_shader;
        w32::ComPtr<w32::ID3D11Buffer> restored_buffer;
        w32::ComPtr<w32::ID3D11SamplerState> vs_sampler, ps_sampler, cs_sampler;
        std::uint32_t restored_first{}, restored_count{};
        gpu.context->VSGetShaderResources(127, 1, vs.GetAddressOf());
        gpu.context->PSGetShaderResources(71, 1, ps.GetAddressOf());
        gpu.context->CSGetShaderResources(37, 1, compute.GetAddressOf());
        gpu.context->OMGetRenderTargets(1, restored_target.GetAddressOf(), nullptr);
        gpu.context->CSGetUnorderedAccessViews(7, 1, restored_uav.GetAddressOf());
        gpu.context->CSGetUnorderedAccessViews(0, 1, cleared_uav.GetAddressOf());
        gpu.context->CSGetShader(restored_shader.GetAddressOf(), nullptr, nullptr);
        context1->CSGetConstantBuffers1(13, 1, restored_buffer.GetAddressOf(), &restored_first, &restored_count);
        gpu.context->VSGetSamplers(15, 1, vs_sampler.GetAddressOf());
        gpu.context->PSGetSamplers(15, 1, ps_sampler.GetAddressOf());
        gpu.context->CSGetSamplers(15, 1, cs_sampler.GetAddressOf());
        gpu.context->ClearState();
        return vs.Get() == srv && ps.Get() == srv && compute.Get() == srv && restored_target.Get() == target &&
            restored_uav.Get() == uav && !cleared_uav.Get() && restored_shader.Get() == shader.Get() &&
            restored_buffer.Get() == buffer.Get() && restored_first == first && restored_count == count &&
            vs_sampler.Get() == sampler.Get() && ps_sampler.Get() == sampler.Get() && cs_sampler.Get() == sampler.Get();
    }

    bool menu_encoding(Device const& gpu, bool linear)
    {
        gpu.context->ClearState();
        RenderTarget material(w32::DXGI_FORMAT_R32G32B32A32_FLOAT, w32::DXGI_FORMAT_UNKNOWN);
        RenderTarget result(RenderTargetOptions{ w32::DXGI_FORMAT_R32G32B32A32_FLOAT,
            w32::DXGI_FORMAT_UNKNOWN, true });
        if (!material.init(gpu.device.Get(), 7, 3) || !result.init(gpu.device.Get(), 7, 3))
            return false;
        const float color[]{ 0.375f, 0.0625f, 0.0f, 1.0f };
        gpu.context->ClearRenderTargetView(material.rtv(), color);
        w32::ComPtr<w32::ID3D11ComputeShader> shader;
        if (!PLUGIN_NAMESPACE::create_compute_shader(gpu.device.Get(), PLUGIN_NAMESPACE::render_shaders::StudioEncode,
            "cs_main", "menu encoding fixture", shader.GetAddressOf()))
            return false;
        auto buffer = constant(gpu, cs::EncodingData{ 7, 3, linear ? 1u : 0u });
        if (!buffer.Get())
            return false;
        gpu.context->CSSetConstantBuffers(0, 1, buffer.GetAddressOf());
        auto* input = material.srv();
        auto* output = result.uav();
        gpu.context->CSSetShaderResources(0, 1, &input);
        gpu.context->CSSetUnorderedAccessViews(0, 1, &output, nullptr);
        gpu.context->CSSetShader(shader.Get(), nullptr, 0);
        gpu.context->Dispatch(1, 1, 1);
        gpu.context->ClearState();
        auto const pixels = read_pixels(gpu, result);
        const float expected = linear ? std::pow(0.375f, 1.0f / 2.2f) : 0.375f;
        const float green = linear ? std::pow(0.0625f, 1.0f / 2.2f) : 0.0625f;
        return pixels.size() == 21 && std::ranges::all_of(pixels, [&](auto const& pixel)
        {
            return std::abs(pixel[0] - expected) < 0.0001f && std::abs(pixel[1] - green) < 0.0001f &&
                pixel[2] == 0.0f && pixel[3] == 1.0f;
        });
    }

    bool profile_rejection()
    {
        auto guid = cs::Pdb_Guid;
        if (!cs::matches_identity(cs::Timestamp, cs::Image_Size, guid, 1) ||
            cs::matches_identity(cs::Timestamp ^ 1, cs::Image_Size, guid, 1) ||
            cs::matches_identity(cs::Timestamp, cs::Image_Size + 1, guid, 1) ||
            cs::matches_identity(cs::Timestamp, cs::Image_Size, guid, 2))
            return false;
        guid[0] ^= 1;
        if (cs::matches_identity(cs::Timestamp, cs::Image_Size, guid, 1))
            return false;
        std::vector<std::uint8_t> image(cs::Image_Size);
        for (auto const& entry : cs::Entries)
            std::copy(entry.prefix.begin(), entry.prefix.end(), image.begin() + entry.rva);
        if (!cs::matches_entries(image))
            return false;
        image[cs::Entries[0].rva] ^= 1;
        return !cs::matches_entries(image) && !cs::matches_entries(std::span<std::uint8_t const>{ image.data(), 100 });
    }
}

int main(int argc, char** argv)
{
    render_test::Device gpu;
    if (!gpu.init())
        return 1;
    bool passed = true;
    auto const check = [&](char const* name, bool result)
    {
        std::printf("%s: %s\n", name, result ? "PASS" : "FAIL");
        passed &= result;
    };
    check("private float target and readable depth", readable_target(gpu));
    check("high shader slots and compute state restore", high_slots_and_compute(gpu));
    check("CS menu encoding Linear Lighting on", menu_encoding(gpu, true));
    check("CS menu encoding Linear Lighting off", menu_encoding(gpu, false));
    check("CS binary entry guard", profile_rejection());
    if (argc == 3 && std::string_view(argv[1]) == "--cs-shaders")
    {
        check("official CS SSS shaders on WARP", render_test::native_sss(gpu, std::filesystem::path(argv[2])));
        check("official CS material composite on WARP", render_test::native_material(gpu, std::filesystem::path(argv[2])));
    }
    else
        std::puts("official CS SSS fixtures: not requested (pass --cs-shaders <1.9.1 Shaders directory>)");
    return passed ? 0 : 1;
}
