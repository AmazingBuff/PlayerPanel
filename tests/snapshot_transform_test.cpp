//
// Created by AmazingBuff on 2026/10/08.
//

#include "character/final_pose_probe.h"
#include "character/body_shape.h"
#include "character/equipment_mirror.h"
#include "character/figure_audit.h"
#include "character/snapshot_transform.h"
#include "character/panel_animation_policy.h"
#include "oar/query_policy.h"
#include "oar/preview_clip.h"
#include "oar/binary_profile.h"
#include "oar/policy.h"
#include "render/dx11/d3d11_util.h"
#include "render/studio/figure_draw_order.h"
#include "render/studio/figure_pipeline_state.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <thread>
#include <limits>

namespace
{
    bool preview_carrier_preserves_native_node_fields()
    {
        PLUGIN_NAMESPACE::oar::PreviewClip carrier;
        carrier.initialize_native(0x1000, 77, "Animations\\MT_Idle.hkx", true);
        RE::hkbClipGenerator const* const native = carrier.get();
        const bool fields = native->id == 0xFFFF && native->cloneState == RE::hkbNode::CloneState::kDefault &&
            std::string_view(native->name.c_str()) == "Animations\\MT_Idle.hkx" &&
            std::string_view(native->animationName.c_str()) == "Animations\\MT_Idle.hkx" &&
            native->animationBindingIndex == 77 && native->playbackSpeed == 1.0f &&
            native->mode == RE::hkbClipGenerator::PlaybackMode::kModeLooping && carrier.user_data() == 0;
        PLUGIN_NAMESPACE::oar::PreviewContext context(nullptr, nullptr);
        const auto* bytes = reinterpret_cast<std::uint8_t const*>(&context.get());
        return fields && std::all_of(bytes, bytes + sizeof(RE::hkbContext), [](std::uint8_t value) { return value == 0; });
    }

    bool equipment_refresh_waits_for_the_latest_assembly()
    {
        PLUGIN_NAMESPACE::EquipmentRefreshGate gate;
        gate.reset(1);
        if (gate.sample(1, 10) || gate.sample(2, 10) || gate.sample(2, 10) ||
            gate.sample(3, 20) || gate.sample(3, 20) || gate.sample(3, 30) ||
            gate.sample(3, 30) || gate.sample(3, 30) || !gate.sample(3, 30) || gate.sample(3, 30))
            return false;
        gate.retry();
        if (gate.sample(3, 30) || gate.sample(3, 30) || gate.sample(3, 30) || !gate.sample(3, 30))
            return false;
        gate.reset(4);
        return !gate.sample(4, 30);
    }

    bool oar_query_profiles_fail_closed()
    {
        namespace oar = PLUGIN_NAMESPACE::oar;
        for (oar::BinaryProfile const& profile : oar::Profiles)
        {
            std::vector<std::uint8_t> image(profile.image_size);
            for (oar::QueryEntry const& entry : profile.query.entries)
                std::copy(entry.prefix.begin(), entry.prefix.end(), image.begin() + entry.rva);
            if (!oar::matches_query(profile.query, image))
                return false;
            for (oar::QueryEntry const& entry : profile.query.entries)
            {
                image[entry.rva] ^= 1;
                if (oar::matches_query(profile.query, image))
                    return false;
                image[entry.rva] ^= 1;
            }
            oar::QueryProfile missing = profile.query;
            missing.entries[0].rva = 0;
            if (oar::matches_query(missing, image))
                return false;
            missing.entries[0].rva = std::numeric_limits<std::uintptr_t>::max();
            if (oar::matches_query(missing, image))
                return false;
        }
        return true;
    }

    bool standing_catalog_does_not_use_live_or_replacement_names()
    {
        namespace oar = PLUGIN_NAMESPACE::oar;
        const std::vector<std::string> project{
            "Animations\\SneakMTIdle.hkx",
            "Animations\\OpenAnimationReplacer\\Author\\MT_Idle.hkx",
            "animations/MT_IDLE.hkx", "Animations\\1hm_idle.hkx", "Animations\\JumpLand.hkx" };
        const std::array<std::string_view, 1> wanted{ "Animations\\MT_Idle.hkx" };
        const auto idle = oar::find_base_animation(project, wanted);
        const std::vector<std::string> other{ project[0], project[1], project[4] };
        oar::QuerySession session;
        int reads = 0;
        const auto first = session.names(&project, project.data(), project.size(), [&](std::size_t i)
        {
            ++reads;
            return project[i];
        });
        const std::size_t count = first.size();
        (void)session.names(&project, project.data(), project.size(), [&](std::size_t i)
        {
            ++reads;
            return project[i];
        });
        (void)session.names(&other, other.data(), other.size(), [&](std::size_t i)
        {
            ++reads;
            return other[i];
        });
        return idle && *idle == 2 && !oar::find_base_animation(other, wanted) && count == project.size() &&
            reads == static_cast<int>(project.size() + other.size());
    }

    bool preview_variants_are_local_and_bounded()
    {
        namespace oar = PLUGIN_NAMESPACE::oar;
        oar::SelectionState preview(123);
        oar::SelectionState other(123);
        oar::VariantOptions options;
        options.key = { oar::DataScope::e_local, 1, 10 };
        options.sequential = true;
        options.sequence = { { 17 }, { 19 } };
        const auto first = preview.variant(options, 1);
        const auto cancelled = preview.variant(options, 1);
        if (!first || !cancelled || first->index != 17 || cancelled->index != 17)
            return false;
        preview.commit(options, *first, 1);
        const auto second = preview.variant(options, 1);
        const auto isolated = other.variant(options, 1);
        if (!second || second->index != 19 || !isolated || isolated->index != 17)
            return false;
        preview.commit(options, *second, 1);
        if (preview.variant(options, 1)->index != 17)
            return false;
        preview.reset();
        if (preview.variant(options, 1)->index != 17)
            return false;
        options.sequential = false;
        options.sequence = { { 17, true }, { 19, true } };
        options.random = { 21, 23 };
        options.cumulative = { 0.0f, 1.0f };
        preview.reset();
        for (std::uint16_t expected : std::array<std::uint16_t, 3>{ 17, 19, 23 })
        {
            const auto pick = preview.variant(options, 1);
            if (!pick || pick->index != expected)
                return false;
            preview.commit(options, *pick, 1);
        }
        options.sequence.clear();
        options.cumulative = { 0.8f, 0.3f };
        if (preview.variant(options, 1))
            return false;
        options.random.clear();
        return !preview.variant(options, 1);
    }

