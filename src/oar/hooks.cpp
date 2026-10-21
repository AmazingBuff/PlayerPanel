#include "oar/hooks.h"

#include "base/def.h"

#include "oar/api/OpenAnimationReplacer-ConditionTypes.h"
#include "oar/binary_profile.h"
#include "oar/ied_conditions.h"
#include "oar/policy.h"
#include "oar/preview_clip.h"

#include "RE/H/hkbCharacterStringData.h"

#include <Windows.h>
#include <detours/detours.h>

#include <atomic>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <shared_mutex>
#include <vector>

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
        using GetSingleton = void* (*)();
        using ForEachProjectData = void (*)(void const*, std::function<void(RE::hkbCharacterStringData*, void*)> const&);
        using ForEachReplacement = void (*)(void const*, std::function<void(void*)> const&, bool);
        ForEachReplacement s_for_each_replacement = nullptr;
        using GetReplacements = void* (*)(void const*, std::uint16_t);
        using EvaluateReplacements = void* (*)(void const*, RE::TESObjectREFR*, RE::hkbClipGenerator*);
        using GetIndex = std::uint16_t (*)(void const*, void*&);
        using EvaluateSet = bool (*)(void const*, RE::TESObjectREFR*, RE::hkbClipGenerator*, void*, bool);
        using EvaluateReplacement = bool (*)(void const*, RE::TESObjectREFR*, RE::hkbClipGenerator*);
        using Parent = void const* (*)(void const*);
        using ReplaceOnLoop = bool (*)(void const*);
        GetSingleton s_get_singleton = nullptr;
        ForEachProjectData s_for_each_project_data = nullptr;
        GetReplacements s_get_replacements = nullptr;
        EvaluateReplacements s_evaluate_replacements = nullptr;
        GetIndex s_get_index = nullptr;
        EvaluateSet s_evaluate_set = nullptr;
        EvaluateReplacement s_evaluate_replacement = nullptr;
        Parent s_parent_submod = nullptr;
        Parent s_parent_mod = nullptr;
        ReplaceOnLoop s_replace_on_loop = nullptr;
        ReplaceOnLoop s_interruptible = nullptr;
        using BlendTime = float (*)(void const*, void*, std::int32_t, bool, bool);
        BlendTime s_blend_time = nullptr;
        std::atomic_bool s_query_ready = false;

        std::optional<uint32_t> ied_provider_version()
        {
            static const std::optional<uint32_t> s_version = []() -> std::optional<uint32_t>
            {
                HMODULE const module = GetModuleHandleW(L"OpenAnimationReplacer-IEDConditionExtensions.dll");
                SKSE::PluginInfo const* const info = SKSE::GetPluginInfo(Ied_Provider.data());
                if (!module || !info)
                    return std::nullopt;
                return info->version;
            }();
            return s_version;
        }

        struct QueryFrame
        {
            QueryFrame(ScopedEvaluation& evaluation, QuerySession& session, AnimationSelection& result,
                std::uint64_t identity, std::size_t names) : scope(evaluation), context(session), output(result), key(identity), name_count(names),
                previous(std::exchange(current, this)) {}
            ~QueryFrame() { current = previous; }
            inline static thread_local QueryFrame* current = nullptr;
            ScopedEvaluation& scope;
            QuerySession& context;
            AnimationSelection& output;
            std::uint64_t key;
            std::size_t name_count;
            void const* selected = nullptr;
            std::uint16_t index = 0xFFFF;
            QueryFrame* previous;

            void reject(std::string reason)
            {
                ++output.rejected;
                if (output.diagnostic.empty())
                    output.diagnostic = std::move(reason);
            }
        };

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

        bool readable(void const* object, std::size_t size)
        {
            if (!object || size > 16 * 1024 * 1024)
                return false;
            std::uintptr_t cursor = reinterpret_cast<std::uintptr_t>(object);
            if (cursor > std::numeric_limits<std::uintptr_t>::max() - size)
                return false;
            const std::uintptr_t end = cursor + size;
            while (cursor < end)
            {
                MEMORY_BASIC_INFORMATION info{};
                if (!VirtualQuery(reinterpret_cast<void const*>(cursor), &info, sizeof(info)) ||
                    info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
                    return false;
                const std::uintptr_t next = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
                if (next <= cursor)
                    return false;
                cursor = std::min(next, end);
            }
            return true;
        }

        template <class T>
        std::span<T const> native_vector(void const* object, std::size_t offset)
        {
            const std::uintptr_t first = read_at<std::uintptr_t>(object, offset);
            const std::uintptr_t last = read_at<std::uintptr_t>(object, offset + sizeof(void*));
            if (last < first || (last - first) % sizeof(T) != 0 || (last - first) / sizeof(T) > 65535)
                return {};
            if (first == last)
                return {};
            if (!readable(reinterpret_cast<void const*>(first), last - first))
                return {};
            return { reinterpret_cast<T const*>(first), (last - first) / sizeof(T) };
        }

        std::string asset_name(RE::hkStringPtr const& name)
        {
            std::string result;
            char const* text = name.c_str();
            std::size_t verified = 0;
            for (std::size_t i = 0; text && i < 1024; ++i)
            {
                if (i >= verified)
                {
                    if (readable(text + i, 64))
                        verified = i + 64;
                    else if (readable(text + i, 1))
                        verified = i + 1;
                    else
                        return {};
                }
                if (text[i] == '\0')
                    return result;
                result.push_back(text[i]);
            }
            return {};
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

        template <class T>
        T const* named_component(Condition const& condition, std::string_view wanted)
        {
            for (std::uint32_t i = 0; i < condition.GetNumComponents(); ++i)
            {
                auto const* const item = condition.GetComponent(i);
                if (!item || item->GetType() != T::CONDITION_COMPONENT_TYPE)
                    continue;
                const RE::BSString name = item->GetName();
                if (name.c_str() && wanted == name.c_str())
                    return static_cast<T const*>(item);
            }
            return nullptr;
        }

        std::optional<bool> random_condition(Condition const& condition, RE::TESObjectREFR* source,
            void* submod, ScopedEvaluation& scope)
        {
            QueryFrame* const query = QueryFrame::current;
            auto const* const state = component<::Conditions::IConditionStateComponent>(condition);
            auto const* const minimum = named_component<::Conditions::INumericConditionComponent>(condition, "Minimum random value");
            auto const* const maximum = named_component<::Conditions::INumericConditionComponent>(condition, "Maximum random value");
            auto const* const value = named_component<::Conditions::INumericConditionComponent>(condition, "Numeric value");
            auto const* const comparison = component<::Conditions::IComparisonConditionComponent>(condition);
            if (!query || !state || !minimum || !maximum || !value || !comparison)
            {
                scope.reject("random condition requires the verified component layout and caller query context");
                return false;
            }
            const auto data_scope = state->GetStateDataScope();
            if ((state->GetAllowedDataScopes() & data_scope) == ::Conditions::StateDataScope::kNone ||
                (data_scope != ::Conditions::StateDataScope::kLocal && data_scope != ::Conditions::StateDataScope::kSubMod))
            {
                scope.reject("random condition scope is not Local or Submod");
                return false;
            }
            const bool local = data_scope == ::Conditions::StateDataScope::kLocal;
            if (!local && !submod)
            {
                scope.reject("random condition submod unavailable");
                return false;
            }
            const StateKey key{ local ? DataScope::e_local : DataScope::e_submod,
                local ? query->key : reinterpret_cast<std::uint64_t>(submod) };
            const auto random = query->context.conditions.random(key, reinterpret_cast<std::uint64_t>(&condition),
                query->key, state->ShouldResetOnLoopOrEcho(), minimum->GetNumericValue(source), maximum->GetNumericValue(source));
            const float requested = value->GetNumericValue(source);
            if (!random || !std::isfinite(requested))
            {
                scope.reject("random condition has invalid numeric bounds/value");
                return false;
            }
            return comparison->GetComparisonResult(*random, requested);
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

        int equipped_type(RE::TESForm const* form)
        {
            if (!form)
                return 0;
            if (RE::TESObjectWEAP const* weapon = form->As<RE::TESObjectWEAP>())
            {
                const int type = static_cast<int>(weapon->GetWeaponType());
                if (type < 0 || type > 9)
                    return -1;
                return weapon->GetWeaponType() == RE::WEAPON_TYPE::kTwoHandAxe &&
                    weapon->HasKeywordString("WeapTypeWarhammer") ? 10 : type;
            }
            if (form->IsArmor())
                return 11;
            if (RE::SpellItem const* spell = form->As<RE::SpellItem>())
            {
                switch (spell->GetAssociatedSkill())
                {
                case RE::ActorValue::kAlteration: return 12;
                case RE::ActorValue::kIllusion: return 13;
                case RE::ActorValue::kDestruction: return 14;
                case RE::ActorValue::kConjuration: return 15;
                case RE::ActorValue::kRestoration: return 16;
                default: return -1;
                }
            }
            if (form->Is(RE::FormType::Scroll))
                return 17;
            return form->Is(RE::FormType::Light) ? 18 : -1;
        }

        std::optional<bool> equipment_condition(Condition const& condition, std::string_view name,
            RE::TESObjectREFR* source, ScopedEvaluation& scope)
        {
            if (!scope.state().hand_override)
                return std::nullopt;
            if (name == "IsEquippedHasEnchantment" || name == "IsEquippedHasEnchantmentWithKeyword" ||
                name == "HasBoundWeaponEquipped")
            {
                scope.reject("previous equipment instance state requires an explicit adapter");
                return false;
            }
            if (name != "IsEquipped" && name != "IsEquippedType" && name != "IsEquippedHasKeyword")
                return std::nullopt;
            const auto* hand = component<::Conditions::IBoolConditionComponent>(condition);
            if (!hand)
            {
                scope.reject("equipment hand component unavailable");
                return false;
            }
            const std::uint32_t id = (*scope.state().hand_override)[hand->GetBoolValue() ? 1 : 0];
            RE::TESForm const* const form = id ? RE::TESForm::LookupByID(id) : nullptr;
            if (id && !form)
            {
                scope.reject("previous equipment form no longer available");
                return false;
            }
            if (name == "IsEquipped")
            {
                const auto* expected = component<::Conditions::IFormConditionComponent>(condition);
                return expected && expected->IsValid() && expected->GetTESFormValue() == form;
            }
            if (name == "IsEquippedHasKeyword")
            {
                const auto* keyword = component<::Conditions::IKeywordConditionComponent>(condition);
                RE::BGSKeywordForm const* keywords = form ? form->As<RE::BGSKeywordForm>() : nullptr;
                return keyword && keywords && keyword->HasKeyword(keywords);
            }
            const auto* numeric = component<::Conditions::INumericConditionComponent>(condition);
            if (!numeric)
            {
                scope.reject("equipment type component unavailable");
                return false;
            }
            const float requested = numeric->GetNumericValue(source);
            if (!std::isfinite(requested) || requested < -128.0f || requested > 127.0f)
            {
                scope.reject("invalid equipment type value");
                return false;
            }
            return equipped_type(form) == static_cast<std::int8_t>(requested);
        }

        bool evaluate_thunk(Condition const* condition, RE::TESObjectREFR* source,
            RE::hkbClipGenerator* clip, void* submod)
        {
            ScopedEvaluation* const scope = ScopedEvaluation::current();
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

            const RE::BSString provider = condition->GetRequiredPluginName();
            if (!projected && provider.c_str() && *provider.c_str())
            {
                return evaluate_ied_condition(*condition, ied_provider_version(), scope->state().hand_override.has_value(),
                    [&] { return s_evaluate(condition, source, clip, submod); },
                    [&](std::string_view reason) { scope->reject(std::string(reason)); });
            }

            RE::BSString storage;
            const std::string_view name = condition_name(*condition, storage);
            if (name == "HasGraphVariable" && QueryFrame::current)
                return s_evaluate(condition, source, clip, submod);
            std::optional<bool> value = equipment_condition(*condition, name, source, *scope);
            if (name == "Random" && QueryFrame::current)
                value = random_condition(*condition, source, submod, *scope);
            if (!value)
                value = standing_predicate(name, scope->state());
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
                bool differs = false;
                if (projected)
                {
                    UnprojectedEvaluation original;
                    differs = s_evaluate(condition, source, clip, submod) != projected_value;
                }
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
            bool differs = false;
            if (scope->mode() == ScopeMode::kProjected)
            {
                UnprojectedEvaluation original;
                differs = s_numeric(numeric, source) != value;
            }
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
                const std::string_view reason = ied_condition_rejection(condition, ied_provider_version(),
                    scope.state().hand_override.has_value());
                if (!reason.empty())
                    scope.reject(std::string(reason));
                return reason.empty();
            }
            RE::BSString storage;
            const auto name = condition_name(condition, storage);
            if (!standing_predicate(name, scope.state()) && !standing_value(name) &&
                name != "HasGraphVariable" && name != "IsMovementDirection" && name != "IsCombatState" &&
                !(name == "Random" && QueryFrame::current) && !forwards_source(name))
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

        std::string native_string(void const* object, std::size_t offset)
        {
            auto const* value = reinterpret_cast<std::string const*>(static_cast<std::uint8_t const*>(object) + offset);
            if (!readable(value, sizeof(*value)) || value->size() > 4096 || !readable(value->data(), value->size()))
                return {};
            return *value;
        }

        std::optional<AnimationDescriptor> describe(void const* replacement, std::uint64_t key, std::size_t name_count)
        {
            if (!readable(replacement, 0xD0)) return std::nullopt;
            AnimationDescriptor descriptor;
            descriptor.path = native_string(replacement, Replacement_Path_Offset);
            descriptor.replacement_key = reinterpret_cast<std::uint64_t>(replacement);
            descriptor.reevaluate_on_loop = s_replace_on_loop(replacement);
            descriptor.interruptible = s_interruptible(replacement);
            // Null ActiveClip and checkWithinSequence=false are audited for both binaries.
            descriptor.interrupt_blend = s_blend_time(replacement, nullptr, 0, false, false);
            descriptor.loop_blend = s_blend_time(replacement, nullptr, 1, false, false);
            if (!std::isfinite(descriptor.interrupt_blend) || descriptor.interrupt_blend < 0.0f ||
                !std::isfinite(descriptor.loop_blend) || descriptor.loop_blend < 0.0f) return std::nullopt;
            const std::uint8_t tag = read_at<std::uint8_t>(replacement, Replacement_Tag_Offset);
            if (tag == 0)
            {
                void* unused_variant = nullptr;
                descriptor.variants.reset();

                descriptor.replacement_key = reinterpret_cast<std::uint64_t>(replacement);
                descriptor.reevaluate_on_loop = s_replace_on_loop(replacement);
                descriptor.index = s_get_index(replacement, unused_variant);
                if (descriptor.index >= name_count || descriptor.path.empty()) return std::nullopt;
                return descriptor;
            }
            if (tag != 1)
                return std::nullopt;
            static_assert(sizeof(std::shared_mutex) == 8);
            auto* mutex = reinterpret_cast<std::shared_mutex*>(
                const_cast<std::uint8_t*>(static_cast<std::uint8_t const*>(replacement)) + Variant_Lock_Offset);
            const std::shared_lock lock(*mutex);
            const std::uint8_t mode = read_at<std::uint8_t>(replacement, 0x78);
            if (mode > 1)
                return std::nullopt;
            const auto active = native_vector<void*>(replacement, 0x30);
            const auto sequence = native_vector<void*>(replacement, 0x48);
            const auto random = native_vector<void*>(replacement, 0x60);
            const auto cumulative = native_vector<float>(replacement, 0x80);
            if (active.empty())
                return std::nullopt;
            const auto entries = [&](std::span<void* const> variants)
            {
                std::vector<VariantEntry> result;
                for (void const* variant : variants)
                {
                    if (!readable(variant, 0x2A))
                    {
                        result.clear();
                        break;
                    }
                    const std::uint16_t index = read_at<std::uint16_t>(variant, 0);
                    const std::string filename = native_string(variant, Variant_Filename_Offset);
                    if (index >= name_count || filename.empty()) { result.clear(); break; }
                    result.push_back({ index, read_at<std::uint8_t>(variant, 0x29) != 0,
                        descriptor.path + "\\" + filename });
                }
                return result;
            };
            VariantOptions options;
            options.sequence = entries(mode == 1 ? active : sequence);
            const std::vector<VariantEntry> random_entries = entries(random);
            if (options.sequence.size() != (mode == 1 ? active.size() : sequence.size()) ||
                random_entries.size() != random.size())
                return std::nullopt;
            for (VariantEntry const& variant : random_entries)
            {
                options.random.push_back(variant.index);
                options.random_paths.push_back(variant.path);
            }
            options.cumulative.assign(cumulative.begin(), cumulative.end());
            options.sequential = mode == 1;
            options.blend_between = read_at<std::uint8_t>(replacement, 0x20) != 0;
            options.reset_on_loop = read_at<std::uint8_t>(replacement, 0x21) != 0;
            options.share_history = read_at<std::uint8_t>(replacement, 0x22) != 0;
            void const* const submod = s_parent_submod(replacement);
            void const* const mod = submod ? s_parent_mod(submod) : nullptr;
            const std::uint32_t data_scope = read_at<std::uint32_t>(replacement, 0x98);
            switch (data_scope)
            {
            case 1: options.key = { DataScope::e_local, key, reinterpret_cast<std::uint64_t>(mod) }; break;
            case 2: options.key = { DataScope::e_submod, reinterpret_cast<std::uint64_t>(submod) }; break;
            case 4: options.key = { DataScope::e_replacer_mod, reinterpret_cast<std::uint64_t>(mod) }; break;
            default: return std::nullopt;
            }
            if (!submod || !mod)
                return std::nullopt;
            if (!readable(active.front(), 2)) return std::nullopt;
            descriptor.index = read_at<std::uint16_t>(active.front(), 0);
            if (descriptor.index >= name_count || descriptor.path.empty()) return std::nullopt;
            descriptor.variants = std::move(options);
            return descriptor;
        }

        bool evaluate_set_thunk(void const* set, RE::TESObjectREFR* source, RE::hkbClipGenerator* clip,
            void* submod, bool trace)
        {
            QueryFrame* const query = QueryFrame::current;
            ScopedEvaluation* const outer = ScopedEvaluation::current();
            if (!query || !outer || !outer->matches(source, clip))
                return s_evaluate_set(set, source, clip, submod, trace);
            ScopedEvaluation candidate(source, clip, outer->state(), ScopeMode::kAudited);
            bool result = true;
            {
                // ConditionSet starts with MSVC shared_mutex, then vector<unique_ptr<ICondition>>.
                // Both builds' EvaluateAll lock that mutex and walk +8/+16; verified in the DLLs.
                const std::shared_lock lock(*const_cast<std::shared_mutex*>(
                    static_cast<std::shared_mutex const*>(set)));
                const auto entries = native_vector<Condition*>(set, Condition_Entries_Offset);
                if (entries.empty() && read_at<std::uintptr_t>(set, 8) != read_at<std::uintptr_t>(set, 16))
                {
                    candidate.reject("condition-set entries are unreadable");
                    result = false;
                }
                for (Condition const* condition : entries)
                {
                    if (!condition || !audit(*condition, candidate))
                    {
                        result = false;
                        break;
                    }
                }
                if (result)
                {
                    Frame frame(source, clip);
                    for (Condition const* condition : entries)
                    {
                        if (!condition->Evaluate(source, clip, submod))
                        {
                            result = false;
                            break;
                        }
                    }
                }
            }
            outer->absorb(candidate);
            if (!candidate.supported())
            {
                query->reject(candidate.diagnostic());
                // Nested AND/TARGET/PLAYER sets may sit below a negation or OR. Their unsupported
                // reads must veto the enclosing candidate, while the query may try the next one.
                if (outer != &query->scope)
                    outer->reject(candidate.diagnostic());
            }
            return result && candidate.supported();
        }

        bool evaluate_replacement_thunk(void const* replacement, RE::TESObjectREFR* source,
            RE::hkbClipGenerator* clip)
        {
            if (!s_evaluate_replacement(replacement, source, clip))
                return false;
            QueryFrame* const query = QueryFrame::current;
            ScopedEvaluation* const scope = ScopedEvaluation::current();
            if (!query || !scope || !scope->matches(source, clip))
                return true;
            // OAR's AnimationReplacements still holds its read lock here. Copy the chosen index
            // before returning to its loop; no ReplacementAnimation/Variant pointer escapes.
            const std::optional<AnimationDescriptor> descriptor = describe(replacement, query->key, query->name_count);
            if (!descriptor || descriptor->index == 0xFFFF || descriptor->path.empty())
            {
                query->reject("replacement metadata unavailable");
                return false;
            }
            query->output.name = descriptor->path;
            query->output.variants = descriptor->variants;
            query->output.replacement_key = descriptor->replacement_key;
            query->output.reevaluate_on_loop = descriptor->reevaluate_on_loop;
            query->output.interruptible = descriptor->interruptible;
            query->output.interrupt_blend = descriptor->interrupt_blend;
            query->output.loop_blend = descriptor->loop_blend;
            query->selected = replacement;
            query->index = descriptor->index;
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

    bool query_supported() { return s_query_ready.load(std::memory_order_acquire); }

    RE::hkbCharacterStringData* character_strings(RE::hkbCharacter const& character)
    {
        if (!readable(&character, Character_Setup_Offset + sizeof(void*)))
            return nullptr;
        void const* const setup = read_at<void*>(std::addressof(character), Character_Setup_Offset);
        if (!readable(setup, Setup_Data_Offset + sizeof(void*)))
            return nullptr;
        void const* const data = read_at<void*>(setup, Setup_Data_Offset);
        if (!readable(data, Character_String_Data_Offset + sizeof(void*)))
            return nullptr;
        auto* const strings = read_at<RE::hkbCharacterStringData*>(data, Character_String_Data_Offset);
        return readable(strings, sizeof(*strings)) ? strings : nullptr;
    }

    std::span<std::string const> animation_names(RE::hkbCharacter& character, QuerySession& session)
    {
        RE::hkbCharacterStringData* const wanted = character_strings(character);
        if (!wanted)
            return {};
        const auto& table = wanted->animationNames;
        const std::size_t count = static_cast<std::size_t>(table.size());
        if (table.size() <= 0 || table.size() > 65535 ||
            !readable(table.data(), count * sizeof(RE::hkStringPtr)))
            return {};
        return session.names(wanted, table.data(), count,
            [&](std::size_t i) { return asset_name(table[static_cast<int>(i)]); });
    }

    AnimationSelection query_animation(RE::TESObjectREFR& source, RE::hkbCharacter& character,
        std::span<std::string_view const> bases, PreviewState state, QuerySession& session,
        QueryRequest const& request)
    {
        AnimationSelection result;
        for (auto const& [name, value] : state.graph_variables)
        {
            (void)name;
            if (!std::isfinite(value))
            {
                result.diagnostic = "preview graph values must be finite";
                return result;
            }
        }
        RE::hkbCharacterStringData* const wanted = character_strings(character);
        if (!wanted)
        {
            result.diagnostic = "character string data unavailable";
            return result;
        }
        const auto decide = [&](void const* project)
        {
            const auto& animation_names = wanted->animationNames;
            if (animation_names.size() <= 0 || animation_names.size() > 65535 ||
                !readable(animation_names.data(), static_cast<std::size_t>(animation_names.size()) * sizeof(RE::hkStringPtr)))
            {
                result.diagnostic = "animation name table unavailable";
                return;
            }
            const auto names = session.names(wanted, animation_names.data(), static_cast<std::size_t>(animation_names.size()),
                [&](std::size_t i) { return asset_name(animation_names[static_cast<int>(i)]); });
            const auto base = find_base_animation(names, bases);
            if (!base)
            {
                result.diagnostic = "standing base animation not present in this project";
                return;
            }
            result.found = true;
            result.base_index = *base;
            result.index = *base;
            result.name = names[*base];
            result.base_name = result.name;
            result.binding_name = result.name;
            size_t clip_key = Amazing_Hash;
            clip_key = hash_combine(clip_key, wanted);
            clip_key = hash_combine(clip_key, *base);
            result.clip_key = hash_combine(clip_key, request.channel);
            if (!project || !query_supported())
                return;
            PreviewClip preview;
            preview.initialize(*base, result.name, true);
            ScopedEvaluation scope(&source, preview.get(), state, ScopeMode::kAudited);
            QueryFrame frame(scope, session, result, result.clip_key, names.size());
            if (void* const replacements = s_get_replacements(project, *base))
            {
                std::function<void(void*)> copy = [&](void* replacement)
                {
                    if (read_at<std::uint8_t>(replacement, Replacement_Disabled_Offset) != 0) return;
                    if (std::optional<AnimationDescriptor> descriptor = describe(replacement, result.clip_key, names.size()))
                    {
                        if (descriptor->index < names.size()) descriptor->binding_name = names[descriptor->index];
                        result.candidates.push_back(std::move(*descriptor));
                    }
                };
                s_for_each_replacement(replacements, copy, false);
                void* const selected = s_evaluate_replacements(replacements, &source, preview.get());
                if (selected && selected == frame.selected && frame.index < names.size())
                {
                    result.index = frame.index;
                    result.binding_name = names[frame.index];
                    result.replaced = true;
                }
                else
                {
                    result.replacement_key = 0;
                    result.variants.reset();
                    result.reevaluate_on_loop = true;
                    result.interruptible = false;
                    result.interrupt_blend = 0.0f;
                    result.loop_blend = 0.0f;
                }
            }
            result.evaluated = scope.evaluated();
            result.overrides = scope.overrides();
        };
        if (query_supported())
        {
            bool visited = false;
            std::function<void(RE::hkbCharacterStringData*, void*)> visit =
                [&](RE::hkbCharacterStringData* strings, void* project)
                {
                    if (!visited && strings == wanted)
                    {
                        visited = true;
                        decide(project);
                    }
                };
            if (void const* singleton = s_get_singleton())
                s_for_each_project_data(singleton, visit);
            if (!visited)
                decide(nullptr);
        }
        else
        {
            decide(nullptr);
        }
        return result;
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
        const auto image = std::span<std::uint8_t const>(reinterpret_cast<std::uint8_t const*>(module), profile->image_size);
        const bool query_verified = matches_query(profile->query, image);
        if (query_verified)
        {
            const auto address = [&](QueryFunction function)
            {
                return base + profile->query.entries[static_cast<std::size_t>(function)].rva;
            };
            s_for_each_replacement = reinterpret_cast<ForEachReplacement>(address(QueryFunction::e_enumerate));
            s_get_singleton = reinterpret_cast<GetSingleton>(address(QueryFunction::e_singleton));
            s_for_each_project_data = reinterpret_cast<ForEachProjectData>(address(QueryFunction::e_projects));
            s_get_replacements = reinterpret_cast<GetReplacements>(address(QueryFunction::e_replacements));
            s_evaluate_replacements = reinterpret_cast<EvaluateReplacements>(address(QueryFunction::e_evaluate));
            s_get_index = reinterpret_cast<GetIndex>(address(QueryFunction::e_index));
            s_evaluate_set = reinterpret_cast<EvaluateSet>(address(QueryFunction::e_conditions));
            s_evaluate_replacement = reinterpret_cast<EvaluateReplacement>(address(QueryFunction::e_replacement_condition));
            s_parent_submod = reinterpret_cast<Parent>(address(QueryFunction::e_parent_submod));
            s_interruptible = reinterpret_cast<ReplaceOnLoop>(address(QueryFunction::e_interruptible));
            s_blend_time = reinterpret_cast<BlendTime>(address(QueryFunction::e_blend_time));
            s_parent_mod = reinterpret_cast<Parent>(address(QueryFunction::e_parent_mod));
            s_replace_on_loop = reinterpret_cast<ReplaceOnLoop>(address(QueryFunction::e_replace_on_loop));
        }
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
        if (result == NO_ERROR && query_verified)
            result = DetourAttach(reinterpret_cast<PVOID*>(&s_evaluate_set), reinterpret_cast<PVOID>(&evaluate_set_thunk));
        if (result == NO_ERROR && query_verified)
            result = DetourAttach(reinterpret_cast<PVOID*>(&s_evaluate_replacement), reinterpret_cast<PVOID>(&evaluate_replacement_thunk));
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
        s_query_ready.store(query_verified, std::memory_order_release);
        const std::optional<uint32_t> ied_version = ied_provider_version();
        if (!ied_version)
            logger::info("OAR IED placement adapter: provider unavailable; native registration unverified");
        else if (*ied_version != Ied_Provider_Version)
            logger::info("OAR IED placement adapter: provider version mismatch ({:08X}); requires embedded 1.0.2.0", *ied_version);
        else
            logger::info("OAR IED placement adapter available: three native placement/shield predicates, current source equipment/configuration; native registration unverified");
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
        Frame frame(&source, &preview_clip);
        const bool matched = audit(condition, scope) && condition.Evaluate(&source, &preview_clip, parent_submod);
        return { matched && scope.supported(), scope.supported(), scope.overrides(), scope.diagnostic() };
    }
}
PLUGIN_NAMESPACE_END
