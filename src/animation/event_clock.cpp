//
// Created by AmazingBuff on 2026/10/20.
//
#include "animation/event_clock.h"
#include "RE/H/hkaAnimation.h"
#include <cmath>
PLUGIN_NAMESPACE_BEGIN
namespace animation
{
std::vector<Annotation> clip_annotations(RE::hkaAnimation const& animation)
{
    std::vector<Annotation> result;
    for (RE::hkaAnnotationTrack const& track : animation.annotationTracks)
        for (RE::hkaAnnotationTrack::Annotation const& annotation : track.annotations)
            if (std::isfinite(annotation.time) && annotation.time >= 0.0f && annotation.time <= animation.duration && annotation.text.c_str())
                result.push_back({ annotation.time, annotation.text.c_str() });
    std::stable_sort(result.begin(), result.end(), [](Annotation const& left, Annotation const& right) { return left.time < right.time; });
    return result;
}

}
PLUGIN_NAMESPACE_END
