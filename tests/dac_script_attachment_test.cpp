//
// Created by AmazingBuff on 2026/10/21.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/dac/script_attachment.cpp"
#include <cassert>
#include <iostream>
#include <span>

namespace
{
    namespace dac = test_plugin::dac;
    using Bytes = std::vector<std::uint8_t>;

    void append_u16(Bytes& bytes, std::uint16_t value)
    {
        bytes.push_back(static_cast<std::uint8_t>(value));
        bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    }
    void append_u32(Bytes& bytes, std::uint32_t value)
    {
        append_u16(bytes, static_cast<std::uint16_t>(value));
        append_u16(bytes, static_cast<std::uint16_t>(value >> 16));
    }
    void append_record(Bytes& bytes, std::uint32_t type, std::uint32_t raw_id, Bytes const& payload)
    {
        append_u32(bytes, type);
        append_u32(bytes, static_cast<std::uint32_t>(payload.size()));
        append_u32(bytes, 0);
        append_u32(bytes, raw_id);
        append_u32(bytes, 0);
        append_u32(bytes, 44);
        bytes.insert(bytes.end(), payload.begin(), payload.end());
    }
    Bytes marker_payload(bool vmad, std::uint16_t scripts = 0)
    {
        Bytes payload;
        append_u32(payload, 0x44494445); // EDID
        append_u16(payload, 1);
        payload.push_back(0);
        if (vmad)
        {
            append_u32(payload, 0x44414D56);
            const Bytes script{ 6, 0, 'M', 'a', 'r', 'k', 'e', 'r', 0, 0, 0 };
            append_u16(payload, static_cast<std::uint16_t>(6 + (scripts ? script.size() : 0)));
            append_u16(payload, 5);
            append_u16(payload, 2);
            append_u16(payload, scripts);
            if (scripts) payload.insert(payload.end(), script.begin(), script.end());
        }
        return payload;
    }
    void write_plugin(std::filesystem::path const& path, std::span<Bytes const> records)
    {
        Bytes plugin;
        Bytes header;
        append_u32(header, 0x52444548); // HEDR
        append_u16(header, 12);
        append_u32(header, 0x3FCCCCCD);
        append_u32(header, static_cast<std::uint32_t>(records.size()));
        append_u32(header, 0x900);
        append_record(plugin, 0x34534554, 0, header);
        std::uint32_t group_size = 24;
        for (Bytes const& record : records) group_size += static_cast<std::uint32_t>(record.size());
        append_u32(plugin, 0x50555247);
        append_u32(plugin, group_size);
        append_u32(plugin, 0x4645474D);
        append_u32(plugin, 0);
        append_u32(plugin, 0);
        append_u32(plugin, 0);
        for (Bytes const& record : records) plugin.insert(plugin.end(), record.begin(), record.end());
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<char const*>(plugin.data()), static_cast<std::streamsize>(plugin.size()));
        assert(output);
    }
    struct FakeActor
    {
        std::uint64_t vm_handle;
        std::uint32_t running_scripts;
        int native_calls;
    };

    void script_free_markers_dispatch_despite_vm_handle()
    {
        const std::filesystem::path path = "build/dac-script-attachments/script-free.esp";
        std::array<Bytes, 2> records;
        append_record(records[0], 0x4645474D, 0x800, marker_payload(false));
        append_record(records[1], 0x4645474D, 0x801, marker_payload(true));
        write_plugin(path, records);
        const auto map_id = [](std::uint32_t raw) { return 0x02000000 | raw; };
        dac::ScriptAttachmentCache cache;
        FakeActor actor{ 42, 0, 0 };
        std::string reason;
        const dac::AttachmentResult first = cache.inspect(path, 0x02000800, &records, map_id);
        const dac::AttachmentResult second = cache.inspect(path, 0x02000801, &records, map_id);
        if (dac::permits_script_attachment(first, reason) && dac::permits_script_attachment(second, reason)) ++actor.native_calls;
        assert(first.state == dac::ScriptAttachment::e_clear && second.state == dac::ScriptAttachment::e_clear);
        assert(actor.vm_handle != 0 && actor.native_calls == 1 && reason.empty());
    }

    void scripted_winning_override_rejects_without_running_instance()
    {
        const std::filesystem::path base_path = "build/dac-script-attachments/base.esp";
        const std::filesystem::path override_path = "build/dac-script-attachments/scripted-override.esp";
        std::array<Bytes, 1> base_records;
        std::array<Bytes, 1> override_records;
        append_record(base_records[0], 0x4645474D, 0x800, marker_payload(false));
        append_record(override_records[0], 0x4645474D, 0x800, marker_payload(true, 1));
        write_plugin(base_path, base_records);
        write_plugin(override_path, override_records);
        const auto map_id = [](std::uint32_t raw) { return 0x02000000 | raw; };
        dac::ScriptAttachmentCache cache;
        const dac::AttachmentResult original = cache.inspect(base_path, 0x02000800, &base_records, map_id);
        assert(original.state == dac::ScriptAttachment::e_clear);
        FakeActor actor{ 0, 0, 0 };
        std::string reason;
        const dac::AttachmentResult winning = cache.inspect(override_path, 0x02000800, &override_records, map_id);
        if (dac::permits_script_attachment(winning, reason)) ++actor.native_calls;
        assert(winning.state == dac::ScriptAttachment::e_attached && !reason.empty());
        assert(actor.running_scripts == 0 && actor.native_calls == 0);
    }
}

int main()
{
    script_free_markers_dispatch_despite_vm_handle();
    scripted_winning_override_rejects_without_running_instance();
    std::cout << "dac_script_attachment_test: 2 cases passed\n";
}
