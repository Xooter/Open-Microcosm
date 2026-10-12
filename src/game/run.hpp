#pragma once

#include "bio/genome.hpp"
#include "bio/locomotion.hpp"
#include "game/lineage.hpp"

#include <entt/entity/entity.hpp>

#include <cstdint>
#include <vector>

class World;

struct RunConfig {
    LocomotionType founder = LocomotionType::Cilia;
    std::uint64_t seed = 1;
    float targetShare = 0.5f; // ganar: el linaje suma esta fracción de la biomasa viva
    int competitors = 24;     // células pasivas que compiten por la biomasa (hasta la fase 7)
    Genome genome = speciesGenome(LocomotionType::Cilia); // genoma del fundador
};

// Una partida roguelite: una gota, un linaje.
//  - Al dividirse, el jugador elige con qué hija seguir.
//  - Al morir, elige un descendiente vivo. Si no queda ninguno, el linaje se extinguió.
//  - Gana cuando su linaje domina la biomasa de la gota (puede seguir jugando).
class Run {
  public:
    enum class State { Playing, ChoosingDaughter, ChoosingHeir, Lost };

    Run() = default;
    void start(World& world, const RunConfig& config, std::uint32_t runId);
    // Procesa los eventos del mundo y actualiza el censo. Llamar después de cada paso.
    void update(World& world, float dt);

    State state() const { return state_; }
    bool simulationPaused() const { return state_ == State::ChoosingDaughter || state_ == State::ChoosingHeir; }

    // Elección (hija o heredero): ciclar candidatos y confirmar.
    const std::vector<entt::entity>& candidates() const { return candidates_; }
    std::size_t selected() const { return selected_; }
    void cycle(int step);
    void select(entt::entity cell);
    void confirm(World& world);

    const RunConfig& config() const { return config_; }
    const Census& census() const { return census_; }
    bool won() const { return won_; }
    bool justWon() { return std::exchange(justWon_, false); }
    float elapsed() const { return elapsed_; }
    float bestShare() const { return bestShare_; }
    std::uint32_t bestGeneration() const { return bestGeneration_; }
    int divisions() const { return divisions_; }
    int conjugations() const { return conjugations_; }
    const char* deathCause() const { return deathCause_; }
    // Genoma más reciente del jugador (el legado que queda al terminar la partida).
    const Genome& lineageGenome() const { return lineageGenome_; }

  private:
    RunConfig config_;
    State state_ = State::Playing;
    std::vector<entt::entity> candidates_;
    std::size_t selected_ = 0;
    Census census_;
    float censusTimer_ = 0.0f;
    float elapsed_ = 0.0f;
    float bestShare_ = 0.0f;
    std::uint32_t bestGeneration_ = 0;
    int divisions_ = 0;
    int conjugations_ = 0;
    bool won_ = false, justWon_ = false;
    const char* deathCause_ = "";
    Genome lineageGenome_;
};
