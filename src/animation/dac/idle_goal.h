//
// Created by AmazingBuff on 2026/10/20.
//
#pragma once
#include <cstdint>
PLUGIN_NAMESPACE_BEGIN
namespace dac
{
    enum class IdleReadiness : std::uint8_t { e_none, e_pending, e_ready, e_failed };
    enum class IdleWork : std::uint8_t { e_none, e_observe, e_dispatch };
    class IdlePreparation
    {
    public:
        IdleWork start(std::uint64_t request, bool retain_ready)
        {
            if (m_request == request && m_status != IdleReadiness::e_none) return IdleWork::e_none;
            const bool reuse = retain_ready && m_status == IdleReadiness::e_ready;
            m_request = request;
            m_status = IdleReadiness::e_pending;
            return reuse ? IdleWork::e_observe : IdleWork::e_dispatch;
        }
        void finish(std::uint64_t request, bool ready)
        {
            if (request == m_request && (m_status == IdleReadiness::e_pending || (!ready && m_status == IdleReadiness::e_ready)))
                m_status = ready ? IdleReadiness::e_ready : IdleReadiness::e_failed;
        }
        [[nodiscard]] IdleReadiness status(std::uint64_t request) const
        { return request == m_request ? m_status : IdleReadiness::e_none; }
    private:
        std::uint64_t m_request = 0;
        IdleReadiness m_status = IdleReadiness::e_none;
    };
}
PLUGIN_NAMESPACE_END
