#pragma once

#include "bio/genome.hpp"
#include "bio/locomotion.hpp"
#include "game/intervention.hpp"
#include "game/lineage.hpp"
#include "game/mass_budget.hpp"
#include "game/region.hpp"
#include "game/timeseries.hpp"

#include <cstdint>
#include <vector>

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
    // Medición (fase 13): la serie temporal de la gota, muestreada cada `sampleInterval` [s de
    // simulación] junto con el censo; el balance de masa al empezar y el último.
    const TimeSeries& series() const { return series_; }
    const MassBudget& startBudget() const { return startBudget_; }
    const MassBudget& budget() const { return budget_; }
    double residual(Element e) const { return budgetResidual(startBudget_, budget_, e); }
    float sampleInterval() const { return sampleInterval_; }
    void setSampleInterval(float seconds) { sampleInterval_ = seconds > 0.05f ? seconds : 0.05f; }
    // Región dibujada (cuantificación por región): se mide en cada muestra y entra en la serie.
    void setRegion(const World& world, std::vector<Vec2> polygon);
    void clearRegion();
    const std::vector<Vec2>& region() const { return region_; }
    const RegionStats& regionStats() const { return regionStats_; }
    // Toma una muestra ya (además de las periódicas).
    void sample(const World& world);
    float elapsed() const { return elapsed_; }
    int divisions() const { return divisions_; }
    int conjugations() const { return conjugations_; }
    // Causa de la última muerte del ser controlado ("" si no murió).
    const char* deathCause() const { return deathCause_; }

    // Intervenciones del usuario de esta sesión (se aplican entre pasos y se anotan con su paso).
    InterventionLog& interventions() { return interventions_; }
    const InterventionLog& interventions() const { return interventions_; }

  private:
    InterventionLog interventions_;
    void buildColumns();

    RunConfig config_;
    Census census_;
    float censusTimer_ = 0.0f;
    TimeSeries series_;
    MassBudget startBudget_, budget_;
    float sampleInterval_ = 1.0f;
    std::vector<Vec2> region_;
    RegionStats regionStats_;
    std::vector<float> row_;
    float elapsed_ = 0.0f;
    int divisions_ = 0;
    int conjugations_ = 0;
    const char* deathCause_ = "";
};