    bool preview_random_conditions_cache_and_reset()
    {
        namespace oar = PLUGIN_NAMESPACE::oar;
        oar::SelectionState preview(123);
        oar::SelectionState other(123);
        const oar::StateKey local{ oar::DataScope::e_local, 1 };
        const oar::StateKey shared{ oar::DataScope::e_submod, 40 };
        const auto first = preview.random(local, 9, 1, true, 0.0f, 1.0f);
        if (!first || preview.random(local, 9, 1, true, 0.0f, 1.0f) != first ||
            other.random(local, 9, 1, true, 0.0f, 1.0f) != first)
            return false;
        preview.loop(2);
        if (preview.random(local, 9, 1, true, 0.0f, 1.0f) != first)
            return false;
        preview.loop(1);
        if (preview.random(local, 9, 1, true, 0.0f, 1.0f) == first)
            return false;
        const auto kept = preview.random(local, 10, 1, false, 0.0f, 1.0f);
        preview.loop(1);
        if (!kept || preview.random(local, 10, 1, false, 0.0f, 1.0f) != kept)
            return false;
        const auto common = preview.random(shared, 11, 1, true, 0.0f, 1.0f);
        if (!common || preview.random(shared, 11, 2, true, 0.0f, 1.0f) != common)
            return false;
        preview.loop(2);
        if (preview.random(shared, 11, 1, true, 0.0f, 1.0f) == common)
            return false;
        return !preview.random(local, 12, 1, true, 2.0f, 1.0f) &&
            !preview.random(local, 12, 1, true, 0.0f, std::numeric_limits<float>::infinity());
    }

    bool preview_variant_weights_and_shared_state()
    {
        namespace oar = PLUGIN_NAMESPACE::oar;
        oar::SelectionState preview(123);
        oar::VariantOptions options;
        options.key = { oar::DataScope::e_local, 1, 10 };
        options.random = { 7, 9 };
        options.cumulative = { 0.25f, 1.0f };
        std::size_t low = 0;
        for (std::size_t i = 0; i < 10000; ++i)
        {
            const auto pick = preview.variant(options, 1);
            const auto cached = preview.variant(options, 1);
            if (!pick || !cached || pick->index != cached->index)
                return false;
            low += pick->index == 7 ? 1 : 0;
            preview.commit(options, *pick, 1);
            preview.loop(1);
        }
        if (low < 2300 || low > 2700)
            return false;
        oar::SelectionState shared(123);
        options.key = { oar::DataScope::e_submod, 20 };
        options.random.clear();
        options.cumulative.clear();
        for (std::uint16_t i = 0; i < 64; ++i)
        {
            options.random.push_back(i);
            options.cumulative.push_back(static_cast<float>(i + 1) / 64.0f);
        }
        const auto first = shared.variant(options, 1);
        if (!first)
            return false;
        shared.commit(options, *first, 1);
        const auto follower = shared.variant(options, 2);
        if (!follower || follower->index != first->index)
            return false;
        shared.commit(options, *follower, 2);
        shared.loop(2);
        if (shared.variant(options, 2)->index != first->index)
            return false;
        shared.loop(1);
        const auto rerolled = shared.variant(options, 2);
        if (!rerolled || rerolled->index == first->index || rerolled->index != shared.variant(options, 1)->index)
            return false;
        options.key.owner = 30;
        options.reset_on_loop = false;
        const auto kept = shared.variant(options, 1);
        if (!kept)
            return false;
        shared.commit(options, *kept, 1);
        for (int i = 0; i < 10; ++i)
        {
            shared.loop(1);
            if (shared.variant(options, 1)->index != kept->index)
                return false;
        }
        return true;
    }

    bool preview_play_once_history_is_committed_on_start()
    {
        namespace oar = PLUGIN_NAMESPACE::oar;
        oar::SelectionState preview(123);
        oar::VariantOptions options;
        options.key = { oar::DataScope::e_submod, 20 };
        options.share_history = true;
        options.sequence = { { 11, true }, { 12, true } };
        options.random = { 13 };
        options.cumulative = { 1.0f };
        const auto cancelled = preview.variant(options, 1);
        if (!cancelled || cancelled->index != 11 || preview.variant(options, 1)->index != 11)
            return false;
        preview.commit(options, *cancelled, 1);
        const auto other = preview.variant(options, 2);
        if (!other || other->index != 12)
            return false;
        preview.commit(options, *other, 2);
        return preview.variant(options, 1)->index == 13;
    }

    bool idle_loops_respect_sequence_and_recheck_flags()
    {
        namespace oar = PLUGIN_NAMESPACE::oar;
        oar::SelectionState preview(123);
        oar::VariantOptions options;
        options.key = { oar::DataScope::e_local, 1, 10 };
        options.sequential = true;
        options.sequence = { { 11 }, { 12 }, { 13 } };
        if (oar::idle_loop_decision(true, true, preview.in_sequence(options, 1)) != oar::LoopDecision::e_recheck)
            return false;
        for (int i = 0; i < 2; ++i)
        {
            const auto pick = preview.variant(options, 1);
            if (!pick)
                return false;
            preview.commit(options, *pick, 1);
            if (oar::idle_loop_decision(true, true, preview.in_sequence(options, 1)) != oar::LoopDecision::e_continue_variants)
                return false;
        }
        preview.commit(options, *preview.variant(options, 1), 1);
        return !preview.in_sequence(options, 1) &&
            oar::idle_loop_decision(false, true, false) == oar::LoopDecision::e_continue_variants &&
            oar::idle_loop_decision(false, false, false) == oar::LoopDecision::e_keep;
    }

    bool latest_weapon_request_rejects_stale_loads()
    {
        PLUGIN_NAMESPACE::LatestAnimationRequest latest;
        if (!latest.observe(10))
            return false;
        const std::uint64_t a = latest.generation();
        latest.observe(20);
        const std::uint64_t b = latest.generation();
        latest.observe(30);
        const std::uint64_t c = latest.generation();
        if (latest.accepts(a) || latest.accepts(b) || !latest.accepts(c) || latest.observe(30))
            return false;
        latest.observe(10);
        return !latest.accepts(a) && !latest.accepts(c) && latest.accepts(latest.generation());
    }

    bool idle_layers_keep_independent_cycles()
    {
        PLUGIN_NAMESPACE::IdleCycleGate body;
        PLUGIN_NAMESPACE::IdleCycleGate weapon;
        return !body.crossed(0, true) && !weapon.crossed(0, true) && body.crossed(1, true) &&
            !weapon.crossed(0, true) && !body.crossed(1, true) && weapon.crossed(1, true) &&
            !body.crossed(2, false) && !body.crossed(2, true) && body.crossed(3, true);
    }

