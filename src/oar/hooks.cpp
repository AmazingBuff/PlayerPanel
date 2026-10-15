#include "oar/hooks.h"

#include "oar/api/OpenAnimationReplacer-ConditionTypes.h"
#include "oar/binary_profile.h"
#include "oar/policy.h"

#include <Windows.h>
#include <detours/detours.h>

#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>

PLUGIN_NAMESPACE_BEGIN
namespace oar
{
    namespace
    {
        using Condition = ::Conditions::ICondition;
        using ComponentType = ::Conditions::ConditionComponentType;
        using Evaluate = bool (*)(Condition const*, RE::TESObjectREFR*, RE::hkbClipGenerator*, void*);
        using NumericValue = float (*)(void const*, RE::TESObjectREFR*);
        Evaluate s_evaluate = nullptr;
        NumericValue s_numeric = nullptr;
        std::atomic_bool s_ready = false;
        // The standing-window state (see hooks.h): armed on the inventory-open edge, ticked down
        // once per rendered frame. Both thunks read it on whatever thread OAR evaluates on.
        std::atomic<std::uint32_t> s_standing_frames = 0;
        std::atomic<WeaponForm> s_standing_form{ WeaponForm::kNone };
        std::atomic<RE::hkbClipGenerator const*> s_selected = nullptr;
        // The active-query chain (see binary_profile.h): resolved only when the build's query
        // entries carry the bytes the profile recorded, so an unverified build never runs one.
        using GetSingleton = void* (*)();
        using ForEachProjectData = void (*)(void const*, std::function<void(RE::hkbCharacterStringData*, void*)> const&);
        using GetReplacements = void* (*)(void const*, std::uint16_t);
        using EvaluateReplacements = void* (*)(void const*, RE::TESObjectREFR*, RE::hkbClipGenerator*);
        GetSingleton s_get_singleton = nullptr;
        ForEachProjectData s_for_each_project_data = nullptr;
        GetReplacements s_get_replacements = nullptr;
        EvaluateReplacements s_evaluate_replacements = nullptr;
        // Diagnostics only: how many evaluations the window actually saw, so the log can say
        // whether the open edge produced an OAR decision at all.
        std::atomic<std::uint32_t> s_window_hits = 0;

        struct Frame
        {
            Frame(RE::TESObjectREFR* source, RE::hkbClipGenerator* clip)
                : m_source(source), m_clip(clip), m_previous(std::exchange(current, this)) {}
            ~Frame() { current = m_previous; }
            Frame(Frame const&) = delete;
            Frame& operator=(Frame const&) = delete;
            inline static thread_local Frame* current = nullptr;
            RE::TESObjectREFR* m_source;
            RE::hkbClipGenerator* m_clip;
            Frame* m_previous;
        };

        template <class T>
        T read_at(void const* object, std::size_t offset)
        {
            T value;
            std::memcpy(&value, static_cast<std::uint8_t const*>(object) + offset, sizeof(value));
            return value;
        }

        std::string_view condition_name(Condition const& condition, RE::BSString& storage)
        {
            storage = condition.GetName();
            return storage.c_str() ? storage.c_str() : "";
        }

        template <class T>
        T* component(Condition const& condition)
        {
            for (std::uint32_t i = 0; i < condition.GetNumComponents(); ++i)
            {
                auto* const item = condition.GetComponent(i);
                if (item && item->GetType() == T::CONDITION_COMPONENT_TYPE)
                    return static_cast<T*>(item);
            }
            return nullptr;
        }

        std::optional<bool> compare(Condition const& condition, float value, RE::TESObjectREFR* source,
            ScopedEvaluation& scope)
        {
            const auto* const comparison = component<::Conditions::IComparisonConditionComponent>(condition);
            const auto* const numeric = component<::Conditions::INumericConditionComponent>(condition);
            if (!comparison || !numeric)
            {
                scope.reject("condition component layout mismatch");
                return std::nullopt;
            }
            return comparison->GetComparisonResult(value, numeric->GetNumericValue(source));
        }

