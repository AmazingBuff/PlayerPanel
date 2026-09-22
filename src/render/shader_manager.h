//
// Created by AmazingBuff on 2026/09/21.
//
// Ported from Highlight-Lootable-Corpses (https://github.com/AmazingBuff/Highlight-Lootable-Corpses.git),
// src/render/shader_manager.h, commit 7a7c51e, reduced to the panel shader set.
// Both repositories are GPL-3.0 with the same author.
//

#pragma once

#include <REX/W32/D3D11.h>

PLUGIN_NAMESPACE_BEGIN

// One-shot shader compilation for every pass of the plugin. compile() runs once when the D3D11
// device is first available; afterwards every pass's init only picks up the ready-made shader
// objects. The vertex-shader blobs stay alive here because CreateInputLayout needs the VS bytecode,
// which is not retrievable from an ID3D11VertexShader.
class ShaderManager
{
public:
    static ShaderManager& instance();

    ShaderManager(ShaderManager const&) = delete;
    ShaderManager& operator=(ShaderManager const&) = delete;

    // Compiles every embedded HLSL and creates the shader objects; returns false (and logs) if any
    // entry fails. Idempotent: a repeated call returns the previous verdict.
    [[nodiscard]] bool compile();

    // Panel geometry: the static and skinned vertex shaders and the panel pixel shader.
    [[nodiscard]] REX::W32::ID3D11VertexShader* panel_static_vs() const noexcept { return m_panel_static_vs; }
    [[nodiscard]] REX::W32::ID3D11VertexShader* panel_skinned_vs() const noexcept { return m_panel_skinned_vs; }
    [[nodiscard]] REX::W32::ID3D11PixelShader* panel_ps() const noexcept { return m_panel_ps; }
    [[nodiscard]] REX::W32::ID3DBlob* panel_static_vs_blob() const noexcept { return m_panel_static_vs_blob; }
    [[nodiscard]] REX::W32::ID3DBlob* panel_skinned_vs_blob() const noexcept { return m_panel_skinned_vs_blob; }

    // Panel composite: the shared fullscreen vertex shader and the two pixel shaders (opaque
    // background fill and offscreen colour copy).
    [[nodiscard]] REX::W32::ID3D11VertexShader* panel_fullscreen_vs() const noexcept { return m_panel_fullscreen_vs; }
    [[nodiscard]] REX::W32::ID3D11PixelShader* panel_background_ps() const noexcept { return m_panel_background_ps; }
    [[nodiscard]] REX::W32::ID3D11PixelShader* panel_composite_ps() const noexcept { return m_panel_composite_ps; }

private:
    ShaderManager();
    ~ShaderManager();

    void release();

    REX::W32::ID3D11VertexShader* m_panel_static_vs;
    REX::W32::ID3D11VertexShader* m_panel_skinned_vs;
    REX::W32::ID3D11PixelShader* m_panel_ps;
    REX::W32::ID3DBlob* m_panel_static_vs_blob;
    REX::W32::ID3DBlob* m_panel_skinned_vs_blob;

    REX::W32::ID3D11VertexShader* m_panel_fullscreen_vs;
    REX::W32::ID3D11PixelShader* m_panel_background_ps;
    REX::W32::ID3D11PixelShader* m_panel_composite_ps;

    bool m_ready;
};

PLUGIN_NAMESPACE_END
