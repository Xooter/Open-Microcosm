#pragma once

#include "bio/bacteria.hpp"
#include "core/rng.hpp"
#include "core/vec2.hpp"
#include "sim/chem_field.hpp"
#include "sim/fft.hpp"
#include "sim/fluid.hpp"
#include "sim/particles.hpp"

#include <cstdint>

struct WorldConfig {
    int gridX = 256, gridY = 128; // potencias de 2 (FFT)
    float cellSize = 8.0f;        // [µm] → mundo de 2048 × 1024 µm
    std::size_t bacteria = 50'000;
    std::size_t detritus = 60;
    std::uint64_t seed = 1;
};

struct MediumParams {
    float o2AirExchange = 0.05f;      // intercambio con el aire por la superficie [1/s]
    float nutrientUptake = 0.02f;     // consumo por microbios no simulados [1/s]
    float detritusDecay = 0.002f;     // descomposición de detritos [1/s]
    float detritusSpawnRate = 0.1f;   // detritos que llegan al campo de visión [1/s]
    float surfaceLight = 1.0f;        // irradiancia relativa de la lámpara
    float inkLifetime = 120.0f;       // [s] la tinta es una herramienta de debug
};

// Mundo toroidal visto desde arriba (vista cenital del microscopio). Coordina el medio
// (agua y química), las partículas ambientales y las bacterias.
class World {
  public:
    explicit World(const WorldConfig& config);

    void step(float dt);

    // Herramientas de desarrollo.
    void setStir(Vec2 position, Vec2 velocity, float radius);
    void clearStir() { stirActive_ = false; }
    void dropInk(Vec2 position, float radius, int count);
    void dropDetritus(Vec2 position);

    float width() const { return width_; }
    float height() const { return height_; }
    float simTime() const { return simTime_; }

    Fluid& fluid() { return fluid_; }
    const Fluid& fluid() const { return fluid_; }
    const ChemField& oxygen() const { return oxygen_; }
    const ChemField& nutrients() const { return nutrients_; }
    const Grid2D& light() const { return light_; }
    const AmbientParticles& ambient() const { return ambient_; }
    Bacteria& bacteria() { return bacteria_; }
    const Bacteria& bacteria() const { return bacteria_; }
    MediumParams& medium() { return medium_; }

    // Hash del estado completo, para tests de determinismo.
    std::uint64_t stateHash() const;

  private:
    void applyStir();
    void updateChemistrySources(float dt);
    void stepAmbient(float dt);
    void computeLight();
    void spawnDetritus(float dt);
    void addDetritus(Rng& rng, Vec2 position);

    WorldConfig config_;
    MediumParams medium_;
    float width_, height_;

    Fft2D fft_;
    Fluid fluid_;
    ChemField oxygen_, nutrients_;
    Grid2D light_, shading_;
    AmbientParticles ambient_;
    Bacteria bacteria_;

    bool stirActive_ = false;
    Vec2 stirPosition_, stirVelocity_;
    float stirRadius_ = 30.0f;

    std::uint64_t step_ = 0, spawnCounter_ = 0, toolCounter_ = 0;
    float spawnAccumulator_ = 0.0f;
    float simTime_ = 0.0f;
};
