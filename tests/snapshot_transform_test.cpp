//
// Created by AmazingBuff on 2026/10/08.
//

#include "character/figure_audit.h"
#include "character/snapshot_transform.h"
#include "animation/pose.h"
#include "animation/clip.h"
#include "animation/action.h"
#include "animation/variant_state.h"
#include "base_animation_cache_cases.h"
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
    namespace animation = PLUGIN_NAMESPACE::animation;
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


    bool preview_variants_are_local_and_bounded()
    {
        namespace oar = PLUGIN_NAMESPACE::oar;
        animation::SelectionState preview(123);
        animation::SelectionState other(123);
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
        animation::SelectionState preview(123);
        animation::SelectionState other(123);
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
        animation::SelectionState preview(123);
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
        animation::SelectionState shared(123);
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
        animation::SelectionState preview(123);
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
        animation::SelectionState preview(123);
        oar::VariantOptions options;
        options.key = { oar::DataScope::e_local, 1, 10 };
        options.sequential = true;
        options.sequence = { { 11 }, { 12 }, { 13 } };
        if (animation::idle_loop_decision(true, true, preview.in_sequence(options, 1)) != animation::LoopDecision::e_recheck)
            return false;
        for (int i = 0; i < 2; ++i)
        {
            const auto pick = preview.variant(options, 1);
            if (!pick)
                return false;
            preview.commit(options, *pick, 1);
            if (animation::idle_loop_decision(true, true, preview.in_sequence(options, 1)) != animation::LoopDecision::e_continue_variants)
                return false;
        }
        preview.commit(options, *preview.variant(options, 1), 1);
        return !preview.in_sequence(options, 1) &&
            animation::idle_loop_decision(false, true, false) == animation::LoopDecision::e_continue_variants &&
            animation::idle_loop_decision(false, false, false) == animation::LoopDecision::e_keep;
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
        return covers_standing_body(body) && !covers_standing_body(std::span(body).first(6)) &&
            panel_idle_bases(animation::WeaponForm::kBow) != panel_idle_bases(animation::WeaponForm::kTwoHanded) &&
            panel_idle_bases(animation::WeaponForm::kCrossbow) != panel_idle_bases(animation::WeaponForm::kBow) &&
            animation::weapon_form_from_hands(animation::HandKind::kBow, animation::HandKind::kEmpty) == animation::WeaponForm::kBow &&
            animation::weapon_form_from_hands(animation::HandKind::kEmpty, animation::HandKind::kCrossbow) == animation::WeaponForm::kCrossbow;
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
        using HK = animation::HandKind;
        const bool forms_split =
            animation::weapon_form_from_hands(HK::kTwoHandedWeapon, HK::kEmpty) == animation::WeaponForm::kTwoHanded &&
            animation::weapon_form_from_hands(HK::kEmpty, HK::kTwoHandedWeapon) == animation::WeaponForm::kTwoHanded &&
            animation::weapon_form_from_hands(HK::kOneHandedWeapon, HK::kOneHandedWeapon) == animation::WeaponForm::kDualWield &&
            animation::weapon_form_from_hands(HK::kOneHandedWeapon, HK::kShield) == animation::WeaponForm::kOneHanded &&
            animation::weapon_form_from_hands(HK::kOneHandedWeapon, HK::kSpell) == animation::WeaponForm::kOneHanded &&
            animation::weapon_form_from_hands(HK::kSpell, HK::kSpell) == animation::WeaponForm::kMagic &&
            animation::weapon_form_from_hands(HK::kStaff, HK::kEmpty) == animation::WeaponForm::kStaff &&
            animation::weapon_form_from_hands(HK::kTorch, HK::kEmpty) == animation::WeaponForm::kTorch &&
            animation::weapon_form_from_hands(HK::kEmpty, HK::kEmpty) == animation::WeaponForm::kNone;
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



    // AFP9c, the R01 framing rule: the figure sits parked ~8000 units below the player with the
    // world still at scale 1, and the delta must move that whole frame onto the studio anchor at the
    // studio scale. This is the assertion whose absence let a figure ship drawn far outside the
    // frustum: the delta is applied to the LIVE tree's worlds, never to the root's local, because a
    // parented node's downward pass does not move the node itself.


    // AFP9 package A, decision rules only (no engine memory here): the coverage ledger must
    // reconcile, and the necessity classifier must order its exceptions the way the audit logs
    // them - palette first, then weapon attachment, then helper, then unknown.






    // AFP10-A, the puppet's assembly gate: "the shell may be moved away from the player" is the copy
    // route's fully-bound rule expressed on the shared census. The trap it must not fall into is
    // measuring binding against the DECLARED bone capacity - the engine allocates the instance
    // palette on demand, and waiting for the declared count is what cost the copy route 600 frames
    // while a 3BA skin rendered fine with 47 of 131 slots.


    // Round 16: a mid-sequence landing (seek_dwell) must survive playback acceptance. commit()
    // advanced "next" past the picked member, so the next loop re-check replayed the entry move
    // (the Start/Loop alternation). The dwell accept path pins the member instead; an activation
    // still restarts the sequence at its first member, for non-local scopes too.
    bool preview_dwell_survives_accept_and_reactivation()
    {
        namespace oar = PLUGIN_NAMESPACE::oar;
        animation::SelectionState preview(123);
        oar::VariantOptions options;
        options.key = { oar::DataScope::e_submod, 20 };
        options.sequential = true;
        options.share_history = true;
        options.sequence = { { 11, true }, { 12, true } };  // all PlayOnce: the pack that defeated the search
        const oar::VariantPick dwell{ 12, 1 };
        preview.seek_dwell(options.key, 1, dwell);
        preview.dwell(options, dwell, 1);
        const auto first = preview.variant(options, 1);
        if (!first || first->index != 12 || first->position != 1)
            return false;
        // A second dwell accept (the loop re-check's same choice) still lands on the same member.
        preview.dwell(options, *first, 1);
        const auto second = preview.variant(options, 1);
        if (!second || second->index != 12)
            return false;
        // An activation forgets the progress for non-local scopes too and restarts at member 0.
        preview.activate(1);
        const auto restarted = preview.variant(options, 1);
        return restarted && restarted->index == 11;
    }


}

