//
// C-G5-CAL K3-C engine-free cases: the binary snapshot header/record layout, the patched record
// count, the two-read flag, clip-name truncation and the fixed track stride.
//
// Build:
//   cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests\k3_snapshot_test.cpp
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/behavior_graph/k3_snapshot_policy.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace test_plugin::animation::behavior_graph;

namespace
{
    std::string const kPath = "build/g5-k3-20261113/k3_snapshot_test.bin";

    void round_trip_writes_header_records_and_count()
    {
        std::filesystem::create_directories("build/g5-k3-20261113");
        std::remove(kPath.c_str());
        K3SnapshotWriter writer;
        assert(writer.open(kPath, "CharacterPanel-test-identity"));
        assert(writer.is_open());

        std::array<K3TrackRecord, 3> tracks{};
        for (std::uint32_t t = 0; t < tracks.size(); ++t)
        {
            for (std::uint32_t c = 0; c < 10; ++c)
                tracks[t].value[c] = static_cast<float>(t * 10 + c) + 0.5f;
        }
        std::array<K3ClipRecord, 1> clips{};
        clips[0].binding_index = 22;
        clips[0].local_time_bits = 0x3F800000u;  // 1.0f
        clips[0].mode = 1;
        char const* const clip_name = "Animations\\1HM_Equip.hkx";
        std::memcpy(clips[0].name, clip_name, std::strlen(clip_name) + 1);
        assert(writer.append(0, 1, 1, 0.01666666753590107, true, clips, tracks));
        assert(writer.append(1, 2, 2, 0.03333333507180214, false,
            std::span<K3ClipRecord const>{}, std::span<K3TrackRecord const>{}));
        assert(writer.records() == 2);
        assert(writer.close());

        std::ifstream file(kPath, std::ios::binary);
        assert(file);
        K3Header header{};
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        assert(std::memcmp(header.magic, "CPK3", 4) == 0);
        assert(header.version == K3_Version);
        assert(header.header_size == sizeof(K3Header));
        assert(header.record_capacity == K3_Record_Capacity);
        assert(header.track_capacity == K3_Track_Capacity);
        assert(header.record_count == 2);
        assert(std::strcmp(header.identity, "CharacterPanel-test-identity") == 0);

        // Record 0: header fields, one clip, three tracks.
        std::uint32_t segment = 0, completed = 0, commanded = 0, track_count = 0;
        std::uint32_t two_read = 0, clip_count = 0;
        std::uint64_t elapsed_bits = 0;
        file.read(reinterpret_cast<char*>(&segment), 4);
        file.read(reinterpret_cast<char*>(&completed), 4);
        file.read(reinterpret_cast<char*>(&commanded), 4);
        file.read(reinterpret_cast<char*>(&track_count), 4);
        file.read(reinterpret_cast<char*>(&two_read), 4);
        file.read(reinterpret_cast<char*>(&clip_count), 4);
        file.read(reinterpret_cast<char*>(&elapsed_bits), 8);
        assert(segment == 0 && completed == 1 && commanded == 1);
        assert(track_count == 3 && two_read == 1 && clip_count == 1);
        double elapsed = 0.0;
        std::memcpy(&elapsed, &elapsed_bits, sizeof(elapsed));
        assert(elapsed > 0.0166 && elapsed < 0.0167);
        std::array<K3ClipRecord, K3_Clip_Capacity> clip_slots{};
        file.read(reinterpret_cast<char*>(clip_slots.data()), sizeof(clip_slots));
        assert(clip_slots[0].binding_index == 22);
        assert(clip_slots[0].local_time_bits == 0x3F800000u);
        assert(clip_slots[0].mode == 1);
        assert(std::strcmp(clip_slots[0].name, "Animations\\1HM_Equip.hkx") == 0);
        std::array<K3TrackRecord, K3_Track_Capacity> track_slots{};
        file.read(reinterpret_cast<char*>(track_slots.data()), sizeof(track_slots));
        assert(track_slots[0].value[0] == 0.5f);
        assert(track_slots[2].value[9] == 29.5f);

        // Record 1: zero tracks/clips, two-read false.
        file.read(reinterpret_cast<char*>(&segment), 4);
        file.read(reinterpret_cast<char*>(&completed), 4);
        file.read(reinterpret_cast<char*>(&commanded), 4);
        file.read(reinterpret_cast<char*>(&track_count), 4);
        file.read(reinterpret_cast<char*>(&two_read), 4);
        file.read(reinterpret_cast<char*>(&clip_count), 4);
        file.read(reinterpret_cast<char*>(&elapsed_bits), 8);
        assert(segment == 1 && completed == 2 && commanded == 2);
        assert(track_count == 0 && two_read == 0 && clip_count == 0);
    }

