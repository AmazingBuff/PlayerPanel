//
// Created by AmazingBuff on 2026/09/21.
//
// Ported from Highlight-Lootable-Corpses (https://github.com/AmazingBuff/Highlight-Lootable-Corpses.git),
// src/render/shader_manager.cpp, commit 7a7c51e, reduced to the panel shader set.
// Both repositories are GPL-3.0 with the same author.
//

#include "shader_manager.h"

#include "render/dx11/d3d11_util.h"
#include "render/shader_sources.h"

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // Compile + create one entry; on failure the outputs stay null and the caller aborts.
    // blob is optional: only the input layouts need the VS bytecode, so the VS without a layout
    // (the fullscreen composite) passes nullptr and the blob is released right after creation.
    bool create_vertex_shader(
        REX::W32::ID3D11Device* device, char const* source, char const* entry, char const* name,
        REX::W32::ID3D11VertexShader** shader, REX::W32::ID3DBlob** blob)
    {
        REX::W32::ID3DBlob* compiled = compile_shader(source, entry, "vs_5_0", name, "shader manager");
        if (!compiled)
            return false;

        device->CreateVertexShader(compiled->GetBufferPointer(), compiled->GetBufferSize(), nullptr, shader);
        if (blob)
            *blob = compiled;
        else
            compiled->Release();

        return *shader != nullptr;
    }

    bool create_pixel_shader(
        REX::W32::ID3D11Device* device, char const* source, char const* entry, char const* name,
        REX::W32::ID3D11PixelShader** shader)
    {
        REX::W32::ID3DBlob* blob = compile_shader(source, entry, "ps_5_0", name, "shader manager");
        if (!blob)
            return false;
        device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, shader);
        blob->Release();
        return *shader != nullptr;
    }
}

ShaderManager& ShaderManager::instance()
{
    static ShaderManager s_instance;
    return s_instance;
}

ShaderManager::ShaderManager() :
    m_panel_static_vs(nullptr),
    m_panel_skinned_vs(nullptr),
    m_panel_ps(nullptr),
    m_panel_static_vs_blob(nullptr),
    m_panel_skinned_vs_blob(nullptr),
    m_panel_fullscreen_vs(nullptr),
    m_panel_background_ps(nullptr),
    m_panel_composite_ps(nullptr),
    m_ready(false) {}

ShaderManager::~ShaderManager()
{
    release();
}

bool ShaderManager::compile()
{
    if (!m_ready)
    {
        RE::BSGraphics::Renderer* renderer = RE::BSGraphics::Renderer::GetSingleton();
        if (!renderer)
        {
            logger::info("Shader precompile skipped, renderer not available at data-loaded");
            return false;
        }

        REX::W32::ID3D11Device* device = renderer->GetRuntimeData().forwarder;
        if (!device)
        {
            logger::info("Shader precompile skipped, D3D11 device not available at data-loaded");
            return false;
        }

        m_ready =
            create_vertex_shader(device, render_shaders::PanelGeometry, "vs_static_main", "panel static", &m_panel_static_vs, &m_panel_static_vs_blob) &&
            create_vertex_shader(device, render_shaders::PanelGeometry, "vs_skinned_main", "panel skinned", &m_panel_skinned_vs, &m_panel_skinned_vs_blob) &&
            create_pixel_shader(device, render_shaders::PanelGeometry, "ps_panel_main", "panel geometry", &m_panel_ps) &&

            create_vertex_shader(device, render_shaders::PanelComposite, "vs_main", "panel fullscreen", &m_panel_fullscreen_vs, nullptr) &&
            create_pixel_shader(device, render_shaders::PanelComposite, "ps_background_main", "panel background", &m_panel_background_ps) &&
            create_pixel_shader(device, render_shaders::PanelComposite, "ps_panel_main", "panel copy", &m_panel_composite_ps);

        if (!m_ready)
        {
            release();
            logger::error("Shader compilation failed, panel rendering disabled");
            return false;
        }

        logger::info("All panel shaders compiled");
    }

    return true;
}

void ShaderManager::release()
{
    auto const release_ptr = [](auto& com_ptr)
    {
        if (com_ptr)
        {
            com_ptr->Release();
            com_ptr = nullptr;
        }
    };

    release_ptr(m_panel_static_vs);
    release_ptr(m_panel_skinned_vs);
    release_ptr(m_panel_ps);
    release_ptr(m_panel_fullscreen_vs);
    release_ptr(m_panel_background_ps);
    release_ptr(m_panel_composite_ps);
    release_ptr(m_panel_static_vs_blob);
    release_ptr(m_panel_skinned_vs_blob);
    m_ready = false;
}

PLUGIN_NAMESPACE_END
