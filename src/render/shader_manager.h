//
// Created by AmazingBuff on 2026/9/19.
//

#pragma once

#include <REX/W32/D3D11.h>

PLUGIN_NAMESPACE_BEGIN

// One-shot shader compilation for every pass of the plugin. init(device) runs once when the
// D3D11 device is first available (renderer init); afterwards every pass's init only picks up
// the ready-made shader objects. The VS blobs stay alive here because CreateInputLayout needs
// the VS bytecode, which is not retrievable from an ID3D11VertexShader.
class ShaderManager
{
public:
    static ShaderManager& instance();

    ShaderManager(ShaderManager const&) = delete;
    ShaderManager& operator=(ShaderManager const&) = delete;

    // Compiles every embedded HLSL and creates the shader objects; returns false (and logs)
    // if any entry fails. Idempotent: a repeated call returns the previous verdict.
    [[nodiscard]] bool compile();

    // Mask fullscreen composite
    [[nodiscard]] REX::W32::ID3D11VertexShader* composite_vs() const noexcept { return m_composite_vs; }
    [[nodiscard]] REX::W32::ID3D11PixelShader* composite_ps() const noexcept { return m_composite_ps; }
private:
    ShaderManager();
    ~ShaderManager();

    void release();

    REX::W32::ID3D11VertexShader* m_composite_vs;
    REX::W32::ID3D11PixelShader* m_composite_ps;

    bool m_ready;
};

PLUGIN_NAMESPACE_END
