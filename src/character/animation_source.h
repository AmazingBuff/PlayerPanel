//
// Created by AmazingBuff on 2026/10/9.
//

#pragma once

#include "RE/H/hkQsTransform.h"
#include "RE/H/hkaAnimationBinding.h"
#include "RE/N/NiAVObject.h"
#include "RE/N/NiMatrix3.h"
#include "RE/N/NiTransform.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <xmmintrin.h>

namespace RE
{
    class BShkbAnimationGraph;
    class TESObjectREFR;
    class hkaAnimation;
}

PLUGIN_NAMESPACE_BEGIN

// One pose sample as plain numbers: Havok stores its vectors as SSE quads with no element accessor,
// and keeping the conversion in plain floats is what lets it be unit-tested without a game.
struct PoseSample
{
    float position[3];
    float quaternion[4];  // x, y, z, w
    float scale;
};

[[nodiscard]] inline PoseSample read_pose(RE::hkQsTransform const& pose)
{
    alignas(16) float translation[4];
    alignas(16) float rotation[4];
    alignas(16) float scale[4];
    _mm_store_ps(translation, pose.translation.quad);
    _mm_store_ps(rotation, pose.rotation.vec.quad);
    _mm_store_ps(scale, pose.scale.quad);

    PoseSample sample{};
    sample.position[0] = translation[0];
    sample.position[1] = translation[1];
    sample.position[2] = translation[2];
    sample.quaternion[0] = rotation[0];
    sample.quaternion[1] = rotation[1];
    sample.quaternion[2] = rotation[2];
    sample.quaternion[3] = rotation[3];
    sample.scale = scale[0];
    return sample;
}

// The rotation a unit quaternion (x, y, z, w) describes, in the convention this codebase measured
// rather than assumed: A * B applies B first, so a rotation multiplies a column vector and
// entry[row][col] is that row and column.
[[nodiscard]] inline RE::NiMatrix3 ni_matrix_from_quaternion(float x, float y, float z, float w)
{
    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    const float xy = x * y;
    const float xz = x * z;
    const float yz = y * z;
    const float xw = x * w;
    const float yw = y * w;
    const float zw = z * w;

    RE::NiMatrix3 matrix;
    matrix.entry[0][0] = 1.0f - 2.0f * (yy + zz);
    matrix.entry[0][1] = 2.0f * (xy - zw);
    matrix.entry[0][2] = 2.0f * (xz + yw);
    matrix.entry[1][0] = 2.0f * (xy + zw);
    matrix.entry[1][1] = 1.0f - 2.0f * (xx + zz);
    matrix.entry[1][2] = 2.0f * (yz - xw);
    matrix.entry[2][0] = 2.0f * (xz - yw);
    matrix.entry[2][1] = 2.0f * (yz + xw);
    matrix.entry[2][2] = 1.0f - 2.0f * (xx + yy);
    return matrix;
}

[[nodiscard]] inline RE::NiTransform ni_local_from_pose(PoseSample const& sample)
{
    RE::NiTransform local;
    local.rotate = ni_matrix_from_quaternion(sample.quaternion[0], sample.quaternion[1], sample.quaternion[2], sample.quaternion[3]);
    local.translate = RE::NiPoint3{ sample.position[0], sample.position[1], sample.position[2] };
    local.scale = sample.scale;
    return local;
}

struct AnimationTrackBinding
{
    std::uint16_t track;
    std::uint16_t bone;
};

[[nodiscard]] inline std::vector<AnimationTrackBinding> resolve_animation_tracks(
    std::span<std::int16_t const> table, std::size_t track_count, std::size_t bone_count)
{
    const std::size_t count = std::min(track_count, table.empty() ? bone_count : table.size());
    std::vector<AnimationTrackBinding> bindings;
    bindings.reserve(count);
    for (std::size_t track = 0; track < count; ++track)
    {
        const std::int32_t bone = table.empty() ? static_cast<std::int32_t>(track) : table[track];
        if (bone < 0 || static_cast<std::size_t>(bone) >= bone_count)
            continue;
        bindings.push_back({ static_cast<std::uint16_t>(track), static_cast<std::uint16_t>(bone) });
    }
    return bindings;
}

// The binding retains its animation; node references belong to the current display graph.
struct ClipPlayback
{
    RE::hkaAnimation* animation;
    RE::hkRefPtr<RE::hkaAnimationBinding> binding_owner;
    std::string name;
    float duration;
    std::vector<std::uint16_t> tracks;  // animation track indices, never skeleton bone indices
    std::vector<RE::NiAVObject*> nodes;

    [[nodiscard]] bool valid() const { return animation != nullptr && !tracks.empty() && tracks.size() == nodes.size(); }
};

size_t play_animation_clip(ClipPlayback const& clip, float seconds, RE::NiAVObject& copy_root, bool loop = true);
[[nodiscard]] RE::hkbClipGenerator const* pick_running_clip(RE::TESObjectREFR& source);
// The state-idle preference: the strongest running clip whose NAME carries the engine's idle
// family marker ("idle", case-insensitive - covers sneakIdle, 1hm_idle and the hand-idle family),
// or null when no idle-named clip is running (locomotion) and the caller should fall back.
[[nodiscard]] RE::hkbClipGenerator const* pick_idle_running_clip(RE::TESObjectREFR& source);
bool resolve_clip_playback(RE::TESObjectREFR& source, RE::NiAVObject& copy_root, RE::hkbClipGenerator const* generator,
    ClipPlayback& out, std::string& out_mapping, std::string& out_coverage);
bool resolve_binding_playback(RE::BShkbAnimationGraph& graph, RE::NiAVObject& copy_root,
    RE::hkaAnimationBinding* binding, std::string const& name, ClipPlayback& out,
    std::string& mapping, std::string& coverage);
[[nodiscard]] float live_clip_time(RE::hkbClipGenerator const* generator);

[[nodiscard]] bool clip_generator_is_live(void const* object);

struct ActiveClipEntry
{
    RE::hkbClipGenerator const* generator;
    std::string name;
};

void collect_running_clip_generators(RE::TESObjectREFR& source, std::vector<ActiveClipEntry>& out);

class ClipPlayer
{
public:
    void reset();
    void set_clip(ClipPlayback clip, bool loop = true);
    void retarget(ClipPlayback clip);
    // Puts the studio clock back to zero, so the next frame draws the clip from its beginning. The
    // inventory pauses the engine and the studio owns the clock, so without this the figure resumes
    // wherever the last visit left it.
    void restart();
    [[nodiscard]] bool ready() const { return m_clip.valid(); }
    [[nodiscard]] std::string const& name() const { return m_clip.name; }
    [[nodiscard]] bool finished() const { return ready() && !m_loop && m_time >= std::max(m_clip.duration, 0.05f); }
    [[nodiscard]] std::uint64_t cycles() const
    {
        return ready() && m_loop ? static_cast<std::uint64_t>(m_time / std::max(m_clip.duration, 0.05f)) : 0;
    }

    // Advances the clock and writes one frame of the clip; returns the nodes written.
    size_t advance(RE::NiAVObject& copy_root);

private:
    ClipPlayback m_clip{};
    double m_time = 0.0;
    std::chrono::steady_clock::time_point m_last_tick{};
    bool m_loop = true;
};

PLUGIN_NAMESPACE_END
