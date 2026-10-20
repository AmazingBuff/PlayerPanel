//
// Created by AmazingBuff on 2026/10/20.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/idle_policy.h"
#include "dac/idle_goal.h"
#include "dac/session_core.h"
#include <array>
#include <cassert>
#include <iostream>
using namespace test_plugin;
namespace
{
    struct FakeActor
    {
        std::vector<dac::EffectValue> effects;
        int equipment = 7;
        bool drawn = false;
        std::string world_clip = "1HM_Equip";
        void native_loop()
        {
            std::erase_if(effects, [](dac::EffectValue const& effect) { return effect.spell == 1; });
            effects.push_back({ .spell = 2, .effect = 22 });
        }
        bool ready() const
        {
            return std::ranges::any_of(effects, [](dac::EffectValue const& effect) { return effect.effect == 22; }) &&
                std::ranges::none_of(effects, [](dac::EffectValue const& effect) { return effect.effect == 11; });
        }
        void restore(dac::Restoration const& restoration)
        {
            std::erase_if(effects, [&](dac::EffectValue const& effect)
            { return std::ranges::find(restoration.spells, effect.spell) != restoration.spells.end(); });
            effects.insert(effects.end(), restoration.baseline.begin(), restoration.baseline.end());
        }
    };
}
void default_equipment_idle_waits_for_native_goal_and_keeps_entry_snapshot()
{
    const auto setting = animation::parse_unarmed_idle("non_combat_loop").value();
    const auto target = animation::idle_target(animation::weapon_form_from_hands(
        animation::HandKind::kOneHandedWeapon, animation::HandKind::kShield), false, setting);
    FakeActor actor{{{.spell = 1, .effect = 11, .elapsed = 4.5f}}};
    const auto baseline = actor.effects;
    assert(!target.direct && target.drawn() && target.form == animation::WeaponForm::kOneHanded);
    assert(target.bases().front() == "Animations\\1hm_idle.hkx");
    dac::SessionCore session;
    dac::IdlePreparation goal;
    session.request(1);
    session.begin(actor.effects);
    const auto old = session.ticket();
    assert(goal.start(1, false) == dac::IdleWork::e_dispatch);
    assert(goal.status(1) == dac::IdleReadiness::e_pending && !actor.ready());
    session.request(2);
    assert(goal.start(2, false) == dac::IdleWork::e_dispatch && !session.accepts(old));
    goal.finish(1, true);
    assert(goal.status(2) == dac::IdleReadiness::e_pending);
    session.touch({1, 2});
    actor.native_loop();
    goal.finish(2, actor.ready());
    assert(goal.status(2) == dac::IdleReadiness::e_ready);
    const auto empty = animation::idle_target(animation::WeaponForm::kNone, false, setting);
    assert(empty.empty() && !empty.direct && empty.drawn() && empty.bases().front() == "Animations\\h2h_idle.hkx");
    session.begin(actor.effects);
    session.request(3);
    assert(goal.start(3, true) == dac::IdleWork::e_observe && goal.status(3) == dac::IdleReadiness::e_pending);
    goal.finish(3, actor.ready());
    assert(goal.status(3) == dac::IdleReadiness::e_ready);
    goal.finish(3, false);
    assert(goal.status(3) == dac::IdleReadiness::e_failed);
    actor.restore(*session.close());
    assert(actor.effects == baseline && actor.equipment == 7 && !actor.drawn && actor.world_clip == "1HM_Equip");
}
void direct_empty_idle_rejects_old_preparation_after_request_and_close()
{
    const auto setting = animation::parse_unarmed_idle("mt_idle").value();
    const auto target = animation::idle_target(animation::WeaponForm::kNone, false, setting);
    assert(target.direct && !target.drawn());
    assert(!animation::parse_unarmed_idle("unknown"));
    dac::SessionCore session;
    dac::IdlePreparation goal;
    FakeActor actor{{{.spell = 1, .effect = 11}}};
    session.request(1); session.begin(actor.effects);
    const auto queued = session.ticket();
    assert(goal.start(1, false) == dac::IdleWork::e_dispatch);
    session.request(2);
    int native_calls = 0;
    if (session.accepts(queued)) { ++native_calls; actor.native_loop(); }
    const std::string visible(target.bases().front());
    const std::string calibration = "Animations\\MT_Idle.hkx";
    assert(visible == calibration && native_calls == 0 && actor.effects.front().effect == 11);
    (void)session.close();
    assert(!session.accepts(queued));
    if (session.accepts(queued)) { ++native_calls; actor.native_loop(); }
    assert(native_calls == 0 && target.direct);
}
int main()
{
    default_equipment_idle_waits_for_native_goal_and_keeps_entry_snapshot();
    direct_empty_idle_rejects_old_preparation_after_request_and_close();
    std::cout << "2 panel idle policy/native preparation cases passed\n";
}
