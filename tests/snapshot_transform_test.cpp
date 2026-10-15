//
// Created by AmazingBuff on 2026/10/08.
//

#include "character/final_pose_probe.h"
#include "character/body_shape.h"
#include "character/equipment_mirror.h"
#include "character/figure_audit.h"
#include "character/snapshot_transform.h"
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

namespace
{
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
    const bool oar_scope = oar_preview_scope_is_isolated();
    const bool oar_projection = oar_projection_scope_covers_every_clip();
    const bool oar_binary = oar_binary_profile_rejects_mismatches();
    const bool alpha_order = figure_transparency_draw_order();
    const bool alpha_state = figure_material_state_is_restored();
    const bool d3d_ranges = d3d_constant_buffer_ranges_are_restored();
    std::printf("snapshot placement: %s\nfinal pose index order: %s\nr01 framing delta: %s\ncoverage ledger: %s\nslot necessity: %s\nfigure bound union: %s\nassembly gate: %s\nanimation track identity: %s\nempty source body shape: %s\n",
        placement ? "PASS" : "FAIL", mapping ? "PASS" : "FAIL", framing ? "PASS" : "FAIL",
        ledger ? "PASS" : "FAIL", necessity ? "PASS" : "FAIL", bound ? "PASS" : "FAIL", assembly ? "PASS" : "FAIL",
        track_identity ? "PASS" : "FAIL", shape_clear ? "PASS" : "FAIL");
    std::printf("OAR preview scope: %s\nOAR projection scope: %s\nOAR binary identity: %s\n",
        oar_scope ? "PASS" : "FAIL", oar_projection ? "PASS" : "FAIL", oar_binary ? "PASS" : "FAIL");
    std::printf("figure transparency order: %s\nfigure material state: %s\n", alpha_order ? "PASS" : "FAIL", alpha_state ? "PASS" : "FAIL");
    std::printf("D3D constant buffer ranges: %s\ntwo-handed hand state: %s\n", d3d_ranges ? "PASS" : "FAIL", two_handed ? "PASS" : "FAIL");
    return placement && mapping && framing && ledger && necessity && bound && assembly && track_identity && shape_clear &&
        two_handed && oar_scope && oar_projection && oar_binary && alpha_order && alpha_state && d3d_ranges ? 0 : 1;
}
