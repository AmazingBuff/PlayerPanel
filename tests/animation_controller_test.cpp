//
// Created by AmazingBuff on 2026/10/20.
//
#define PLUGIN_NAMESPACE_BEGIN namespace test_plugin {
#define PLUGIN_NAMESPACE_END }
#include "animation/event_clock.h"
#include "dac/session_core.h"
#include <cassert>
#include <iostream>

using namespace test_plugin;
namespace
{
    struct FakeEngine
    {
        std::vector<dac::EffectValue> effects;
        int equipment = 7;
        int restored = 0;
        std::vector<dac::EffectValue> capture() const
        {
            std::vector<dac::EffectValue> result;
            for (auto const& effect : effects) if (effect.spell < 100) result.push_back(effect);
            return result;
        }
        void native_phase()
        {
            std::erase_if(effects, [](auto const& effect) { return effect.spell == 1; });
            effects.push_back({ .spell = 2, .effect = 22 });
        }
        void restore(dac::Restoration const& transaction)
        {
            ++restored;
            std::erase_if(effects, [&](auto const& effect)
            { return std::ranges::find(transaction.spells, effect.spell) != transaction.spells.end(); });
            effects.insert(effects.end(), transaction.baseline.begin(), transaction.baseline.end());
        }
    };
}

void entry_snapshot_restores_native_phase_and_preserves_equipment()
{
    const dac::EffectValue entry{ .spell=1, .effect=11, .caster=5, .source=6, .casting_source=3,
        .inactive=true, .magnitude=0.0f, .duration=30.0f, .elapsed=4.5f, .dual=true };
    FakeEngine actor{ { entry, { .spell=101, .effect=1001 } } };
    dac::SessionCore session;
    session.request(1);
    session.begin(actor.capture());
    const dac::EventTicket ticket = session.ticket();
    animation::EventClock clock;
    clock.start({ {0.0,"entry"}, {2.5,"phase"}, {3.333333,"end"} },3.333333,0.0,false);
    int events=0;
    const auto event = [&](animation::Annotation const& annotation)
    {
        ++events;
        if (annotation.event=="phase" && session.accepts(ticket))
        {
            session.touch({1,2}); // native literal spell plus keyword-affected original group
            actor.native_phase();
        }
    };
    clock.advance(2.5,true,[](double){},event,[]{});
    assert(events==2 && actor.capture().front().spell==2 && clock.seconds()==2.5);
    actor.equipment=9;
    session.begin(actor.capture()); // appearance/retarget cannot replace the immutable entry
    const auto closing=session.close();
    assert(closing && !session.accepts(ticket));
    actor.restore(*closing);
    assert(actor.capture()==std::vector<dac::EffectValue>{entry});
    assert(actor.effects.front().spell==101 && actor.equipment==9 && actor.restored==1);
    assert(!session.close());
}

void stale_events_and_old_worlds_cannot_restore_over_new_sessions()
{
    FakeEngine actor_a{{{.spell=1,.effect=11}}}, actor_b{{{.spell=3,.effect=33}}};
    dac::SessionCore first,second;
    first.request(1); second.request(1);
    first.begin(actor_a.capture()); second.begin(actor_b.capture());
    const auto old=first.ticket(), other=second.ticket();
    actor_a.restore(*first.close());
    first.begin(actor_a.capture()); first.request(2);
    assert(!first.accepts(old) && second.accepts(other));
    const auto obsolete_request=first.ticket();
    first.request(3);
    assert(!first.accepts(obsolete_request));
    const auto current=first.ticket();
    animation::EventClock resumed;
    resumed.start({{0.1,"past"},{2.5,"future"}},3.333333,0.5,true);
    int events=0;
    resumed.advance(2.0,false,[](double){},[&](animation::Annotation const& event)
    {
        ++events;
        assert(event.event=="future");
        if(first.accepts(current)) { first.touch({1,2}); actor_a.native_phase(); }
    },[]{});
    assert(events==1 && actor_a.capture().front().spell==2);
    first.reset(); // PreLoadGame/NewGame discards, never restores old values
    assert(!first.accepts(current) && !first.close() && actor_a.restored==1);
    assert(second.accepts(other) && actor_b.capture().front().spell==3 && actor_b.restored==0);
    second.touch({3}); actor_b.restore(*second.close());
    assert(actor_b.capture().front().effect==33 && actor_b.restored==1);
}
int main()
{
    entry_snapshot_restores_native_phase_and_preserves_equipment();
    stale_events_and_old_worlds_cannot_restore_over_new_sessions();
    std::cout<<"2 native-session transaction/event cases passed\n";
}