int main()
{
    bool passed = true;
    passed = preview_carrier_preserves_native_node_fields() && passed;
    passed = equipment_refresh_waits_for_the_latest_assembly() && passed;
    passed = oar_query_profiles_fail_closed() && passed;
    passed = PLUGIN_NAMESPACE::tests::fixed_bases_resolve_once_across_menu_sessions() && passed;
    passed = preview_variants_are_local_and_bounded() && passed;
    passed = preview_random_conditions_cache_and_reset() && passed;
    passed = preview_variant_weights_and_shared_state() && passed;
    passed = preview_play_once_history_is_committed_on_start() && passed;
    passed = idle_loops_respect_sequence_and_recheck_flags() && passed;
    passed = interrupted_pose_blend_starts_from_the_visible_pose() && passed;
    passed = preview_original_answers_suspend_projection() && passed;
    passed = panel_transition_returns_to_a_standing_idle() && passed;
    passed = PLUGIN_NAMESPACE::tests::project_replacement_and_load_reset_refresh_sparse_indices() && passed;
    passed = d3d_constant_buffer_ranges_are_restored() && passed;
    passed = figure_transparency_draw_order() && passed;
    passed = figure_material_state_is_restored() && passed;
    passed = oar_preview_scope_is_isolated() && passed;
    passed = oar_projection_scope_covers_every_clip() && passed;
    passed = panel_pose_answers_standing_and_weapon_form() && passed;
    passed = oar_binary_profile_rejects_mismatches() && passed;
    passed = animation_mapping_preserves_track_identity() && passed;
    passed = snapshot_placement_preserves_subtree() && passed;
    passed = preview_dwell_survives_accept_and_reactivation() && passed;
    std::printf("SceneGraph retained checks: %s\n", passed ? "PASS" : "FAIL");
    return passed ? 0 : 1;
}
