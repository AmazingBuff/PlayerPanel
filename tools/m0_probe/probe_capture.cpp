//
// Created by AmazingBuff on 2026/09/28.
//

#include "probe.h"

#include <RE/Skyrim.h>
#include <REL/Module.h>
#include <SKSE/SKSE.h>
#include <SKSE/Version.h>
#include <Windows.h>
#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "probe_config.h"

using namespace std::literals;
namespace logger = SKSE::log;

namespace CharacterPanelProbe
{
    namespace
    {
        constexpr std::size_t Routine_Byte_Cap = 64 * 1024;
        constexpr std::size_t Capture_Byte_Cap = 1024 * 1024;
        constexpr std::size_t Leaf_Byte_Cap = 256;

        struct RoutineSpec
        {
            std::string_view name;
            REL::RelocationID id;
        };

        constexpr RoutineSpec Routines[] = {
            { "BSCullingProcess_CullingContext_ctor", REL::RelocationID(100211, 106919) },
            { "BSCullingProcess_Process", REL::RelocationID(100213, 106921) },
            { "BSShaderAccumulator_ctor", REL::RelocationID(99920, 106564) },
            { "BSShaderAccumulator_GetCurrentAccumulator", REL::RelocationID(98997, 105651) },
            { "BSShaderAccumulator_SetCurrentAccumulator", REL::RelocationID(98998, 105652) },
            { "Renderer_SubmitAccumulator", REL::RelocationID(99789, 106436) },
            { "Renderer_StartAccumulating", REL::RelocationID(99790, 106437) },
            { "Renderer_FinishAccumulatingPostResolveDepth", REL::RelocationID(99791, 106438) },
            { "Inventory3DManager_Begin3D", REL::RelocationID(50881, 51754) },
            { "Inventory3DManager_Render", REL::RelocationID(50882, 51755) },
            { "Inventory3DManager_End3D", REL::RelocationID(50883, 51756) },
            { "UI3DSceneManager_AttachChild", REL::RelocationID(51859, 52731) },
            { "UI3DSceneManager_DetachChild", REL::RelocationID(51861, 52733) },
            { "NiObject_Clone", REL::RelocationID(68835, 70187) },
            { "MenuManager_DrawInterfaceStart", REL::RelocationID(79947, 82084) }
        };

        struct DumpContext
        {
            std::filesystem::path directory;
            std::ofstream& manifest;
            std::size_t total_bytes{ 0 };
            bool has_error{ false };
            bool has_partial{ false };
        };

        struct CodeBounds
        {
            std::uintptr_t begin{ 0 };
            std::uintptr_t end{ 0 };
            bool has_unwind{ false };
            bool partial{ false };
            std::string error;
        };

        struct ExecutableModule
        {
            HMODULE handle{ nullptr };
            std::uintptr_t base{ 0 };
            std::uintptr_t text_begin{ 0 };
            std::uintptr_t text_end{ 0 };
            std::string name;
            bool code_allowed{ false };
        };

        struct VirtualCodeTarget
        {
            std::string name;
            std::uintptr_t address{ 0 };
        };

        [[nodiscard]] bool is_readable(std::uintptr_t address, std::size_t size);

        [[nodiscard]] bool read_process_memory(std::uintptr_t address, void* destination, std::size_t size)
        {
            if (size == 0 || address > (std::numeric_limits<std::uintptr_t>::max)() - size ||
                !is_readable(address, size))
                return false;
            SIZE_T bytes_read = 0;
            return ReadProcessMemory(
                       GetCurrentProcess(), reinterpret_cast<void const*>(address), destination, size, &bytes_read) != 0 &&
                bytes_read == size;
        }

        [[nodiscard]] bool is_executable_readable(std::uintptr_t address, std::size_t size)
        {
            if (size == 0 || address > (std::numeric_limits<std::uintptr_t>::max)() - size)
                return false;
            std::uintptr_t cursor = address;
            std::uintptr_t const end = address + size;
            while (cursor < end)
            {
                MEMORY_BASIC_INFORMATION info{};
                if (VirtualQuery(reinterpret_cast<void const*>(cursor), &info, sizeof(info)) == 0)
                    return false;
                if (info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD) != 0 ||
                    (info.Protect & PAGE_NOACCESS) != 0)
                    return false;
                DWORD const protection = info.Protect & 0xFF;
                bool const executable = protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE ||
                    protection == PAGE_EXECUTE_WRITECOPY;
                if (!executable)
                    return false;
                std::uintptr_t const region_end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
                if (region_end <= cursor)
                    return false;
                cursor = (std::min)(region_end, end);
            }
            return true;
        }

