#include "game/world.hpp"

#include "core/profiling.hpp"
#include "core/rng.hpp"
#include "core/units.hpp"

#include <cmath>
#include <algorithm>

namespace {

// Masa orgánica de una partícula esférica, en pmol equivalentes de glucosa
// (densidad ≈ agua, 1e-12 g/µm³; glucosa 180 g/mol).
float organicMassPmol(float radiusUm) {
    const float volume = 4.0f / 3.0f * units::kPi * radiusUm * radiusUm * radiusUm;
    return volume / 180.0f;
}

float radiusFromMass(float massPmol) {
    return std::cbrt(massPmol * 180.0f * 3.0f / (4.0f * units::kPi));
}

constexpr float kInkRadius = 0.2f;          // [µm]
constexpr float kMinDetritusMass = 0.001f;  // [pmol] debajo de esto se disolvió del todo

template <typename T> void hashVector(std::uint64_t& h, const std::vector<T>& v) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(v.data());
    for (std::size_t i = 0; i < v.size() * sizeof(T); ++i) {
        h ^= bytes[i];
        h *= 0x100000001B3ull; // FNV-1a
    }
}

} // namespace

World::World(const WorldConfig& config)
    : config_(config), width_(config.gridX * config.cellSize),
      height_(config.gridY * config.cellSize), fft_(config.gridX, config.gridY),
      fluid_(config.gridX, config.gridY, config.cellSize),
      oxygen_(config.gridX, config.gridY, config.cellSize, units::kDiffusionO2,
              units::kO2Saturation),
      nutrients_(config.gridX, config.gridY, config.cellSize, units::kDiffusionGlucose, 0.0f),
      light_(config.gridX, config.gridY, config.cellSize, 1.0f),
      shading_(config.gridX, config.gridY, config.cellSize) {
    for (std::size_t i = 0; i < config.detritus; ++i) {
        Rng rng(config.seed, rng_stream::kSpawn, spawnCounter_++, 0);
        addDetritus(rng, {rng.uniform(0.0f, width_), rng.uniform(0.0f, height_)});
    }
    bacteria_.spawn(config.bacteria, width_, height_, config.seed);
    bacteria_.resetMemory(nutrients_.grid());
    fluid_.solve(fft_);
    computeLight();
}

void World::step(float dt) {
    PROFILE_SCOPE("World::step");
    // Orden de AGENTS.md: fluido → química → partículas → organismos.
    fluid_.clearForces();
    applyStir();
    fluid_.solve(fft_);

    oxygen_.advect(fluid_, dt);
    nutrients_.advect(fluid_, dt);
    updateChemistrySources(dt);
    stepAmbient(dt); // deposita nutrientes de los detritos y su sombra
    oxygen_.diffuse(fft_, dt);
    nutrients_.diffuse(fft_, dt);
    computeLight();

    bacteria_.step(fluid_, nutrients_.grid(), dt, config_.seed, step_, width_, height_);

    spawnDetritus(dt);
    simTime_ += dt;
    ++step_;
}

void World::setStir(Vec2 position, Vec2 velocity, float radius) {
    stirActive_ = true;
    stirPosition_ = position;
    stirVelocity_ = velocity;
    stirRadius_ = radius;
}

void World::applyStir() {
    if (!stirActive_)
        return;
    // Fuerza tal que el agua bajo el cursor se mueva aproximadamente a la velocidad del mouse:
    // en Stokes–Brinkman, u ≈ f / (ν/R² + α).
    const float response = fluid_.viscosity / (stirRadius_ * stirRadius_) +
                           fluid_.brinkmanFriction();
    fluid_.addForce(stirPosition_, stirVelocity_ * response, stirRadius_);
}

void World::updateChemistrySources(float dt) {
    PROFILE_SCOPE("chemistrySources");
    // Soluciones exactas de dc/dt = k (c_sat − c) y dc/dt = −λ c.
    const float o2Keep = std::exp(-medium_.o2AirExchange * dt);
    const float nutrientKeep = std::exp(-medium_.nutrientUptake * dt);
    auto& o2 = oxygen_.grid().v;
    auto& food = nutrients_.grid().v;
    for (std::size_t k = 0; k < o2.size(); ++k) {
        o2[k] = units::kO2Saturation + (o2[k] - units::kO2Saturation) * o2Keep;
        food[k] *= nutrientKeep;
    }
}

