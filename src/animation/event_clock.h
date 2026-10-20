//
// Created by AmazingBuff on 2026/10/20.
//
#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
namespace RE { class hkaAnimation; }
PLUGIN_NAMESPACE_BEGIN
namespace animation
{
    struct Annotation
    {
        double time;
        std::string event;
    };
    class EventClock
    {
    public:
        // annotations are validated and stable-sorted once at the loaded-animation boundary.
        void start(std::vector<Annotation> annotations, double duration, double time, bool resume)
        {
            m_annotations = std::move(annotations);
            m_duration = duration;
            m_time = std::clamp(time, 0.0, duration);
            m_next = 0;
            m_at_end = false;
            while (m_next < m_annotations.size() &&
                (resume ? m_annotations[m_next].time <= m_time : m_annotations[m_next].time < m_time)) ++m_next;
        }
        [[nodiscard]] double seconds() const { return m_time; }
        [[nodiscard]] bool finished() const { return m_at_end; }
        template<class Tick, class Event, class Boundary>
        void advance(double delta, bool loop, Tick&& tick, Event&& event, Boundary&& boundary)
        {
            for (;;)
            {
                while (m_next < m_annotations.size() && m_annotations[m_next].time <= m_time)
                {
                    Annotation const& annotation = m_annotations[m_next++];
                    event(annotation);
                }
                if (m_time >= m_duration)
                {
                    if (m_at_end) { tick(delta); return; }
                    m_at_end = true;
                    boundary();
                    if (!loop) { tick(delta); return; }
                    m_time = 0.0;
                    m_next = 0;
                    m_at_end = false;
                    continue;
                }
                if (delta <= 0.0) return;
                const double next = m_next < m_annotations.size() ? m_annotations[m_next].time : m_duration;
                const double step = std::min(delta, next - m_time);
                tick(step);
                m_time += step;
                delta -= step;
            }
        }
    private:
        std::vector<Annotation> m_annotations;
        double m_duration = 1.0;
        double m_time = 0.0;
        std::size_t m_next = 0;
        bool m_at_end = false;
    };
    [[nodiscard]] std::vector<Annotation> clip_annotations(RE::hkaAnimation const& animation);
}
PLUGIN_NAMESPACE_END
