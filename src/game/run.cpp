#include "game/run.hpp"

#include "game/world.hpp"

#include <algorithm>

void Run::start(World& world, const RunConfig& config, std::uint32_t runId) {
    *this = Run{};
    config_ = config;
    world.setRun(runId);
    world.populate(config.competitors);
    world.spawnPlayer(config.founder, {0.5f * world.width(), 0.5f * world.height()}, &config.genome);
    lineageGenome_ = config.genome;
    census_ = world.census();
}

void Run::update(World& world, float dt) {
    if (state_ == State::Lost)
        return;
    const std::vector<WorldEvent> events = world.takeEvents();
    if (!events.empty())
        census_ = world.census(); // el HUD refleja la división o la muerte en el acto
    for (const WorldEvent& ev : events) {
        switch (ev.type) {
        case WorldEvent::Type::PlayerDivided:
            ++divisions_;
            candidates_ = {ev.a, ev.b};
            selected_ = 0;
            state_ = State::ChoosingDaughter;
            break;
        case WorldEvent::Type::PlayerConjugated:
            ++conjugations_;
            break;
        case WorldEvent::Type::PlayerDied:
            deathCause_ = ev.cause;
            candidates_ = world.lineageMembers();
            selected_ = 0;
            state_ = candidates_.empty() ? State::Lost : State::ChoosingHeir;
            break;
        }
    }
    if (simulationPaused())
        return;

    elapsed_ += dt;
    censusTimer_ -= dt;
    if (censusTimer_ <= 0.0f) { // el censo recorre todas las células: una vez por segundo
        censusTimer_ = 1.0f;
        census_ = world.census();
        if (const Heredity* h = world.registry().try_get<Heredity>(world.player()))
            lineageGenome_ = h->genome;
        bestShare_ = std::max(bestShare_, census_.share());
        bestGeneration_ = std::max(bestGeneration_, census_.maxGeneration);
        if (!won_ && census_.share() >= config_.targetShare)
            won_ = justWon_ = true;
        // El jugador pudo morir de otra forma (por ejemplo, fagocitado) sin dejar herederos.
        if (census_.lineageCells == 0)
            state_ = State::Lost;
    }
}

void Run::cycle(int step) {
    if (candidates_.empty())
        return;
    const auto n = static_cast<int>(candidates_.size());
    selected_ = static_cast<std::size_t>(((static_cast<int>(selected_) + step) % n + n) % n);
}

void Run::select(entt::entity cell) {
    const auto it = std::find(candidates_.begin(), candidates_.end(), cell);
    if (it != candidates_.end())
        selected_ = static_cast<std::size_t>(it - candidates_.begin());
}

void Run::confirm(World& world) {
    if (candidates_.empty())
        return;
    world.setPlayer(candidates_[selected_]);
    if (const Heredity* h = world.registry().try_get<Heredity>(world.player()))
        lineageGenome_ = h->genome;
    candidates_.clear();
    state_ = State::Playing;
}
