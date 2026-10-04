#pragma once
// The private offscreen studio target (color + depth + depth-on state) and
// its configuration signature. (2026-10-05 src migration: split out of tools/m0_proto, behavior byte-identical)

#include <cstdint>
#include <REX/W32/D3D11.h>

namespace CharacterPanelProto
{
    // v4.1: the render target the visible menu preview draws into (the
    // format-28 UI composite, runs 11-19) — captured AddRef'd during
    // replays, because the target bound at DrawInterfaceStart ENTRY
    // proved to be a different, non-visible intermediate (format 24,
    // run 20: the composite quad drew there and never showed). The
    // engine reallocates this pool target on resolution change, which
    // shows up as a pointer change and triggers a recapture.
    inline REX::W32::ID3D11RenderTargetView* s_panel_rtv = nullptr;

    struct OffscreenTarget
    {
        REX::W32::ID3D11Texture2D* color = nullptr;
        REX::W32::ID3D11RenderTargetView* rtv = nullptr;
        REX::W32::ID3D11ShaderResourceView* srv = nullptr;
        REX::W32::ID3D11Texture2D* depth_texture = nullptr;
        REX::W32::ID3D11DepthStencilView* dsv = nullptr;
        REX::W32::ID3D11DepthStencilState* ds_state = nullptr;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        REX::W32::DXGI_FORMAT format = REX::W32::DXGI_FORMAT_UNKNOWN;
        REX::W32::DXGI_FORMAT depth_format = REX::W32::DXGI_FORMAT_UNKNOWN;

        void destroy();

        bool create(REX::W32::ID3D11Device* device,
            const REX::W32::D3D11_TEXTURE2D_DESC& template_desc, REX::W32::DXGI_FORMAT a_depth_format);
    };

    // Configuration identity of a studio target: detects description
    // changes (resolution change) and makes creation failures sticky per
    // configuration within one panel open.
    struct TargetSig
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t format = 0;
        std::uint32_t depth_format = 0;
        bool operator==(const TargetSig&) const = default;
    };

    OffscreenTarget& offscreen_target();
}