        bool evaluate_thunk(Condition const* condition, RE::TESObjectREFR* source,
            RE::hkbClipGenerator* clip, void* submod)
        {
            ScopedEvaluation* const scope = ScopedEvaluation::current();
            // The standing-window pre-condition (state-idle v3): armed on the INVENTORY-OPEN edge,
            // while the world is still advancing and OAR is evaluating for the player. Every pose
            // condition OAR asks in that window is answered for a STANDING figure in the player's
            // weapon stance, so the replacement OAR decides on is that stance's idle - played in
            // the panel whether or not this session ever ran it. The generator under evaluation is
            // filed as the selection; the window's own expiry (or a close) drops the flag and the
            // engine's original path answers again.
            if (!scope && standing_window_open() && source && source->IsPlayerRef())
            {
                s_selected.store(clip, std::memory_order_release);
                if (s_window_hits.fetch_add(1, std::memory_order_relaxed) == 0)
                    logger::info("SCOPY OAR window hit clip=0x{:x} form={}", reinterpret_cast<std::uintptr_t>(clip),
                        weapon_form_name(standing_weapon_form()));
                RE::BSString window_storage;
                const std::string_view name = condition_name(*condition, window_storage);
                PreviewState const standing{
                    .weapon_drawn = weapon_form_is_drawn(standing_weapon_form())
                };
                if (std::optional<bool> const answer = standing_predicate(name, standing))
                {
                    return finish_predicate(*answer, condition->IsDisabled(), condition->IsNegated());
                }
                if (name == "IsEquippedType")
                {
                    if (const auto type = weapon_form_type(standing_weapon_form()))
                    {
                        const auto* const comparison = component<::Conditions::IComparisonConditionComponent>(*condition);
                        const auto* const numeric = component<::Conditions::INumericConditionComponent>(*condition);
                        if (comparison && numeric)
                            return finish_predicate(comparison->GetComparisonResult(*type, *type),
                                condition->IsDisabled(), condition->IsNegated());
                    }
                }
                return s_evaluate(condition, source, clip, submod);
            }
            if (!scope)
                return s_evaluate(condition, source, clip, submod);
            Frame frame(source, clip);
            if (!scope->matches(source, clip))
                return s_evaluate(condition, source, clip, submod);
            scope->note_evaluated();
            // A projection wraps OAR's own decision: a condition this module does not model must
            // keep the engine's answer instead of vetoing a replacement OAR would have accepted.
            const bool projected = scope->mode() == ScopeMode::kProjected;
            if (condition->IsDisabled())
                return true;

            RE::BSString storage;
            const std::string_view name = condition_name(*condition, storage);
            std::optional<bool> value = standing_predicate(name, scope->state());
            if (!value)
            {
                if (const auto number = standing_value(name))
                {
                    value = compare(*condition, *number, source, *scope);
                }
                else if (name == "IsMovementDirection" || name == "IsCombatState")
                {
                    const auto* const numeric = component<::Conditions::INumericConditionComponent>(*condition);
                    if (!numeric)
                    {
                        scope->reject("state/direction component layout mismatch");
                    }
                    else
                    {
                        const float requested = numeric->GetNumericValue(source);
                        value = name == "IsCombatState" ? std::trunc(requested) == 0.0f : requested == 0.0f;
                    }
                }
                else if (name == "HasGraphVariable")
                {
                    const auto* const text = component<::Conditions::ITextConditionComponent>(*condition);
                    if (!text)
                    {
                        scope->reject("graph-variable component layout mismatch");
                    }
                    else
                    {
                        const RE::BSString variable = text->GetTextValue();
                        const auto found = scope->state().graph_variables.find(variable.c_str() ? variable.c_str() : "");
                        // HasGraphVariable tests existence, not the variable's numeric truth/value. An
                        // audited caller supplies the preview's complete table; a projection does not.
                        if (found != scope->state().graph_variables.end())
                            value = true;
                        else if (!projected)
                            value = false;
                    }
                }
            }
            if (value)
            {
                const bool projected_value = finish_predicate(*value, false, condition->IsNegated());
                // The engine's own answer for the SAME evaluation is the only direct measure of
                // "this projection changed an input OAR decides on"; asking it costs one call.
                const bool differs = projected && s_evaluate(condition, source, clip, submod) != projected_value;
                scope->note_override(name, differs);
                return projected_value;
            }
            if (projected || forwards_source(name))
                return s_evaluate(condition, source, clip, submod);
            scope->reject(fmt::format("unsupported preview condition: {}", name));
            return false;
        }

