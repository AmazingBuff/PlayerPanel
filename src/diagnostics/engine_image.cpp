//
// Created by AmazingBuff on 2026/10/13.
//

#include "diagnostics/engine_image.h"

#if CHARACTER_PANEL_SCENE_COPY_EXPERIMENT

#include <Windows.h>

#include <cstring>
#include <fstream>

namespace
{
    constexpr std::size_t Dump_Chunk = 4u * 1024u * 1024u;
    constexpr std::size_t Max_Text_Size = 64u * 1024u * 1024u;
    constexpr std::size_t Vtable_Slots = 0x31;
    constexpr std::uint64_t Fnv_Offset = 14695981039346656037ull;
    constexpr std::uint64_t Fnv_Prime = 1099511628211ull;
    constexpr char const* Text_File = "CharacterPanel-engine-text.bin";
    constexpr char const* Anchor_File = "CharacterPanel-engine-text.txt";
    // The two engine frames a CrashLogger log in this repo attributed to the animation classes, as
    // RVAs of the 1.6.1170 image (docs/diagnostics/crash-2026-10-12-01-27-26.log). Named by the crash
    // logger's own database, so they are hints to check against the dump, not facts.
    constexpr std::uint32_t Hint_Anim_Graph = 0xbae860;
    constexpr std::uint32_t Hint_Anim_Manager = 0xba3c50;

    struct Section
    {
        char name[9]{};
        std::uint32_t rva = 0;
        std::uint32_t virtual_size = 0;
        std::uint32_t raw_size = 0;
    };

    struct Layout
    {
        std::uintptr_t base = 0;
        std::uint32_t entry = 0;
        std::uint32_t image_size = 0;
        std::uint32_t text_rva = 0;
        std::uint32_t text_size = 0;
        std::vector<Section> sections;
    };

    // Every range this file touches is checked against the page map first: a section header read out
    // of a hostile or half-mapped image must not become a fault in the game's own process.
    [[nodiscard]] bool readable(void const* a_address, std::size_t a_size)
    {
        if (!a_address || a_size == 0)
            return false;

        std::uintptr_t cursor = reinterpret_cast<std::uintptr_t>(a_address);
        std::uintptr_t const end = cursor + a_size;
        while (cursor < end)
        {
            MEMORY_BASIC_INFORMATION info{};
            if (::VirtualQuery(reinterpret_cast<void const*>(cursor), &info, sizeof(info)) == 0)
                return false;
            if (info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0)
                return false;

            std::uintptr_t const region_end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
            if (region_end <= cursor)
                return false;
            cursor = region_end;
        }
        return true;
    }

    template <class T>
    [[nodiscard]] T read_at(std::uintptr_t a_address)
    {
        T value{};
        std::memcpy(&value, reinterpret_cast<void const*>(a_address), sizeof(T));
        return value;
    }

    [[nodiscard]] bool read_layout(Layout& a_layout)
    {
        std::uintptr_t const base = REL::Module::get().base();
        if (!base || !readable(reinterpret_cast<void const*>(base), 0x40))
            return false;

        std::uintptr_t const pe = base + read_at<std::uint32_t>(base + 0x3C);
        if (!readable(reinterpret_cast<void const*>(pe), 0x18) || read_at<std::uint32_t>(pe) != 0x00004550u)
            return false;

        std::uint16_t const section_count = read_at<std::uint16_t>(pe + 6);
        std::uint16_t const optional_size = read_at<std::uint16_t>(pe + 20);
        std::uintptr_t const optional = pe + 24;
        if (!readable(reinterpret_cast<void const*>(optional), optional_size) || read_at<std::uint16_t>(optional) != 0x20Bu)
            return false;

        std::uintptr_t const table = optional + optional_size;
        if (!readable(reinterpret_cast<void const*>(table), static_cast<std::size_t>(section_count) * 40))
            return false;

        a_layout.base = base;
        a_layout.entry = read_at<std::uint32_t>(optional + 16);
        a_layout.image_size = read_at<std::uint32_t>(optional + 56);

        for (std::uint16_t index = 0; index < section_count; ++index)
        {
            std::uintptr_t const entry = table + 40 * static_cast<std::uintptr_t>(index);
            Section section;
            std::memcpy(section.name, reinterpret_cast<void const*>(entry), 8);
            section.virtual_size = read_at<std::uint32_t>(entry + 8);
            section.rva = read_at<std::uint32_t>(entry + 12);
            section.raw_size = read_at<std::uint32_t>(entry + 16);
            // Two sections carry the name `.text` in these images; the code is the larger one.
            if (std::strncmp(section.name, ".text", 5) == 0 && section.virtual_size > a_layout.text_size)
            {
                a_layout.text_rva = section.rva;
                a_layout.text_size = section.virtual_size;
            }
            a_layout.sections.push_back(section);
        }

        return a_layout.text_size > 0 && a_layout.text_size <= Max_Text_Size;
    }

