#pragma once
#include <REX/W32/D3D11.h>
#include <atomic>
#include <mutex>

PLUGIN_NAMESPACE_BEGIN

class PresentHook
{
public:
    using Callback = void (*)(REX::W32::IDXGISwapChain*);
    static PresentHook& instance();
    PresentHook(PresentHook const&) = delete;
    PresentHook& operator=(PresentHook const&) = delete;
    bool install(Callback callback);
private:
    PresentHook() = default;
    using PresentFunc = REX::W32::HRESULT(*)(REX::W32::IDXGISwapChain*, uint32_t, uint32_t);
    static REX::W32::HRESULT present_thunk(REX::W32::IDXGISwapChain*, uint32_t, uint32_t) noexcept;
    std::atomic<PresentFunc> m_ref_original_present{ nullptr };
    std::atomic<Callback> m_callback{ nullptr };
    std::mutex m_install_mutex;
    bool m_installed{ false };
};

PLUGIN_NAMESPACE_END