        [[nodiscard]] bool is_readable(std::uintptr_t address, std::size_t size)
        {
            if (size == 0 || address > (std::numeric_limits<std::uintptr_t>::max)() - size)
                return false;
            std::uintptr_t cursor = address;
            std::uintptr_t const end = address + size;
            while (cursor < end)
            {
                MEMORY_BASIC_INFORMATION info{};
                if (VirtualQuery(reinterpret_cast<void const*>(cursor), &info, sizeof(info)) == 0)
                    return false;
                if (info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD) != 0 ||
                    (info.Protect & PAGE_NOACCESS) != 0)
                    return false;
                DWORD const protection = info.Protect & 0xFF;
                bool const readable = protection == PAGE_READONLY || protection == PAGE_READWRITE ||
                    protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READ ||
                    protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
                if (!readable)
                    return false;
                std::uintptr_t const region_end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
                if (region_end <= cursor)
                    return false;
                cursor = (std::min)(region_end, end);
            }
            return true;
        }

        [[nodiscard]] CodeBounds resolve_bounds(
            std::uintptr_t address,
            std::uintptr_t text_begin,
            std::uintptr_t text_end,
            std::uintptr_t module_base)
        {
            CodeBounds result{ .begin = address, .end = address };
            if (address < text_begin || address >= text_end)
            {
                result.error = "address outside Skyrim executable .text";
                return result;
            }

            std::uintptr_t image_base = 0;
            PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(address, &image_base, nullptr);
            if (function && image_base == module_base)
            {
                result.begin = image_base + function->BeginAddress;
                result.end = image_base + function->EndAddress;
                result.has_unwind = true;
                if (result.begin < text_begin || result.end > text_end || result.begin >= result.end)
                {
                    result.error = "unwind bounds outside Skyrim executable .text";
                    return result;
                }
                return result;
            }

            result.end = (std::min)(address + Leaf_Byte_Cap, text_end);
            result.partial = true;
            if (result.end <= result.begin)
                result.error = "no readable leaf/thunk range";
            return result;
        }

        [[nodiscard]] bool find_executable_module(std::uintptr_t address, ExecutableModule& result)
        {
            HMODULE handle = nullptr;
            if (!GetModuleHandleExW(
                    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(address), &handle))
                return false;

            std::uintptr_t const base = reinterpret_cast<std::uintptr_t>(handle);
            IMAGE_DOS_HEADER dos{};
            if (!read_process_memory(base, &dos, sizeof(dos)))
                return false;
            if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0)
                return false;
            std::uintptr_t const dos_offset = static_cast<std::uintptr_t>(dos.e_lfanew);
            if (base > (std::numeric_limits<std::uintptr_t>::max)() - dos_offset)
                return false;
            std::uintptr_t const nt_address = base + dos_offset;
            IMAGE_NT_HEADERS64 nt{};
            if (!read_process_memory(nt_address, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE ||
                nt.OptionalHeader.SizeOfImage == 0 || nt.OptionalHeader.SizeOfImage > 0x40000000)
                return false;
            if (base > (std::numeric_limits<std::uintptr_t>::max)() - nt.OptionalHeader.SizeOfImage)
                return false;
            std::uintptr_t const image_end = base + nt.OptionalHeader.SizeOfImage;
            if (image_end < base || nt.FileHeader.NumberOfSections == 0 || nt.FileHeader.NumberOfSections > 96)
                return false;

            std::size_t const section_offset = sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + nt.FileHeader.SizeOfOptionalHeader;
            if (nt_address > (std::numeric_limits<std::uintptr_t>::max)() - section_offset)
                return false;
            std::uintptr_t const section_address = nt_address + section_offset;
            std::size_t const section_bytes = static_cast<std::size_t>(nt.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
            if (section_address < nt_address || section_address > image_end || section_bytes > image_end - section_address)
                return false;

            for (WORD index = 0; index < nt.FileHeader.NumberOfSections; ++index)
            {
                IMAGE_SECTION_HEADER header{};
                if (!read_process_memory(section_address + static_cast<std::size_t>(index) * sizeof(header), &header, sizeof(header)))
                    return false;
                if ((header.Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0)
                    continue;
                if (base > (std::numeric_limits<std::uintptr_t>::max)() - header.VirtualAddress)
                    return false;
                std::uintptr_t const begin = base + header.VirtualAddress;
                std::size_t const section_size = (std::max)(header.Misc.VirtualSize, header.SizeOfRawData);
                if (begin < base || begin > image_end || section_size > image_end - begin)
                    return false;
                std::uintptr_t const end = begin + section_size;
                if (address < begin || address >= end)
                    continue;

                wchar_t path[32768]{};
                DWORD const length = GetModuleFileNameW(handle, path, static_cast<DWORD>(std::size(path)));
                std::wstring const module_path(path, length);
                std::wstring module_name = std::filesystem::path(module_path).filename().wstring();
                std::transform(module_name.begin(), module_name.end(), module_name.begin(), towlower);
                result.handle = handle;
                result.base = base;
                result.text_begin = begin;
                result.text_end = end;
                result.name = std::filesystem::path(module_path).filename().string();
                result.code_allowed = module_name == L"skyrimse.exe" || module_name == L"communityshaders.dll" ||
                    module_name == L"characterpanelprobe.dll";
                return true;
            }
            return false;
        }

        void dump_code_address(DumpContext& context, std::string_view name, std::uintptr_t address)
        {
            ExecutableModule module{};
            if (!find_executable_module(address, module))
            {
                context.has_error = true;
                context.manifest << "code." << name << ".status=error\n";
                context.manifest << "code." << name << ".error=owning executable section unavailable\n";
                return;
            }

            context.manifest << "code." << name << ".module=" << module.name << "\n";
            context.manifest << "code." << name << ".module_base=0x" << std::hex << module.base << std::dec << "\n";
            context.manifest << "code." << name << ".entry=0x" << std::hex << address << std::dec << "\n";
            context.manifest << "code." << name << ".entry_relative=0x" << std::hex << (address - module.base) << std::dec << "\n";
            if (!module.code_allowed)
            {
                context.has_partial = true;
                context.manifest << "code." << name << ".status=metadata-only\n";
                return;
            }

            CodeBounds const bounds = resolve_bounds(address, module.text_begin, module.text_end, module.base);
            if (!bounds.error.empty())
            {
                context.has_error = true;
                context.manifest << "code." << name << ".status=error\n";
                context.manifest << "code." << name << ".error=" << bounds.error << "\n";
                return;
            }

            std::size_t const requested_size = static_cast<std::size_t>(bounds.end - bounds.begin);
            std::size_t const size = (std::min)(requested_size, Routine_Byte_Cap);
            bool const capped = size < requested_size;
            if (size == 0 || context.total_bytes + size > Capture_Byte_Cap ||
                !is_executable_readable(bounds.begin, size))
            {
                context.has_error = true;
                context.manifest << "code." << name << ".status=error\n";
                context.manifest << "code." << name << ".error=bounded executable read rejected\n";
                return;
            }

            std::vector<std::uint8_t> bytes(size);
            SIZE_T bytes_read = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void const*>(bounds.begin), bytes.data(), size,
                    &bytes_read) || bytes_read != size)
            {
                context.has_error = true;
                context.manifest << "code." << name << ".status=error\n";
                context.manifest << "code." << name << ".error=ReadProcessMemory failed\n";
                return;
            }

            std::filesystem::path const output = context.directory / fmt::format("{}.bin", name);
            std::ofstream file(output, std::ios::binary);
            file.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            file.flush();
            if (!file)
            {
                context.has_error = true;
                context.manifest << "code." << name << ".status=error\n";
                context.manifest << "code." << name << ".error=file write failed\n";
                return;
            }

            context.total_bytes += size;
            bool const partial = bounds.partial || capped;
            context.has_partial = context.has_partial || partial;
            context.manifest << "code." << name << ".status=" << (partial ? "partial" : "complete") << "\n";
            context.manifest << "code." << name << ".begin_relative=0x" << std::hex << (bounds.begin - module.base) << std::dec << "\n";
            context.manifest << "code." << name << ".natural_end_relative=0x" << std::hex << (bounds.end - module.base) << std::dec << "\n";
            context.manifest << "code." << name << ".actual_end_relative=0x" << std::hex
                             << (bounds.begin + size - module.base) << std::dec << "\n";
            context.manifest << "code." << name << ".begin=0x" << std::hex << bounds.begin << std::dec << "\n";
            context.manifest << "code." << name << ".end=0x" << std::hex << bounds.end << std::dec << "\n";
            context.manifest << "code." << name << ".bytes=" << size << "\n";
            context.manifest << "code." << name << ".unwind=" << (bounds.has_unwind ? "true" : "false") << "\n";
            context.manifest << "code." << name << ".partial=" << (partial ? "true" : "false") << "\n";
            context.manifest << "code." << name << ".file=" << output.filename().string() << "\n";
        }

        void dump_routine(DumpContext& context, RoutineSpec const& routine)
        {
            std::uintptr_t const address = routine.id.address();
            dump_code_address(context, routine.name, address);
        }

        [[nodiscard]] std::string pointer_string(void const* pointer)
        {
            return pointer ? fmt::format("0x{:X}", reinterpret_cast<std::uintptr_t>(pointer)) : "null"s;
        }

        void write_camera(std::ostream& output, RE::NiCamera* camera)
        {
            if (!camera)
            {
                output << "camera=null\n";
                return;
            }
            output << "camera=" << pointer_string(camera) << "\n";
            output << "camera.world.translate=" << camera->world.translate.x << ',' << camera->world.translate.y << ','
                   << camera->world.translate.z << "\n";
            RE::NiFrustum const& frustum = camera->GetRuntimeData2().viewFrustum;
            output << "camera.frustum=" << frustum.fLeft << ',' << frustum.fRight << ',' << frustum.fTop << ','
                   << frustum.fBottom << ',' << frustum.fNear << ',' << frustum.fFar << "\n";
        }

        void write_vtable_entries(
            std::ostringstream& output,
            std::vector<VirtualCodeTarget>& targets,
            bool& metadata_incomplete,
            char const* prefix,
            void* object,
            std::initializer_list<std::size_t> slots)
        {
            output << prefix << ".object=" << pointer_string(object) << "\n";
            if (!object)
            {
                metadata_incomplete = true;
                return;
            }

            std::uintptr_t vtable_address = 0;
            if (!read_process_memory(reinterpret_cast<std::uintptr_t>(object), &vtable_address, sizeof(vtable_address)) ||
                vtable_address == 0)
            {
                metadata_incomplete = true;
                return;
            }
            for (std::size_t slot : slots)
            {
                std::uintptr_t entry_address = 0;
                if (slot > ((std::numeric_limits<std::uintptr_t>::max)() - vtable_address) / sizeof(void*) ||
                    !read_process_memory(vtable_address + slot * sizeof(void*), &entry_address, sizeof(entry_address)))
                {
                    metadata_incomplete = true;
                    output << prefix << ".vtable[0x" << std::hex << slot << "]=unavailable\n" << std::dec;
                    continue;
                }
                void* entry = reinterpret_cast<void*>(entry_address);
                output << prefix << ".vtable[0x" << std::hex << slot << "]= " << pointer_string(entry) << std::dec << "\n";
                if (entry)
                    targets.push_back({ fmt::format("{}_vtable_{:02X}", prefix, slot), entry_address });
            }
        }

        void write_accumulator_metadata(
            std::ostringstream& output,
            std::vector<VirtualCodeTarget>& targets,
            bool& metadata_incomplete,
            char const* prefix,
            RE::BSShaderAccumulator* accumulator)
        {
            if (!accumulator)
            {
                metadata_incomplete = true;
                output << prefix << "=null\n";
                return;
            }

            auto const& data = accumulator->GetRuntimeData();
            output << prefix << ".camera=" << pointer_string(accumulator->camera) << "\n";
            output << prefix << ".render_mode=" << static_cast<std::uint32_t>(data.renderMode) << "\n";
            output << prefix << ".current_pass=" << data.currentPass << "\n";
            output << prefix << ".current_bucket=" << data.currentBucket << "\n";
            output << prefix << ".current_active=" << (data.currentActive ? "true" : "false") << "\n";
            output << prefix << ".eye_position=" << data.eyePosition.x << ',' << data.eyePosition.y << ','
                   << data.eyePosition.z << "\n";
            write_vtable_entries(output, targets, metadata_incomplete, prefix, accumulator, { 0x25, 0x26, 0x27, 0x2A, 0x2B });
        }

        void write_menu_metadata(
            std::ostringstream& output,
            std::vector<VirtualCodeTarget>& targets,
            bool& metadata_incomplete)
        {
            RE::UI* ui = RE::UI::GetSingleton();
            output << "ui=" << pointer_string(ui) << "\n";
            if (ui)
            {
                output << "ui.inventory_open=" << (ui->IsMenuOpen("InventoryMenu"sv) ? "true" : "false") << "\n";
                output << "ui.game_paused=" << (ui->GameIsPaused() ? "true" : "false") << "\n";
            }
            else
                metadata_incomplete = true;

            RE::BSGraphics::Renderer* renderer = RE::BSGraphics::Renderer::GetSingleton();
            output << "renderer=" << pointer_string(renderer) << "\n";
            if (renderer)
            {
                auto const& runtime = renderer->GetRuntimeData();
                output << "renderer.window0=" << runtime.renderWindows[0].windowWidth << 'x'
                       << runtime.renderWindows[0].windowHeight << "\n";
                output << "renderer.main_desc=unavailable_without_stable_texture_reference\n";
            }
            else
                metadata_incomplete = true;

            RE::UI3DSceneManager* manager = RE::UI3DSceneManager::GetSingleton();
            output << "ui3d=" << pointer_string(manager) << "\n";
            if (!manager)
            {
                metadata_incomplete = true;
                return;
            }

            {
                RE::BSSpinLockGuard guard(manager->lock);
                output << "ui3d.culling=" << pointer_string(manager->cullingProcess) << "\n";
                output << "ui3d.camera=" << pointer_string(manager->camera.get()) << "\n";
                output << "ui3d.primary_accumulator=" << pointer_string(manager->unk10.get()) << "\n";
                output << "ui3d.secondary_accumulator=" << pointer_string(manager->unk18.get()) << "\n";
                output << "ui3d.current_light_scheme=" << static_cast<std::uint32_t>(manager->currentlightScheme) << "\n";
                std::uint32_t menu_object_count = 0;
                for (auto const& menu_object : manager->menuObjects)
                {
                    if (menu_object)
                        ++menu_object_count;
                }
                output << "ui3d.menu_object_count=" << menu_object_count << "\n";
                output << "ui3d.menu_light_count=" << manager->menuLights.size() << "\n";

                write_camera(output, manager->camera.get());
                if (manager->cullingProcess)
                {
                    auto* culler = manager->cullingProcess;
                    output << "culler.cull_mode=" << static_cast<std::uint32_t>(culler->cullMode.get()) << "\n";
                    output << "culler.recurse_to_geometry=" << (culler->recurseToGeometry ? "true" : "false") << "\n";
                    output << "culler.is_grouping_alphas=" << (culler->isGroupingAlphas ? "true" : "false") << "\n";
                    output << "culler.camera=" << pointer_string(culler->camera) << "\n";
                    output << "culler.visible_set=" << pointer_string(culler->visibleSet) << "\n";
                    output << "culler.update_accumulate=" << (culler->updateAccumulateFlag ? "true" : "false") << "\n";
                    write_vtable_entries(output, targets, metadata_incomplete, "culler", culler, { 0x16, 0x17, 0x18 });
                }
                else
                    metadata_incomplete = true;
                write_accumulator_metadata(output, targets, metadata_incomplete, "primary_accumulator", manager->unk10.get());
                write_accumulator_metadata(output, targets, metadata_incomplete, "secondary_accumulator", manager->unk18.get());
            }
        }

        void write_module_metadata(std::ostream& output)
        {
            output << "build_identity=" << Build_Identity << "\n";
            output << "runtime=" << REL::Module::get().version().string() << "\n";
            output << "runtime_supported="
                   << (REL::Module::IsAE() && REL::Module::get().version() == SKSE::RUNTIME_SSE_1_6_1170 ? "true" : "false")
                   << "\n";
            output << "skyrim_module=" << std::filesystem::path(REL::Module::get().filePath()).string() << "\n";
            HMODULE community_shaders = GetModuleHandleW(L"CommunityShaders.dll");
            output << "community_shaders=" << pointer_string(community_shaders) << "\n";
            if (community_shaders)
            {
                wchar_t path[32768]{};
                DWORD const length = GetModuleFileNameW(community_shaders, path, static_cast<DWORD>(std::size(path)));
                output << "community_shaders_path=" << std::filesystem::path(std::wstring(path, length)).string() << "\n";
            }
        }
    }

    void Probe::capture(CaptureKind kind) noexcept
    {
        try
        {
            if (!supported_runtime())
            {
                logger::warn("Probe capture ignored because Skyrim AE 1.6.1170 is not active");
                return;
            }

            std::filesystem::path const log_directory = SKSE::log::log_directory().value_or(std::filesystem::path{});
            if (log_directory.empty())
            {
                logger::warn("Cannot capture probe evidence: log directory unavailable");
                return;
            }

            std::uint32_t index = m_capture_index;
            std::filesystem::path directory;
            while (true)
            {
                directory = log_directory / "CharacterPanelProbe" / fmt::format("capture-{:03}", index);
                if (!std::filesystem::exists(directory))
                    break;
                if (index == (std::numeric_limits<std::uint32_t>::max)())
                {
                    logger::warn("Cannot allocate a unique probe capture directory");
                    return;
                }
                ++index;
            }
            m_capture_index = index == (std::numeric_limits<std::uint32_t>::max)() ? index : index + 1;
            std::error_code ec;
            std::filesystem::create_directories(directory, ec);
            if (ec)
            {
                logger::warn("Cannot create capture directory {}: {}", directory.string(), ec.message());
                return;
            }

            std::ostringstream metadata;
            std::vector<VirtualCodeTarget> virtual_targets;
            bool metadata_incomplete = false;
            write_module_metadata(metadata);
            write_menu_metadata(metadata, virtual_targets, metadata_incomplete);

            std::ofstream manifest(directory / "manifest.txt");
            if (!manifest)
            {
                logger::warn("Cannot open probe manifest in {}", directory.string());
                return;
            }

            manifest << "method=CharacterPanelProbe bounded read-only evidence\n";
            manifest << "capture_index=" << index << "\n";
            manifest << "trigger=" << (kind == CaptureKind::kCodeAndMenu ? "F8" : "F9") << "\n";
            manifest << "code_per_routine_cap=" << Routine_Byte_Cap << "\n";
            manifest << "code_capture_cap=" << Capture_Byte_Cap << "\n";
            manifest << "leaf_cap=" << Leaf_Byte_Cap << "\n";
            manifest << "code_capture=" << (kind == CaptureKind::kCodeAndMenu ? "true" : "false") << "\n";
            manifest << "metadata_incomplete=" << (metadata_incomplete ? "true" : "false") << "\n";
            manifest << metadata.str();

            DumpContext context{ .directory = directory, .manifest = manifest };
            context.has_partial = metadata_incomplete;
            if (kind == CaptureKind::kCodeAndMenu)
            {
                for (RoutineSpec const& routine : Routines)
                    dump_routine(context, routine);
                for (VirtualCodeTarget const& target : virtual_targets)
                    dump_code_address(context, target.name, target.address);
            }
            manifest << "code_bytes_total=" << context.total_bytes << "\n";
            manifest << "capture_status=" << (context.has_error ? "error" : context.has_partial ? "partial" : "complete") << "\n";
            manifest << "complete=" << (!context.has_error && !context.has_partial ? "true" : "false") << "\n";
            manifest.flush();
            if (!manifest)
                logger::warn("Probe manifest write reported an error in {}", directory.string());
            manifest.close();
            logger::info("Probe capture {} written to {}", index, directory.string());
        }
        catch (std::exception const& error)
        {
            logger::error("Probe capture failed: {}", error.what());
        }
        catch (...)
        {
            logger::error("Probe capture failed with an unknown exception");
        }
    }
}
