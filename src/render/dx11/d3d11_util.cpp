//
// Created by AmazingBuff on 2026/9/13.
//

#include "d3d11_util.h"

PLUGIN_NAMESPACE_BEGIN

namespace
{
    REX::W32::ID3DBlob* compile_shader(char const* source, char const* entry, char const* target, char const* name, char const* log_prefix)
    {
        if (!source || !entry || !target)
            return nullptr;

        REX::W32::ID3DBlob* blob = nullptr;
        REX::W32::ID3DBlob* err = nullptr;
        REX::W32::HRESULT const hr = REX::W32::D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, entry, target, 0, 0, &blob, &err);
        if (!REX::W32::SUCCESS(hr))
        {
            logger::error("{} shader compile failed [{} {}] ({:X}): {}",
                log_prefix ? log_prefix : "?",
                name ? name : "?",
                target,
                static_cast<unsigned int>(hr),
                err ? static_cast<char const*>(err->GetBufferPointer()) : "no diagnostics");
            if (blob)
            {
                blob->Release();
                blob = nullptr;
            }
        }
        if (err)
            err->Release();

        return blob;
    }
}

bool create_vertex_shader(REX::W32::ID3D11Device* device, char const* source, char const* entry, char const* name, REX::W32::ID3D11VertexShader** shader, REX::W32::ID3DBlob** blob)
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

bool create_pixel_shader(REX::W32::ID3D11Device* device, char const* source, char const* entry, char const* name, REX::W32::ID3D11PixelShader** shader)
{
    REX::W32::ID3DBlob* blob = compile_shader(source, entry, "ps_5_0", name, "shader manager");
    if (!blob)
        return false;
    device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, shader);
    blob->Release();
    return *shader != nullptr;
}

bool render_target_dimensions(REX::W32::ID3D11RenderTargetView* target, uint32_t& width, uint32_t& height)
{
    if (!target)
        return false;

    REX::W32::D3D11_RENDER_TARGET_VIEW_DESC view_desc{};
    target->GetDesc(&view_desc);

    uint32_t mip_slice = 0;
    switch (view_desc.viewDimension)
    {
    case REX::W32::D3D11_RTV_DIMENSION_TEXTURE2D:
        mip_slice = view_desc.texture2D.mipSlice;
        break;
    case REX::W32::D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
        mip_slice = view_desc.texture2DArray.mipSlice;
        break;
    case REX::W32::D3D11_RTV_DIMENSION_TEXTURE2DMS:
    case REX::W32::D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
        break;
    default:
        return false;
    }

    REX::W32::ID3D11Resource* resource = nullptr;
    target->GetResource(&resource);
    if (!resource)
        return false;

    REX::W32::ID3D11Texture2D* texture = nullptr;
    REX::W32::HRESULT const query_hr = resource->QueryInterface(REX::W32::IID_ID3D11Texture2D, reinterpret_cast<void**>(&texture));
    resource->Release();
    if (!REX::W32::SUCCESS(query_hr) || !texture)
        return false;

    REX::W32::D3D11_TEXTURE2D_DESC texture_desc{};
    texture->GetDesc(&texture_desc);
    texture->Release();

    if (mip_slice >= texture_desc.mipLevels || mip_slice >= 32)
        return false;

    width = std::max(1u, texture_desc.width >> mip_slice);
    height = std::max(1u, texture_desc.height >> mip_slice);
    return true;
}

