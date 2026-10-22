//
// Created by AmazingBuff on 2026/10/22.
//
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

PLUGIN_NAMESPACE_BEGIN
namespace animation
{
    inline constexpr std::array<std::string_view, 15> Trace_Body_Nodes{
        "NPC Root [Root]", "NPC COM [COM ]", "NPC Pelvis [Pelv]", "NPC Spine2 [Spn2]",
        "NPC Head [Head]", "NPC L Foot [Lft ]", "NPC R Foot [Rft ]",
        "NPC L Clavicle [LClv]", "NPC R Clavicle [RClv]", "NPC L UpperArm [LUar]", "NPC R UpperArm [RUar]",
        "NPC L Forearm [LLar]", "NPC R Forearm [RLar]", "NPC L Hand [LHnd]", "NPC R Hand [RHnd]"
    };
    inline constexpr std::array<double, 12> Trace_Sample_Times{ 0.0, 0.016, 0.033, 0.066, 0.1, 0.2, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0 };

    class PoseTraceBudget
    {
    public:
        PoseTraceBudget() : m_request(0), m_windows(0), m_samples(0), m_next(Trace_Sample_Times.size()), m_denied(false), m_notified(false) {}
        void request(std::uint64_t request)
        {
            if (request == m_request) return;
            m_request = request;
            m_windows = 0;
            m_samples = 0;
            m_denied = false;
            m_notified = false;
            stop();
        }
        void stop() { m_next = Trace_Sample_Times.size(); }
        [[nodiscard]] bool arm(std::uint64_t request)
        {
            this->request(request);
            stop();
            if (m_windows >= 4 || m_samples >= 24) { m_denied = true; return false; }
            ++m_windows;
            m_next = 0;
            return true;
        }
        [[nodiscard]] std::optional<std::size_t> sample(double elapsed)
        {
            if (m_next >= Trace_Sample_Times.size() || m_samples >= 24) return std::nullopt;
            if (elapsed > 4.1) { stop(); return std::nullopt; }
            if (elapsed < Trace_Sample_Times[m_next]) return std::nullopt;
            const std::size_t slot = m_next++;
            while (m_next < Trace_Sample_Times.size() && Trace_Sample_Times[m_next] <= elapsed) ++m_next;
            ++m_samples;
            return slot;
        }
        [[nodiscard]] bool exhausted_notice()
        {
            if (m_notified || (!m_denied && m_samples < 24)) return false;
            m_notified = true;
            return true;
        }
        [[nodiscard]] std::size_t windows() const { return m_windows; }
        [[nodiscard]] std::size_t samples() const { return m_samples; }
    private:
        std::uint64_t m_request;
        std::size_t m_windows;
        std::size_t m_samples;
        std::size_t m_next;
        bool m_denied;
        bool m_notified;
    };
}
PLUGIN_NAMESPACE_END
