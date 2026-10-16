#include "render/community_shaders/view.h"

#include "render/community_shaders/bridge.h"
#include "render/community_shaders/gpu_data.h"
#include "render/dx11/d3d11_util.h"
#include "render/shader_sources.h"
#include "render/studio/figure_passes.h"

#include <cmath>
#include <cstring>

PLUGIN_NAMESPACE_BEGIN
namespace community_shaders
{
    namespace
    {
        // All offsets below are pinned to the official 1.9.1 PDB, not to current GitHub main.
        template <class T>
        T& field(void* object, std::size_t offset)
        {
            return *reinterpret_cast<T*>(static_cast<std::byte*>(object) + offset);
        }

        class Buffer
        {
        public:
            bool init(REX::W32::ID3D11Device* device, std::uint32_t size)
            {
                m_resource.Reset();
                native.desc = { .byteWidth = size, .usage = REX::W32::D3D11_USAGE_DYNAMIC,
                    .bindFlags = REX::W32::D3D11_BIND_CONSTANT_BUFFER,
                    .cpuAccessFlags = REX::W32::D3D11_CPU_ACCESS_WRITE };
                const auto hr = device->CreateBuffer(&native.desc, nullptr, m_resource.GetAddressOf());
                native.resource = m_resource.Get();
                return REX::W32::SUCCESS(hr) && native.resource;
            }

