#include "render_fixture.h"
#include "render/community_shaders/gpu_data.h"

#include <d3dcompiler.h>

#include <cmath>
#include <cstdio>
#include <fstream>

namespace render_test
{
    namespace
    {
        namespace cs = PLUGIN_NAMESPACE::community_shaders;

        // Use the Windows SDK include interface here (Open/Close), as D3DCompile requires.
        class Includes final : public ID3DInclude
        {
        public:
            explicit Includes(std::filesystem::path root) : m_root(std::move(root)) {}
            HRESULT Open(D3D_INCLUDE_TYPE, char const* file, void const*, void const** data, UINT* bytes) override
            {
                std::ifstream stream(m_root / file, std::ios::binary);
                if (!stream)
                    return E_FAIL;
                const std::string contents((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
                auto* allocation = new char[contents.size()];
                std::memcpy(allocation, contents.data(), contents.size());
                *data = allocation;
                *bytes = static_cast<UINT>(contents.size());
                return S_OK;
            }
            HRESULT Close(void const* data) override
            {
                delete[] static_cast<char const*>(data);
                return S_OK;
            }
        private:
            std::filesystem::path m_root;
        };

    }

    w32::ComPtr<w32::ID3D11ComputeShader> compile_native_shader(Device const& gpu, std::filesystem::path const& root,
        wchar_t const* file, std::initializer_list<char const*> defines)
    {
        Includes includes(root);
        // CS Util::CompileShader adds COMPUTESHADER for cs_5_0 entries.
        std::vector<D3D_SHADER_MACRO> macros{ { "COMPUTESHADER", "" } };
        for (auto* define : defines)
            if (define)
                macros.push_back({ define, "" });
        macros.push_back({ nullptr, nullptr });
        w32::ComPtr<ID3DBlob> code, errors;
        const auto result = D3DCompileFromFile((root / file).c_str(), macros.data(),
            &includes, "main", "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, code.GetAddressOf(), errors.GetAddressOf());
        w32::ComPtr<w32::ID3D11ComputeShader> shader;
        if (FAILED(result))
            std::printf("CS fixture compile failed: %s\n", errors.Get() ? static_cast<char const*>(errors->GetBufferPointer()) : "no diagnostics");
        else
            gpu.device->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, shader.GetAddressOf());
        return shader;
    }

    namespace
    {
        struct Shaders
        {
            w32::ComPtr<w32::ID3D11ComputeShader> prepass, horizontal, vertical, burley;
        };