    bool interrupted_pose_blend_starts_from_the_visible_pose()
    {
        RE::NiTransform first;
        first.translate = { 0.0f, 0.0f, 0.0f };
        first.scale = 1.0f;
        first.rotate = PLUGIN_NAMESPACE::ni_matrix_from_quaternion(0.0f, 0.0f, 0.0f, 1.0f);
        RE::NiTransform second = first;
        second.translate.x = 20.0f;
        second.scale = 2.0f;
        second.rotate = PLUGIN_NAMESPACE::ni_matrix_from_quaternion(0.0f, 0.0f, 1.0f, 0.0f);
        const RE::NiTransform visible = PLUGIN_NAMESPACE::blend_panel_pose(first, second, 0.4f);
        RE::NiTransform latest = first;
        latest.translate.x = -10.0f;
        const RE::NiTransform start = PLUGIN_NAMESPACE::blend_panel_pose(visible, latest, 0.0f);
        const RE::NiTransform end = PLUGIN_NAMESPACE::blend_panel_pose(visible, latest, 1.0f);
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 3; ++column)
                if (std::abs(start.rotate.entry[row][column] - visible.rotate.entry[row][column]) > 0.00001f)
                    return false;
        return std::abs(start.translate.x - 8.0f) < 0.00001f &&
            std::abs(start.scale - 1.4f) < 0.00001f && std::abs(end.translate.x + 10.0f) < 0.00001f;
    }

    bool preview_original_answers_suspend_projection()
    {
        namespace oar = PLUGIN_NAMESPACE::oar;
        auto* source = reinterpret_cast<RE::TESObjectREFR*>(0x1000);
        auto* preview_clip = reinterpret_cast<RE::hkbClipGenerator*>(0x2000);
        auto* world_clip = reinterpret_cast<RE::hkbClipGenerator*>(0x3000);
        oar::PreviewState state;
        oar::ScopedEvaluation preview(source, preview_clip, state);
        if (!preview.matches(source, preview_clip) || preview.matches(source, world_clip))
            return false;
        {
            oar::UnprojectedEvaluation original;
            if (oar::ScopedEvaluation::current())
                return false;
        }
        return oar::ScopedEvaluation::current() == &preview;
    }

    bool panel_transition_returns_to_a_standing_idle()
    {
        using namespace PLUGIN_NAMESPACE;
        const std::array<std::string_view, 7> body{ "NPC Pelvis [Pelv]", "NPC Spine1 [Spn1]", "NPC Head [Head]",
            "NPC L Thigh [LThg]", "NPC R Thigh [RThg]", "NPC L Calf [LClf]", "NPC R Calf [RClf]" };
        return panel_phase(false, true, false) == PanelAnimationPhase::e_loading &&
            panel_phase(true, true, false) == PanelAnimationPhase::e_transition &&
            panel_phase(true, true, true) == PanelAnimationPhase::e_idle &&
            panel_phase(true, false, false) == PanelAnimationPhase::e_idle &&
            covers_standing_body(body) && !covers_standing_body(std::span(body).first(6)) &&
            panel_idle_bases(oar::WeaponForm::kBow) != panel_idle_bases(oar::WeaponForm::kTwoHanded) &&
            panel_idle_bases(oar::WeaponForm::kCrossbow) != panel_idle_bases(oar::WeaponForm::kBow) &&
            oar::weapon_form_from_hands(oar::HandKind::kBow, oar::HandKind::kEmpty) == oar::WeaponForm::kBow &&
            oar::weapon_form_from_hands(oar::HandKind::kEmpty, oar::HandKind::kCrossbow) == oar::WeaponForm::kCrossbow;
    }

    bool panel_animation_state_keeps_stance_and_transition_contract()
    {
        using namespace PLUGIN_NAMESPACE;
        // oar-idle3: a sheathed weapon swap is an equip transition, not a sheathe — the panel
        // plays the new weapon's equip (problem 3), and the entering-stance snapshot keeps a
        // drawn answer consistent against its own recorded equipment (problems 2).
        const auto sword_to_sword = panel_transition_bases(oar::WeaponForm::kOneHanded, false);
        const auto sheathe = panel_transition_bases(oar::WeaponForm::kOneHanded, true);
        return !sword_to_sword.empty() && sword_to_sword.front() == "Animations\\1HM_Equip.hkx" &&
            !sheathe.empty() && sheathe.front() == "Animations\\1HM_Unequip.hkx" &&
            panel_transition_bases(oar::WeaponForm::kNone, true).empty();
    }

    bool project_name_catalogue_is_reused_across_sessions()
    {
        namespace oar = PLUGIN_NAMESPACE::oar;
        const std::vector<std::string> project{
            "Animations\\MT_Idle.hkx", "Animations\\1hm_idle.hkx", "Animations\\JumpLand.hkx" };
        const std::array<std::string_view, 1> wanted{ "Animations\\MT_Idle.hkx" };
        // A fresh session (what a panel open resets to) must answer from the process catalogue
        // when a previous session already read the same table (oar-idle2 problem 5: the open
        // frame rebuilt ~15k names on the main thread). Reads stay session-local on a miss.
        oar::QuerySession cold;
        std::size_t cold_reads = 0;
        (void)cold.names(&project, project.data(), project.size(), [&](std::size_t i)
        { ++cold_reads; return project[i]; });
        oar::QuerySession warm;
        std::size_t warm_reads = 0;
        const auto names = warm.names(&project, project.data(), project.size(), [&](std::size_t i)
        { ++warm_reads; return project[i]; });
        const bool reused = warm_reads == 0 && names.size() == project.size() &&
            oar::find_base_animation(names, wanted) && *oar::find_base_animation(names, wanted) == 0;
        // A different owner (or a replaced table pointer — load boundary) rebuilds.
        const std::vector<std::string> reloaded{ "Animations\\MT_Idle.hkx", "Animations\\SneakMTIdle.hkx" };
        oar::QuerySession after_load;
        std::size_t reload_reads = 0;
        (void)after_load.names(&project, reloaded.data(), reloaded.size(), [&](std::size_t i)
        { ++reload_reads; return reloaded[i]; });
        return reused && reload_reads == reloaded.size();
    }

    bool d3d_constant_buffer_ranges_are_restored()
    {
        REX::W32::ComPtr<REX::W32::ID3D11Device> device;
        REX::W32::ComPtr<REX::W32::ID3D11DeviceContext> context;
        static constexpr uint32_t s_d3d11_sdk_version = 7;
        const REX::W32::HRESULT device_hr = REX::W32::D3D11CreateDevice(nullptr, REX::W32::D3D_DRIVER_TYPE_WARP,
            nullptr, 0, nullptr, 0, s_d3d11_sdk_version, device.GetAddressOf(), nullptr, context.GetAddressOf());
        if (!REX::W32::SUCCESS(device_hr))
            return false;
        REX::W32::ComPtr<REX::W32::ID3D11DeviceContext1> context1;
        const REX::W32::HRESULT context_hr = context->QueryInterface(REX::W32::IID_ID3D11DeviceContext1,
            reinterpret_cast<void**>(context1.GetAddressOf()));
        if (!REX::W32::SUCCESS(context_hr))
            return false;

        const REX::W32::D3D11_BUFFER_DESC buffer_desc{
            .byteWidth = 4096,
            .usage = REX::W32::D3D11_USAGE_DEFAULT,
            .bindFlags = REX::W32::D3D11_BIND_CONSTANT_BUFFER,
        };
        REX::W32::ComPtr<REX::W32::ID3D11Buffer> buffer;
        const REX::W32::HRESULT buffer_hr = device->CreateBuffer(&buffer_desc, nullptr, buffer.GetAddressOf());
        if (!REX::W32::SUCCESS(buffer_hr))
            return false;
        const uint32_t first = 16;
        const uint32_t count = 32;
        context1->VSSetConstantBuffers1(10, 1, buffer.GetAddressOf(), &first, &count);
        context1->PSSetConstantBuffers1(11, 1, buffer.GetAddressOf(), &first, &count);
        context->VSSetConstantBuffers(0, 1, buffer.GetAddressOf());
        REX::W32::ComPtr<REX::W32::ID3D11Buffer> full_binding;
        uint32_t full_first = 0;
        uint32_t full_count = 0;
        context1->VSGetConstantBuffers1(0, 1, full_binding.GetAddressOf(), &full_first, &full_count);
        {
            PLUGIN_NAMESPACE::D3D11StateCapture capture(context.Get());
            context->ClearState();
        }
        REX::W32::ComPtr<REX::W32::ID3D11Buffer> restored_vs;
        REX::W32::ComPtr<REX::W32::ID3D11Buffer> restored_ps;
        REX::W32::ComPtr<REX::W32::ID3D11Buffer> restored_full;
        uint32_t vs_first = 0;
        uint32_t vs_count = 0;
        uint32_t ps_first = 0;
        uint32_t ps_count = 0;
        uint32_t restored_full_first = 0;
        uint32_t restored_full_count = 0;
        context1->VSGetConstantBuffers1(10, 1, restored_vs.GetAddressOf(), &vs_first, &vs_count);
        context1->PSGetConstantBuffers1(11, 1, restored_ps.GetAddressOf(), &ps_first, &ps_count);
        context1->VSGetConstantBuffers1(0, 1, restored_full.GetAddressOf(), &restored_full_first, &restored_full_count);
        return restored_vs.Get() == buffer.Get() && restored_ps.Get() == buffer.Get() &&
            vs_first == first && vs_count == count && ps_first == first && ps_count == count &&
            restored_full.Get() == full_binding.Get() && restored_full_first == full_first && restored_full_count == full_count;
    }

    bool figure_transparency_draw_order()
    {
        using PLUGIN_NAMESPACE::FigureGeometry;
        RE::BSGeometry* const brow = reinterpret_cast<RE::BSGeometry*>(0x10);
        RE::BSGeometry* const face = reinterpret_cast<RE::BSGeometry*>(0x20);
        RE::BSGeometry* const cutout = reinterpret_cast<RE::BSGeometry*>(0x30);
        RE::BSGeometry* const far_layer = reinterpret_cast<RE::BSGeometry*>(0x40);
        RE::BSGeometry* const tied_layer = reinterpret_cast<RE::BSGeometry*>(0x50);
        std::vector<FigureGeometry> geometry{
            { true, false, false, 10.0f, brow },
            { false, false, false, 9.0f, face },
            { true, true, false, 12.0f, far_layer },
            { false, true, false, 8.0f, cutout },
            { true, false, false, 12.0f, tied_layer },
        };
        PLUGIN_NAMESPACE::order_figure_geometry(geometry);
        if (geometry[0].geometry != face || geometry[1].geometry != cutout ||
            geometry[2].geometry != far_layer || geometry[3].geometry != tied_layer || geometry[4].geometry != brow)
            return false;

        geometry = {
            { true, false, false, 10.0f, brow },
            { true, false, true, 1.0f, cutout },
            { true, false, false, 20.0f, far_layer },
            { true, false, false, 30.0f, tied_layer },
        };
        PLUGIN_NAMESPACE::order_figure_geometry(geometry);
        return geometry[0].geometry == brow && geometry[1].geometry == cutout &&
            geometry[2].geometry == tied_layer && geometry[3].geometry == far_layer;
    }

    bool figure_material_state_is_restored()
    {
        RE::BSGraphics::RendererShadowState::FLAT_RUNTIME_DATA state{};
        state.viewPort = { 10.0f, 20.0f, 1920.0f, 1080.0f, 0.0f, 1.0f };
        state.depthStencilDepthMode = RE::BSGraphics::DepthStencilDepthMode::kTestEqual;
        state.depthStencilDepthModePrevious = RE::BSGraphics::DepthStencilDepthMode::kTestWrite;
        state.depthStencilStencilMode = 3;
        state.stencilRef = 7;
        state.rasterStateCullMode = RE::BSGraphics::RASTER_STATE_CULL_MODE_FRONT;
        state.alphaBlendMode = 3;
        state.alphaBlendAlphaToCoverage = 1;
        state.alphaBlendWriteMode = 11;
        state.alphaBlendModeExtra = 1;
        state.alphaTestEnabled = true;
        state.alphaTestRef = 0.4f;
        state.stateUpdateFlags.set(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET,
            RE::BSGraphics::ShaderFlags::DIRTY_VERTEX_DESC);
        const RE::BSGraphics::RendererShadowState::FLAT_RUNTIME_DATA before = state;
        const REX::W32::D3D11_VIEWPORT studio{ 0.0f, 0.0f, 512.0f, 1024.0f, 0.0f, 1.0f };
        {
            PLUGIN_NAMESPACE::FigurePipelineState scope(state);
            scope.prepare(studio, false, true, false);
            if (state.depthStencilDepthMode != RE::BSGraphics::DepthStencilDepthMode::kTestWrite || !state.alphaTestEnabled ||
                state.rasterStateCullMode != RE::BSGraphics::RASTER_STATE_CULL_MODE_BACK)
                return false;
            scope.prepare(studio, true, false, true);
            if (state.depthStencilDepthMode != RE::BSGraphics::DepthStencilDepthMode::kTest || state.alphaTestEnabled ||
                state.rasterStateCullMode != RE::BSGraphics::RASTER_STATE_CULL_MODE_NONE ||
                !state.stateUpdateFlags.all(RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND,
                    RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_TEST_REF, RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_ENABLE_TEST) ||
                state.stateUpdateFlags.any(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET))
                return false;
            state.alphaBlendMode = 6;
            state.alphaTestRef = 0.8f;
            state.stateUpdateFlags = {};
        }
        return state.viewPort.width == before.viewPort.width && state.viewPort.topLeftX == before.viewPort.topLeftX &&
            state.depthStencilDepthMode == before.depthStencilDepthMode &&
            state.depthStencilDepthModePrevious == before.depthStencilDepthModePrevious &&
            state.depthStencilStencilMode == before.depthStencilStencilMode && state.stencilRef == before.stencilRef &&
            state.rasterStateCullMode == before.rasterStateCullMode && state.alphaBlendMode == before.alphaBlendMode &&
            state.alphaBlendAlphaToCoverage == before.alphaBlendAlphaToCoverage &&
            state.alphaBlendWriteMode == before.alphaBlendWriteMode && state.alphaBlendModeExtra == before.alphaBlendModeExtra &&
            state.alphaTestEnabled == before.alphaTestEnabled && state.alphaTestRef == before.alphaTestRef &&
            state.stateUpdateFlags.all(RE::BSGraphics::ShaderFlags::DIRTY_RENDERTARGET,
                RE::BSGraphics::ShaderFlags::DIRTY_VERTEX_DESC, RE::BSGraphics::ShaderFlags::DIRTY_VIEWPORT,
                RE::BSGraphics::ShaderFlags::DIRTY_DEPTH_MODE, RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_BLEND,
                RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_TEST_REF, RE::BSGraphics::ShaderFlags::DIRTY_ALPHA_ENABLE_TEST);
    }

    bool oar_preview_scope_is_isolated()
    {
        namespace oar = PLUGIN_NAMESPACE::oar;
        auto* const source = reinterpret_cast<RE::TESObjectREFR*>(0x1000);
        auto* const clip = reinterpret_cast<RE::hkbClipGenerator*>(0x2000);
        auto* const world_clip = reinterpret_cast<RE::hkbClipGenerator*>(0x3000);
        oar::PreviewState state;
        state.weapon_drawn = true;
        bool isolated_thread = false;
        bool nested_restored = false;
        bool standing = false;
        {
            oar::ScopedEvaluation outer(source, clip, state);
            std::thread other([&] { isolated_thread = oar::ScopedEvaluation::current() == nullptr; });
            other.join();
            oar::PreviewState inner_state;
            {
                oar::ScopedEvaluation inner(source, world_clip, inner_state);
                if (oar::ScopedEvaluation::current() != &inner)
                    return false;
            }
            nested_restored = oar::ScopedEvaluation::current() == &outer && outer.matches(source, clip) &&
                !outer.matches(source, world_clip);
            const auto in_air = oar::standing_predicate("IsInAir", state);
            const auto sneaking = oar::standing_predicate("IsSneaking", state);
            standing = in_air && !*in_air && sneaking && !*sneaking &&
                oar::finish_predicate(*in_air, false, true) && oar::finish_predicate(true, true, true) &&
                *oar::standing_predicate("IsWeaponDrawn", state) && oar::forwards_source("IsForm") &&
                !oar::forwards_source("Random");
        }
        return standing && nested_restored && isolated_thread && oar::ScopedEvaluation::current() == nullptr;
    }

    bool oar_projection_scope_covers_every_clip()
    {
        namespace oar = PLUGIN_NAMESPACE::oar;
        auto* const source = reinterpret_cast<RE::TESObjectREFR*>(0x1000);
        auto* const other_source = reinterpret_cast<RE::TESObjectREFR*>(0x1800);
        auto* const first_clip = reinterpret_cast<RE::hkbClipGenerator*>(0x2000);
        auto* const second_clip = reinterpret_cast<RE::hkbClipGenerator*>(0x3000);
        oar::PreviewState state;
        const oar::ScopedEvaluation projection(source, nullptr, state, oar::ScopeMode::kProjected);
        const bool covers_every_clip = projection.matches(source, first_clip) &&
            projection.matches(source, second_clip) && projection.matches(source, nullptr);
        const bool still_scoped_to_source = !projection.matches(other_source, first_clip);
        const bool reports_mode = projection.mode() == oar::ScopeMode::kProjected;
        bool counted = false;
        bool tallied = false;
        {
            oar::ScopedEvaluation audited(source, first_clip, state);
            const bool defaults_to_audited = audited.mode() == oar::ScopeMode::kAudited;
            const bool named_clip_only = audited.matches(source, first_clip) && !audited.matches(source, second_clip);
            audited.note_evaluated();
            audited.note_override("IsInAir", false);
            counted = defaults_to_audited && named_clip_only && audited.evaluated() == 1 && audited.overrides() == 1;
        }
        {
            oar::OverrideTally tally;
            oar::ScopedEvaluation projected(source, nullptr, state, oar::ScopeMode::kProjected, &tally);
            projected.note_override("IsInAir", false);
            projected.note_override("IsInAir", true);
            projected.note_override("CurrentMovementSpeed", true);
            const auto air = tally.find("IsInAir");
            const auto speed = tally.find("CurrentMovementSpeed");
            tallied = projected.overrides() == 3 && air != tally.end() && air->second.overrides == 2 &&
                air->second.differing == 1 && speed != tally.end() && speed->second.overrides == 1 &&
                speed->second.differing == 1;
        }
        return covers_every_clip && still_scoped_to_source && reports_mode && counted && tallied;
    }

    bool two_handed_weapon_keeps_one_hand_state()
    {
        auto* const sword = reinterpret_cast<RE::TESBoundObject*>(0x1000);
        auto* const blade = reinterpret_cast<RE::TESBoundObject*>(0x2000);
        auto* const other = reinterpret_cast<RE::TESBoundObject*>(0x3000);
        // The removal loop must keep the weapon in either entry (the engine flips which one reports
        // it), must still take away a third item, and must still replace the weapon when the player
        // has moved on to something else.
        const bool keeps_the_weapon = PLUGIN_NAMESPACE::hand_item_is_wanted(blade, nullptr, blade) &&
            PLUGIN_NAMESPACE::hand_item_is_wanted(blade, blade, blade) &&
            PLUGIN_NAMESPACE::hand_item_is_wanted(other, other, blade);
        const bool still_removes_others = !PLUGIN_NAMESPACE::hand_item_is_wanted(blade, other, other) &&
            !PLUGIN_NAMESPACE::hand_item_is_wanted(other, blade, blade) &&
            !PLUGIN_NAMESPACE::hand_item_is_wanted(nullptr, blade, blade);
        // The removal loop asks "is there something to take off", not "is the wanted item held": an
        // EMPTY hand must skip the removal branch outright (asking the held-question of nothing
        // answers no, and falling through put a null item into slot_for/RemoveItem - crash
        // 2026-10-15 23:02:27, first dressing while the player held a one-handed weapon).
        const bool empty_hand_has_nothing_to_remove = !PLUGIN_NAMESPACE::hand_removal_needed(nullptr, blade, blade) &&
            !PLUGIN_NAMESPACE::hand_removal_needed(nullptr, nullptr, nullptr) &&
            PLUGIN_NAMESPACE::hand_removal_needed(other, blade, blade) &&
            PLUGIN_NAMESPACE::hand_removal_needed(blade, other, other) &&
            !PLUGIN_NAMESPACE::hand_removal_needed(blade, nullptr, blade) &&
            !PLUGIN_NAMESPACE::hand_removal_needed(other, other, blade);
        // The equip loop must be satisfied by the entry the engine did NOT name.
        const bool satisfied_from_the_other_entry =
            PLUGIN_NAMESPACE::hand_object_equipped(blade, blade, nullptr, blade) &&
            PLUGIN_NAMESPACE::hand_object_equipped(blade, blade, blade, nullptr) &&
            PLUGIN_NAMESPACE::hand_object_equipped(other, nullptr, other, nullptr);
        // ... and one-handed items must not borrow the other hand's state.
        const bool one_handed_stays_per_entry = !PLUGIN_NAMESPACE::hand_object_equipped(sword, nullptr, nullptr, sword) &&
            !PLUGIN_NAMESPACE::hand_object_equipped(sword, other, other, sword);
        return keeps_the_weapon && still_removes_others && empty_hand_has_nothing_to_remove &&
            satisfied_from_the_other_entry && one_handed_stays_per_entry;
    }

    bool panel_pose_answers_standing_and_weapon_form()
    {
        namespace oar = PLUGIN_NAMESPACE::oar;
        // The panel's pre-condition (state-idle v3): the standing figure's pose predicates answer
        // "no" for everything the player might be doing, and the weapon form is what stays.
        oar::PreviewState const standing{ .weapon_drawn = false };
        oar::PreviewState const drawn{ .weapon_drawn = true };
        const bool pose_is_standing =
            oar::standing_predicate("IsSneaking", standing) == false &&
            oar::standing_predicate("IsSwimming", standing) == false &&
            oar::standing_predicate("IsAttacking", standing) == false &&
            oar::standing_predicate("IsRunning", standing) == false &&
            oar::standing_predicate("IsInAir", standing) == false &&
            oar::standing_predicate("IsOnMount", standing) == false &&
            oar::standing_predicate("MOUNT", standing) == false;
        const bool weapon_state_passes_through =
            oar::standing_predicate("IsWeaponDrawn", standing) == false &&
            oar::standing_predicate("IsWeaponDrawn", drawn) == true;
        // The form follows the two hands: a two-handed weapon or a bow owns the stance whichever
        // entry the engine named, two one-handed weapons are the dual stance, and a shield or a
        // spell beside a single one-handed weapon leaves it the one-handed stance.
        using HK = oar::HandKind;
        const bool forms_split =
            oar::weapon_form_from_hands(HK::kTwoHandedWeapon, HK::kEmpty) == oar::WeaponForm::kTwoHanded &&
            oar::weapon_form_from_hands(HK::kEmpty, HK::kTwoHandedWeapon) == oar::WeaponForm::kTwoHanded &&
            oar::weapon_form_from_hands(HK::kOneHandedWeapon, HK::kOneHandedWeapon) == oar::WeaponForm::kDualWield &&
            oar::weapon_form_from_hands(HK::kOneHandedWeapon, HK::kShield) == oar::WeaponForm::kOneHanded &&
            oar::weapon_form_from_hands(HK::kOneHandedWeapon, HK::kSpell) == oar::WeaponForm::kOneHanded &&
            oar::weapon_form_from_hands(HK::kSpell, HK::kSpell) == oar::WeaponForm::kMagic &&
            oar::weapon_form_from_hands(HK::kStaff, HK::kEmpty) == oar::WeaponForm::kStaff &&
            oar::weapon_form_from_hands(HK::kTorch, HK::kEmpty) == oar::WeaponForm::kTorch &&
            oar::weapon_form_from_hands(HK::kEmpty, HK::kEmpty) == oar::WeaponForm::kNone;
        return pose_is_standing && weapon_state_passes_through && forms_split;
    }

    bool oar_binary_profile_rejects_mismatches()
    {
        namespace oar = PLUGIN_NAMESPACE::oar;
        const auto accepts_own = [](oar::BinaryProfile const& profile)
        {
            return oar::matches(profile, profile.timestamp, profile.image_size, profile.pdb_guid, profile.pdb_age,
                profile.evaluate_prefix, profile.numeric_prefix);
        };
        bool verified = true;
        for (auto const& profile : oar::Profiles)
            verified = verified && accepts_own(profile);

        auto damaged = oar::Profile_3_1_6.evaluate_prefix;
        damaged[0] ^= 1;
        const bool rejects_damage = !oar::matches(oar::Profile_3_1_6, oar::Profile_3_1_6.timestamp,
            oar::Profile_3_1_6.image_size, oar::Profile_3_1_6.pdb_guid, oar::Profile_3_1_6.pdb_age,
            damaged, oar::Profile_3_1_6.numeric_prefix);

        // The two verified builds share Evaluate's bytes but not GetValue's, so a profile must
        // reject the other build's identity even before the byte comparison runs.
        const bool rejects_other_build = !oar::matches(oar::Profile_3_2_1, oar::Profile_3_1_6.timestamp,
            oar::Profile_3_1_6.image_size, oar::Profile_3_1_6.pdb_guid, oar::Profile_3_1_6.pdb_age,
            oar::Profile_3_1_6.evaluate_prefix, oar::Profile_3_1_6.numeric_prefix);

        return verified && rejects_damage && rejects_other_build;
    }

    bool animation_mapping_preserves_track_identity()
    {
        const std::int16_t table[] = { 7, -1, 2, 0 };
        const float samples[] = { 10.0f, 20.0f, 30.0f, 40.0f };
        float bones[8]{};
        const auto bindings = PLUGIN_NAMESPACE::resolve_animation_tracks(table, std::size(samples), std::size(bones));
        for (auto const& binding : bindings)
        {
            if (binding.track >= std::size(samples) || binding.bone >= std::size(bones))
                return false;
            bones[binding.bone] = samples[binding.track];
        }
        return bindings.size() == 3 && bones[7] == 10.0f && bones[2] == 30.0f && bones[0] == 40.0f;
    }

    bool empty_source_clears_body_shape()
    {
        const PLUGIN_NAMESPACE::BodyMorphValues source;
        const PLUGIN_NAMESPACE::BodyMorphValues target{ { "OldPresetSlider", 1.0f } };
        PLUGIN_NAMESPACE::BodyShapeSync sync;
        sync.mismatched = PLUGIN_NAMESPACE::body_shape_mismatches(source, target);
        sync.applied = sync.mismatched != 0;
        return sync.mismatched == 1 && sync.copied == 0 && sync.changed();
    }

    bool snapshot_placement_preserves_subtree()
    {
        RE::NiTransform captured;
        captured.translate = { 12000.0f, -5000.0f, 800.0f };
        captured.rotate.SetEulerAnglesXYZ(0.0f, 0.0f, 1.5707963f);
        captured.scale = 2.0f;
        const RE::NiPoint3 center = captured * RE::NiPoint3{ 4.0f, 8.0f, 10.0f };
        const RE::NiPoint3 joint = captured * RE::NiPoint3{ 5.0f, 8.0f, 10.0f };
        const RE::NiPoint3 child = captured * RE::NiPoint3{ 7.0f, 8.0f, 10.0f };
        const RE::NiPoint3 anchor{ 0.0f, -485.0f, 0.0f };
        RE::NiTransform placement;
        placement.translate = anchor;
        placement.rotate.SetEulerAnglesXYZ(0.0f, 0.0f, 1.5707963f);
        placement.scale = 0.35f;
        const RE::NiTransform delta = PLUGIN_NAMESPACE::snapshot_delta(captured, placement, center, anchor);
        return (delta * center - anchor).Length() <= 0.002f &&
            std::abs((delta * child - delta * joint).Length() - 0.7f) <= 0.002f;
    }

    bool final_pose_discriminates_index_order()
    {
        std::vector<PLUGIN_NAMESPACE::PoseSample> pose(12);
        std::vector<PLUGIN_NAMESPACE::TransformSample> matching;
        for (std::size_t i = 0; i < pose.size(); ++i)
        {
            const float half_angle = static_cast<float>(i) * 0.0261799f;
            pose[i].position[0] = static_cast<float>(i);
            pose[i].quaternion[2] = std::sin(half_angle);
            pose[i].quaternion[3] = std::cos(half_angle);
            pose[i].scale = 1.0f;
            matching.push_back(PLUGIN_NAMESPACE::pose_local(pose[i]));
        }
        std::vector<PLUGIN_NAMESPACE::TransformSample> shifted = matching;
        std::rotate(shifted.begin(), shifted.begin() + 1, shifted.end());
        const PLUGIN_NAMESPACE::OrderScore exact = PLUGIN_NAMESPACE::score_pose_order(pose, matching, PLUGIN_NAMESPACE::Pose_Match_Degrees);
        const PLUGIN_NAMESPACE::OrderScore wrong = PLUGIN_NAMESPACE::score_pose_order(pose, shifted, PLUGIN_NAMESPACE::Pose_Match_Degrees);
        const std::vector<PLUGIN_NAMESPACE::OrderScore> candidates{ wrong, exact };
        const PLUGIN_NAMESPACE::OrderDecision chosen = PLUGIN_NAMESPACE::decide_pose_order(candidates);
        return exact.compared == 12 && exact.within == 12 && wrong.ratio() < PLUGIN_NAMESPACE::Pose_Match_Min_Ratio &&
            chosen.match && chosen.best == 1;
    }

    // AFP9c, the R01 framing rule: the figure sits parked ~8000 units below the player with the
    // world still at scale 1, and the delta must move that whole frame onto the studio anchor at the
    // studio scale. This is the assertion whose absence let a figure ship drawn far outside the
    // frustum: the delta is applied to the LIVE tree's worlds, never to the root's local, because a
    // parented node's downward pass does not move the node itself.
    bool figure_world_delta_frames_the_figure()
    {
        RE::NiTransform parked;
        parked.translate = { -604.7f, 37.8f, -7684.0f };
        parked.rotate.SetEulerAnglesXYZ(0.0f, 0.0f, 0.0f);
        parked.scale = 1.0f;
        const RE::NiPoint3 feet = parked * RE::NiPoint3{ 0.0f, 0.0f, 0.0f };
        const RE::NiPoint3 center = parked * RE::NiPoint3{ 0.0f, 0.0f, 60.0f };  // bound center above the feet
        const RE::NiPoint3 anchor{ 0.0f, 485.0f, 0.0f };

        const RE::NiTransform delta = PLUGIN_NAMESPACE::figure_world_delta(parked, center, anchor, 0.35f, 0.0f);
        const RE::NiPoint3 framed_center = delta * center;
        const RE::NiPoint3 framed_feet = delta * feet;
        return (framed_center - anchor).Length() <= 0.002f &&
            std::abs((framed_center - framed_feet).Length() - 60.0f * 0.35f) <= 0.002f;
    }

    // AFP9 package A, decision rules only (no engine memory here): the coverage ledger must
    // reconcile, and the necessity classifier must order its exceptions the way the audit logs
    // them - palette first, then weapon attachment, then helper, then unknown.
    bool coverage_ledger_reconciles()
    {
        const std::uint32_t output_slots = 116;
        const std::uint32_t mapping_slots = 98;
        const std::uint32_t written = 92;
        const std::uint32_t unreachable = 6;
        const std::uint32_t attempted = std::min(mapping_slots, output_slots);
        const std::uint32_t unprocessed = output_slots - attempted;
        return attempted == 98 && unprocessed == 18 &&
            written + unreachable + unprocessed == output_slots;
    }

    bool slot_necessity_orders_exceptions()
    {
        // The palette holds pointers and is never dereferenced, so the test classifies against
        // fabricated identities: the classifier must decide by membership, not by reading anything.
        PLUGIN_NAMESPACE::SkinPalette bound;
        bound.bound = 2;
        bound.skinned = 3;
        const auto* const head = reinterpret_cast<RE::NiAVObject const*>(0x1000);
        const auto* const other = reinterpret_cast<RE::NiAVObject const*>(0x2000);
        bound.nodes.insert(head);

        PLUGIN_NAMESPACE::SkinPalette unbound;  // a figure the engine never rendered

        using PLUGIN_NAMESPACE::figure_slot_necessity;
        using PLUGIN_NAMESPACE::SlotNecessity;
        const bool palette_first =
            figure_slot_necessity("NPC Head [Head]", head, bound) == SlotNecessity::e_necessary &&
            figure_slot_necessity("WeaponDagger", other, bound) == SlotNecessity::e_weapon_attachment;
        const bool helper_default = figure_slot_necessity("NPC L Calf [Lcalf]", other, bound) == SlotNecessity::e_helper;
        const bool unbound_is_unknown =
            figure_slot_necessity("NPC Head [Head]", head, unbound) == SlotNecessity::e_unknown &&
            figure_slot_necessity("WeaponAxe", other, unbound) == SlotNecessity::e_weapon_attachment;
        // A slot whose name resolved to no node cannot be necessary: no palette in this tree can
        // reference a node that is not there. This is the unreachable case, and it must not read
        // anything out of the palette array to reach its verdict.
        const bool unresolved_never_necessary =
            figure_slot_necessity("NPC Head [Head]", nullptr, bound) == SlotNecessity::e_helper &&
            figure_slot_necessity("Ghost [Ghost]", nullptr, unbound) == SlotNecessity::e_unknown;
        return palette_first && helper_default && unbound_is_unknown && unresolved_never_necessary;
    }

    bool figure_bound_merges_spheres()
    {
        // Two spheres the union rule merges without a helper branch: second fully inside the first
        // keeps the first; disjoint ones grow the radius to cover both.
        PLUGIN_NAMESPACE::FigureBound out;
        out.center = { 0.0f, 0.0f, 0.0f };
        out.radius = 10.0f;
        out.valid = true;
        PLUGIN_NAMESPACE::FigureBound inner;
        inner.center = { 3.0f, 0.0f, 0.0f };
        inner.radius = 4.0f;   // 3 + 4 <= 10: contained
        PLUGIN_NAMESPACE::FigureBound outer;
        outer.center = { 30.0f, 0.0f, 0.0f };
        outer.radius = 5.0f;   // disjoint
        const auto merge = [](PLUGIN_NAMESPACE::FigureBound& a_out, PLUGIN_NAMESPACE::FigureBound const& a_other)
        {
            const float dx = a_other.center.x - a_out.center.x;
            const float dist = std::sqrt(dx * dx);
            if (a_out.radius >= a_other.radius + dist)
                return;
            if (a_other.radius >= a_out.radius + dist)
            {
                a_out.center = a_other.center;
                a_out.radius = a_other.radius;
                return;
            }
            const float merged = (dist + a_out.radius + a_other.radius) * 0.5f;
            const float t = dist > 1e-6f ? (merged - a_out.radius) / dist : 0.0f;
            a_out.center = a_out.center + (a_other.center - a_out.center) * t;
            a_out.radius = merged;
        };
        merge(out, inner);
        const bool contained_kept = out.center.x == 0.0f && out.radius == 10.0f;
        merge(out, outer);
        // merged radius = (dist + r_out + r_other) / 2 = (30 + 10 + 5) / 2 = 22.5,
        // center slides by t = (22.5 - 10) / 30 toward the outer sphere: 30 * t = 12.5.
        const bool disjoint_grew = std::abs(out.radius - 22.5f) <= 0.001f && std::abs(out.center.x - 12.5f) <= 0.001f;
        return contained_kept && disjoint_grew;
    }

    // AFP10-A, the puppet's assembly gate: "the shell may be moved away from the player" is the copy
    // route's fully-bound rule expressed on the shared census. The trap it must not fall into is
    // measuring binding against the DECLARED bone capacity - the engine allocates the instance
    // palette on demand, and waiting for the declared count is what cost the copy route 600 frames
    // while a 3BA skin rendered fine with 47 of 131 slots.
    bool assembly_gate_measures_allocated_slots()
    {
        PLUGIN_NAMESPACE::FigureAssembly census;
        const bool empty_is_not_ready = !census.assembled() && !census.bound_ready();

        census.geometries = 16;
        census.skinned = 12;
        census.roots = 2;
        census.bone_count = 131;      // declared by the nif
        census.alloc_slots = 47;      // what the engine actually allocated
        census.bound_nodes = 47;      // every allocated slot bound
        const bool allocated_is_ready = census.assembled() && census.bound_ready();
        // The declared count is not the bar: 47 of 131 is ready, and that is the whole point.
        const bool declared_is_not_the_bar = census.bound_nodes < census.bone_count && census.bound_ready();

        census.bound_nodes = 46;      // one allocated slot short
        const bool short_of_allocated = !census.bound_ready();
        census.bound_nodes = 47;
        census.alloc_slots = 0;       // no skin data anywhere: nothing to bind against
        const bool no_allocation = !census.bound_ready();

        // The settle identity moves when the mesh set moves and holds when it does not.
        PLUGIN_NAMESPACE::FigureAssembly same = census;
        const bool stable_signature = same.signature() == census.signature();
        ++same.geometries;
        const bool grew_signature = same.signature() != census.signature();

        return empty_is_not_ready && allocated_is_ready && declared_is_not_the_bar &&
            short_of_allocated && no_allocation && stable_signature && grew_signature;
    }
}

