#include "present_hook.h"

PLUGIN_NAMESPACE_BEGIN

PresentHook& PresentHook::instance()
{
    static PresentHook s_instance;
    return s_instance;
}

REX::W32::HRESULT PresentHook::present_thunk(REX::W32::IDXGISwapChain* swap_chain,
    uint32_t sync_interval, uint32_t flags) noexcept
{
    // Callbacks and the trampoline call run on the DXGI Present ABI boundary; neither may throw.
    PresentHook& self = instance();
    static constexpr uint32_t s_present_test = 1;
    if ((flags & s_present_test) == 0)
    {
        Callback callback = self.m_callback.load(std::memory_order_acquire);
        if (callback)
            callback(swap_chain);
    }
    return self.m_ref_original_present.load(std::memory_order_acquire)(swap_chain, sync_interval, flags);
}

bool PresentHook::install(Callback callback)
{
    if (!callback)
        return false;
    std::lock_guard lock(m_install_mutex);
    if (m_installed)
        return true;
    RE::BSGraphics::Renderer* renderer = RE::BSGraphics::Renderer::GetSingleton();
    if (!renderer || !renderer->GetRuntimeData().renderWindows[0].swapChain)
        return false;
    auto* swap_chain = renderer->GetRuntimeData().renderWindows[0].swapChain;
    void** table = *reinterpret_cast<void***>(swap_chain);
    static constexpr std::size_t s_present_slot = 8;
    m_ref_original_present.store(reinterpret_cast<PresentFunc>(table[s_present_slot]), std::memory_order_release);
    m_callback.store(callback, std::memory_order_release);
    REL::Relocation<uintptr_t> vtable{ reinterpret_cast<uintptr_t>(table) };
    vtable.write_vfunc(s_present_slot, &PresentHook::present_thunk);
    m_installed = true;
    logger::info("Hook swap chain present");
    return true;
}

PLUGIN_NAMESPACE_END
