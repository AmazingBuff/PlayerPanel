//
// Stage 0 spike — see the header and docs/plans/panel-render-2b.md. Disposable.
//

#include "render/spike/engine_pass_spike.h"

#include "preview/preview_actor.h"
#include "render/dx11/d3d11_util.h"

#include <fstream>

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // One square target: the gate is "did the pass land here", not "is it framed".
    constexpr std::uint32_t Spike_Target_Size = 512;

    // Signature of RE::BSBatchRenderer::SetupAndDrawPass: an x64 member call, so the instance is the
    // first argument.
    using DrawFn = void (*)(RE::BSBatchRenderer*, RE::BSRenderPass*, std::uint32_t, bool, std::uint32_t);

    DrawFn s_original = nullptr;

    // The armed geometry. Written on the game thread and read on the render thread; a plain pointer
    // is acceptable only because this is a spike that fires once, and the pointer stays valid for as
    // long as the preview lives.
    RE::BSGeometry* s_target = nullptr;
    bool s_armed = false;
    bool s_done = false;

    REX::W32::ID3D11Texture2D* s_colour = nullptr;
    REX::W32::ID3D11RenderTargetView* s_colour_view = nullptr;
    REX::W32::ID3D11Texture2D* s_depth = nullptr;
    REX::W32::ID3D11DepthStencilView* s_depth_view = nullptr;

    void dump_target_tga(REX::W32::ID3D11Device* device, REX::W32::ID3D11DeviceContext* context,
        REX::W32::ID3D11Texture2D* texture, char const* name)
    {
        REX::W32::D3D11_TEXTURE2D_DESC desc{};
        texture->GetDesc(&desc);

        REX::W32::D3D11_TEXTURE2D_DESC staging_desc = desc;
        staging_desc.usage = REX::W32::D3D11_USAGE_STAGING;
        staging_desc.bindFlags = 0;
        staging_desc.cpuAccessFlags = REX::W32::D3D11_CPU_ACCESS_READ;
        staging_desc.miscFlags = 0;
        REX::W32::ID3D11Texture2D* staging = nullptr;
        if (!REX::W32::SUCCESS(device->CreateTexture2D(&staging_desc, nullptr, &staging)) || !staging)
            return;

        context->CopyResource(staging, texture);
        REX::W32::D3D11_MAPPED_SUBRESOURCE mapped{};
        if (!REX::W32::SUCCESS(context->Map(staging, 0, REX::W32::D3D11_MAP_READ, 0, &mapped)) || !mapped.data)
        {
            staging->Release();
            return;
        }

        std::optional<std::filesystem::path> const directory = logger::log_directory();
        if (directory)
        {
            std::filesystem::path const path = *directory / (std::string(name) + ".tga");
            std::ofstream file(path, std::ios::binary);
            if (file)
            {
                std::uint8_t header[18]{};
                header[2] = 2;  // uncompressed truecolour
                header[12] = static_cast<std::uint8_t>(desc.width & 0xFF);
                header[13] = static_cast<std::uint8_t>((desc.width >> 8) & 0xFF);
                header[14] = static_cast<std::uint8_t>(desc.height & 0xFF);
                header[15] = static_cast<std::uint8_t>((desc.height >> 8) & 0xFF);
                header[16] = 32;   // bits per pixel
                header[17] = 0x28; // top-down, 8 alpha bits
                file.write(reinterpret_cast<char const*>(header), sizeof(header));

                bool const rgba_memory = desc.format == REX::W32::DXGI_FORMAT_R8G8B8A8_UNORM ||
                                         desc.format == REX::W32::DXGI_FORMAT_R8G8B8A8_TYPELESS;
                for (std::uint32_t row = 0; row < desc.height; ++row)
                {
                    std::uint8_t const* const row_bytes = static_cast<std::uint8_t const*>(mapped.data) +
                        static_cast<std::size_t>(row) * mapped.rowPitch;
                    for (std::uint32_t column = 0; column < desc.width; ++column)
                    {
                        std::uint32_t texel = 0;
                        std::memcpy(&texel, row_bytes + static_cast<std::size_t>(column) * 4u, sizeof(texel));
                        std::uint8_t bgra[4];
                        if (rgba_memory)
                        {
                            bgra[0] = static_cast<std::uint8_t>((texel >> 16) & 0xFF);
                            bgra[1] = static_cast<std::uint8_t>((texel >> 8) & 0xFF);
                            bgra[2] = static_cast<std::uint8_t>(texel & 0xFF);
                            bgra[3] = static_cast<std::uint8_t>((texel >> 24) & 0xFF);
                        }
                        else
                        {
                            bgra[0] = static_cast<std::uint8_t>(texel & 0xFF);
                            bgra[1] = static_cast<std::uint8_t>((texel >> 8) & 0xFF);
                            bgra[2] = static_cast<std::uint8_t>((texel >> 16) & 0xFF);
                            bgra[3] = static_cast<std::uint8_t>((texel >> 24) & 0xFF);
                        }
                        file.write(reinterpret_cast<char const*>(bgra), sizeof(bgra));
                    }
                }
                logger::info("Panel spike: dumped {} ({}x{}) to {}", name, desc.width, desc.height, path.string());
            }
        }

        context->Unmap(staging, 0);
        staging->Release();
    }

    RE::BSGeometry* find_skinned_geometry(RE::TESObjectREFR& ref)
    {
        RE::NiAVObject* const root = ref.GetCurrent3D();
        if (!root)
            return nullptr;

        RE::BSGeometry* found = nullptr;
        RE::BSVisit::TraverseScenegraphGeometries(root, [&found](RE::BSGeometry* geometry) {
            if (found || !geometry)
                return RE::BSVisit::BSVisitControl::kStop;

            RE::BSGeometry::GEOMETRY_RUNTIME_DATA const& runtime = geometry->GetGeometryRuntimeData();
            if (runtime.shaderProperty && runtime.skinInstance)
                found = geometry;

            return found ? RE::BSVisit::BSVisitControl::kStop : RE::BSVisit::BSVisitControl::kContinue;
        });
        return found;
    }

    bool ensure_target(REX::W32::ID3D11Device* device)
    {
        if (s_colour && s_colour_view)
            return true;

        REX::W32::D3D11_TEXTURE2D_DESC desc{};
        desc.width = desc.height = Spike_Target_Size;
        desc.mipLevels = desc.arraySize = 1;
        desc.format = REX::W32::DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.sampleDesc.count = 1;
        desc.bindFlags = REX::W32::D3D11_BIND_RENDER_TARGET;
        if (!REX::W32::SUCCESS(device->CreateTexture2D(&desc, nullptr, &s_colour)) || !s_colour)
        {
            s_colour = nullptr;
            return false;
        }
        if (!REX::W32::SUCCESS(device->CreateRenderTargetView(s_colour, nullptr, &s_colour_view)) || !s_colour_view)
        {
            s_colour->Release();
            s_colour = nullptr;
            s_colour_view = nullptr;
            return false;
        }

        REX::W32::D3D11_TEXTURE2D_DESC depth_desc{};
        depth_desc.width = depth_desc.height = Spike_Target_Size;
        depth_desc.mipLevels = depth_desc.arraySize = 1;
        depth_desc.format = REX::W32::DXGI_FORMAT_D32_FLOAT;
        depth_desc.sampleDesc.count = 1;
        depth_desc.bindFlags = REX::W32::D3D11_BIND_DEPTH_STENCIL;
        device->CreateTexture2D(&depth_desc, nullptr, &s_depth);
        if (s_depth)
            device->CreateDepthStencilView(s_depth, nullptr, &s_depth_view);

        return true;
    }

    void thunk(RE::BSBatchRenderer* self, RE::BSRenderPass* pass, std::uint32_t technique, bool alpha_test,
        std::uint32_t render_flags)
    {
        if (!s_original)
            return;

        bool const redirect = s_armed && !s_done && pass && s_target && pass->geometry == s_target;
        if (!redirect)
        {
            s_original(self, pass, technique, alpha_test, render_flags);
            return;
        }

        s_done = true;

        char const* const node_name = pass->geometry->name.c_str();
        logger::info("Panel spike: pass for node=\"{}\" technique={:#x} alphaTest={} renderFlags={:#x} "
                     "pass(shader={:X}, passEnum={:#x}, lights={}, shadowLights={})",
            node_name && node_name[0] != '\0' ? node_name : "?", technique,
            alpha_test ? "yes" : "no", render_flags,
            reinterpret_cast<std::uintptr_t>(pass->shader), pass->passEnum,
            static_cast<unsigned>(pass->numLights), static_cast<unsigned>(pass->numShadowLights));

        RE::BSGraphics::Renderer* const renderer = RE::BSGraphics::Renderer::GetSingleton();
        bool redirected = false;
        if (renderer)
        {
            auto const& renderer_runtime = renderer->GetRuntimeData();
            REX::W32::ID3D11Device* const device = renderer_runtime.forwarder;
            REX::W32::ID3D11DeviceContext* const context = renderer_runtime.context;
            if (device && context && ensure_target(device))
            {
                D3D11StateCapture capture(context);
                capture.capture();

                float const clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                context->ClearRenderTargetView(s_colour_view, clear);
                if (s_depth_view)
                    context->ClearDepthStencilView(s_depth_view, REX::W32::D3D11_CLEAR_DEPTH, 1.0f, 0);

                REX::W32::ID3D11RenderTargetView* const targets[1] = { s_colour_view };
                context->OMSetRenderTargets(1, targets, s_depth_view);
                REX::W32::D3D11_VIEWPORT const viewport{
                    .topLeftX = 0.0f,
                    .topLeftY = 0.0f,
                    .width = static_cast<float>(Spike_Target_Size),
                    .height = static_cast<float>(Spike_Target_Size),
                    .minDepth = 0.0f,
                    .maxDepth = 1.0f
                };
                context->RSSetViewports(1, &viewport);

                s_original(self, pass, technique, alpha_test, render_flags);
                redirected = true;

                capture.restore();
                dump_target_tga(device, context, s_colour, "PlayerPanel_spike_pass");
            }
        }

        if (!redirected)
            s_original(self, pass, technique, alpha_test, render_flags);
    }
}