int main()
{
    const bool placement = snapshot_placement_preserves_subtree();
    const bool mapping = final_pose_discriminates_index_order();
    const bool framing = figure_world_delta_frames_the_figure();
    const bool ledger = coverage_ledger_reconciles();
    const bool necessity = slot_necessity_orders_exceptions();
    const bool bound = figure_bound_merges_spheres();
    const bool assembly = assembly_gate_measures_allocated_slots();
    const bool track_identity = animation_mapping_preserves_track_identity();
    const bool shape_clear = empty_source_clears_body_shape();
    const bool two_handed = two_handed_weapon_keeps_one_hand_state();
    const bool panel_pose = panel_pose_answers_standing_and_weapon_form();
    const bool oar_scope = oar_preview_scope_is_isolated();
    const bool oar_projection = oar_projection_scope_covers_every_clip();
    const bool oar_binary = oar_binary_profile_rejects_mismatches();
    const bool alpha_order = figure_transparency_draw_order();
    const bool alpha_state = figure_material_state_is_restored();
    const bool d3d_ranges = d3d_constant_buffer_ranges_are_restored();
    const bool query_profiles = oar_query_profiles_fail_closed();
    const bool standing_catalog = standing_catalog_does_not_use_live_or_replacement_names();
    const bool local_variants = preview_variants_are_local_and_bounded();
    const bool original_answers = preview_original_answers_suspend_projection();
    const bool panel_transitions = panel_transition_returns_to_a_standing_idle();
    const bool gear_refresh = equipment_refresh_waits_for_the_latest_assembly();
    const bool carrier_layout = preview_carrier_preserves_native_node_fields();
    const bool random_conditions = preview_random_conditions_cache_and_reset();
    const bool variant_weights = preview_variant_weights_and_shared_state();
    const bool play_once = preview_play_once_history_is_committed_on_start();
    const bool idle_recheck = idle_loops_respect_sequence_and_recheck_flags();
    const bool latest_weapon = latest_weapon_request_rejects_stale_loads();
    const bool idle_cycles = idle_layers_keep_independent_cycles();
    const bool blend_interrupt = interrupted_pose_blend_starts_from_the_visible_pose();
    const bool stance_contract = panel_animation_state_keeps_stance_and_transition_contract();
    const bool catalogue_reuse = project_name_catalogue_is_reused_across_sessions();
    std::printf("snapshot placement: %s\nfinal pose index order: %s\nr01 framing delta: %s\ncoverage ledger: %s\nslot necessity: %s\nfigure bound union: %s\nassembly gate: %s\nanimation track identity: %s\nempty source body shape: %s\n",
        placement ? "PASS" : "FAIL", mapping ? "PASS" : "FAIL", framing ? "PASS" : "FAIL",
        ledger ? "PASS" : "FAIL", necessity ? "PASS" : "FAIL", bound ? "PASS" : "FAIL", assembly ? "PASS" : "FAIL",
        track_identity ? "PASS" : "FAIL", shape_clear ? "PASS" : "FAIL");
    std::printf("OAR preview scope: %s\nOAR projection scope: %s\nOAR binary identity: %s\n",
        oar_scope ? "PASS" : "FAIL", oar_projection ? "PASS" : "FAIL", oar_binary ? "PASS" : "FAIL");
    std::printf("figure transparency order: %s\nfigure material state: %s\n", alpha_order ? "PASS" : "FAIL", alpha_state ? "PASS" : "FAIL");
    std::printf("D3D constant buffer ranges: %s\ntwo-handed hand state: %s\npanel pose answer: %s\n",
        d3d_ranges ? "PASS" : "FAIL", two_handed ? "PASS" : "FAIL", panel_pose ? "PASS" : "FAIL");
    std::printf("OAR query profile rejection: %s\nstanding animation catalog: %s\npreview variant isolation: %s\noriginal answer isolation: %s\npanel transition contract: %s\n",
        query_profiles ? "PASS" : "FAIL", standing_catalog ? "PASS" : "FAIL", local_variants ? "PASS" : "FAIL",
        original_answers ? "PASS" : "FAIL", panel_transitions ? "PASS" : "FAIL");
    std::printf("equipment assembly debounce: %s\n", gear_refresh ? "PASS" : "FAIL");
    std::printf("preview native carrier layout: %s\n", carrier_layout ? "PASS" : "FAIL");
    std::printf("preview Random cache: %s\nvariant weights and shared state: %s\nPlayOnce commit on start: %s\nidle loop decision flags: %s\nlatest weapon request: %s\nindependent idle cycles: %s\ninterrupted pose blend: %s\n",
        random_conditions ? "PASS" : "FAIL", variant_weights ? "PASS" : "FAIL", play_once ? "PASS" : "FAIL",
        idle_recheck ? "PASS" : "FAIL", latest_weapon ? "PASS" : "FAIL", idle_cycles ? "PASS" : "FAIL", blend_interrupt ? "PASS" : "FAIL");
    std::printf("stance and transition contract: %s\nproject catalogue reuse: %s\n",
        stance_contract ? "PASS" : "FAIL", catalogue_reuse ? "PASS" : "FAIL");
    return placement && mapping && framing && ledger && necessity && bound && assembly && track_identity && shape_clear &&
        two_handed && oar_scope && oar_projection && oar_binary && alpha_order && alpha_state && d3d_ranges && panel_pose &&
        query_profiles && standing_catalog && local_variants && original_answers && panel_transitions && gear_refresh && carrier_layout &&
        random_conditions && variant_weights && play_once && idle_recheck && latest_weapon && idle_cycles && blend_interrupt &&
        stance_contract && catalogue_reuse ? 0 : 1;
}
