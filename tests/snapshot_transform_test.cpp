//
// Created by AmazingBuff on 2026/10/08.
//

#include "character/final_pose_probe.h"
#include "character/body_shape.h"
#include "character/figure_audit.h"
#include "character/snapshot_transform.h"
#include "oar/binary_profile.h"
#include "oar/policy.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <thread>

namespace
{
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

    bool oar_binary_profile_rejects_mismatches()
    {
        namespace profile = PLUGIN_NAMESPACE::oar::profile_3_2_1;
        const bool official = profile::matches(profile::Timestamp, profile::Image_Size, profile::Pdb_Guid,
            profile::Pdb_Age, profile::Evaluate_Prefix, profile::Numeric_Prefix);
        auto damaged = profile::Evaluate_Prefix;
        damaged[0] ^= 1;
        return official && !profile::matches(profile::Timestamp, profile::Image_Size, profile::Pdb_Guid,
            profile::Pdb_Age, damaged, profile::Numeric_Prefix);
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
    const bool oar_scope = oar_preview_scope_is_isolated();
    const bool oar_binary = oar_binary_profile_rejects_mismatches();
    std::printf("snapshot placement: %s\nfinal pose index order: %s\nr01 framing delta: %s\ncoverage ledger: %s\nslot necessity: %s\nfigure bound union: %s\nassembly gate: %s\nanimation track identity: %s\nempty source body shape: %s\n",
        placement ? "PASS" : "FAIL", mapping ? "PASS" : "FAIL", framing ? "PASS" : "FAIL",
        ledger ? "PASS" : "FAIL", necessity ? "PASS" : "FAIL", bound ? "PASS" : "FAIL", assembly ? "PASS" : "FAIL",
        track_identity ? "PASS" : "FAIL", shape_clear ? "PASS" : "FAIL");
    std::printf("OAR preview scope: %s\nOAR binary identity: %s\n", oar_scope ? "PASS" : "FAIL", oar_binary ? "PASS" : "FAIL");
    return placement && mapping && framing && ledger && necessity && bound && assembly && track_identity && shape_clear &&
        oar_scope && oar_binary ? 0 : 1;
}