        float numeric_thunk(void const* numeric, RE::TESObjectREFR* source)
        {
            ScopedEvaluation* const scope = ScopedEvaluation::current();
            const Frame* const frame = Frame::current;
            if (!scope || !frame || source != frame->m_source || !scope->matches(frame->m_source, frame->m_clip) ||
                read_at<std::int32_t>(numeric, Numeric_Type_Offset) != 3)
                return s_numeric(numeric, source);

            // MSVC 14.44 std::string layout and both offsets verified in the Nexus DLL/PDB.
            static_assert(sizeof(std::string) == 0x20);
            const auto& name = *reinterpret_cast<std::string const*>(
                static_cast<std::uint8_t const*>(numeric) + Graph_Name_Offset);
            const auto found = scope->state().graph_variables.find(name);
            if (found == scope->state().graph_variables.end())
            {
                // A projection supplies only the variables it overrides; everything else stays the
                // engine's value (an audited query is answered against its complete preview table).
                if (scope->mode() == ScopeMode::kProjected)
                    return s_numeric(numeric, source);
                scope->reject(fmt::format("unmapped numeric graph variable: {}", name));
                return 0.0f;
            }
            const float value = found->second;
            // Same differential as the predicate path: the engine's own read of this variable is
            // what OAR would have decided on without the projection.
            const bool differs = scope->mode() == ScopeMode::kProjected && s_numeric(numeric, source) != value;
            scope->note_override(name, differs);
            switch (read_at<std::int32_t>(numeric, Graph_Type_Offset))
            {
            case 0: return value;
            case 1:
                if (static_cast<double>(value) >= std::numeric_limits<std::int32_t>::min() &&
                    static_cast<double>(value) <= std::numeric_limits<std::int32_t>::max())
                    return static_cast<float>(static_cast<std::int32_t>(value));
                break;
            case 2: return value != 0.0f ? 1.0f : 0.0f;
            default: break;
            }
            scope->reject("invalid numeric graph variable type/value");
            return 0.0f;
        }

        bool audit(Condition const& condition, ScopedEvaluation& scope, unsigned depth = 0)
        {
            if (depth > 64)
            {
                scope.reject("preview condition nesting exceeds 64");
                return false;
            }
            if (condition.IsDisabled())
                return true;
            const RE::BSString provider = condition.GetRequiredPluginName();
            if (provider.c_str() && *provider.c_str())
            {
                scope.reject("third-party condition requires an explicit preview adapter");
                return false;
            }
            RE::BSString storage;
            const auto name = condition_name(condition, storage);
            if (!standing_predicate(name, scope.state()) && !standing_value(name) &&
                name != "HasGraphVariable" && name != "IsMovementDirection" && name != "IsCombatState" && !forwards_source(name))
            {
                scope.reject(fmt::format("unsupported preview condition: {}", name));
                return false;
            }
            for (std::uint32_t i = 0; i < condition.GetNumComponents(); ++i)
            {
                auto* const item = condition.GetComponent(i);
                if (item && item->GetType() == ComponentType::kMulti)
                {
                    auto* const multi = static_cast<::Conditions::IMultiConditionComponent*>(item);
                    multi->ForEachCondition([&](std::unique_ptr<Condition>& child)
                    {
                        if (!child)
                            scope.reject("null condition in preview tree");
                        return child && audit(*child, scope, depth + 1)
                            ? RE::BSVisit::BSVisitControl::kContinue : RE::BSVisit::BSVisitControl::kStop;
                    });
                    if (!scope.supported())
                        return false;
                }
                else if (item && (item->GetType() == ComponentType::kCustom || item->GetType() == ComponentType::kPreset))
                {
                    scope.reject("custom/preset component requires an explicit preview adapter");
                    return false;
                }
            }
            return true;
        }