D3D11StateCapture::D3D11StateCapture(REX::W32::ID3D11DeviceContext* context) :
    m_ref_context(context),
    m_render_targets{},
    m_unordered_access_views{},
    m_depth_stencil(nullptr),
    m_blend(nullptr),
    m_blend_factor{},
    m_sample_mask(0),
    m_depth(nullptr),
    m_stencil_ref(0),
    m_rasterizer(nullptr),
    m_viewport_count(0),
    m_viewports{},
    m_scissor_count(0),
    m_scissor_rects{},
    m_input_layout(nullptr),
    m_topology(REX::W32::D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED),
    m_vertex_buffers{},
    m_vertex_strides{},
    m_vertex_offsets{},
    m_index_buffer(nullptr),
    m_index_format(REX::W32::DXGI_FORMAT_UNKNOWN),
    m_index_offset(0),
    m_vertex_shader(nullptr),
    m_vertex_instances{},
    m_vertex_instance_count(Class_Instance_Count),
    m_pixel_shader(nullptr),
    m_pixel_instances{},
    m_pixel_instance_count(Class_Instance_Count),
    m_geometry_shader(nullptr),
    m_geometry_instances{},
    m_geometry_instance_count(Class_Instance_Count),
    m_hull_shader(nullptr),
    m_hull_instances{},
    m_hull_instance_count(Class_Instance_Count),
    m_domain_shader(nullptr),
    m_domain_instances{},
    m_domain_instance_count(Class_Instance_Count),
    m_vertex_cbs{},
    m_pixel_cbs{},
    m_pixel_srvs{},
    m_pixel_sampler(nullptr)
{
    capture();
}

D3D11StateCapture::~D3D11StateCapture()
{
    restore();

    for (REX::W32::ID3D11RenderTargetView* render_target : m_render_targets)
    {
        if (render_target)
            render_target->Release();
    }
    for (REX::W32::ID3D11UnorderedAccessView* unordered_access_view : m_unordered_access_views)
    {
        if (unordered_access_view)
            unordered_access_view->Release();
    }
    if (m_depth_stencil)
        m_depth_stencil->Release();
    if (m_blend)
        m_blend->Release();
    if (m_depth)
        m_depth->Release();
    if (m_rasterizer)
        m_rasterizer->Release();
    if (m_input_layout)
        m_input_layout->Release();
    for (REX::W32::ID3D11Buffer* vertex_buffer : m_vertex_buffers)
    {
        if (vertex_buffer)
            vertex_buffer->Release();
    }
    if (m_index_buffer)
        m_index_buffer->Release();
    if (m_vertex_shader)
        m_vertex_shader->Release();
    if (m_pixel_shader)
        m_pixel_shader->Release();
    for (REX::W32::ID3D11ClassInstance* instance : m_vertex_instances)
    {
        if (instance)
            instance->Release();
    }
    for (REX::W32::ID3D11ClassInstance* instance : m_pixel_instances)
    {
        if (instance)
            instance->Release();
    }
    for (REX::W32::ID3D11ClassInstance* instance : m_geometry_instances)
    {
        if (instance)
            instance->Release();
    }
    for (REX::W32::ID3D11ClassInstance* instance : m_hull_instances)
    {
        if (instance)
            instance->Release();
    }
    for (REX::W32::ID3D11ClassInstance* instance : m_domain_instances)
    {
        if (instance)
            instance->Release();
    }
    if (m_geometry_shader)
        m_geometry_shader->Release();
    if (m_hull_shader)
        m_hull_shader->Release();
    if (m_domain_shader)
        m_domain_shader->Release();
    for (REX::W32::ID3D11Buffer* cb : m_vertex_cbs)
    {
        if (cb)
            cb->Release();
    }
    for (REX::W32::ID3D11Buffer* cb : m_pixel_cbs)
    {
        if (cb)
            cb->Release();
    }
    for (REX::W32::ID3D11ShaderResourceView* srv : m_pixel_srvs)
    {
        if (srv)
            srv->Release();
    }
    if (m_pixel_sampler)
        m_pixel_sampler->Release();
}

