//
// Created by AmazingBuff on 2026/10/20.
//
#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

PLUGIN_NAMESPACE_BEGIN
namespace dac
{
    struct EffectValue
    {
        std::uint32_t spell = 0;
        std::uint32_t effect = 0;
        std::uint32_t effect_index = 0;
        std::uint32_t caster = 0;
        std::uint32_t source = 0;
        std::int32_t casting_source = 0;
        bool inactive = false;
        float magnitude = 0.0f;
        float duration = 0.0f;
        float elapsed = 0.0f;
        bool dual = false;
        bool operator==(EffectValue const&) const = default;
    };
    struct EventTicket
    {
        std::uint64_t epoch = 0;
        std::uint64_t request = 0;
    };
    struct Restoration
    {
        std::vector<EffectValue> baseline;
        std::vector<std::uint32_t> spells;
    };
    // Engine-free transaction boundary; callers serialize it, then perform native work
    // outside their bookkeeping lock. No ActiveEffect pointer enters the snapshot.
    class SessionCore
    {
    public:
        void begin(std::vector<EffectValue> snapshot)
        {
            if (m_open) return;
            ++m_epoch;
            m_open = true;
            m_baseline = std::move(snapshot);
            m_touched.clear();
        }
        void request(std::uint64_t generation) { m_request = generation; }
        [[nodiscard]] EventTicket ticket() const { return { m_epoch, m_request }; }
        [[nodiscard]] bool accepts(EventTicket ticket) const
        { return m_open && ticket.epoch == m_epoch && ticket.request == m_request; }
        [[nodiscard]] bool open() const { return m_open; }
        [[nodiscard]] std::uint64_t epoch() const { return m_epoch; }
        void touch(std::vector<std::uint32_t> const& spells)
        {
            for (std::uint32_t spell : spells)
                if (std::ranges::find(m_touched, spell) == m_touched.end()) m_touched.push_back(spell);
        }
        [[nodiscard]] std::optional<Restoration> close()
        {
            if (!m_open) return std::nullopt;
            m_open = false;
            ++m_epoch;
            Restoration restoration;
            restoration.spells = std::move(m_touched);
            for (EffectValue const& effect : m_baseline)
                if (std::ranges::find(restoration.spells, effect.spell) != restoration.spells.end())
                    restoration.baseline.push_back(effect);
            m_baseline.clear();
            return restoration;
        }
        void reset()
        {
            m_open = false;
            ++m_epoch;
            m_baseline.clear();
            m_touched.clear();
        }
    private:
        bool m_open = false;
        std::uint64_t m_epoch = 0;
        std::uint64_t m_request = 0;
        std::vector<EffectValue> m_baseline;
        std::vector<std::uint32_t> m_touched;
    };
}
PLUGIN_NAMESPACE_END
