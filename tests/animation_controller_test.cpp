//
// Created by AmazingBuff on 2026/10/19.
//
#ifndef PLUGIN_NAMESPACE_BEGIN
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#endif
#include "animation/sequence.h"
#include "animation/variant_state.h"
#include <cassert>
#include <iostream>

using namespace test_plugin;

void equip_phase_hands_over_to_complete_descriptor_sequence()
{
    oar::AnimationSelection stable;
    stable.found = true;
    stable.index = stable.base_index = 1;
    stable.name = stable.base_name = "Animations\\MT_Idle.hkx";
    oar::AnimationSelection anchor = stable;
    anchor.index = 42;
    anchor.name = "Animations\\OpenAnimationReplacer\\pack\\Sword idle Start\\1hm_idle.hkx";
    // Hash deduplication registered the Loop under another name; descriptors retain both paths.
    anchor.candidates = {
        { 42, anchor.name, "alias.hkx" },
        { 7, "Animations\\OpenAnimationReplacer\\pack\\Sword idle Loop\\1hm_idle.hkx", "other-pack.hkx" }
    };
    std::string diagnostic;
    std::vector<animation::Stage> stages = animation::idle_sequence(anchor, stable, animation::WeaponForm::kOneHanded, diagnostic);
    assert(stages.size() == 2 && stages[1].clip.index == 7);
    auto equip = animation::direct_selection(stable, 8, "Animations\\1HM_Equip.hkx");
    stages.insert(stages.begin(), { { animation::ActionKind::e_equip, animation::WeaponForm::kOneHanded, false }, equip, 0.75 });
    animation::PlaybackTimeline playback;
    playback.start(stages);
    assert(playback.seconds() == 0.75);
    assert(!playback.advance(0.2, 1.0));
    assert(playback.advance(0.1, 1.0));
    assert(playback.current().action.kind == animation::ActionKind::e_weapon_idle_start && playback.seconds() == 0.0);
    assert(playback.advance(1.0, 1.0));
    assert(playback.current().clip.index == 7 && playback.current().action.loop && playback.seconds() == 0.0);
    assert(!playback.advance(3.0, 1.0));
    // A missing explicit Loop must hand over to the stable idle, never dwell in Start.
    anchor.candidates.resize(1);
    stages = animation::idle_sequence(anchor, stable, animation::WeaponForm::kOneHanded, diagnostic);
    assert(!diagnostic.empty() && stages.back().action.kind == animation::ActionKind::e_standing_idle);
    assert(!stages.front().action.loop && stages.back().clip.index == stable.index);
}

void latest_actor_request_preserves_other_actor_playback_and_masked_pose()
{
    animation::RequestGeneration actor_a;
    animation::RequestGeneration actor_b;
    const auto stale_a = actor_a.next();
    const auto current_b = actor_b.next();
    const auto newest_a = actor_a.next();
    assert(!actor_a.accepts(stale_a) && actor_a.accepts(newest_a) && actor_b.accepts(current_b));
    oar::AnimationSelection idle;
    idle.found = true;
    idle.index = 3;
    animation::PlaybackTimeline playback_b;
    playback_b.start({ { { animation::ActionKind::e_weapon_idle_loop, animation::WeaponForm::kOneHanded, true }, idle, 0.4 } });
    assert(!playback_b.advance(0.2, 1.0));
    const double phase_b = playback_b.seconds();
    animation::PlaybackTimeline playback_a;
    const std::vector<animation::Stage> pending_a{
        { { animation::ActionKind::e_equip, animation::WeaponForm::kOneHanded, false }, idle, 0.5 },
        { { animation::ActionKind::e_weapon_idle_loop, animation::WeaponForm::kOneHanded, true }, idle, 0.0 }
    };
    playback_a.start(pending_a);
    assert(playback_a.advance(0.6, 1.0));
    assert(!playback_a.advance(0.3, 1.0));
    // The old proposal finished loading after A handed over; acceptance follows its current phase.
    auto accepted_a = pending_a;
    const std::size_t skipped = animation::resume_continuation(accepted_a, playback_a);
    assert(skipped == 1 && accepted_a.front().action.loop && accepted_a.front().start_seconds == playback_a.seconds());
    oar::ConditionRandomState random_a(17), random_b(23);
    const oar::StateKey key{ oar::DataScope::e_local, 3, 0 };
    const auto before_b = random_b.random(key, 4, 3, true, 0.0f, 1.0f);
    (void)random_a.random(key, 4, 3, true, 0.0f, 1.0f);
    random_a.loop(3);
    (void)actor_a.next();
    assert(playback_b.seconds() == phase_b && random_b.random(key, 4, 3, true, 0.0f, 1.0f) == before_b);
    std::unordered_map<std::string, float> pose_a, pose_b;
    const std::vector<std::pair<std::string, float>> body{ { "NPC Pelvis [Pelv]", 10.0f }, { "NPC L Hand [LHnd]", 2.0f } };
    const std::vector<std::pair<std::string, float>> hand{ { "NPC Pelvis [Pelv]", 99.0f }, { "NPC L Hand [LHnd]", 5.0f } };
    animation::compose_contributor(pose_b, std::span(body), animation::BoneMask::e_full_body);
    animation::compose_contributor(pose_a, std::span(body), animation::BoneMask::e_full_body);
    animation::compose_contributor(pose_a, std::span(hand), animation::BoneMask::e_left_hand);
    assert(pose_a.at("NPC Pelvis [Pelv]") == 10.0f && pose_a.at("NPC L Hand [LHnd]") == 5.0f);
    assert(pose_b.at("NPC L Hand [LHnd]") == 2.0f);
}

int main()
{
    equip_phase_hands_over_to_complete_descriptor_sequence();
    latest_actor_request_preserves_other_actor_playback_and_masked_pose();
    std::cout << "2 animation behavior cases passed\n";
}
