//
// Created by AmazingBuff on 2026/10/22.
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

PLUGIN_NAMESPACE_BEGIN
namespace animation
{
    struct AcceptedMapping
    {
        std::string original;
        std::string logical;
        std::string bound;
        std::uint16_t base_index;
        std::uint16_t index;
        std::uint16_t variant_index;
        std::size_t variant_position;
        int action;
        bool replaced;
        bool operator==(AcceptedMapping const&) const = default;
    };

    class MappingPublication
    {
    public:
        MappingPublication() : m_request(0) {}
        [[nodiscard]] bool accept(std::uint64_t request, AcceptedMapping const& mapping)
        {
            if (m_last && request == m_request && *m_last == mapping) return false;
            m_request = request;
            m_last = mapping;
            return true;
        }
    private:
        std::uint64_t m_request;
        std::optional<AcceptedMapping> m_last;
    };
}
PLUGIN_NAMESPACE_END
