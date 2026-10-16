#include "render_fixture.h"
#include "render/community_shaders/gpu_data.h"

#include <bit>
#include <cmath>
#include <cstdio>

namespace render_test
{
    namespace
    {
        namespace cs = PLUGIN_NAMESPACE::community_shaders;

        bool run(Device const& gpu, w32::ID3D11ComputeShader* shader, bool linear, bool reflections)
        {
            gpu.context->ClearState();
            RenderTarget main(RenderTargetOptions{ w32::DXGI_FORMAT_R32G32B32A32_FLOAT,
                w32::DXGI_FORMAT_UNKNOWN, true });
            RenderTarget normal_output(RenderTargetOptions{ w32::DXGI_FORMAT_R8G8B8A8_UNORM,
                w32::DXGI_FORMAT_UNKNOWN, true });
            RenderTarget motion(RenderTargetOptions{ w32::DXGI_FORMAT_R16G16_FLOAT,
                w32::DXGI_FORMAT_UNKNOWN, true });
            std::array<std::unique_ptr<RenderTarget>, 8> inputs;
            for (auto& input : inputs)
            {
                input = std::make_unique<RenderTarget>(w32::DXGI_FORMAT_R32G32B32A32_FLOAT, w32::DXGI_FORMAT_UNKNOWN);
                if (!input->init(gpu.device.Get(), 9, 9))
                    return false;
            }
            if (!main.init(gpu.device.Get(), 9, 9) || !normal_output.init(gpu.device.Get(), 9, 9) ||
                !motion.init(gpu.device.Get(), 9, 9))
                return false;
            const float diffuse[]{ 0.25f, 0.0625f, 0, 1 };
            gpu.context->ClearRenderTargetView(main.rtv(), diffuse);
            const std::array<std::array<float, 4>, 8> values{{
                { 0.125f, 0, 0, 1 }, { 0.5f, 0.5f, 0.5f, 1 }, { 0.5f, 0.5f, 0.5f, 1 },
                { 0, 0, 0, 1 }, { 0.5f, 0, 0, 1 }, { 0.2f, 0, 0, 1 }, { 0, 0, 0, 1 }, { 0, 0, 0, 1 }
            }};
            for (std::size_t i = 0; i < inputs.size(); ++i)
                gpu.context->ClearRenderTargetView(inputs[i]->rtv(), values[i].data());
            const float white[4]{ 1, 1, 1, 1 };
            w32::D3D11_TEXTURE2D_DESC cube_desc{
                .width = 1, .height = 1, .mipLevels = 1, .arraySize = 6,
                .format = w32::DXGI_FORMAT_R32G32B32A32_FLOAT, .sampleDesc = { 1, 0 },
                .usage = w32::D3D11_USAGE_IMMUTABLE, .bindFlags = w32::D3D11_BIND_SHADER_RESOURCE,
                .miscFlags = w32::D3D11_RESOURCE_MISC_TEXTURECUBE
            };
            std::array<w32::D3D11_SUBRESOURCE_DATA, 6> faces;
            for (auto& face : faces)
                face = { .sysMem = white, .sysMemPitch = 16 };
            w32::ComPtr<w32::ID3D11Texture2D> cube;
            w32::ComPtr<w32::ID3D11ShaderResourceView> cube_srv;
            if (!w32::SUCCESS(gpu.device->CreateTexture2D(&cube_desc, faces.data(), cube.GetAddressOf())) ||
                !w32::SUCCESS(gpu.device->CreateShaderResourceView(cube.Get(), nullptr, cube_srv.GetAddressOf())))
                return false;
            w32::D3D11_SHADER_RESOURCE_VIEW_DESC cube_view{};
            cube_srv->GetDesc(&cube_view);
            if (cube_view.viewDimension != w32::D3D11_SRV_DIMENSION_TEXTURECUBE)
                return false;
            cs::SharedData shared;
            shared.dimensions = { 9, 9, 1.0f / 9, 1.0f / 9 };
            shared.camera = { 100, 1, 99, 100 };
            shared.ambient_r.x = shared.ambient_g.x = shared.ambient_b.x = 0.25f / 0.282095f;
            cs::FeatureData features{};
            features.linear_lighting[0] = linear ? 1 : 0;
            features.linear_lighting[7] = std::bit_cast<std::uint32_t>(1.6f); // ambientGamma
            features.linear_lighting[18] = std::bit_cast<std::uint32_t>(1.0f); // ambientMult
            cs::FrameData frame;
            frame.view_projection_inverse.m[2][2] = -1;
            auto shared_cb = constant(gpu, shared), feature_cb = constant(gpu, features), frame_cb = constant(gpu, frame);
            if (!shared_cb.Get() || !feature_cb.Get() || !frame_cb.Get())
                return false;
            gpu.context->CSSetConstantBuffers(5, 1, shared_cb.GetAddressOf());
            gpu.context->CSSetConstantBuffers(6, 1, feature_cb.GetAddressOf());
            gpu.context->CSSetConstantBuffers(12, 1, frame_cb.GetAddressOf());
            w32::D3D11_SAMPLER_DESC sampler_desc{};
            sampler_desc.filter = w32::D3D11_FILTER_MIN_MAG_MIP_POINT;
            sampler_desc.addressU = sampler_desc.addressV = sampler_desc.addressW = w32::D3D11_TEXTURE_ADDRESS_CLAMP;
            sampler_desc.maxLOD = w32::D3D11_FLOAT32_MAX;
            w32::ComPtr<w32::ID3D11SamplerState> sampler;
            if (!w32::SUCCESS(gpu.device->CreateSamplerState(&sampler_desc, sampler.GetAddressOf())))
                return false;
            gpu.context->CSSetSamplers(0, 1, sampler.GetAddressOf());
            w32::ID3D11ShaderResourceView* views[16]{ inputs[0]->srv(), inputs[1]->srv(), inputs[2]->srv(), inputs[3]->srv(),
                inputs[4]->srv(), inputs[5]->srv(), cube_srv.Get(), cube_srv.Get(), nullptr, inputs[6]->srv() };
            w32::ID3D11UnorderedAccessView* outputs[]{ main.uav(), normal_output.uav(), motion.uav() };
            gpu.context->CSSetShaderResources(0, 16, views);
            gpu.context->CSSetUnorderedAccessViews(0, 3, outputs, nullptr);
            gpu.context->CSSetShader(shader, nullptr, 0);
            gpu.context->Dispatch(2, 2, 1);
            gpu.context->ClearState();
            auto const pixels = read_pixels(gpu, main);
            const float diffuse_linear = linear ? 0.25f : std::pow(0.25f, 1.6f);
            const float environment = linear ? std::pow(0.25f, 1.6f) : std::pow(0.25f * 0.65f, 1.6f);
            const float red_linear = diffuse_linear + 0.125f + (reflections ? 0.2f * environment : 0.0f);
            const float expected = linear ? red_linear : std::pow(red_linear, 1.0f / 1.6f);
            if (pixels.size() != 81)
                return false;
            for (auto const& pixel : pixels)
                if (!std::isfinite(pixel[0]) || std::abs(pixel[0] - expected) > 0.0002f ||
                    std::abs(pixel[1] - 0.0625f) > 0.0002f || pixel[2] != 0 || pixel[3] != 1)
                {
                    std::printf("native composite expected=%f actual=%f green=%f\n", expected, pixel[0], pixel[1]);
                    return false;
                }
            return true;
        }
    }

    bool native_material(Device const& gpu, std::filesystem::path const& root)
    {
        for (bool reflections : { false, true })
            for (bool ssgi : { false, true })
            {
                auto shader = compile_native_shader(gpu, root, L"DeferredCompositeCS.hlsl",
                    { "INTERIOR", reflections ? "DYNAMIC_CUBEMAPS" : nullptr, ssgi ? "SSGI" : nullptr });
                if (!shader.Get())
                    return false;
                for (bool linear : { false, true })
                {
                    const bool result = run(gpu, shader.Get(), linear, reflections);
                    std::printf("native composite LL=%d reflection=%d SSGI-neutral=%d: %s\n", linear, reflections, ssgi, result ? "PASS" : "FAIL");
                    if (!result)
                        return false;
                }
            }
        return true;
    }
}