    [[nodiscard]] std::uint64_t hash_bytes(std::uint8_t const* a_data, std::size_t a_size, std::uint64_t a_hash)
    {
        for (std::size_t index = 0; index < a_size; ++index)
        {
            a_hash ^= a_data[index];
            a_hash *= Fnv_Prime;
        }
        return a_hash;
    }

    [[nodiscard]] std::uint64_t hash_memory(std::uint8_t const* a_data, std::size_t a_size)
    {
        std::uint64_t hash = Fnv_Offset;
        for (std::size_t offset = 0; offset < a_size; offset += Dump_Chunk)
        {
            std::size_t const chunk = std::min(Dump_Chunk, a_size - offset);
            hash = hash_bytes(a_data + offset, chunk, hash);
        }
        return hash;
    }

    [[nodiscard]] std::uint64_t hash_file(std::filesystem::path const& a_path, std::size_t a_size)
    {
        std::ifstream in(a_path, std::ios::binary);
        if (!in)
            return 0;

        std::vector<std::uint8_t> buffer(Dump_Chunk);
        std::uint64_t hash = Fnv_Offset;
        std::size_t remaining = a_size;
        while (remaining > 0)
        {
            std::size_t const chunk = std::min(buffer.size(), remaining);
            in.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(chunk));
            if (in.gcount() != static_cast<std::streamsize>(chunk))
                return 0;
            hash = hash_bytes(buffer.data(), chunk, hash);
            remaining -= chunk;
        }
        return hash;
    }

    [[nodiscard]] std::string rva_text(std::uintptr_t a_base, std::uintptr_t a_address)
    {
        if (!a_base || a_address < a_base)
            return "unresolved";
        return fmt::format("0x{:x}", a_address - a_base);
    }

    void write_vtable(std::ofstream& a_out, char const* a_name, std::uint64_t a_id, std::uintptr_t a_table, std::uintptr_t a_base)
    {
        if (!a_table || !readable(reinterpret_cast<void const*>(a_table), Vtable_Slots * sizeof(std::uintptr_t)))
        {
            a_out << fmt::format("vtable {} id={} unreadable\n", a_name, a_id);
            return;
        }

        a_out << fmt::format("vtable {} id={} rva={}\n", a_name, a_id, rva_text(a_base, a_table));
        for (std::size_t slot = 0; slot < Vtable_Slots; ++slot)
        {
            std::uintptr_t const target = read_at<std::uintptr_t>(a_table + slot * sizeof(std::uintptr_t));
            a_out << fmt::format("  slot=0x{:02x} rva={}\n", slot, rva_text(a_base, target));
        }
    }
}
#endif

PLUGIN_NAMESPACE_BEGIN

#if CHARACTER_PANEL_SCENE_COPY_EXPERIMENT

