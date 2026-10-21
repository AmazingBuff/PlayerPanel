//
// Created by AmazingBuff on 2026/10/21.
//
#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <istream>
#include <string>
#include <vector>

PLUGIN_NAMESPACE_BEGIN
namespace dac
{
    enum class ScriptAttachment : std::uint8_t
    {
        e_clear,
        e_attached,
        e_unavailable
    };
    struct AttachmentResult
    {
        ScriptAttachment state;
        char const* reason;
    };
    using FormIdMapper = std::function<std::uint32_t(std::uint32_t)>;

    [[nodiscard]] AttachmentResult read_script_attachment(std::istream& input, std::uint32_t runtime_id, FormIdMapper const& map_id);
    [[nodiscard]] bool permits_script_attachment(AttachmentResult result, std::string& reason);

    class ScriptAttachmentCache
    {
    public:
        [[nodiscard]] AttachmentResult inspect(std::filesystem::path const& path, std::uint32_t runtime_id,
            void const* file_identity, FormIdMapper const& map_id);

    private:
        struct Entry
        {
            std::uint32_t runtime_id;
            void const* file_identity;
            AttachmentResult result;
        };
        std::vector<Entry> m_entries;
    };
}
PLUGIN_NAMESPACE_END