            bool upload(REX::W32::ID3D11DeviceContext* context, void const* data) const
            {
                REX::W32::D3D11_MAPPED_SUBRESOURCE mapped{};
                if (!REX::W32::SUCCESS(context->Map(native.resource, 0, REX::W32::D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                    return false;
                std::memcpy(mapped.data, data, native.desc.byteWidth);
                context->Unmap(native.resource, 0);
                return true;
            }

            NativeBuffer native;
        private:
            REX::W32::ComPtr<REX::W32::ID3D11Buffer> m_resource;
        };

        struct Resources
        {
            REX::W32::ID3D11Device* owner{};
            std::uint32_t width{}, height{};
            // Slots 1..7 of Lighting.hlsl: motion, normal, albedo, specular, reflectance, masks, masks2.
            std::array<std::unique_ptr<RenderTarget>, 7> material;
            RenderTarget extraction{ RenderTargetOptions{ REX::W32::DXGI_FORMAT_R16G16B16A16_FLOAT,
                REX::W32::DXGI_FORMAT_UNKNOWN, true } };
            RenderTarget horizontal{ RenderTargetOptions{ REX::W32::DXGI_FORMAT_R16G16B16A16_FLOAT,
                REX::W32::DXGI_FORMAT_UNKNOWN, true } };
            RenderTarget resolved{ RenderTargetOptions{ REX::W32::DXGI_FORMAT_R16G16B16A16_FLOAT,
                REX::W32::DXGI_FORMAT_UNKNOWN, true } };
            RenderTarget normal_output{ RenderTargetOptions{ REX::W32::DXGI_FORMAT_R8G8B8A8_UNORM,
                REX::W32::DXGI_FORMAT_UNKNOWN, true } };
            Buffer permutation, shared, features, frame, sss, resolve, strict_lights;
            REX::W32::ComPtr<REX::W32::ID3D11Buffer> frame_backup;
            REX::W32::ComPtr<REX::W32::ID3D11ComputeShader> resolve_shader;
            REX::W32::ComPtr<REX::W32::ID3D11SamplerState> point_sampler;
            REX::W32::ComPtr<REX::W32::ID3D11Texture2D> environment;
            REX::W32::ComPtr<REX::W32::ID3D11ShaderResourceView> environment_srv;

            bool init_environment(REX::W32::ID3D11Device* device)
            {
                environment.Reset();
                environment_srv.Reset();
                const float white[4]{ 1, 1, 1, 1 };
                std::array<REX::W32::D3D11_SUBRESOURCE_DATA, 6> faces;
                for (auto& face : faces)
                    face = { .sysMem = white, .sysMemPitch = sizeof(white) };
                REX::W32::D3D11_TEXTURE2D_DESC desc{
                    .width = 1, .height = 1, .mipLevels = 1, .arraySize = 6,
                    .format = REX::W32::DXGI_FORMAT_R32G32B32A32_FLOAT, .sampleDesc = { 1, 0 },
                    .usage = REX::W32::D3D11_USAGE_IMMUTABLE, .bindFlags = REX::W32::D3D11_BIND_SHADER_RESOURCE,
                    .miscFlags = REX::W32::D3D11_RESOURCE_MISC_TEXTURECUBE
                };
                return REX::W32::SUCCESS(device->CreateTexture2D(&desc, faces.data(), environment.GetAddressOf())) &&
                    REX::W32::SUCCESS(device->CreateShaderResourceView(environment.Get(), nullptr, environment_srv.GetAddressOf()));
            }

            bool init(REX::W32::ID3D11Device* device, std::uint32_t w, std::uint32_t h)
            {
                if (owner == device && width == w && height == h)
                    return true;
                width = height = 0;
                if (owner != device)
                {
                    owner = nullptr;
                    resolve_shader.Reset();
                    point_sampler.Reset();
                    frame_backup.Reset();
                    const REX::W32::D3D11_BUFFER_DESC backup_desc{
                        .byteWidth = sizeof(FrameData), .usage = REX::W32::D3D11_USAGE_DEFAULT
                    };
                    if (!REX::W32::SUCCESS(device->CreateBuffer(&backup_desc, nullptr, frame_backup.GetAddressOf())))
                        return false;
                    if (!create_compute_shader(device, render_shaders::StudioEncode, "cs_main", "studio menu encoding",
                        resolve_shader.GetAddressOf()))
                        return false;
                    REX::W32::D3D11_SAMPLER_DESC sampler{};
                    sampler.filter = REX::W32::D3D11_FILTER_MIN_MAG_MIP_POINT;
                    sampler.addressU = sampler.addressV = sampler.addressW = REX::W32::D3D11_TEXTURE_ADDRESS_CLAMP;
                    sampler.maxLOD = REX::W32::D3D11_FLOAT32_MAX;
                    if (!REX::W32::SUCCESS(device->CreateSamplerState(&sampler, point_sampler.GetAddressOf())) ||
                        !permutation.init(device, 32) || !shared.init(device, sizeof(SharedData)) ||
                        !features.init(device, sizeof(FeatureData)) || !frame.init(device, sizeof(FrameData)) ||
                        !sss.init(device, sizeof(SssData)) || !resolve.init(device, sizeof(EncodingData)) ||
                        !strict_lights.init(device, 0x4C0) || !init_environment(device))
                        return false;
                }
                static constexpr std::array formats{
                    REX::W32::DXGI_FORMAT_R16G16_FLOAT,
                    REX::W32::DXGI_FORMAT_R8G8B8A8_UNORM, REX::W32::DXGI_FORMAT_R8G8B8A8_UNORM,
                    REX::W32::DXGI_FORMAT_R16G16B16A16_FLOAT, REX::W32::DXGI_FORMAT_R16G16B16A16_FLOAT,
                    REX::W32::DXGI_FORMAT_R8G8B8A8_UNORM, REX::W32::DXGI_FORMAT_R8G8B8A8_UNORM
                };
                for (std::size_t i = 0; i < material.size(); ++i)
                {
                    if (!material[i])
                        material[i] = std::make_unique<RenderTarget>(RenderTargetOptions{
                            formats[i], REX::W32::DXGI_FORMAT_UNKNOWN, i == 0 });
                    if (!material[i]->init(device, w, h))
                        return false;
                }
                if (!extraction.init(device, w, h) || !horizontal.init(device, w, h) || !resolved.init(device, w, h) ||
                    !normal_output.init(device, w, h))
                    return false;
                owner = device;
                width = w;
                height = h;
                logger::info("CP-CS private view {}x{}: 8 material targets, readable depth, SSS scratch; no world history dispatch", w, h);
                return true;
            }
        };

        Resources& resources()
        {
            static Resources instance;
            return instance;
        }

        void isolate_features(FeatureData& data)
        {
            // GetFeatureBufferData(false) supplies CS's menu skylighting fallback. Keep material,
            // hair, skin and Linear Lighting settings, but disable effects requiring a world view.
            data.cubemaps.fill(std::byte{});
            data.terrain_shadows.fill(std::byte{});
            data.lights.fill(std::byte{}); // Zero ClusterSize makes GetClusterIndex return false.
            data.wetness.fill(std::byte{});
            data.clouds.fill(std::byte{});
            field<std::uint32_t>(data.ibl.data(), 0) = 0;
            data.terrain_blending.fill(std::byte{});
            data.fog.fill(std::byte{});
            // Preserve particle material intensity; only world rain/sun/volumetric effects are off.
            field<std::uint32_t>(data.effects.data(), 0x14) = 0;
            field<std::uint32_t>(data.effects.data(), 0x30) = 0;
            field<std::uint32_t>(data.effects.data(), 0x60) = 0;
        }

        void clear_compute(REX::W32::ID3D11DeviceContext* context)
        {
            REX::W32::ID3D11ShaderResourceView* srvs[16]{};
            REX::W32::ID3D11UnorderedAccessView* uavs[8]{};
            context->CSSetShaderResources(0, 16, srvs);
            context->CSSetUnorderedAccessViews(0, 8, uavs, nullptr);
            context->CSSetShader(nullptr, nullptr, 0);
        }

        bool unavailable(std::string_view reason)
        {
            static std::unordered_set<std::string_view> reported;
            if (reported.insert(reason).second)
                logger::warn("CP-CS view bypassed: {}; native forward fallback", reason);
            return false;
        }
    }

    // The snapshot contains 16-byte-aligned GPU data. Padding here has no external ABI.
#pragma warning(push)
#pragma warning(disable: 4324)
    struct View::State
    {
        FigurePassRequest const& request;
        Resources& gpu = resources();
        void* cs{};
        void* deferred{};
        void* skin{};
        void* lights{};
        void* cache{};
        bool entered{}, deferred_phase{}, alpha_blend{}, missing_shader{}, upload_failed{}, draw_seen{};
        bool native_sss{}, native_deferred{};
        bool classify_skin{}, beast_race{};
        std::array<std::byte, 14> saved_early{};
        std::array<std::byte, 8> saved_descriptors{};
        std::array<std::byte, 0x84> saved_parameters{};
        std::array<std::byte, 0x4C0> saved_strict_data{};
        std::array<std::byte, 0x1C> saved_light_cache{};
        NativeBuffer* saved_strict_buffer{};
        RE::ShadowSceneNode* saved_scene{};
        FrameData saved_frame{};
        FrameData frame_data{};
        SharedData shared_data{};
        REX::W32::ID3D11Buffer** native_frame_address{};
        REX::W32::ID3D11Buffer* saved_frame_buffer{};
        RE::BSGraphics::RendererShadowState::FLAT_RUNTIME_DATA& shadow;
        RE::BSGraphics::RendererShadowState::FLAT_RUNTIME_DATA saved_shadow;
        RE::NiTransform saved_ambient;
        std::uint32_t saved_technique{};
        bool saved_interior{};
        bool* temporal_enabled{};
        bool saved_temporal{};
        FeatureData feature_data{};
        DrawOverride callback{ this, [](void* owner) { static_cast<State*>(owner)->after_draw(); } };

        explicit State(FigurePassRequest const& value) : request(value),
            shadow(RE::BSGraphics::RendererShadowState::GetSingleton()->GetRuntimeData()),
            saved_shadow(shadow) {}

        bool initialize()
        {
            auto& api = bridge();
            cs = api.object(State_Rva);
            deferred = api.object(Deferred_Rva);
            skin = reinterpret_cast<void*>(api.base + Sss_Rva);
            lights = reinterpret_cast<void*>(api.base + Lights_Rva);
            cache = api.object(Shader_Cache_Rva);
            if (!cs || !deferred || !cache || !request.camera || !request.target->uav() ||
                !request.target->depth_srv() || current_view || field<bool>(deferred, 0x16E8))
                return unavailable("native view resources unavailable or another view is active");
            if (!api.function<bool (*)(void*)>(Function::e_cache_enabled)(cache))
                return false; // Explicitly disabled CS is an ordinary forward view.
            if (!request.light_slots || request.light_count != 3 || !request.directional_light ||
                !std::ranges::all_of(std::span{ request.light_slots, request.light_count }, [](auto* light) { return light != nullptr; }))
                return unavailable("studio light rig not ready");
            // LLF selects strict lights from this global accumulator. A world accumulator would
            // silently discard our injected key/fill lights in favour of its prebuilt light clusters.
            saved_scene = request.accumulator->GetRuntimeData().activeShadowSceneNode;
            auto* scene = saved_scene;
            auto const& scenes = RE::BSShaderManager::State::GetSingleton().shadowSceneNode;
            if (!scene || scene == scenes[0])
            {
                scene = nullptr;
                for (std::size_t i = 1; i < std::size(scenes); ++i)
                    if (scenes[i] && scenes[i] != scenes[0])
                    {
                        scene = scenes[i];
                        break;
                    }
                if (!scene)
                    return unavailable("no initialized non-world shadow scene");
            }
            auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
            auto* device = renderer->GetRuntimeData().forwarder;
            if (!gpu.init(device, request.target->width(), request.target->height()))
                return unavailable("private GPU resource allocation failed");

            using Features = std::pair<unsigned char*, std::size_t> (*)(bool);
            auto const packed = api.function<Features>(Function::e_features)(false);
            if (!packed.first || packed.second != sizeof(FeatureData))
                return unavailable("feature buffer layout mismatch");
            std::memcpy(&feature_data, packed.first, sizeof(feature_data));
            isolate_features(feature_data);

            const auto frame_location = field<std::uintptr_t>(reinterpret_cast<void*>(api.base), Per_Frame_Relocation_Rva);
            native_frame_address = reinterpret_cast<REX::W32::ID3D11Buffer**>(frame_location);
            if (!native_frame_address || !*native_frame_address)
                return unavailable("native frame buffer unavailable");
            saved_frame_buffer = *native_frame_address;
            REX::W32::D3D11_BUFFER_DESC frame_desc{};
            saved_frame_buffer->GetDesc(&frame_desc);
            if (frame_desc.byteWidth != sizeof(FrameData))
                return unavailable("frame buffer layout mismatch");

            auto* cached_frame = reinterpret_cast<FrameData*>(api.base + Frame_Cache_Rva);
            saved_frame = *cached_frame;
            std::memcpy(saved_early.data(), static_cast<std::byte*>(cs) + 0xA, saved_early.size());
            std::memcpy(saved_descriptors.data(), static_cast<std::byte*>(cs) + 0x38, saved_descriptors.size());
            std::memcpy(saved_parameters.data(), static_cast<std::byte*>(cs) + 0x18C, saved_parameters.size());
            native_sss = field<bool>(skin, 0x3C2);
            native_deferred = field<bool>(deferred, 0x16E8);
            if (field<bool>(lights, 8))
            {
                saved_strict_buffer = field<NativeBuffer*>(lights, 0x5C0);
                if (!saved_strict_buffer || saved_strict_buffer->desc.byteWidth != 0x4C0)
                    return unavailable("strict-light buffer layout mismatch");
                std::memcpy(saved_strict_data.data(), static_cast<std::byte*>(lights) + 0x100, saved_strict_data.size());
                std::memcpy(saved_light_cache.data(), static_cast<std::byte*>(lights) + 0x624, saved_light_cache.size());
                if (!gpu.strict_lights.upload(request.context, saved_strict_data.data()))
                    return false;
            }
            auto& manager = RE::BSShaderManager::State::GetSingleton();
            saved_ambient = manager.directionalAmbientTransform;
            saved_technique = manager.currentShaderTechnique;
            saved_interior = manager.interior;
            entered = true;
            // Also preserve the actual bytes: an engine constant-group alias can still refer to
            // the original frame CB while the global pointer is redirected. This copy stays on GPU.
            request.context->CopyResource(gpu.frame_backup.Get(), saved_frame_buffer);

            // Some UI callers leave their accumulator pointing at the main scene. Only this
            // window borrows an initialized non-world scene; no scene graph is reparented.
            request.accumulator->GetRuntimeData().activeShadowSceneNode = scene;

            // Native camera setup writes into the substitute frame buffer, never the world's CB.
            *native_frame_address = gpu.frame.native.resource;
            (*native_frame_address)->AddRef(); // The temporary native global holds its own reference.
            field<NativeBuffer*>(cs, 0x1B8) = &gpu.permutation.native;
            field<NativeBuffer*>(cs, 0x1C0) = &gpu.shared.native;
            field<NativeBuffer*>(cs, 0x1C8) = &gpu.features.native;
            if (saved_strict_buffer)
                field<NativeBuffer*>(lights, 0x5C0) = &gpu.strict_lights.native;
            field<bool>(cs, 0x1AC) = false;
            field<bool>(cs, 0x1AD) = false;
            field<std::uint32_t>(cs, 0x1D8) &= ~3u; // Local menu view, no reflection/world flags.
            field<bool>(skin, 0x3C2) = false;
            manager.interior = true;
            for (auto& row : manager.directionalAmbientTransform.rotate.entry)
                std::fill(std::begin(row), std::end(row), 0.0f);
            manager.directionalAmbientTransform.translate = { 0.25f, 0.25f, 0.28f };

            // The post-interface hook can run after CS restored the world's TAA flag. Keep this
            // view's native camera setup unjittered regardless of detour installation order.
            if (auto* image_space = RE::ImageSpaceManager::GetSingleton())
                if (auto* taa = image_space->GetRuntimeData().BSImagespaceShaderISTemporalAA)
                {
                    temporal_enabled = &taa->taaEnabled;
                    saved_temporal = *temporal_enabled;
                    *temporal_enabled = false;
                }

            RE::BSGraphics::State::GetSingleton()->SetCameraData(request.camera, 0);
            auto const& eye = shadow.cameraData.getEye();
            FrameData frame = saved_frame;
            frame.view = eye.viewMat;
            frame.projection = eye.projMatrixUnjittered;
            frame.view_projection = eye.viewProjMatrixUnjittered;
            frame.unjittered = frame.previous_unjittered = frame.view_projection;
            frame.projection_unjittered = frame.projection;
            frame.projection_inverse = frame.projection_unjittered_inverse = frame.projection.Invert();
            frame.view_inverse = frame.view.Invert();
            frame.view_projection_inverse = frame.view_projection.Invert();
            auto const& pos = shadow.posAdjust.getEye();
            frame.position = frame.previous_position = { pos.x, pos.y, pos.z, 0.0f };
            frame.params.y = 1.0f;
            frame.resolution = frame.resolution_inverse = { 1, 1, 1, 1 };
            *cached_frame = frame;

            SharedData shared;
            auto const& frustum = request.camera->GetRuntimeData2().viewFrustum;
            if (!std::isfinite(frustum.fNear) || !std::isfinite(frustum.fFar) ||
                frustum.fNear <= 0.0f || frustum.fFar <= frustum.fNear ||
                !std::isfinite(frame.projection.m[0][0]) || frame.projection.m[0][0] == 0.0f)
                return unavailable("invalid menu camera projection");
            shared.camera = { frustum.fFar, frustum.fNear, frustum.fFar - frustum.fNear, frustum.fFar * frustum.fNear };
            shared.dimensions = { static_cast<float>(gpu.width), static_cast<float>(gpu.height),
                1.0f / gpu.width, 1.0f / gpu.height };
            shared.timer = field<float>(cs, 0x80);
            shared.active_frame = field<std::uint32_t>(cs, 0x214);
            // A constant SH radiance has c0 = radiance / Y00. Directional terms stay zero.
            shared.ambient_r.x = shared.ambient_g.x = 0.25f / 0.282095f;
            shared.ambient_b.x = 0.28f / 0.282095f;
            if (auto* directional = request.directional_light)
            {
                auto const& direction = directional->GetWorldDirection();
                auto const& light = directional->GetLightRuntimeData();
                shared.light_direction = { -direction.x, -direction.y, -direction.z, 0.0f };
                shared.light_color = { light.diffuse.red * light.fade, light.diffuse.green * light.fade,
                    light.diffuse.blue * light.fade, 1.0f };
            }
            // MSVC member aggregate return: RCX=this, RDX=output. A free function returning
            // Vector4 would put its hidden return pointer in RCX and corrupt the feature object.
            using Hdr = void (*)(void*, Vector4*);
            api.function<Hdr>(Function::e_hdr_data)(reinterpret_cast<void*>(api.base + Hdr_Rva), &shared.hdr);
            classify_skin = request.source && request.source->GetRace();
            if (classify_skin)
            {
                auto* keyword = field<RE::BGSKeyword*>(skin, 0x3F8);
                beast_race = !keyword || request.source->GetRace()->HasKeyword(keyword);
            }
            frame_data = frame;
            shared_data = shared;
            if (!gpu.frame.upload(request.context, &frame) || !gpu.shared.upload(request.context, &shared) ||
                !gpu.features.upload(request.context, &feature_data))
                return unavailable("view constant upload failed");
            current_view = &callback;
            return true;
        }

        ~State()
        {
            if (!entered)
                return;
            current_view = nullptr;
            if (deferred_phase)
                bridge().function<void (*)(void*)>(Function::e_blend_forward)(deferred);
            field<bool>(deferred, 0x16E8) = native_deferred;
            field<bool>(skin, 0x3C2) = native_sss;
            std::memcpy(static_cast<std::byte*>(cs) + 0xA, saved_early.data(), saved_early.size());
            std::memcpy(static_cast<std::byte*>(cs) + 0x38, saved_descriptors.data(), saved_descriptors.size());
            std::memcpy(static_cast<std::byte*>(cs) + 0x18C, saved_parameters.data(), saved_parameters.size());
            *reinterpret_cast<FrameData*>(bridge().base + Frame_Cache_Rva) = saved_frame;
            auto* temporary_frame = *native_frame_address;
            *native_frame_address = saved_frame_buffer;
            if (temporary_frame)
                temporary_frame->Release();
            request.context->CopyResource(saved_frame_buffer, gpu.frame_backup.Get());
            request.accumulator->GetRuntimeData().activeShadowSceneNode = saved_scene;
            if (saved_strict_buffer)
            {
                field<NativeBuffer*>(lights, 0x5C0) = saved_strict_buffer;
                std::memcpy(static_cast<std::byte*>(lights) + 0x100, saved_strict_data.data(), saved_strict_data.size());
                std::memcpy(static_cast<std::byte*>(lights) + 0x624, saved_light_cache.data(), saved_light_cache.size());
            }
            auto& manager = RE::BSShaderManager::State::GetSingleton();
            manager.directionalAmbientTransform = saved_ambient;
            manager.currentShaderTechnique = saved_technique;
            manager.interior = saved_interior;
            if (temporal_enabled)
                *temporal_enabled = saved_temporal;
            shadow = saved_shadow;
        }

        void bind_targets()
        {
            REX::W32::ID3D11ShaderResourceView* depth = nullptr;
            request.context->PSSetShaderResources(17, 1, &depth);
            REX::W32::ID3D11RenderTargetView* targets[8]{ request.target->rtv() };
            if (deferred_phase)
                for (std::size_t i = 0; i < gpu.material.size(); ++i)
                    targets[i + 1] = gpu.material[i]->rtv();
            request.context->OMSetRenderTargets(deferred_phase ? 8 : 1, targets,
                alpha_blend ? request.target->read_only_dsv() : request.target->dsv());
            depth = alpha_blend ? request.target->depth_srv() : nullptr;
            request.context->PSSetShaderResources(17, 1, &depth);
            shadow.stateUpdateFlags.reset(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET);
        }

        void bind_constants()
        {
            if (saved_strict_buffer)
            {
                // LLF normally binds b3 only once per frame. Our substitute must also bind when
                // the world or the native inventory model already consumed that frame checker.
                auto* strict = gpu.strict_lights.native.resource;
                request.context->PSSetConstantBuffers(3, 1, &strict);
            }
            REX::W32::ID3D11Buffer* buffers[]{ gpu.permutation.native.resource, gpu.shared.native.resource,
                gpu.features.native.resource };
            request.context->VSSetConstantBuffers(4, 3, buffers);
            request.context->PSSetConstantBuffers(4, 3, buffers);
            request.context->CSSetConstantBuffers(5, 2, buffers + 1);
            auto* frame = gpu.frame.native.resource;
            request.context->VSSetConstantBuffers(12, 1, &frame);
            request.context->PSSetConstantBuffers(12, 1, &frame);
            request.context->CSSetConstantBuffers(12, 1, &frame);
        }

        void after_draw()
        {
            draw_seen = true;
            if (classify_skin && deferred_phase)
            {
                auto& flags = field<std::uint32_t>(cs, 0x1D8);
                flags = beast_race ? flags | 4u : flags & ~4u;
            }
            // Always upload: the native CPU cache is shared by world and menu draws. A prior
            // native upload must not make a fresh private buffer look up to date.
            upload_failed |= !gpu.permutation.upload(request.context, static_cast<std::byte*>(cs) + 0x1D0);
            // Native dirty-state work can also rewrite the per-frame buffer. Reapply this view's
            // unjittered matrices and full-resolution ratios after it, rather than only rebinding.
            upload_failed |= !gpu.frame.upload(request.context, &frame_data) ||
                !gpu.shared.upload(request.context, &shared_data) || !gpu.features.upload(request.context, &feature_data);
            bind_constants();
            bind_targets();
            REX::W32::ID3D11ShaderResourceView* clusters[3]{};
            request.context->PSSetShaderResources(35, 3, clusters);
            REX::W32::ID3D11ShaderResourceView* environment[]{ gpu.environment_srv.Get(), gpu.environment_srv.Get() };
            request.context->PSSetShaderResources(30, 2, environment);
            if (deferred_phase)
            {
                auto* shader = field<RE::BSShader*>(cs, 0x10);
                using GetPixel = RE::BSGraphics::PixelShader* (*)(void*, RE::BSShader const&, std::uint32_t);
                auto* replacement = shader ? bridge().function<GetPixel>(Function::e_pixel_shader)(cache, *shader,
                    field<std::uint32_t>(cs, 0x198)) : nullptr;
                REX::W32::ComPtr<REX::W32::ID3D11PixelShader> bound;
                request.context->PSGetShader(bound.GetAddressOf(), nullptr, nullptr);
                missing_shader |= !replacement || replacement->shader != bound.Get();
            }
        }

        void begin_deferred()
        {
            bridge().function<void (*)(void*)>(Function::e_blend_deferred)(deferred);
            deferred_phase = true;
            field<bool>(deferred, 0x16E8) = true;
            constexpr float zero[4]{};
            for (auto const& target : gpu.material)
                request.context->ClearRenderTargetView(target->rtv(), zero);
            bind_targets();
            invalidate_technique();
        }

        void invalidate_technique()
        {
            RE::BSShaderManager::State::GetSingleton().currentShaderTechnique = std::numeric_limits<std::uint32_t>::max();
            field<std::uint32_t>(cs, 0x19C) = field<std::uint32_t>(cs, 0x1A0) = std::numeric_limits<std::uint32_t>::max();
        }

        void begin_forward(bool blend)
        {
            if (deferred_phase)
            {
                bridge().function<void (*)(void*)>(Function::e_blend_forward)(deferred);
                deferred_phase = false;
                field<bool>(deferred, 0x16E8) = false;
                invalidate_technique();
            }
            alpha_blend = blend;
            bind_targets();
        }

        void dispatch(REX::W32::ID3D11ComputeShader* shader)
        {
            request.context->CSSetShader(shader, nullptr, 0);
            request.context->Dispatch((gpu.width + 7) / 8, (gpu.height + 7) / 8, 1);
            REX::W32::ID3D11UnorderedAccessView* null_uav = nullptr;
            request.context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
        }

        bool scatter()
        {
            if (!field<bool>(skin, 8) || !field<bool>(skin, 0x3C2))
                return true;
            using GetCompute = REX::W32::ID3D11ComputeShader* (*)(void*);
            auto& api = bridge();
            auto* prepass = api.function<GetCompute>(Function::e_sss_pre)(skin);
            const int mode = field<int>(skin, 0x58);
            auto* first = api.function<GetCompute>(mode == 0 ? Function::e_sss_horizontal : Function::e_sss_burley)(skin);
            auto* second = mode == 0 ? api.function<GetCompute>(Function::e_sss_vertical)(skin) : nullptr;
            if (!prepass || !first || (mode == 0 && !second) || (mode != 0 && mode != 1))
                return false;

            SssData data;
            std::memcpy(&data, static_cast<std::byte*>(skin) + 0xD0, sizeof(data));
            data.fov = std::atan(1.0f / shadow.cameraData.getEye().projMat.m[0][0]) * 2.0f * (180.0f / 3.14159265359f);
            data.base_profile = { field<float>(skin, 0x60), field<float>(skin, 0x64), 0, 0 };
            data.human_profile = { field<float>(skin, 0x80), field<float>(skin, 0x84), 0, 0 };
            data.samples = field<std::uint32_t>(skin, 0xA0);
            data.scatter_mode = mode == 0 ? static_cast<std::uint32_t>(std::clamp(field<int>(skin, 0x5C), 0, 2)) : 1;
            std::memcpy(&data.mean_free_path_base, static_cast<std::byte*>(skin) + 0xA4, 16);
            std::memcpy(&data.mean_free_path_human, static_cast<std::byte*>(skin) + 0xB4, 16);
            if (!gpu.sss.upload(request.context, &data))
                return false;
            auto* buffer = gpu.sss.native.resource;
            request.context->CSSetConstantBuffers(1, 1, &buffer);
            auto* sampler = gpu.point_sampler.Get();
            request.context->CSSetSamplers(0, 1, &sampler);
            REX::W32::ID3D11ShaderResourceView* inputs[]{ request.target->srv(), request.target->depth_srv(),
                gpu.material[5]->srv(), gpu.material[2]->srv(), gpu.material[1]->srv() };
            request.context->CSSetShaderResources(0, 5, inputs);
            auto* output = gpu.extraction.uav();
            request.context->CSSetUnorderedAccessViews(0, 1, &output, nullptr);
            dispatch(prepass);
            inputs[0] = gpu.extraction.srv();
            request.context->CSSetShaderResources(0, 1, inputs);
            output = mode == 0 ? gpu.horizontal.uav() : request.target->uav();
            request.context->CSSetUnorderedAccessViews(0, 1, &output, nullptr);
            dispatch(first);
            if (mode == 0)
            {
                inputs[0] = gpu.horizontal.srv();
                request.context->CSSetShaderResources(0, 1, inputs);
                output = request.target->uav();
                request.context->CSSetUnorderedAccessViews(0, 1, &output, nullptr);
                dispatch(second);
            }
            return true;
        }

        bool resolve_material()
        {
            if (upload_failed)
                return unavailable("permutation constant upload failed");
            if (missing_shader)
            {
                static bool reported = false;
                if (!reported)
                    logger::info("CP-CS deferred shaders not ready; redraw native forward this frame, retry on next frame");
                reported = true;
                return false;
            }
            request.context->OMSetRenderTargets(0, nullptr, nullptr);
            REX::W32::ID3D11ShaderResourceView* null_resources[128]{};
            request.context->PSSetShaderResources(0, 128, null_resources);
            clear_compute(request.context);
            bind_constants();
            const bool scattered = scatter();
            clear_compute(request.context);
            if (!scattered)
                return unavailable("SSS compute shader unavailable");
            using GetComposite = REX::W32::ID3D11ComputeShader* (*)(void*);
            auto* composite = bridge().function<GetComposite>(Function::e_composite)(deferred);
            if (!composite)
                return unavailable("native material composite shader unavailable");
            // Run only CS's material resolve. SSGI inputs are neutral, the reflection environment
            // belongs to this studio, and output normals/motion are private scratch allocations.
            REX::W32::ID3D11ShaderResourceView* material_inputs[16]{
                gpu.material[3]->srv(), gpu.material[2]->srv(), gpu.material[1]->srv(), gpu.material[5]->srv(),
                request.target->depth_srv(), gpu.material[4]->srv(), gpu.environment_srv.Get(), gpu.environment_srv.Get(),
                nullptr, gpu.material[6]->srv()
            };
            REX::W32::ID3D11UnorderedAccessView* material_outputs[]{ request.target->uav(),
                gpu.normal_output.uav(), gpu.material[0]->uav() };
            auto* sampler = gpu.point_sampler.Get();
            request.context->CSSetSamplers(0, 1, &sampler);
            request.context->CSSetShaderResources(0, 16, material_inputs);
            request.context->CSSetUnorderedAccessViews(0, 3, material_outputs, nullptr);
            dispatch(composite);
            clear_compute(request.context);
            EncodingData constants{ gpu.width, gpu.height, feature_data.linear_lighting[0] };
            if (!gpu.resolve.upload(request.context, &constants))
                return false;
            auto* buffer = gpu.resolve.native.resource;
            request.context->CSSetConstantBuffers(0, 1, &buffer);
            auto* input = request.target->srv();
            request.context->CSSetShaderResources(0, 1, &input);
            auto* output = gpu.resolved.uav();
            request.context->CSSetUnorderedAccessViews(0, 1, &output, nullptr);
            dispatch(gpu.resolve_shader.Get());
            clear_compute(request.context);
            request.context->CopyResource(request.target->texture(), gpu.resolved.texture());
            static std::uint32_t completed = 0;
            if (++completed % 120 == 1)
                logger::info("CP-CS material view frame={} SSS={} mode={} profile={} output=menu-forward LL={} targets=private world-history=untouched",
                    completed, field<bool>(skin, 8) && field<bool>(skin, 0x3C2), field<int>(skin, 0x58),
                    classify_skin ? (beast_race ? "beast" : "human") : "native", constants.linear_lighting);
            return true;
        }

        bool resolve()
        {
            bool complete;
            {
                // Material setters may cache texture identity across opaque/transparent passes.
                // Compute unbinding must not leave that cache pointing at now-empty PS slots.
                D3D11StateCapture material_bindings(request.context);
                complete = resolve_material();
            }
            begin_forward(false);
            return complete;
        }
    };
#pragma warning(pop)

    View::View(FigurePassRequest const& request)
    {
        if (!bridge().ready)
            return;
        auto state = std::make_unique<State>(request);
        if (state->initialize())
            m_state = std::move(state);
    }

    View::~View() = default;
    bool View::active() const { return m_state != nullptr; }
    void View::begin_deferred() { m_state->begin_deferred(); }
    bool View::resolve() { return m_state->resolve(); }
    void View::begin_forward(bool alpha_blend) { m_state->begin_forward(alpha_blend); }
    void View::before_pass() { m_state->draw_seen = false; }
    void View::after_pass()
    {
        // A failed BeginTechnique can return before SetDirtyStates/State::Draw is reached.
        // That missing geometry also invalidates this frame's deferred material result.
        m_state->missing_shader |= m_state->deferred_phase && !m_state->draw_seen;
    }
}
PLUGIN_NAMESPACE_END
