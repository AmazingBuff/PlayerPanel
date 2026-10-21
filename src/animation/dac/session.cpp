//
// Created by AmazingBuff on 2026/10/20.
//
#include "animation/dac/session.h"
#include "animation/dac/native.h"
#include "animation/dac/session_core.h"
#include "RE/M/MagicCaster.h"
#include "RE/M/MenuOpenCloseEvent.h"
#include <mutex>
#include <chrono>

PLUGIN_NAMESPACE_BEGIN
namespace dac
{
class Session::State
{
public:
    explicit State(RE::Actor& actor) : source(actor.GetHandle()), id(actor.GetFormID()) {}
    RE::ActorHandle source;
    RE::FormID id;
    std::mutex mutex;
    SessionCore core;
    std::vector<std::uint32_t> controlled;
    IdlePreparation preparation;
    IdleGoal goal;
    EventTicket goal_ticket;
    std::chrono::steady_clock::time_point goal_started;
    std::uint64_t menu = 0;
    std::size_t signature = 0;
    std::atomic<std::uint64_t> revision{0};
    std::atomic_bool observation_queued{false};
    std::atomic_bool retired{false};
    std::unordered_set<std::string> rejected;
};
namespace
{
    std::mutex g_registry_mutex;
    std::vector<std::weak_ptr<Session::State>> g_sessions;
    std::atomic<std::uint64_t> g_world{1}, g_menu{0};
    std::atomic_bool g_open{false};

