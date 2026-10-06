//
// Created by AmazingBuff on 2026/9/19.
//

#include "shader_manager.h"

#include "render/shader_sources.h"
#include "dx11/d3d11_util.h"

PLUGIN_NAMESPACE_BEGIN

ShaderManager& ShaderManager::instance()
{
    static ShaderManager s_instance;
    return s_instance;
}

ShaderManager::ShaderManager() :
    m_composite_vs(nullptr),
    m_composite_ps(nullptr),
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

        m_ready = create_vertex_shader(device, render_shaders::Composite, "vs_main", "composite", &m_composite_vs, nullptr) &&
            create_pixel_shader(device, render_shaders::Composite, "ps_main", "composite", &m_composite_ps);

        if (!m_ready)
        {
            release();
            logger::error("Shader compilation failed, rendering disabled");
            return false;
        }

        logger::info("All shaders compiled");
    }

    return true;
}

void ShaderManager::release()
{
    auto const release_ptr = [](auto& com_ptr) {
        if (com_ptr)
        {
            com_ptr->Release();
            com_ptr = nullptr;
        }
    };

    release_ptr(m_composite_vs);
    release_ptr(m_composite_ps);
}

PLUGIN_NAMESPACE_END
