//
// Created by AmazingBuff on 2026/10/31.
//
#pragma once

#include <cstdint>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // C-G3B-S3: the settle gate between the equip phase and the single unequip injection. The
    // gate watches the candidate's currentDefaultState word and the active-node count once per
    // driven step. The slice proceeds only when the state reached the pre-registered settled
    // value and the tail then held one unchanged (default, nodes) reading for three consecutive
    // samples: a disturbance restarts the run, and a window that ends off the settled reading is
    // a no-settle outcome, never an upgrade into a timed injection.
    inline constexpr std::int32_t Unequip_Settle_Target = 1;
    inline constexpr std::uint32_t Unequip_Settle_Stable_Required = 3;

    enum class UnequipSettleVerdict : std::uint8_t
    {
        e_settled,
        e_no_settle
    };

    class UnequipSettleGate
    {
    public:
        void observe(std::uint32_t const a_step, std::int32_t const a_default, std::int32_t const a_nodes)
        {
            m_last_step = a_step;
            if (!m_reached && a_default == Unequip_Settle_Target)
            {
                m_reached = true;
                m_target_step = a_step;
            }
            if (m_seen && a_default == m_last_default && a_nodes == m_last_nodes)
            {
                if (m_run < Unequip_Settle_Stable_Required)
                {
                    ++m_run;
                    if (m_run == Unequip_Settle_Stable_Required)
                    {
                        m_settled_step = a_step;
                        m_baseline_nodes = a_nodes;
                    }
                }
            }
            else
            {
                m_run = 1;
            }
            m_last_default = a_default;
            m_last_nodes = a_nodes;
            m_seen = true;
        }

        [[nodiscard]] UnequipSettleVerdict verdict() const noexcept
        {
            if (m_reached && m_run >= Unequip_Settle_Stable_Required &&
                m_last_default == Unequip_Settle_Target)
                return UnequipSettleVerdict::e_settled;
            return UnequipSettleVerdict::e_no_settle;
        }

        [[nodiscard]] bool reached_target() const noexcept { return m_reached; }
        [[nodiscard]] std::uint32_t target_step() const noexcept { return m_target_step; }
        [[nodiscard]] std::uint32_t settled_step() const noexcept { return m_settled_step; }
        [[nodiscard]] std::int32_t baseline_nodes() const noexcept { return m_baseline_nodes; }
        [[nodiscard]] std::uint32_t last_step() const noexcept { return m_last_step; }
        [[nodiscard]] std::int32_t last_default() const noexcept { return m_last_default; }
        [[nodiscard]] std::int32_t last_nodes() const noexcept { return m_last_nodes; }

    private:
        bool m_seen{};
        bool m_reached{};
        std::uint32_t m_run{};
        std::uint32_t m_target_step{};
        std::uint32_t m_settled_step{};
        std::uint32_t m_last_step{};
        std::int32_t m_baseline_nodes{};
        std::int32_t m_last_default{};
        std::int32_t m_last_nodes{};
    };
}
PLUGIN_NAMESPACE_END