void EngineImage::dump_once()
{
    static bool attempted = false;
    if (attempted)
        return;
    attempted = true;

    std::optional<std::filesystem::path> const directory = SKSE::log::log_directory();
    if (!directory)
    {
        logger::warn("SCOPY ENGINE text dump skipped reason=no-log-directory");
        return;
    }

    Layout layout;
    if (!read_layout(layout))
    {
        logger::warn("SCOPY ENGINE text dump skipped reason=pe-headers-unreadable");
        return;
    }

    std::uint8_t const* const text = reinterpret_cast<std::uint8_t const*>(layout.base + layout.text_rva);
    if (!readable(text, layout.text_size))
    {
        logger::warn("SCOPY ENGINE text dump skipped reason=text-section-not-committed rva=0x{:x} size=0x{:x}",
            layout.text_rva, layout.text_size);
        return;
    }

    std::filesystem::path const text_path = *directory / Text_File;
    std::filesystem::path const anchor_path = *directory / Anchor_File;

    std::error_code error;
    bool const cached = std::filesystem::exists(text_path, error) &&
                        std::filesystem::file_size(text_path, error) == layout.text_size;

    std::uint64_t hash = 0;
    if (cached)
    {
        hash = hash_file(text_path, layout.text_size);
    }
    else
    {
        std::ofstream out(text_path, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            logger::warn("SCOPY ENGINE text dump skipped reason=open-failed path='{}'", text_path.string());
            return;
        }

        std::size_t written = 0;
        for (std::size_t offset = 0; offset < layout.text_size; offset += Dump_Chunk)
        {
            std::size_t const chunk = std::min(Dump_Chunk, layout.text_size - offset);
            out.write(reinterpret_cast<char const*>(text + offset), static_cast<std::streamsize>(chunk));
            written += chunk;
        }
        out.close();

        if (!out || written != layout.text_size)
        {
            logger::warn("SCOPY ENGINE text dump failed path='{}' written=0x{:x} expected=0x{:x}",
                text_path.string(), written, layout.text_size);
            return;
        }
        hash = hash_memory(text, layout.text_size);
    }

    // The anchors: resolved through the same address library the plugin runs on, so a dump whose bytes
    // at these RVAs are not the expected prologues is a dump that must not be trusted.
    std::uintptr_t const base = layout.base;
    std::uintptr_t const update_graph_manager = REL::Relocation<std::uintptr_t>{ RELOCATION_ID(32155, 32899) }.address();
    std::uintptr_t const graph_variable_bool = REL::Relocation<std::uintptr_t>{ RELOCATION_ID(62696, 63613) }.address();
    std::uintptr_t const vtable_behavior = REL::Relocation<std::uintptr_t>{ RE::VTABLE_hkbBehaviorGraph[0] }.address();
    std::uintptr_t const vtable_anim_graph = REL::Relocation<std::uintptr_t>{ RE::VTABLE_BShkbAnimationGraph[0] }.address();
    std::uintptr_t const vtable_character = REL::Relocation<std::uintptr_t>{ RE::VTABLE_hkbCharacter[0] }.address();

    std::ofstream anchor(anchor_path, std::ios::trunc);
    if (!anchor)
    {
        logger::warn("SCOPY ENGINE text dump sidecar not written path='{}'", anchor_path.string());
        return;
    }

    anchor << "CharacterPanel engine image dump - read it with tools/re/engine_scan.py\n";
    anchor << fmt::format("build={}\n", Plugin::Plugin_Build_Identity);
    anchor << fmt::format("runtime={} module-base=0x{:x} image-size=0x{:x} entry-rva=0x{:x}\n",
        REL::Module::get().version().string(), base, layout.image_size, layout.entry);
    anchor << "notes: the engine ships Steam-wrapped; the on-disk .text is encrypted (8.000 bits/byte\n";
    anchor << "       entropy, no CC padding, entry point in a .bind stub), so this file is the only\n";
    anchor << "       readable copy. File offset 0 is RVA text-rva; add module-base for a runtime address.\n";

    anchor << "\nsections\n";
    for (Section const& section : layout.sections)
    {
        anchor << fmt::format("  {} rva=0x{:x} virtual-size=0x{:x} raw-size=0x{:x}\n",
            section.name, section.rva, section.virtual_size, section.raw_size);
    }

    anchor << fmt::format("\ndump file={} text-rva=0x{:x} text-size=0x{:x} bytes=0x{:x} fnv1a=0x{:016x} cached={}\n",
        Text_File, layout.text_rva, layout.text_size, layout.text_size, hash, cached ? 1 : 0);

    anchor << "\nanchors\n";
    anchor << fmt::format("  IAnimationGraphManagerHolder::UpdateAnimationGraphManager id=32899 rva={}\n",
        rva_text(base, update_graph_manager));
    anchor << fmt::format("  BShkbAnimationGraph::GetGraphVariableBool id=63613 rva={}\n",
        rva_text(base, graph_variable_bool));
    anchor << fmt::format("  crashlog-hint animation-graph-function id=63568 rva=0x{:x} (see notes)\n", Hint_Anim_Graph);
    anchor << fmt::format("  crashlog-hint animation-manager-function id=63358 rva=0x{:x} (see notes)\n", Hint_Anim_Manager);

    anchor << "\n";
    write_vtable(anchor, "hkbBehaviorGraph", 220287, vtable_behavior, base);
    write_vtable(anchor, "BShkbAnimationGraph", 232870, vtable_anim_graph, base);
    write_vtable(anchor, "hkbCharacter", 220345, vtable_character, base);
    anchor.close();

    logger::info("SCOPY ENGINE text module='{}' base=0x{:x} text-rva=0x{:x} text-size=0x{:x} fnv1a=0x{:016x} cached={} path='{}'",
        "SkyrimSE.exe", base, layout.text_rva, layout.text_size, hash, cached ? 1 : 0, text_path.string());
    logger::info("SCOPY ENGINE anchors path='{}' update-graph-manager={} graph-variable-bool={} vtables=hkbBehaviorGraph:BShkbAnimationGraph:hkbCharacter",
        anchor_path.string(), rva_text(base, update_graph_manager), rva_text(base, graph_variable_bool));
}

#else

void EngineImage::dump_once()
{
}

#endif

PLUGIN_NAMESPACE_END