void World::stepAmbient(float dt) {
    PROFILE_SCOPE("stepAmbient");
    const auto n = static_cast<std::ptrdiff_t>(ambient_.size());

    // Sin inercia: velocidad = la del agua + browniano (Stokes–Einstein según el radio).
#pragma omp parallel for schedule(static)
    for (std::ptrdiff_t i = 0; i < n; ++i) {
        Rng rng(config_.seed, rng_stream::kAmbient, static_cast<std::uint64_t>(i), step_);
        const float s = std::sqrt(2.0f * units::stokesEinsteinDiffusion(ambient_.radius[i]) * dt);
        const Vec2 u = fluid_.velocityAt({ambient_.x[i], ambient_.y[i]});
        ambient_.x[i] = wrapCoord(ambient_.x[i] + u.x * dt + s * rng.normal(), width_);
        ambient_.y[i] = wrapCoord(ambient_.y[i] + u.y * dt + s * rng.normal(), height_);
        ambient_.age[i] += dt;
    }

    // Descomposición y sombra: escrituras cruzadas en la grilla, por eso es serial.
    Grid2D& food = nutrients_.grid();
    std::fill(shading_.v.begin(), shading_.v.end(), 0.0f);
    const float cellArea = shading_.dx * shading_.dx;
    // µM que aporta 1 pmol disuelto en el volumen de una celda (dx² × espesor de la película).
    const double cellLiters = static_cast<double>(cellArea) * fluid_.gapHeight *
                              units::kLitersPerCubicMicrometer;
    const auto microMolarPerPmol = static_cast<float>(1e-6 / cellLiters);
    const float decayed = 1.0f - std::exp(-medium_.detritusDecay * dt);

    for (std::size_t i = 0; i < ambient_.size();) {
        if (ambient_.kind[i] == ParticleKind::Ink) {
            if (ambient_.age[i] > medium_.inkLifetime) {
                ambient_.swapRemove(i);
                continue;
            }
        } else {
            const float released = ambient_.mass[i] * decayed;
            ambient_.mass[i] -= released;
            const int cx = food.cellX(ambient_.x[i]), cy = food.cellY(ambient_.y[i]);
            food.at(cx, cy) += released * microMolarPerPmol;
            if (ambient_.mass[i] < kMinDetritusMass) {
                ambient_.swapRemove(i);
                continue;
            }
            ambient_.radius[i] = radiusFromMass(ambient_.mass[i]);
            // Fracción de la celda cubierta por la sombra proyectada del detrito.
            shading_.at(cx, cy) += units::kPi * ambient_.radius[i] * ambient_.radius[i] / cellArea;
        }
        ++i;
    }
}

void World::computeLight() {
    // Vista cenital: la lámpara ilumina uniforme y los detritos atenúan (Beer–Lambert).
    for (std::size_t k = 0; k < light_.v.size(); ++k)
        light_.v[k] = medium_.surfaceLight * std::exp(-shading_.v[k]);
}

void World::spawnDetritus(float dt) {
    spawnAccumulator_ += medium_.detritusSpawnRate * dt;
    while (spawnAccumulator_ >= 1.0f) {
        spawnAccumulator_ -= 1.0f;
        Rng rng(config_.seed, rng_stream::kSpawn, spawnCounter_++, 0);
        addDetritus(rng, {rng.uniform(0.0f, width_), rng.uniform(0.0f, height_)});
    }
}

void World::addDetritus(Rng& rng, Vec2 position) {
    const float radius = rng.uniform(1.0f, 5.0f);
    ambient_.push(ParticleKind::Detritus, wrapCoord(position.x, width_),
                  wrapCoord(position.y, height_), radius, organicMassPmol(radius));
}

void World::dropInk(Vec2 position, float radius, int count) {
    Rng rng(config_.seed, rng_stream::kTools, toolCounter_++, step_);
    for (int k = 0; k < count; ++k) {
        const float r = radius * std::sqrt(rng.uniform());
        const float a = rng.uniform(0.0f, 2.0f * units::kPi);
        ambient_.push(ParticleKind::Ink, wrapCoord(position.x + r * std::cos(a), width_),
                      wrapCoord(position.y + r * std::sin(a), height_), kInkRadius, 0.0f);
    }
}

void World::dropDetritus(Vec2 position) {
    Rng rng(config_.seed, rng_stream::kTools, toolCounter_++, step_);
    addDetritus(rng, position);
}

std::uint64_t World::stateHash() const {
    std::uint64_t h = 0xCBF29CE484222325ull;
    hashVector(h, fluid_.ux().v);
    hashVector(h, fluid_.uy().v);
    hashVector(h, oxygen_.grid().v);
    hashVector(h, nutrients_.grid().v);
    hashVector(h, ambient_.x);
    hashVector(h, ambient_.y);
    hashVector(h, ambient_.mass);
    hashVector(h, bacteria_.x);
    hashVector(h, bacteria_.y);
    hashVector(h, bacteria_.heading);
    return h;
}