        bool run(Device const& gpu, Shaders const& shaders, bool linear, bool burley, bool patterned)
        {
            gpu.context->ClearState();
            static constexpr std::uint32_t size = 9;
            RenderTarget main(RenderTargetOptions{ w32::DXGI_FORMAT_R32G32B32A32_FLOAT,
                w32::DXGI_FORMAT_UNKNOWN, true });
            RenderTarget extracted(RenderTargetOptions{ w32::DXGI_FORMAT_R32G32B32A32_FLOAT,
                w32::DXGI_FORMAT_UNKNOWN, true });
            RenderTarget horizontal(RenderTargetOptions{ w32::DXGI_FORMAT_R32G32B32A32_FLOAT,
                w32::DXGI_FORMAT_UNKNOWN, true });
            RenderTarget depth(w32::DXGI_FORMAT_R32G32B32A32_FLOAT, w32::DXGI_FORMAT_UNKNOWN);
            RenderTarget albedo(w32::DXGI_FORMAT_R32G32B32A32_FLOAT, w32::DXGI_FORMAT_UNKNOWN);
            RenderTarget mask(w32::DXGI_FORMAT_R32G32B32A32_FLOAT, w32::DXGI_FORMAT_UNKNOWN);
            RenderTarget normal(w32::DXGI_FORMAT_R32G32B32A32_FLOAT, w32::DXGI_FORMAT_UNKNOWN);
            for (auto* target : { &main, &extracted, &horizontal, &depth, &albedo, &mask, &normal })
                if (!target->init(gpu.device.Get(), size, size))
                    return false;
            const float depth_value[]{ 0.5f, 0, 0, 1 }, albedo_value[]{ 0.5f, 0.4f, 0.3f, 1 }, normal_value[]{ 0.5f, 0.5f, 0, 1 };
            gpu.context->ClearRenderTargetView(depth.rtv(), depth_value);
            gpu.context->ClearRenderTargetView(albedo.rtv(), albedo_value);
            gpu.context->ClearRenderTargetView(normal.rtv(), normal_value);
            std::vector<std::array<float, 4>> colors(size * size), masks(size * size);
            for (std::uint32_t y = 0; y < size; ++y)
                for (std::uint32_t x = 0; x < size; ++x)
                {
                    colors[y * size + x] = { patterned ? (x % 2 ? 0.8f : 0.05f) : 0.25f, 0.15f, 0.05f, 1 };
                    // The first column is a non-SSS material and must keep its original pixels.
                    masks[y * size + x] = { x == 0 ? 0.0f : 1.0f, 1, 0, 1 };
                }
            gpu.context->UpdateSubresource(main.texture(), 0, nullptr, colors.data(), size * 16, 0);
            gpu.context->UpdateSubresource(mask.texture(), 0, nullptr, masks.data(), size * 16, 0);
            cs::SharedData shared;
            shared.dimensions = { size, size, 1.0f / size, 1.0f / size };
            shared.camera = { 100, 1, 99, 100 };
            cs::FeatureData features{};
            features.linear_lighting[0] = linear ? 1 : 0;
            cs::FrameData frame;
            cs::SssData sss;
            sss.kernels[0] = sss.kernels[21] = { patterned ? 0.5f : 1.0f, patterned ? 0.5f : 1.0f, patterned ? 0.5f : 1.0f, 0 };
            if (patterned)
                sss.kernels[1] = sss.kernels[22] = { 0.5f, 0.5f, 0.5f, 1 };
            sss.base_profile = sss.human_profile = { 0.25f, 1, 0, 0 };
            sss.fov = 90;
            sss.samples = 16;
            sss.scatter_mode = burley ? 1 : 2;
            sss.mean_free_path_base = sss.mean_free_path_human = { 1, 0.37f, 0.3f, 2.67f };
            auto shared_cb = constant(gpu, shared), feature_cb = constant(gpu, features), frame_cb = constant(gpu, frame), sss_cb = constant(gpu, sss);
            if (!shared_cb.Get() || !feature_cb.Get() || !frame_cb.Get() || !sss_cb.Get())
                return false;
            gpu.context->CSSetConstantBuffers(5, 1, shared_cb.GetAddressOf());
            gpu.context->CSSetConstantBuffers(6, 1, feature_cb.GetAddressOf());
            gpu.context->CSSetConstantBuffers(12, 1, frame_cb.GetAddressOf());
            gpu.context->CSSetConstantBuffers(1, 1, sss_cb.GetAddressOf());
            w32::D3D11_SAMPLER_DESC desc{};
            desc.filter = w32::D3D11_FILTER_MIN_MAG_MIP_POINT;
            desc.addressU = desc.addressV = desc.addressW = w32::D3D11_TEXTURE_ADDRESS_CLAMP;
            desc.maxLOD = w32::D3D11_FLOAT32_MAX;
            w32::ComPtr<w32::ID3D11SamplerState> sampler;
            if (!w32::SUCCESS(gpu.device->CreateSamplerState(&desc, sampler.GetAddressOf())))
                return false;
            gpu.context->CSSetSamplers(0, 1, sampler.GetAddressOf());
            w32::ID3D11ShaderResourceView* inputs[]{ main.srv(), depth.srv(), mask.srv(), albedo.srv(), normal.srv() };
            gpu.context->CSSetShaderResources(0, 5, inputs);
            auto const dispatch = [&](w32::ID3D11ComputeShader* shader, w32::ID3D11UnorderedAccessView* output)
            {
                gpu.context->CSSetUnorderedAccessViews(0, 1, &output, nullptr);
                gpu.context->CSSetShader(shader, nullptr, 0);
                gpu.context->Dispatch(2, 2, 1);
                output = nullptr;
                gpu.context->CSSetUnorderedAccessViews(0, 1, &output, nullptr);
            };
            dispatch(shaders.prepass.Get(), extracted.uav());
            inputs[0] = extracted.srv();
            gpu.context->CSSetShaderResources(0, 1, inputs);
            dispatch(burley ? shaders.burley.Get() : shaders.horizontal.Get(), burley ? main.uav() : horizontal.uav());
            if (!burley)
            {
                inputs[0] = horizontal.srv();
                gpu.context->CSSetShaderResources(0, 1, inputs);
                dispatch(shaders.vertical.Get(), main.uav());
            }
            gpu.context->ClearState();
            auto const pixels = read_pixels(gpu, main);
            if (pixels.size() != colors.size())
                return false;
            bool changed_skin = false;
            for (std::size_t i = 0; i < pixels.size(); ++i)
            {
                const bool skin_pixel = i % size != 0;
                for (std::size_t channel = 0; channel < 3; ++channel)
                {
                    if (!std::isfinite(pixels[i][channel]) || pixels[i][channel] < 0)
                    {
                        std::printf("non-finite SSS pixel %zu channel %zu: %f\n", i, channel, pixels[i][channel]);
                        return false;
                    }
                    const float difference = std::abs(pixels[i][channel] - colors[i][channel]);
                    if ((!skin_pixel || !patterned) && difference > 0.0002f)
                    {
                        std::printf("SSS pixel %zu channel %zu: expected=%f actual=%f\n", i, channel, colors[i][channel], pixels[i][channel]);
                        return false;
                    }
                    changed_skin |= skin_pixel && difference > 0.001f;
                }
            }
            return !patterned || burley || changed_skin;
        }
    }

    bool native_sss(Device const& gpu, std::filesystem::path const& root)
    {
        Shaders shaders{
            compile_native_shader(gpu, root, L"SubsurfaceScattering/DiffuseExtractionCS.hlsl"),
            compile_native_shader(gpu, root, L"SubsurfaceScattering/SeparableSSSCS.hlsl", { "HORIZONTAL" }),
            compile_native_shader(gpu, root, L"SubsurfaceScattering/SeparableSSSCS.hlsl"),
            compile_native_shader(gpu, root, L"SubsurfaceScattering/SeparableSSSCS.hlsl", { "BURLEY" })
        };
        if (!shaders.prepass.Get() || !shaders.horizontal.Get() || !shaders.vertical.Get() || !shaders.burley.Get())
            return false;
        for (bool linear : { false, true })
            for (bool burley : { false, true })
                for (bool patterned : { false, true })
                {
                    const bool result = run(gpu, shaders, linear, burley, patterned);
                    std::printf("native SSS LL=%d Burley=%d patterned=%d: %s\n", linear, burley, patterned, result ? "PASS" : "FAIL");
                    if (!result)
                        return false;
                }
        return true;
    }
}
