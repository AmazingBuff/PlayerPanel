//
// Created by AmazingBuff on 2026/10/21.
//
#include "dac/script_attachment.h"
#include <array>
#include <fstream>

PLUGIN_NAMESPACE_BEGIN
namespace dac
{
namespace
{
    constexpr std::uint32_t Tes4 = 0x34534554;
    constexpr std::uint32_t Grup = 0x50555247;
    constexpr std::uint32_t Mgef = 0x4645474D;
    constexpr std::uint32_t Vmad = 0x44414D56;
    constexpr std::uint32_t Xxxx = 0x58585858;
    constexpr std::uint64_t Record_Header_Size = 24;

    std::uint32_t u32(std::uint8_t const* bytes)
    {
        return std::uint32_t(bytes[0]) | std::uint32_t(bytes[1]) << 8 |
            std::uint32_t(bytes[2]) << 16 | std::uint32_t(bytes[3]) << 24;
    }
    std::uint16_t u16(std::uint8_t const* bytes)
    { return static_cast<std::uint16_t>(std::uint16_t(bytes[0]) | std::uint16_t(bytes[1]) << 8); }

    bool read_at(std::istream& input, std::uint64_t offset, void* buffer, std::size_t size)
    {
        input.seekg(static_cast<std::streamoff>(offset));
        return static_cast<bool>(input.read(static_cast<char*>(buffer), static_cast<std::streamsize>(size)));
    }
    AttachmentResult unavailable(char const* reason)
    { return { ScriptAttachment::e_unavailable, reason }; }

    AttachmentResult read_mgef(std::istream& input, std::uint64_t begin, std::uint64_t end)
    {
        bool found = false;
        AttachmentResult result{ ScriptAttachment::e_clear, "" };
        std::uint32_t extended_size = 0;
        bool extended = false;
        while (begin < end)
        {
            std::array<std::uint8_t, 6> header{};
            if (end - begin < header.size() || !read_at(input, begin, header.data(), header.size()))
                return unavailable("truncated MGEF subrecord header");
            begin += header.size();
            const std::uint32_t type = u32(header.data());
            const std::uint32_t size = extended ? extended_size : u16(header.data() + 4);
            if (size > end - begin) return unavailable("MGEF subrecord exceeds record boundary");
            if (type == Xxxx)
            {
                std::array<std::uint8_t, 4> length{};
                if (extended || size != length.size() || !read_at(input, begin, length.data(), length.size()))
                    return unavailable("invalid extended MGEF subrecord size");
                extended_size = u32(length.data());
                extended = true;
            }
            else
            {
                extended = false;
                if (type == Vmad)
                {
                    std::array<std::uint8_t, 6> vmad{};
                    if (found || size < vmad.size() || !read_at(input, begin, vmad.data(), vmad.size()))
                        return unavailable("duplicate or truncated VMAD header");
                    found = true;
                    const std::uint16_t version = u16(vmad.data());
                    const std::uint16_t format = u16(vmad.data() + 2);
                    const std::uint16_t scripts = u16(vmad.data() + 4);
                    if (version != 5 || (format != 1 && format != 2))
                        return unavailable("unsupported VMAD version or object format");
                    if (scripts != 0) result = { ScriptAttachment::e_attached, "MGEF definition attaches Papyrus scripts" };
                    else if (size != vmad.size()) return unavailable("unsupported empty VMAD payload");
                }
            }
            begin += size;
        }
        return extended ? unavailable("unfinished extended MGEF subrecord") : result;
    }
}

AttachmentResult read_script_attachment(std::istream& input, std::uint32_t runtime_id, FormIdMapper const& map_id)
{
    input.seekg(0, std::ios::end);
    const std::streamoff length = input.tellg();
    if (length < static_cast<std::streamoff>(Record_Header_Size)) return unavailable("plugin header unavailable");
    const std::uint64_t file_end = static_cast<std::uint64_t>(length);
    std::array<std::uint8_t, Record_Header_Size> header{};
    if (!read_at(input, 0, header.data(), header.size()) || u32(header.data()) != Tes4 ||
        u32(header.data() + 4) > file_end - Record_Header_Size)
        return unavailable("invalid TES4 header");
    std::uint64_t offset = Record_Header_Size + u32(header.data() + 4);
    bool found = false;
    AttachmentResult result = unavailable("MGEF definition not found in contributing plugin");
    while (offset < file_end)
    {
        if (file_end - offset < Record_Header_Size || !read_at(input, offset, header.data(), header.size()) ||
            u32(header.data()) != Grup)
            return unavailable("invalid top-level plugin group");
        const std::uint32_t group_size = u32(header.data() + 4);
        if (group_size < Record_Header_Size || group_size > file_end - offset)
            return unavailable("plugin group exceeds file boundary");
        const std::uint64_t group_end = offset + group_size;
        if (u32(header.data() + 8) == Mgef)
        {
            if (u32(header.data() + 12) != 0) return unavailable("unsupported MGEF group layout");
            std::uint64_t record = offset + Record_Header_Size;
            while (record < group_end)
            {
                if (group_end - record < Record_Header_Size || !read_at(input, record, header.data(), header.size()) ||
                    u32(header.data()) != Mgef)
                    return unavailable("invalid MGEF record header");
                const std::uint32_t size = u32(header.data() + 4);
                const std::uint64_t payload = record + Record_Header_Size;
                if (size > group_end - payload) return unavailable("MGEF record exceeds group boundary");
                if (map_id(u32(header.data() + 12)) == runtime_id)
                {
                    if (found) return unavailable("duplicate MGEF definition in contributing plugin");
                    found = true;
                    const std::uint32_t flags = u32(header.data() + 8);
                    if ((flags & 0x40000) != 0) return unavailable("compressed MGEF definition unsupported");
                    if ((flags & 0x1020) != 0) return unavailable("deleted or ignored MGEF definition");
                    result = read_mgef(input, payload, payload + size);
                    if (result.state == ScriptAttachment::e_unavailable) return result;
                }
                record = payload + size;
            }
        }
        offset = group_end;
    }
    return result;
}

bool permits_script_attachment(AttachmentResult result, std::string& reason)
{
    if (result.state == ScriptAttachment::e_clear) return true;
    reason = result.reason;
    return false;
}

AttachmentResult ScriptAttachmentCache::inspect(std::filesystem::path const& path, std::uint32_t runtime_id,
    void const* file_identity, FormIdMapper const& map_id)
{
    for (Entry const& entry : m_entries)
        if (entry.runtime_id == runtime_id && entry.file_identity == file_identity) return entry.result;
    std::ifstream input(path, std::ios::binary);
    if (!input) return unavailable("contributing plugin cannot be opened");
    const AttachmentResult result = read_script_attachment(input, runtime_id, map_id);
    if (result.state != ScriptAttachment::e_unavailable) m_entries.push_back({ runtime_id, file_identity, result });
    return result;
}
}
PLUGIN_NAMESPACE_END
