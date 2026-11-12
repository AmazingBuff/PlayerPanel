//
// C-G5-CAL K3-C: candidate same-generation full snapshot writer (2026-11-13 pre-registration).
//
// Engine-free: the caller reads poseLocal twice and hands the raw float32 samples here. The file
// is little-endian (x86), fixed-record, and bounded to 512 records (~2.7 MiB). The record count is
// patched at close; the identity is copied into the header for later matching.
//
#pragma once

#include <array>
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
    inline constexpr std::uint32_t K3_Track_Capacity = 116;
    inline constexpr std::uint32_t K3_Clip_Capacity = 8;
    inline constexpr std::uint32_t K3_Record_Capacity = 512;
    inline constexpr std::size_t K3_Identity_Length = 96;
    inline constexpr std::size_t K3_Clip_Name_Length = 64;
    inline constexpr std::uint16_t K3_Version = 1;

    struct K3ClipRecord
    {
        std::uint32_t binding_index;
        std::uint32_t local_time_bits;
        std::uint8_t mode;
        std::uint8_t pad[3];
        char name[K3_Clip_Name_Length];
    };
    static_assert(sizeof(K3ClipRecord) == 76);

    struct K3TrackRecord
    {
        float value[10];  // translation xyz, rotation xyzw, scale xyz
    };
    static_assert(sizeof(K3TrackRecord) == 40);

    struct K3Header
    {
        char magic[4];
        std::uint16_t version;
        std::uint16_t header_size;
        std::uint32_t record_capacity;
        std::uint32_t track_capacity;
        std::uint32_t record_count;
        char identity[K3_Identity_Length];
    };
    static_assert(sizeof(K3Header) == 116);

    class K3SnapshotWriter
    {
    public:
        K3SnapshotWriter() = default;
        ~K3SnapshotWriter() { (void)close(); }
        K3SnapshotWriter(K3SnapshotWriter const&) = delete;
        K3SnapshotWriter& operator=(K3SnapshotWriter const&) = delete;

        [[nodiscard]] bool open(std::string a_path, std::string_view a_identity)
        {
            (void)close();
            m_path = std::move(a_path);
            m_file.open(m_path, std::ios::binary | std::ios::trunc);
            if (!m_file)
                return false;
            K3Header header{};
            std::memcpy(header.magic, "CPK3", 4);
            header.version = K3_Version;
            header.header_size = static_cast<std::uint16_t>(sizeof(K3Header));
            header.record_capacity = K3_Record_Capacity;
            header.track_capacity = K3_Track_Capacity;
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
            std::span<K3ClipRecord const> a_clips, std::span<K3TrackRecord const> a_tracks)
        {
            if (!m_file || m_overflow)
                return false;
            if (m_records >= K3_Record_Capacity || a_tracks.size() > K3_Track_Capacity)
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
                a_clips.size() < K3_Clip_Capacity ? a_clips.size() : K3_Clip_Capacity);
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
            K3ClipRecord empty{};
            for (std::uint32_t i = 0; i < K3_Clip_Capacity; ++i)
                m_file.write(reinterpret_cast<char const*>(i < clip_count ? &a_clips[i] : &empty),
                    sizeof(K3ClipRecord));
            K3TrackRecord zero{};
            for (std::uint32_t i = 0; i < K3_Track_Capacity; ++i)
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
            m_file.seekp(static_cast<std::streamoff>(offsetof(K3Header, record_count)), std::ios::beg);
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
        K3Header m_header{};
        std::string m_path;
        std::uint32_t m_records{ 0 };
        bool m_overflow{ false };
    };

    // ---- K3-S: source-side per-apply snapshot (v2) ---------------------------------------

    // v99b: the 20 s F7 window produced ~1,050 source applies at the observed ~52.5/s, so the
    // original 1,024 cap truncated the tail (v99a run 1: overflow=true, records=1024). 1,536
    // records x 5,268 B = 8,091,648 B (7.72 MiB), still under the pre-registered 8 MiB budget.
    inline constexpr std::uint32_t K3S_Record_Capacity = 1536;
    inline constexpr std::uint32_t K3S_Track_Capacity = K3_Track_Capacity;
    inline constexpr std::uint32_t K3S_Clip_Capacity = K3_Clip_Capacity;
    inline constexpr std::uint16_t K3S_Version = 2;
    inline constexpr std::uint8_t K3S_Flag_Clip_Overflow = 1u << 0;
    inline constexpr std::uint8_t K3S_Flag_No_Pose = 1u << 1;

#pragma pack(push, 1)
    struct K3SourceRecord
    {
        std::uint64_t tsc;
        std::uint32_t seq;
        std::uint16_t track_count;
        std::uint16_t clip_count;
        std::uint8_t path;
        std::uint8_t two_read_equal;
        std::uint8_t flags;
        std::uint8_t pad;
        K3ClipRecord clips[K3S_Clip_Capacity];
        K3TrackRecord tracks[K3S_Track_Capacity];
    };
    static_assert(sizeof(K3SourceRecord) == 20 + 8 * 76 + 116 * 40);

    struct K3SourceHeader
    {
        char magic[4];
        std::uint16_t version;
        std::uint16_t header_size;
        std::uint32_t source;
        std::uint32_t record_capacity;
        std::uint32_t track_capacity;
        std::uint32_t record_size;
        std::uint32_t record_count;
        char identity[K3_Identity_Length];
    };
    static_assert(sizeof(K3SourceHeader) == 124);
#pragma pack(pop)

    class K3SourceWriter
    {
    public:
        K3SourceWriter() = default;
        ~K3SourceWriter() { (void)close(); }
        K3SourceWriter(K3SourceWriter const&) = delete;
        K3SourceWriter& operator=(K3SourceWriter const&) = delete;

        [[nodiscard]] bool open(std::string a_path, std::string_view a_identity)
        {
            (void)close();
            m_path = std::move(a_path);
            m_file.open(m_path, std::ios::binary | std::ios::trunc);
            if (!m_file)
                return false;
            K3SourceHeader header{};
            std::memcpy(header.magic, "CPK3", 4);
            header.version = K3S_Version;
            header.header_size = static_cast<std::uint16_t>(sizeof(K3SourceHeader));
            header.source = 1;
            header.record_capacity = K3S_Record_Capacity;
            header.track_capacity = K3S_Track_Capacity;
            header.record_size = static_cast<std::uint32_t>(sizeof(K3SourceRecord));
            header.record_count = 0;
            std::memset(header.identity, 0, sizeof(header.identity));
            std::memcpy(header.identity, a_identity.data(),
                a_identity.size() < sizeof(header.identity) ? a_identity.size() : sizeof(header.identity) - 1);
            m_file.write(reinterpret_cast<char const*>(&header), sizeof(header));
            return static_cast<bool>(m_file);
        }

        [[nodiscard]] bool append(K3SourceRecord const& a_record)
        {
            if (!m_file || m_overflow)
                return false;
            if (m_records >= K3S_Record_Capacity)
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
            m_file.seekp(static_cast<std::streamoff>(offsetof(K3SourceHeader, record_count)), std::ios::beg);
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