void D3D11StateCapture::capture()
{
    if (!m_ref_context)
        return;

    m_ref_context->OMGetRenderTargetsAndUnorderedAccessViews(
        Render_Target_Count,
        m_render_targets,
        &m_depth_stencil,
        0,
        REX::W32::D3D11_PS_CS_UAV_REGISTER_COUNT,
        m_unordered_access_views);
    m_ref_context->OMGetBlendState(&m_blend, m_blend_factor, &m_sample_mask);
    m_ref_context->OMGetDepthStencilState(&m_depth, &m_stencil_ref);
    m_ref_context->RSGetState(&m_rasterizer);

    m_viewport_count = REX::W32::D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    m_ref_context->RSGetViewports(&m_viewport_count, m_viewports);
    m_scissor_count = REX::W32::D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    m_ref_context->RSGetScissorRects(&m_scissor_count, m_scissor_rects);

    m_ref_context->IAGetInputLayout(&m_input_layout);
    m_ref_context->IAGetPrimitiveTopology(&m_topology);
    m_ref_context->IAGetVertexBuffers(0, Vertex_Buffer_Count, m_vertex_buffers, m_vertex_strides, m_vertex_offsets);
    m_ref_context->IAGetIndexBuffer(&m_index_buffer, &m_index_format, &m_index_offset);

    m_ref_context->VSGetShader(&m_vertex_shader, m_vertex_instances, &m_vertex_instance_count);
    m_ref_context->PSGetShader(&m_pixel_shader, m_pixel_instances, &m_pixel_instance_count);
    m_ref_context->GSGetShader(&m_geometry_shader, m_geometry_instances, &m_geometry_instance_count);
    m_ref_context->HSGetShader(&m_hull_shader, m_hull_instances, &m_hull_instance_count);
    m_ref_context->DSGetShader(&m_domain_shader, m_domain_instances, &m_domain_instance_count);
    m_ref_context->VSGetConstantBuffers(0, 2, m_vertex_cbs);
    m_ref_context->PSGetConstantBuffers(0, 2, m_pixel_cbs);
    m_ref_context->PSGetShaderResources(0, 3, m_pixel_srvs);
    m_ref_context->PSGetSamplers(0, 1, &m_pixel_sampler);
}

void D3D11StateCapture::restore() const noexcept
{
    uint32_t render_target_count = 0;
    for (uint32_t index = 0; index < Render_Target_Count; ++index)
    {
        if (m_render_targets[index])
            render_target_count = index + 1;
    }

    REX::W32::ID3D11UnorderedAccessView* null_unordered_access_views[Max_Unordered_Access_View_Count]{};
    uint32_t keep_unordered_access_counts[Max_Unordered_Access_View_Count];
    for (uint32_t& count : keep_unordered_access_counts)
        count = std::numeric_limits<uint32_t>::max();
    m_ref_context->OMSetRenderTargetsAndUnorderedAccessViews(
        0,
        nullptr,
        m_depth_stencil,
        0,
        REX::W32::D3D11_PS_CS_UAV_REGISTER_COUNT,
        null_unordered_access_views,
        keep_unordered_access_counts);

    uint32_t const unordered_access_start = std::min(render_target_count, static_cast<uint32_t>(REX::W32::D3D11_PS_CS_UAV_REGISTER_COUNT));
    uint32_t const unordered_access_count = REX::W32::D3D11_PS_CS_UAV_REGISTER_COUNT - unordered_access_start;
    m_ref_context->OMSetRenderTargetsAndUnorderedAccessViews(
        render_target_count,
        m_render_targets,
        m_depth_stencil,
        unordered_access_start,
        unordered_access_count,
        unordered_access_count ? m_unordered_access_views + unordered_access_start : nullptr,
        keep_unordered_access_counts);
    m_ref_context->OMSetBlendState(m_blend, m_blend_factor, m_sample_mask);
    m_ref_context->OMSetDepthStencilState(m_depth, m_stencil_ref);
    m_ref_context->RSSetState(m_rasterizer);
    m_ref_context->RSSetViewports(m_viewport_count, m_viewports);
    m_ref_context->RSSetScissorRects(m_scissor_count, m_scissor_rects);
    m_ref_context->IASetInputLayout(m_input_layout);
    m_ref_context->IASetPrimitiveTopology(m_topology);
    m_ref_context->IASetVertexBuffers(0, Vertex_Buffer_Count, m_vertex_buffers, m_vertex_strides, m_vertex_offsets);
    m_ref_context->IASetIndexBuffer(m_index_buffer, m_index_format, m_index_offset);
    m_ref_context->VSSetShader(m_vertex_shader, m_vertex_instances, m_vertex_instance_count);
    m_ref_context->PSSetShader(m_pixel_shader, m_pixel_instances, m_pixel_instance_count);
    m_ref_context->GSSetShader(m_geometry_shader, m_geometry_instances, m_geometry_instance_count);
    m_ref_context->HSSetShader(m_hull_shader, m_hull_instances, m_hull_instance_count);
    m_ref_context->DSSetShader(m_domain_shader, m_domain_instances, m_domain_instance_count);
    m_ref_context->VSSetConstantBuffers(0, 2, m_vertex_cbs);
    m_ref_context->PSSetConstantBuffers(0, 2, m_pixel_cbs);
    m_ref_context->PSSetShaderResources(0, 3, m_pixel_srvs);
    m_ref_context->PSSetSamplers(0, 1, &m_pixel_sampler);
}

