//
// C-G5-CAL K5: full-sample snapshot format v1 (2026-11-14 implementation
// pre-registration, docs/diagnostics/g5-k6-plan-20261114/README.md §10).
//
// The candidate and source writers mirror the K3 writers (same meta fields, same
// two-read rule, same track extraction) and add per-clip resource identity columns
// so the offline reconciliation can compare every active clip against the
// registered data FNV sets. Runtime code records drift facts only; the
// set-membership verdict is made offline (K6 plan §10.5). Engine-free: callers
// read poseLocal and hand the raw samples here.
//
#pragma once

#include "animation/behavior_graph/animation_resource_identity.h"
#include "animation/behavior_graph/k3_snapshot_policy.h"
#include "animation/behavior_graph/observation_chain_policy.h"

#include "RE/H/hkaAnimation.h"
#include "RE/H/hkaAnimationBinding.h"
#include "RE/H/hkaSplineCompressedAnimation.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <span>
#include <string>
#include <string_view>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    inline constexpr std::uint32_t K5_Track_Capacity = 116;
    inline constexpr std::uint32_t K5_Clip_Capacity = 8;
    inline constexpr std::size_t K5_Identity_Length = 96;
    inline constexpr std::size_t K5_Clip_Name_Length = 64;
    inline constexpr std::uint16_t K5_Version = 1;
    // 1 baseline + 120 equip + 129 unequip + headroom; 384 x 5,664 B ~= 2.07 MiB,
    // inside the pre-registered 8 MiB transform budget (K6 plan §10.3).
    inline constexpr std::uint32_t K5_Record_Capacity = 384;
    // 1,400 x 5,652 B ~= 7.55 MiB, inside the same budget; a cap exhausted before
    // the unequip completion is INVALID by the run criteria, not truncated.
    inline constexpr std::uint32_t K5S_Record_Capacity = 1400;
    inline constexpr std::uint16_t K5S_Version = 2;
    inline constexpr std::uint8_t K5_Flag_Clip_Overflow = 1u << 0;
    inline constexpr std::uint8_t K5_Flag_No_Pose = 1u << 1;
    inline constexpr std::uint8_t K5_Flag_Identity_Failed = 1u << 2;

#pragma pack(push, 1)
    // K3ClipRecord (76 B) + the identity columns: both stable-id pointer components,
    // duration bits and the K1-C4 resource hashes. name has the same 64 B layout.
    struct K5ClipRecord
    {
        std::uint64_t node_ptr;
        std::uint64_t binding_ptr;
        std::uint32_t binding_index;
        std::uint32_t local_time_bits;
        std::uint8_t mode;
        std::uint8_t pad[3];
        char name[K5_Clip_Name_Length];
        std::uint32_t duration_bits;
        std::uint64_t data_fnv;
        std::uint64_t blockoff_fnv;
        std::uint64_t map_fnv;
        std::uint32_t map_count;
    };
    static_assert(sizeof(K5ClipRecord) == 124);

    struct K5Header
    {
        char magic[4];
        std::uint16_t version;
        std::uint16_t header_size;
        std::uint32_t record_capacity;
        std::uint32_t track_capacity;
        std::uint32_t record_count;
        char identity[K5_Identity_Length];
    };
    static_assert(sizeof(K5Header) == 116);

    struct K5SourceHeader
    {
        char magic[4];
        std::uint16_t version;
        std::uint16_t header_size;
        std::uint32_t source;
        std::uint32_t record_capacity;
        std::uint32_t track_capacity;
        std::uint32_t record_size;
        std::uint32_t record_count;
        char identity[K5_Identity_Length];
    };
    static_assert(sizeof(K5SourceHeader) == 124);

    struct K5SourceRecord
    {
        std::uint64_t tsc;
        std::uint32_t seq;
        std::uint16_t track_count;
        std::uint16_t clip_count;
        std::uint8_t path;
        std::uint8_t two_read_equal;
        std::uint8_t flags;
        std::uint8_t pad;
        K5ClipRecord clips[K5_Clip_Capacity];
        K3TrackRecord tracks[K5_Track_Capacity];
    };
    static_assert(sizeof(K5SourceRecord) == 20 + 8 * sizeof(K5ClipRecord) + 116 * sizeof(K3TrackRecord));
