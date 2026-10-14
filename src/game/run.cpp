#include "game/run.hpp"

#include "game/world.hpp"

void Run::start(World& world, const RunConfig& config, std::uint32_t runId) {
    *this = Run{};
    config_ = config;
    world.setRun(runId);
    world.populateEcosystem(config.wildlife);
    world.populate(config.competitors);
    if (config.spawnFounder)
        world.spawnPlayer(config.founder, {0.5f * world.width(), 0.5f * world.height()}, &config.genome);
    census_ = world.census();
}

void Run::update(World& world, float dt) {
    const std::vector<WorldEvent> events = world.takeEvents();
    if (!events.empty())
        census_ = world.census(); // el panel refleja la división o la muerte en el acto
    for (const WorldEvent& ev : events) {
        switch (ev.type) {
        case WorldEvent::Type::PlayerDivided: ++divisions_; break; // el ser sigue con la madre
        case WorldEvent::Type::PlayerConjugated: ++conjugations_; break;
        case WorldEvent::Type::PlayerDied: deathCause_ = ev.cause; break; // se puede controlar otro ser
        }
    }
    elapsed_ += dt;
    censusTimer_ -= dt;
    if (censusTimer_ <= 0.0f) { // el censo recorre todas las células: una vez por segundo
        censusTimer_ = 1.0f;
        census_ = world.census();
    }
}