RenderTarget::RenderTarget(REX::W32::DXGI_FORMAT color_format, REX::W32::DXGI_FORMAT depth_format) :
    m_color_format(color_format),
    m_depth_format(depth_format),
    m_ref_device(nullptr),
    m_texture(nullptr),
    m_depth_texture(nullptr),
    m_dsv(nullptr),
    m_rtv(nullptr),
    m_srv(nullptr),
    m_width(0),
    m_height(0) {}

RenderTarget::~RenderTarget()
{
    release();
}

bool RenderTarget::matches(REX::W32::ID3D11Device* device, uint32_t width, uint32_t height) const
{
    const bool unchanged = m_ref_device == device && m_width == width && m_height == height && m_srv && m_dsv;
    return unchanged;
}

void RenderTarget::release()
{
    if (m_dsv)
    {
        m_dsv->Release();
        m_dsv = nullptr;
    }
    if (m_depth_texture)
    {
        m_depth_texture->Release();
        m_depth_texture = nullptr;
    }
    if (m_srv)
    {
        m_srv->Release();
        m_srv = nullptr;
    }
    if (m_rtv)
    {
        m_rtv->Release();
        m_rtv = nullptr;
    }
    if (m_texture)
    {
        m_texture->Release();
        m_texture = nullptr;
    }
    m_width = 0;
    m_height = 0;
    m_ref_device = nullptr;
}

bool RenderTarget::init(REX::W32::ID3D11Device* device, uint32_t width, uint32_t height)
{
    REX::W32::D3D11_TEXTURE2D_DESC td{};
    td.width = width;
    td.height = height;
    td.mipLevels = 1;
    td.arraySize = 1;
    td.format = m_color_format;
    td.sampleDesc.count = 1;
    td.usage = REX::W32::D3D11_USAGE_DEFAULT;
    td.bindFlags = REX::W32::D3D11_BIND_RENDER_TARGET | REX::W32::D3D11_BIND_SHADER_RESOURCE;

    REX::W32::HRESULT const tex_hr = device->CreateTexture2D(&td, nullptr, &m_texture);
    if (!REX::W32::SUCCESS(tex_hr) || !m_texture)
    {
        logger::error("Mask overlay: failed to create mask texture ({:X})", static_cast<unsigned int>(tex_hr));
        release();
        return false;
    }
    REX::W32::HRESULT const rtv_hr = device->CreateRenderTargetView(m_texture, nullptr, &m_rtv);
    REX::W32::HRESULT const srv_hr = device->CreateShaderResourceView(m_texture, nullptr, &m_srv);
    if (!REX::W32::SUCCESS(rtv_hr) || !m_rtv || !REX::W32::SUCCESS(srv_hr) || !m_srv)
    {
        logger::error("Mask overlay: failed to create mask views (rtv={:X}, srv={:X})", static_cast<unsigned int>(rtv_hr), static_cast<unsigned int>(srv_hr));
        release();
        return false;
    }

    if (m_depth_format != REX::W32::DXGI_FORMAT_UNKNOWN)
    {
        td.format = m_depth_format;
        td.bindFlags = REX::W32::D3D11_BIND_DEPTH_STENCIL;
        REX::W32::HRESULT const depth_hr = device->CreateTexture2D(&td, nullptr, &m_depth_texture);
        if (!REX::W32::SUCCESS(depth_hr) || !REX::W32::SUCCESS(device->CreateDepthStencilView(m_depth_texture, nullptr, &m_dsv))) {
          logger::error("Mask overlay: failed to create private depth target");
          release();
          return false;
        }
    }

    m_ref_device = device;
    m_width = width;
    m_height = height;
    return true;
}

PLUGIN_NAMESPACE_END