#pragma pack(pop)

    // ---- per-clip resource identity cache ------------------------------------------------

    // Keyed by binding pointer. A hit is validated against cheap live header fields
    // (duration bits, map count, spline data bytes) so a same-address ABA mostly
    // recomputes instead of aliasing a stale identity. Fixed 64 slots; eviction only
    // costs a recompute, never correctness. Two instances (candidate drive thread,
    // source detour) never share state, so no cross-thread synchronization is needed.
    inline constexpr std::size_t K5_Identity_Cache_Slots = 64;

    class K5IdentityCache
    {
    public:
        struct Entry
        {
            bool ok;
            AnimationResourceIdentity identity;
        };

        // noexcept: read_animation_resource_identity allocates nothing and guards every
        // dereference; the catch is a belt-and-suspenders backstop for the detour call.
        [[nodiscard]] Entry identity_for(RE::hkaAnimationBinding const* a_binding) noexcept
        {
            if (!a_binding || !plausible_object_pointer(reinterpret_cast<std::uintptr_t>(a_binding)))
                return { false, AnimationResourceIdentity{} };
            std::uintptr_t const key = reinterpret_cast<std::uintptr_t>(a_binding);
            for (Slot& slot : m_slots)
            {
                std::uintptr_t const slot_key = slot.key.load(std::memory_order_acquire);
                if (slot_key == key)
                {
                    Entry const cached{ slot.ok, slot.identity };
                    if (validate(cached, a_binding))
                        return cached;
                    break;
                }
                if (slot_key == Writing_Sentinel)
                    continue;  // another writer owns the value window; treat as a miss
            }
            Entry const computed = compute(a_binding);
            store(key, computed);
            return computed;
        }

    private:
        struct Slot
        {
            std::atomic<std::uintptr_t> key{ 0 };
            bool ok{ false };
            AnimationResourceIdentity identity{};
        };

        // Publishing protocol: a slot transitions 0 -> sentinel (CAS, one writer owns the
        // value window), then value, then the real key (release). Readers treat the
        // sentinel as a miss, so a racing reader can never pair a key with another
        // binding's identity. Eviction CASes the old key to the sentinel and picks the
        // next slot on failure, so two evicting writers never interleave their pairs.
        static constexpr std::uintptr_t Writing_Sentinel = ~std::uintptr_t{ 0 };

        // The live header fields must still match the cached identity; failures and
        // previously-failed entries are recomputed on every sight (cheap, and the
        // only way a once-unreadable binding can recover).
        static bool validate(Entry const& a_entry, RE::hkaAnimationBinding const* a_binding) noexcept
        {
            if (!a_entry.ok)
                return false;
            if (a_entry.identity.map_count !=
                static_cast<std::uint32_t>(std::max(a_binding->transformTrackToBoneIndices.size(), 0)))
                return false;
            RE::hkaAnimation const* const animation = a_binding->animation.get();
            if (!animation || !plausible_object_pointer(reinterpret_cast<std::uintptr_t>(animation)))
                return false;
            if (a_entry.identity.duration_bits != std::bit_cast<std::uint32_t>(animation->duration))
                return false;
            if (!a_entry.identity.spline)
                return true;
            auto const* const spline = static_cast<RE::hkaSplineCompressedAnimation const*>(animation);
            return spline->data.size() >= 0 &&
                static_cast<std::uint32_t>(spline->data.size()) == a_entry.identity.data_bytes;
        }

        static Entry compute(RE::hkaAnimationBinding const* a_binding) noexcept
        {
            Entry out{ false, AnimationResourceIdentity{} };
            try
            {
                out.ok = read_animation_resource_identity(a_binding, out.identity);
            }
            catch (...)
            {
                out = { false, AnimationResourceIdentity{} };
            }
            return out;
        }

        void store(std::uintptr_t a_key, Entry const& a_entry) noexcept
        {
            Slot* target = nullptr;
            for (Slot& slot : m_slots)
            {
                std::uintptr_t expected = 0;
                if (slot.key.compare_exchange_strong(expected, Writing_Sentinel,
                    std::memory_order_release))
                {
                    target = &slot;
                    break;
                }
            }
            if (target == nullptr)
            {
                for (std::size_t i = 0; i < K5_Identity_Cache_Slots; ++i)
                {
                    Slot& slot = m_slots[m_next.fetch_add(1, std::memory_order_relaxed) %
                        K5_Identity_Cache_Slots];
                    std::uintptr_t expected = slot.key.load(std::memory_order_relaxed);
                    if (expected != Writing_Sentinel &&
                        slot.key.compare_exchange_strong(expected, Writing_Sentinel,
                            std::memory_order_release))
                    {
                        target = &slot;
                        break;
                    }
                }
            }
            if (target == nullptr)
                return;  // every slot is mid-write; the caller simply recomputes next sight
            target->ok = a_entry.ok;
            target->identity = a_entry.identity;
            target->key.store(a_key, std::memory_order_release);
        }

        std::array<Slot, K5_Identity_Cache_Slots> m_slots{};
        std::atomic<std::size_t> m_next{ 0 };
    };

    // ---- reconciliation counters ----------------------------------------------------------

    // Atomics so the source-side instance is shared between the detour threads and the
    // render drain; the candidate instance runs on one thread but keeps the same shape.
    struct K5ReconCounters
    {
        std::atomic<std::uint64_t> attempted{ 0 };
        std::atomic<std::uint64_t> recorded{ 0 };
        std::atomic<std::uint64_t> dropped{ 0 };
        std::atomic<std::uint64_t> truncated{ 0 };
        std::atomic<std::uint64_t> two_read_bad{ 0 };
        std::atomic<std::uint64_t> clip_overflow{ 0 };
        std::atomic<std::uint64_t> no_pose{ 0 };
        std::atomic<std::uint64_t> write_failed{ 0 };
        std::atomic<std::uint64_t> identity_failed_clips{ 0 };
        std::atomic<std::uint64_t> drift_records{ 0 };
    };

    // ---- candidate writer ------------------------------------------------------------------

    class K5SnapshotWriter
    {
    public:
        K5SnapshotWriter() = default;
        ~K5SnapshotWriter() { (void)close(); }
        K5SnapshotWriter(K5SnapshotWriter const&) = delete;
        K5SnapshotWriter& operator=(K5SnapshotWriter const&) = delete;

        [[nodiscard]] bool open(std::string a_path, std::string_view a_identity)
        {
            (void)close();
            m_path = std::move(a_path);
            m_file.open(m_path, std::ios::binary | std::ios::trunc);
            if (!m_file)
                return false;
            K5Header header{};
            std::memcpy(header.magic, "CPK5", 4);
            header.version = K5_Version;
            header.header_size = static_cast<std::uint16_t>(sizeof(K5Header));
            header.record_capacity = K5_Record_Capacity;
            header.track_capacity = K5_Track_Capacity;
            header.record_count = 0;
            std::memset(header.identity, 0, sizeof(header.identity));
            std::memcpy(header.identity, a_identity.data(),
                a_identity.size() < sizeof(header.identity) ? a_identity.size() : sizeof(header.identity) - 1);
            m_header = header;
            m_file.write(reinterpret_cast<char const*>(&header), sizeof(header));
            return static_cast<bool>(m_file);
        }

        [[nodiscard]] bool append(std::uint8_t a_segment, std::uint32_t a_completed,
            std::uint32_t a_commanded, double a_elapsed, bool a_two_read_equal,
            std::span<K5ClipRecord const> a_clips, std::span<K3TrackRecord const> a_tracks)
        {
            if (!m_file || m_overflow)
                return false;
            if (m_records >= K5_Record_Capacity || a_tracks.size() > K5_Track_Capacity)
            {
                m_overflow = true;
                return false;
            }
            std::uint32_t const segment = a_segment;
            std::uint32_t const completed = a_completed;
            std::uint32_t const commanded = a_commanded;
            std::uint32_t const track_count = static_cast<std::uint32_t>(a_tracks.size());
            std::uint32_t const two_read_equal = a_two_read_equal ? 1u : 0u;
            std::uint32_t const clip_count = static_cast<std::uint32_t>(
                a_clips.size() < K5_Clip_Capacity ? a_clips.size() : K5_Clip_Capacity);
            double const elapsed = a_elapsed;
            std::uint64_t elapsed_raw = 0;
            std::memcpy(&elapsed_raw, &elapsed, sizeof(elapsed_raw));
            write(segment);
            write(completed);
            write(commanded);
            write(track_count);
            write(two_read_equal);
            write(clip_count);
            write(elapsed_raw);
            K5ClipRecord empty{};
            for (std::uint32_t i = 0; i < K5_Clip_Capacity; ++i)
                m_file.write(reinterpret_cast<char const*>(i < clip_count ? &a_clips[i] : &empty),
                    sizeof(K5ClipRecord));
            K3TrackRecord zero{};
            for (std::uint32_t i = 0; i < K5_Track_Capacity; ++i)
                m_file.write(reinterpret_cast<char const*>(i < track_count ? &a_tracks[i] : &zero),
                    sizeof(K3TrackRecord));
            if (!m_file)
                return false;
            ++m_records;
            return true;
        }

        [[nodiscard]] bool close()
        {
            if (!m_file)
                return false;
            m_file.seekp(static_cast<std::streamoff>(offsetof(K5Header, record_count)), std::ios::beg);
            m_file.write(reinterpret_cast<char const*>(&m_records), sizeof(m_records));
            m_file.close();
            return !m_file.fail();
        }

        [[nodiscard]] std::uint32_t records() const { return m_records; }
        [[nodiscard]] bool overflow() const { return m_overflow; }
        [[nodiscard]] bool is_open() const { return m_file.is_open(); }
        [[nodiscard]] std::string const& path() const { return m_path; }

    private:
        template <class T>
        void write(T const& a_value)
        {
            m_file.write(reinterpret_cast<char const*>(&a_value), sizeof(a_value));
        }

        std::ofstream m_file;
        K5Header m_header{};
        std::string m_path;
        std::uint32_t m_records{ 0 };
        bool m_overflow{ false };
    };

    // ---- source writer ----------------------------------------------------------------------

    class K5SourceWriter
    {
    public:
        K5SourceWriter() = default;
        ~K5SourceWriter() { (void)close(); }
        K5SourceWriter(K5SourceWriter const&) = delete;
        K5SourceWriter& operator=(K5SourceWriter const&) = delete;

        [[nodiscard]] bool open(std::string a_path, std::string_view a_identity)
        {
            (void)close();
            m_path = std::move(a_path);
            m_file.open(m_path, std::ios::binary | std::ios::trunc);
            if (!m_file)
                return false;
            K5SourceHeader header{};
            std::memcpy(header.magic, "CPK5", 4);
            header.version = K5S_Version;
            header.header_size = static_cast<std::uint16_t>(sizeof(K5SourceHeader));
            header.source = 1;
            header.record_capacity = K5S_Record_Capacity;
            header.track_capacity = K5_Track_Capacity;
            header.record_size = static_cast<std::uint32_t>(sizeof(K5SourceRecord));
            header.record_count = 0;
            std::memset(header.identity, 0, sizeof(header.identity));
            std::memcpy(header.identity, a_identity.data(),
                a_identity.size() < sizeof(header.identity) ? a_identity.size() : sizeof(header.identity) - 1);
            m_file.write(reinterpret_cast<char const*>(&header), sizeof(header));
            return static_cast<bool>(m_file);
        }

        [[nodiscard]] bool append(K5SourceRecord const& a_record)
        {
            if (!m_file || m_overflow)
                return false;
            if (m_records >= K5S_Record_Capacity)
            {
                m_overflow = true;
                return false;
            }
            m_file.write(reinterpret_cast<char const*>(&a_record), sizeof(a_record));
            if (!m_file)
                return false;
            ++m_records;
            return true;
        }

        [[nodiscard]] bool close()
        {
            if (!m_file)
                return false;
            m_file.seekp(static_cast<std::streamoff>(offsetof(K5SourceHeader, record_count)), std::ios::beg);
            m_file.write(reinterpret_cast<char const*>(&m_records), sizeof(m_records));
            m_file.close();
            return !m_file.fail();
        }

        [[nodiscard]] std::uint32_t records() const { return m_records; }
        [[nodiscard]] bool overflow() const { return m_overflow; }
        [[nodiscard]] bool is_open() const { return m_file.is_open(); }
        [[nodiscard]] std::string const& path() const { return m_path; }

    private:
        std::ofstream m_file;
        std::string m_path;
        std::uint32_t m_records{ 0 };
        bool m_overflow{ false };
    };
}
PLUGIN_NAMESPACE_END
