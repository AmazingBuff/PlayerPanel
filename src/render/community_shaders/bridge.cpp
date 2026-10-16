#include "render/community_shaders/bridge.h"
#include "render/community_shaders/view.h"

#include <Windows.h>
#include <detours/detours.h>

PLUGIN_NAMESPACE_BEGIN
namespace community_shaders
{
    thread_local DrawOverride* current_view = nullptr;

    Bridge& bridge()
    {
        static Bridge instance;
        return instance;
    }

    namespace
    {
        bool verify_image(HMODULE module)
        {
            auto const* base = reinterpret_cast<std::uint8_t const*>(module);
            auto const* dos = reinterpret_cast<IMAGE_DOS_HEADER const*>(base);
            if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || dos->e_lfanew > 0x1000)
                return false;
            auto const* nt = reinterpret_cast<IMAGE_NT_HEADERS64 const*>(base + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
                nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                nt->FileHeader.TimeDateStamp != Timestamp || nt->OptionalHeader.SizeOfImage != Image_Size)
                return false;
            auto const& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
            if (directory.Size < sizeof(IMAGE_DEBUG_DIRECTORY) || directory.Size > 4096 ||
                directory.VirtualAddress > Image_Size - directory.Size)
                return false;
            auto const* entries = reinterpret_cast<IMAGE_DEBUG_DIRECTORY const*>(base + directory.VirtualAddress);
            bool identity = false;
            for (std::size_t i = 0; i < directory.Size / sizeof(IMAGE_DEBUG_DIRECTORY); ++i)
            {
                auto const& entry = entries[i];
                if (entry.Type != IMAGE_DEBUG_TYPE_CODEVIEW || entry.SizeOfData < 24 ||
                    entry.SizeOfData > Image_Size || entry.AddressOfRawData > Image_Size - entry.SizeOfData)
                    continue;
                auto const* record = base + entry.AddressOfRawData;
                std::uint32_t age{};
                std::memcpy(&age, record + 20, sizeof(age));
                identity |= std::memcmp(record, "RSDS", 4) == 0 && matches_identity(
                    nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage, { record + 4, 16 }, age);
            }
            return identity && matches_entries({ base, Image_Size });
        }

        void draw_thunk(void* state)
        {
            bridge().draw(state);
            if (current_view)
                current_view->after_draw(current_view->context);
        }
    }

    bool install()
    {
        auto& api = bridge();
        if (api.ready)
            return true;
        const HMODULE module = GetModuleHandleW(L"CommunityShaders.dll");
        if (!module)
        {
            logger::info("CP-CS not installed; native forward studio path");
            return false;
        }
        if (!verify_image(module))
        {
            logger::warn("CP-CS refused: expected official CS 1.9.1 DLL/PDB identity and verified entry bytes; native forward fallback");
            return false;
        }

        api.base = reinterpret_cast<std::uintptr_t>(module);
        api.draw = api.function<decltype(api.draw)>(Function::e_draw);
        LONG result = DetourTransactionBegin();
        if (result != NO_ERROR)
            return false;
        result = DetourUpdateThread(GetCurrentThread());
        if (result == NO_ERROR)
            result = DetourAttach(reinterpret_cast<PVOID*>(&api.draw), reinterpret_cast<PVOID>(&draw_thunk));
        if (result != NO_ERROR)
            DetourTransactionAbort();
        else
            result = DetourTransactionCommit();
        if (result != NO_ERROR)
        {
            api.draw = nullptr;
            logger::error("CP-CS draw hook transaction failed ({})", result);
            return false;
        }
        api.ready = true;
        logger::info("CP-CS 1.9.1 verified: isolated material view hook installed; outside preview original State::Draw only");
        return true;
    }
}
PLUGIN_NAMESPACE_END