EnginePassSpike& EnginePassSpike::instance()
{
    static EnginePassSpike s_instance;
    return s_instance;
}

void EnginePassSpike::install()
{
    // install_frame_hook is re-run on kPostLoadGame/kNewGame as a retry, but a branch cannot quietly
    // be taken back afterwards: writing it again would read our own E9 as "the original" and chain
    // the trampoline cell back into the thunk - an infinite recursion on the next draw.
    static bool installed = false;
    if (installed)
        return;
    installed = true;

    // RELOCATION_ID(100854, 107644) is BSBatchRenderer::SetupAndDrawPass - the entry Community
    // Shaders hooks in three of its features, and the one CommonLibSSE also exposes as a callable
    // member. Hooking it keeps the spike inside the engine's accumulation, where the pass, the
    // lights and the per-frame state are all live.
    static REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(100854, 107644) };
    std::uintptr_t const address = target.address();
    // write_branch<5> chains: its return value assumes the site already held a JMP rel32. Two
    // conditions must hold for that to be safe here. The byte must be E9, and the branch must stay
    // inside the game module: on 1.6.1170 another plugin patches this entry first with a JMP into
    // its own trampoline (outside the image), so chaining through it both hijacks that hook and
    // feeds the thunk a call whose second argument is not the BSRenderPass* clib-ng's header
    // claims - reading it as a pass crashed the render thread (15:32:56 dump).
    if (*reinterpret_cast<std::uint8_t const*>(address) != 0xE9)
    {
        logger::error("Panel spike: the entry at {:X} does not start with a rel32 jump; "
                      "the spike will not engage",
            address);
        return;
    }
    std::uintptr_t const branch_target =
        address + 5 + static_cast<std::uintptr_t>(
                          static_cast<std::intptr_t>(*reinterpret_cast<const std::int32_t*>(address + 1)));
    REL::Module const& module = REL::Module::get();
    auto const text = module.segment(REL::Segment::textx);
    std::uintptr_t const text_begin = text.address();
    std::uintptr_t const text_end = text_begin + text.size();
    if (branch_target < text_begin || branch_target >= text_end)
    {
        logger::error("Panel spike: the entry at {:X} already branches outside the game module "
                      "({:X}); another plugin owns it and the spike will not engage",
            address, branch_target);
        return;
    }
    s_original = reinterpret_cast<DrawFn>(
        target.write_branch<5>(reinterpret_cast<std::uintptr_t>(&thunk)));

    logger::info("Panel spike: detoured SetupAndDrawPass at {:X}; original continues at {:X}",
        address, reinterpret_cast<std::uintptr_t>(s_original));
}

void EnginePassSpike::on_frame()
{
    if (s_done || s_armed)
        return;

    RE::TESObjectREFR* const preview = PreviewActor::instance().current_reference();
    if (!preview)
        return;

    RE::BSGeometry* const geometry = find_skinned_geometry(*preview);
    if (!geometry)
        return;

    s_target = geometry;
    s_armed = true;
    char const* const node_name = geometry->name.c_str();
    logger::info("Panel spike: armed on node=\"{}\"; the next pass for it is redirected into a private {}x{} target",
        node_name && node_name[0] != '\0' ? node_name : "?", Spike_Target_Size, Spike_Target_Size);
}

PLUGIN_NAMESPACE_END
