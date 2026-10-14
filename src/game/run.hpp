#pragma once

#include "bio/genome.hpp"
#include "bio/locomotion.hpp"
#include "game/lineage.hpp"

#include <cstdint>

class World;

// Parámetros de una sesión de sandbox.
struct RunConfig {
    LocomotionType founder = LocomotionType::Cilia; // ser que se controla al empezar
    bool spawnFounder = true; // false: la placa empieza sin ser controlado (se elige con la herramienta Controlar)
    int biome = 0;            // BiomeId de la gota
    std::uint64_t seed = 1;
    float wildlife = 1.0f;    // escala de las poblaciones silvestres iniciales (ecosistema)
    int competitors = 0;      // células pasivas extra (debug)
    Genome genome = speciesGenome(LocomotionType::Cilia); // genoma del ser inicial
};

// Una sesión de sandbox: una gota que se observa y en la que se interviene. No hay victoria ni
// derrota: lleva el censo del ecosistema y cuenta lo que le pasa al ser controlado.
class Run {
  public:
    Run() = default;
    void start(World& world, const RunConfig& config, std::uint32_t runId);
    // Procesa los eventos del mundo y actualiza el censo. Llamar después de cada paso.
    void update(World& world, float dt);

    const RunConfig& config() const { return config_; }
    const Census& census() const { return census_; }
    float elapsed() const { return elapsed_; }
    int divisions() const { return divisions_; }
    int conjugations() const { return conjugations_; }
    // Causa de la última muerte del ser controlado ("" si no murió).
    const char* deathCause() const { return deathCause_; }

  private:
    RunConfig config_;
    Census census_;
    float censusTimer_ = 0.0f;
    float elapsed_ = 0.0f;
    int divisions_ = 0;
    int conjugations_ = 0;
    const char* deathCause_ = "";
};