        bool verify_image(HMODULE module, BinaryProfile const& profile)
        {
            auto const* const base = reinterpret_cast<std::uint8_t const*>(module);
            auto const* const dos = reinterpret_cast<IMAGE_DOS_HEADER const*>(base);
            if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || dos->e_lfanew > 0x1000)
                return false;
            auto const* const nt = reinterpret_cast<IMAGE_NT_HEADERS64 const*>(base + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
                nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                nt->FileHeader.TimeDateStamp != profile.timestamp ||
                nt->OptionalHeader.SizeOfImage != profile.image_size)
                return false;
            const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
            if (directory.Size < sizeof(IMAGE_DEBUG_DIRECTORY) || directory.Size > 4096 ||
                directory.VirtualAddress > profile.image_size - directory.Size)
                return false;
            auto const* const entries = reinterpret_cast<IMAGE_DEBUG_DIRECTORY const*>(base + directory.VirtualAddress);
            for (std::size_t i = 0; i < directory.Size / sizeof(IMAGE_DEBUG_DIRECTORY); ++i)
            {
                auto const& entry = entries[i];
                if (entry.Type != IMAGE_DEBUG_TYPE_CODEVIEW || entry.SizeOfData < 24 ||
                    entry.SizeOfData > profile.image_size ||
                    entry.AddressOfRawData > profile.image_size - entry.SizeOfData)
                    continue;
                auto const* const record = base + entry.AddressOfRawData;
                if (std::memcmp(record, "RSDS", 4) == 0 && matches(profile, nt->FileHeader.TimeDateStamp,
                    nt->OptionalHeader.SizeOfImage, { record + 4, 16 }, read_at<std::uint32_t>(record, 20),
                    { base + profile.evaluate_rva, profile.evaluate_prefix.size() },
                    { base + profile.numeric_rva, profile.numeric_prefix.size() }))
                    return true;
            }
            return false;
        }
    }

    void arm_standing_window(std::uint32_t a_frames)
    {
        s_standing_frames.store(a_frames, std::memory_order_release);
        clear_selected_clip();
    }

    void tick_standing_window()
    {
        std::uint32_t left = s_standing_frames.load(std::memory_order_acquire);
        while (left != 0 && !s_standing_frames.compare_exchange_weak(left, left - 1,
            std::memory_order_acq_rel, std::memory_order_acquire))
        {
        }
        if (left == 1)
        {
            // The window just closed: report what it saw, so a fallback in the panel's capture can
            // be told apart from a window that never ran.
            logger::info("SCOPY OAR window closed hits={} selected={}", s_window_hits.load(std::memory_order_relaxed),
                s_selected.load(std::memory_order_acquire) != nullptr ? 1 : 0);
            s_window_hits.store(0, std::memory_order_relaxed);
        }
    }

    bool standing_window_open()
    {
        return s_standing_frames.load(std::memory_order_acquire) != 0;
    }

    void set_standing_weapon_form(WeaponForm a_form)
    {
        s_standing_form.store(a_form, std::memory_order_release);
    }

    WeaponForm standing_weapon_form()
    {
        return s_standing_form.load(std::memory_order_acquire);
    }

    SelectedClip selected_clip()
    {
        return SelectedClip{ s_selected.load(std::memory_order_acquire) };
    }

    void clear_selected_clip()
    {
        s_selected.store(nullptr, std::memory_order_release);
    }

    bool query_supported()
    {
        return s_get_singleton && s_for_each_project_data && s_get_replacements && s_evaluate_replacements;
    }

    void* query_replacement(RE::TESObjectREFR& a_source, RE::hkbClipGenerator& a_clip, std::uint16_t a_base_index,
        std::uint32_t& a_candidates, std::uint32_t& a_with_replacements)
    {
        a_candidates = 0;
        a_with_replacements = 0;
        if (!query_supported())
            return nullptr;
        void* const singleton = s_get_singleton();
        if (!singleton)
            return nullptr;
        void* found = nullptr;
        // The project datas are walked until one of them produces a replacement for the base index
        // whose conditions hold; the conditions run through the hooks above, so the standing
        // answers are in force for the whole decision.
        std::function<void(RE::hkbCharacterStringData*, void*)> visit =
            [&](RE::hkbCharacterStringData*, void* a_project_data)
            {
                if (found || !a_project_data)
                    return;
                ++a_candidates;
                void* const replacements = s_get_replacements(a_project_data, a_base_index);
                if (!replacements)
                    return;
                ++a_with_replacements;
                found = s_evaluate_replacements(replacements, std::addressof(a_source), std::addressof(a_clip));
            };
        s_for_each_project_data(singleton, visit);
        return found;
    }

    bool install_hooks()
    {
        if (hooks_ready())
            return true;
        HMODULE const module = GetModuleHandleW(L"OpenAnimationReplacer.dll");
        const SKSE::PluginInfo* const info = SKSE::GetPluginInfo("OpenAnimationReplacer");
        if (!module || !info)
        {
            logger::info("OAR hooks unavailable: Open Animation Replacer is not loaded");
            return false;
        }
        BinaryProfile const* const profile = [&]() -> BinaryProfile const*
        {
            for (auto const& candidate : Profiles)
            {
                if (info->version == REL::Version(candidate.version).pack() && verify_image(module, candidate))
                    return &candidate;
            }
            return nullptr;
        }();
        if (!profile)
        {
            logger::error("OAR hooks refused: no verified profile matches this DLL/PDB identity, entry bytes "
                          "and plugin version {}.{}.{}.{}",
                (info->version >> 24) & 0xFF, (info->version >> 16) & 0xFF,
                (info->version >> 4) & 0xFFF, info->version & 0xF);
            return false;
        }
        const auto base = reinterpret_cast<std::uintptr_t>(module);
        s_evaluate = reinterpret_cast<Evaluate>(base + profile->evaluate_rva);
        s_numeric = reinterpret_cast<NumericValue>(base + profile->numeric_rva);
        LONG result = DetourTransactionBegin();
        if (result != NO_ERROR)
        {
            s_evaluate = nullptr;
            s_numeric = nullptr;
            logger::error("OAR hook transaction begin failed ({})", result);
            return false;
        }
        result = DetourUpdateThread(GetCurrentThread());
        if (result == NO_ERROR)
            result = DetourAttach(reinterpret_cast<PVOID*>(&s_evaluate), reinterpret_cast<PVOID>(&evaluate_thunk));
        if (result == NO_ERROR)
            result = DetourAttach(reinterpret_cast<PVOID*>(&s_numeric), reinterpret_cast<PVOID>(&numeric_thunk));
        if (result != NO_ERROR)
            DetourTransactionAbort();
        else
            result = DetourTransactionCommit();
        if (result != NO_ERROR)
        {
            s_evaluate = nullptr;
            s_numeric = nullptr;
            logger::error("OAR hook transaction failed ({})", result);
            return false;
        }
        s_ready.store(true, std::memory_order_release);
        // The active-query chain: an entry that is zero, or whose bytes do not match what the
        // profile recorded, leaves the query unarmed - the two condition hooks keep working.
        QueryProfile const& query = profile->query;
        const auto image = std::span<std::uint8_t const>(reinterpret_cast<std::uint8_t const*>(module), profile->image_size);
        const bool complete = std::ranges::all_of(query.entries, [](QueryEntry const& a_entry) { return a_entry.rva != 0; });
        const std::array<std::uint8_t, 16> unused_guid{};
        if (complete && matches_query(query, 0, 0, unused_guid, 0, image))
        {
            s_get_singleton = reinterpret_cast<GetSingleton>(base + query.entries[0].rva);
            s_for_each_project_data = reinterpret_cast<ForEachProjectData>(base + query.entries[1].rva);
            s_get_replacements = reinterpret_cast<GetReplacements>(base + query.entries[2].rva);
            s_evaluate_replacements = reinterpret_cast<EvaluateReplacements>(base + query.entries[3].rva);
        }
        logger::info("OAR {}.{}.{} hooks installed: ConditionBase::Evaluate + NumericValue::GetValue; preview scopes only; active query {}",
            profile->version[0], profile->version[1], profile->version[2],
            query_supported() ? "available" : "unavailable for this build");
        return true;
    }

    bool hooks_ready() { return s_ready.load(std::memory_order_acquire); }

    EvaluationResult evaluate_condition(Condition const& condition, RE::TESObjectREFR& source,
        RE::hkbClipGenerator& preview_clip, PreviewState const& state, void* parent_submod)
    {
        if (!hooks_ready())
            return { false, false, 0, "verified OAR hooks are not installed" };
        for (auto const& [name, value] : state.graph_variables)
        {
            (void)name;
            if (!std::isfinite(value))
                return { false, false, 0, "preview graph values must be finite" };
        }
        ScopedEvaluation scope(&source, &preview_clip, state);
        const bool matched = audit(condition, scope) && condition.Evaluate(&source, &preview_clip, parent_submod);
        return { matched && scope.supported(), scope.supported(), scope.overrides(), scope.diagnostic() };
    }
}
PLUGIN_NAMESPACE_END