    void overflow_stops_writing_and_is_reported()
    {
        std::filesystem::create_directories("build/g5-k3-20261113");
        std::remove(kPath.c_str());
        K3SnapshotWriter writer;
        assert(writer.open(kPath, "overflow-test"));
        std::array<K3TrackRecord, K3_Track_Capacity> tracks{};
        std::uint32_t written = 0;
        for (std::uint32_t i = 0; i < K3_Record_Capacity + 5; ++i)
        {
            if (writer.append(0, i + 1, i + 1, 0.0, true, std::span<K3ClipRecord const>{}, tracks))
                ++written;
            else
                break;
        }
        assert(written == K3_Record_Capacity);
        assert(writer.overflow());
        assert(writer.close());
    }

    // K3-S: v2 header + source record round-trip and the 1,024-record cap.
    void source_round_trip_writes_v2_header_and_record()
    {
        std::filesystem::create_directories("build/g5-k3-20261113");
        std::string const path = "build/g5-k3-20261113/k3s_test.bin";
        std::remove(path.c_str());
        K3SourceWriter writer;
        assert(writer.open(path, "source-identity"));
        K3SourceRecord record{};
        record.seq = 7;
        record.tsc = 0x1122334455667788ull;
        record.track_count = 4;
        record.clip_count = 1;
        record.path = 1;
        record.two_read_equal = 1;
        record.clips[0].binding_index = 75;
        record.clips[0].local_time_bits = 0x40000000u;  // 2.0f
        record.clips[0].mode = 0;
        std::memcpy(record.clips[0].name, "Animations\\1HM_Unequip.hkx", 26);
        for (std::uint32_t i = 0; i < 4; ++i)
            for (std::uint32_t c = 0; c < 10; ++c)
                record.tracks[i].value[c] = static_cast<float>(i * 10 + c) * 0.25f;
        assert(writer.append(record));
        assert(writer.records() == 1);
        assert(writer.close());

        std::ifstream file(path, std::ios::binary);
        assert(file);
        K3SourceHeader header{};
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        assert(std::memcmp(header.magic, "CPK3", 4) == 0);
        assert(header.version == 2);
        assert(header.header_size == sizeof(K3SourceHeader));
        assert(header.source == 1);
        assert(header.record_capacity == K3S_Record_Capacity);
        assert(header.track_capacity == K3S_Track_Capacity);
        assert(header.record_size == sizeof(K3SourceRecord));
        assert(header.record_count == 1);
        assert(std::strcmp(header.identity, "source-identity") == 0);
        K3SourceRecord back{};
        file.read(reinterpret_cast<char*>(&back), sizeof(back));
        assert(back.seq == 7);
        assert(back.tsc == 0x1122334455667788ull);
        assert(back.track_count == 4 && back.clip_count == 1 && back.path == 1);
        assert(back.two_read_equal == 1 && back.flags == 0);
        assert(back.clips[0].binding_index == 75);
        assert(back.clips[0].local_time_bits == 0x40000000u);
        assert(std::strcmp(back.clips[0].name, "Animations\\1HM_Unequip.hkx") == 0);
        assert(back.tracks[3].value[9] == 9.75f);  // (3*10+9) * 0.25
    }

    void source_writer_caps_at_its_capacity()
    {
        std::filesystem::create_directories("build/g5-k3-20261113");
        std::string const path = "build/g5-k3-20261113/k3s_overflow.bin";
        std::remove(path.c_str());
        K3SourceWriter writer;
        assert(writer.open(path, "overflow"));
        K3SourceRecord record{};
        std::uint32_t written = 0;
        for (std::uint32_t i = 0; i < K3S_Record_Capacity + 3; ++i)
        {
            record.seq = i + 1;
            if (writer.append(record))
                ++written;
            else
                break;
        }
        assert(written == K3S_Record_Capacity);
        assert(writer.overflow());
        assert(writer.close());
    }
}

int main()
{
    round_trip_writes_header_records_and_count();
    overflow_stops_writing_and_is_reported();
    source_round_trip_writes_v2_header_and_record();
    source_writer_caps_at_its_capacity();
    std::puts("4 K3 snapshot cases passed (engine-free)");
}