    std::vector<std::shared_ptr<Session::State>> sessions()
    {
        std::vector<std::shared_ptr<Session::State>> result;
        std::lock_guard lock(g_registry_mutex);
        std::erase_if(g_sessions, [](auto const& state) { return state.expired(); });
        for (auto const& weak : g_sessions)
            if (auto state = weak.lock()) result.push_back(std::move(state));
        return result;
    }
    bool current(std::uint64_t world, std::uint64_t menu)
    { return g_open.load() && g_world.load() == world && g_menu.load() == menu; }
    std::uint64_t session_epoch(Session::State& state)
    {
        std::lock_guard lock(state.mutex);
        return state.core.epoch();
    }
    void observe_goal(Session::State& state, RE::Actor& actor)
    {
        EventTicket ticket;
        IdleReadiness status;
        {
            std::lock_guard lock(state.mutex);
            ticket = state.goal_ticket;
            status = state.preparation.status(ticket.request);
            if (!state.core.accepts(ticket) || (status != IdleReadiness::e_pending && status != IdleReadiness::e_ready)) return;
        }
        const bool ready = idle_goal_ready(actor, state.goal);
        if (ready && status == IdleReadiness::e_ready) return;
        const bool expired = status == IdleReadiness::e_ready ||
            std::chrono::steady_clock::now() - state.goal_started > std::chrono::seconds(2);
        if (!ready && !expired) return;
        const std::string diagnostic = ready ? std::string{} : idle_goal_diagnostic(actor, state.goal);
        std::lock_guard lock(state.mutex);
        if (!state.core.accepts(ticket)) return;
        state.preparation.finish(ticket.request, ready);
        if (!ready)
            logger::info("DAC actor={:08x} request={} Smooth_loop goal markers diagnostic='{}'", state.id,
                ticket.request, diagnostic);
        logger::info("DAC actor={:08x} request={} Smooth_loop preparation {}", state.id, ticket.request, ready ? "ready" : status == IdleReadiness::e_ready ? "failed: goal markers changed" : "failed: marker observation timed out");
    }
    void observe_now(Session::State& state, RE::Actor& actor)
    {
        observe_goal(state, actor);
        const std::size_t signature = effect_signature(actor, state.controlled);
        if (signature != state.signature)
        {
            state.signature = signature;
            state.revision.fetch_add(1);
            logger::info("DAC actor={:08x} epoch={} native-effect revision={}", state.id, session_epoch(state), state.revision.load());
        }
    }
    bool begin(Session::State& state, std::uint64_t world, std::uint64_t menu)
    {
        if (!current(world, menu) || state.retired.load()) return false;
        {
            std::lock_guard lock(state.mutex);
            if (state.core.open()) return state.menu == menu;
        }
        auto const actor = state.source.get();
        auto const* rules = native_rules();
        if (!actor || !rules) return false;
        std::vector<std::uint32_t> scope = entry_scope(*actor, *rules);
        std::vector<EffectValue> snapshot = capture_effects(*actor, scope);
        const std::size_t count = snapshot.size();
        {
            std::lock_guard lock(state.mutex);
            if (!current(world, menu) || state.retired.load()) return false;
            state.controlled = std::move(scope);
            state.menu = menu;
            state.core.begin(std::move(snapshot));
        }
        state.signature = effect_signature(*actor, state.controlled);
        state.revision.fetch_add(1);
        logger::info("DAC snapshot actor={:08x} epoch={} effects={} scope={}", state.id, session_epoch(state), count, state.controlled.size());
        return true;
    }
    void close(Session::State& state, std::uint64_t world)
    {
        std::optional<Restoration> restoration;
        std::uint64_t epoch = 0;
        {
            std::lock_guard lock(state.mutex);
            restoration = state.core.close(); // invalidate tickets before any engine callback
            epoch = state.core.epoch();
            state.menu = 0;
        }
        if (!restoration || world != g_world.load()) return;
        const auto actor = state.source.get();
        const bool restored = actor && restore_effects(*actor, *restoration);
        state.revision.fetch_add(1);
        logger::info("DAC restore actor={:08x} epoch={} baseline={} spell-groups={} result={}", state.id, epoch,
            restoration->baseline.size(), restoration->spells.size(), restored ? "matched" : "MISMATCH");
    }
    class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(RE::MenuOpenCloseEvent const* event,
            RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
        {
            if (!event || event->menuName != RE::InventoryMenu::MENU_NAME) return RE::BSEventNotifyControl::kContinue;
            const std::uint64_t menu = g_menu.fetch_add(1) + 1;
            const std::uint64_t world = g_world.load();
            g_open.store(event->opening); // stop every old queued event before restoration
            if (event->opening)
            {
                for (auto const& state : sessions()) (void)begin(*state, world, menu);
            }
            else
                for (auto const& state : sessions()) close(*state, world);
            return RE::BSEventNotifyControl::kContinue;
        }
    };
}

void install()
{
    static MenuSink s_sink;
    if (RE::UI* ui = RE::UI::GetSingleton())
    {
        ui->AddEventSink(&s_sink);
        logger::info("DAC native inventory lifecycle sink installed");
    }
}
void reset_world()
{
    g_open.store(false);
    g_menu.fetch_add(1);
    g_world.fetch_add(1);
    for (auto const& state : sessions())
    {
        std::lock_guard lock(state->mutex);
        state->retired.store(true);
        state->core.reset(); // old records cannot reopen against an Actor from a new save
        state->menu = 0;
        state->revision.fetch_add(1);
    }
    logger::info("DAC old-world sessions discarded world={}", g_world.load());
}

Session::Session(RE::Actor& source) : m_state(std::make_shared<State>(source))
{
    {
        std::lock_guard lock(g_registry_mutex);
        g_sessions.push_back(m_state);
    }
    const std::uint64_t world = g_world.load(), menu = g_menu.load();
    SKSE::GetTaskInterface()->AddUITask([state = m_state, world, menu] { (void)begin(*state, world, menu); });
}
Session::~Session()
{
    m_state->retired.store(true);
    // Keep the transaction alive even when a display record retires between UI events.
    const std::uint64_t world = g_world.load();
    {
        std::lock_guard lock(m_state->mutex);
        m_state->core.request(UINT64_MAX);
    }
    SKSE::GetTaskInterface()->AddUITask([state = m_state, world] { close(*state, world); });
}
void Session::request(std::uint64_t generation)
{
    std::lock_guard lock(m_state->mutex);
    m_state->core.request(generation);
}
namespace
{
    bool dispatch_now(Session::State& state, std::uint64_t world, std::uint64_t menu,
        std::uint64_t generation, std::string const& event)
    {
        if (!current(world, menu) || state.retired.load() || !begin(state, world, menu)) return false;
        const auto actor = state.source.get();
        auto const* rules = native_rules();
        if (!actor || !rules) return false;
        std::vector<std::uint32_t> touched;
        std::string reason;
        if (!event_scope(*actor, event, *rules, state.controlled, touched, reason))
        {
            if (!reason.empty() && state.rejected.insert(event).second)
                logger::warn("DAC actor={:08x} event='{}' rejected: {}", state.id, event, reason);
            return false;
        }
        RE::MagicCaster* instant = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
        if (!instant) return false;
        {
            std::lock_guard lock(state.mutex);
            EventTicket ticket = state.core.ticket();
            ticket.request = generation;
            if (!current(world, menu) || state.retired.load() || !state.core.accepts(ticket)) return false;
            state.core.touch(touched);
        }
        const bool dual = instant->GetIsDualCasting();
        native_dispatch(*actor, event);
        instant->SetDualCasting(dual);
        observe_now(state, *actor);
        logger::info("DAC actor={:08x} epoch={} request={} event='{}' native call completed revision={}", state.id,
            session_epoch(state), generation, event, state.revision.load());
        return true;
    }
}
void Session::dispatch(std::string event, std::uint64_t generation)
{
    const std::uint64_t world = g_world.load(), menu = g_menu.load();
    SKSE::GetTaskInterface()->AddUITask([state = m_state, world, menu, generation, event = std::move(event)]
    { (void)dispatch_now(*state, world, menu, generation, event); });
}
void Session::prepare_idle(std::uint64_t generation, bool retain_ready)
{
    IdleWork work;
    {
        std::lock_guard lock(m_state->mutex);
        work = m_state->preparation.start(generation, retain_ready);
    }
    if (work == IdleWork::e_none) return;
    logger::info("DAC actor={:08x} request={} Smooth_loop preparation requested", m_state->id, generation);
    const std::uint64_t world = g_world.load(), menu = g_menu.load();
    SKSE::GetTaskInterface()->AddUITask([state = m_state, world, menu, generation, work]
    {
        if (!current(world, menu) || state->retired.load()) return;
        bool prepared = begin(*state, world, menu);
        IdleGoal goal;
        std::string reason;
        auto const* rules = native_rules();
        prepared = prepared && rules && smooth_idle_goal(*rules, goal, reason);
        if (prepared)
        {
            std::lock_guard lock(state->mutex);
            EventTicket ticket = state->core.ticket();
            ticket.request = generation;
            if (!state->core.accepts(ticket)) return;
            state->goal = std::move(goal);
            state->goal_ticket = ticket;
            state->goal_started = std::chrono::steady_clock::now();
        }
        if (prepared && work == IdleWork::e_dispatch)
            prepared = dispatch_now(*state, world, menu, generation, "Smooth_loop");
        else if (prepared)
        {
            const auto actor = state->source.get();
            if (actor) observe_now(*state, *actor);
        }
        if (!prepared)
        {
            std::lock_guard lock(state->mutex);
            state->preparation.finish(generation, false);
            logger::warn("DAC actor={:08x} request={} Smooth_loop preparation failed: {}", state->id, generation,
                reason.empty() ? "native session/tag unavailable or rejected" : reason);
        }
    });
}
IdleReadiness Session::idle_readiness(std::uint64_t generation) const
{
    std::lock_guard lock(m_state->mutex);
    if (!g_open.load() || m_state->retired.load()) return IdleReadiness::e_failed;
    return m_state->preparation.status(generation);
}
void Session::observe()
{
    if (m_state->observation_queued.exchange(true)) return;
    const std::uint64_t world = g_world.load(), menu = g_menu.load();
    SKSE::GetTaskInterface()->AddUITask([state = m_state, world, menu]
    {
        state->observation_queued.store(false);
        if (!current(world, menu) || !begin(*state, world, menu)) return;
        const auto actor = state->source.get();
        if (actor) observe_now(*state, *actor);
    });
}
std::uint64_t Session::revision() const { return m_state->revision.load(); }
std::string Session::smooth_markers() const
{
    const auto actor = m_state->source.get();
    return actor ? smooth_marker_report(*actor) : std::string{};
}
}
PLUGIN_NAMESPACE_END
