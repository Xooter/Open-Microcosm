#include "game/world.hpp"

#include "core/profiling.hpp"
#include "core/rng.hpp"
#include "core/units.hpp"
#include "sim/polygon.hpp"

#include <cmath>
#include <algorithm>
#include <limits>

namespace {
SpeciesId speciesOfEntity(const entt::registry& reg, entt::entity e);
} // namespace

namespace {

// Masa orgánica seca de una partícula esférica, en pmol equivalentes de glucosa
// (densidad ≈ agua, 1e-12 g/µm³; glucosa 180 g/mol).
float organicMassPmol(float radiusUm) {
    const float volume = 4.0f / 3.0f * units::kPi * radiusUm * radiusUm * radiusUm;
    return units::kDryMassFraction * volume / 180.0f;
}

float radiusFromMass(float massPmol) {
    return std::cbrt(massPmol * 180.0f * 3.0f / (4.0f * units::kPi * units::kDryMassFraction));
}

constexpr float kInkRadius = 0.2f;          // [µm]
// LOD: margen alrededor de la vista con detalle completo, y hasta dónde se simplifica (más
// lejos, simulación estadística) [µm].
constexpr float kLodNearMargin = 60.0f;
constexpr float kPerceptionCell = 128.0f;  // [µm]
constexpr float kPerceptionRange = 400.0f; // [µm] ~300 de alcance + el radio de las grandes
constexpr float kLodMidMargin = 400.0f;

// Osmotrofia (levaduras): captación máxima relativa a la biomasa y afinidad.
constexpr float kOsmotrophUptakePerHour = 1.0f;
constexpr float kOsmotrophHalfSaturation = 5.0f; // [µM]
constexpr float kViolaceinDose = 1e-3f;           // daño por bacteria violeta ingerida
constexpr float kMinDetritusMass = 0.0003f; // [pmol] debajo de esto se disolvió del todo
// Sedimentación de Stokes: v = 2/9·Δρ·g·r²/µ. Un grano de cuarzo (Δρ = 1650 kg/m³) de 2 µm de
// radio cae a ~14 µm/s y toca el vidrio en segundos; la arcilla más fina tarda horas y sigue al
// agua con browniano. Simplificación: las asentadas ya están en el fondo (el agua no las lleva).
constexpr float kSettledRadius = 2.0f; // [µm]

// Fracción de la luz que bloquea cada mineral (el cuarzo y la mica son transparentes).
float mineralOpacity(MineralKind k) {
    switch (k) {
    case MineralKind::Clay: return 0.5f;
    case MineralKind::IronOxide: return 1.0f;
    case MineralKind::Sulfur: return 0.3f;
    default: return 0.1f;
    }
}

template <typename T> void hashVector(std::uint64_t& h, const std::vector<T>& v) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(v.data());
    for (std::size_t i = 0; i < v.size() * sizeof(T); ++i) {
        h ^= bytes[i];
        h *= 0x100000001B3ull; // FNV-1a
    }
}

} // namespace

// O₂ disuelto en equilibrio con el aire [µM] (agua dulce; ~457 a 0 °C, ~284 a 20 °C, ~206 a 40 °C).
static float o2SaturationAt(float celsius) {
    const float t = std::clamp(celsius, -2.0f, 80.0f);
    return t < 20.0f ? 457.0f + (284.0f - 457.0f) * t / 20.0f : std::max(80.0f, 284.0f + (206.0f - 284.0f) * (t - 20.0f) / 20.0f);
}

// Viscosidad del agua (Vogel): 1,0 mPa·s a 20 °C, 1,8 a 0 °C, 0,47 a 60 °C.
static float waterViscosity(float celsius) {
    return 2.414e-2f * std::pow(10.0f, 247.8f / (celsius + 273.15f - 140.0f)); // [mPa·s]
}

World::World(const WorldConfig& config)
    : config_(config), medium_(config.medium), width_(config.gridX * config.cellSize),
      height_(config.gridY * config.cellSize), fft_(config.gridX, config.gridY),
      fluid_(config.gridX, config.gridY, config.cellSize),
      oxygen_(config.gridX, config.gridY, config.cellSize, units::kDiffusionO2,
              config.medium.anoxic ? 0.0f : o2SaturationAt(config.medium.temperature)),
      nutrients_(config.gridX, config.gridY, config.cellSize, units::kDiffusionGlucose, 0.0f),
      phosphate_(config.gridX, config.gridY, config.cellSize, units::kDiffusionPhosphate, config.medium.phosphate),
      nitrogen_(config.gridX, config.gridY, config.cellSize, units::kDiffusionAmmonium, config.medium.nitrogen),
      silicate_(config.gridX, config.gridY, config.cellSize, units::kDiffusionSilicate, config.medium.silicate),
      iron_(config.gridX, config.gridY, config.cellSize, units::kDiffusionIron, config.medium.iron),
      vitamin_(config.gridX, config.gridY, config.cellSize, units::kDiffusionVitamin, config.medium.vitamin),
      co2_(config.gridX, config.gridY, config.cellSize, units::kDiffusionCO2, units::kCO2Saturation),
      toxin_(config.gridX, config.gridY, config.cellSize, units::kDiffusionSmallSolute, 0.0f),
      autoinducer_(config.gridX, config.gridY, config.cellSize, units::kDiffusionSmallSolute, 0.0f),
      phage_(config.gridX, config.gridY, config.cellSize, units::kDiffusionVirus, config.medium.phage),
      light_(config.gridX, config.gridY, config.cellSize, 1.0f),
      shading_(config.gridX, config.gridY, config.cellSize),
      slime_(config.gridX, config.gridY, config.cellSize),
      hyphaCover_(config.gridX, config.gridY, config.cellSize),
      ph_(config.gridX, config.gridY, config.cellSize, 8.0f),
      inflowPattern_(config.gridX, config.gridY, config.cellSize, 1.0f) {
    // Temperatura: Q10 = 2 para las tasas biológicas; el agua más fría es más viscosa y frena a
    // los nadadores (a bajo Reynolds la velocidad es fuerza / viscosidad).
    temperatureFactor_ = std::pow(2.0f, (medium_.temperature - 20.0f) / 10.0f);
    o2Saturation_ = medium_.anoxic ? 0.0f : o2SaturationAt(medium_.temperature);
    viscositySpeed_ = waterViscosity(20.0f) / waterViscosity(medium_.temperature);
    bucketsX_ = std::max(1, static_cast<int>(width_ / bucketSize_));
    bucketsY_ = std::max(1, static_cast<int>(height_ / bucketSize_));
    buckets_.resize(static_cast<std::size_t>(bucketsX_) * bucketsY_);
    // Placa: pocillo circular centrado; afuera solo hay vidrio.
    if (config.dish)
        dish_ = {{0.5f * width_, 0.5f * height_}, 0.5f * std::min(width_, height_) - config.dishMargin};
    // El agua se frena en las dos últimas celdas junto a la pared.
    dishMask_.build(dish_, config.gridX, config.gridY, config.cellSize, 2.0f * config.cellSize);
    fluid_.setFlowMask(dish_.active() ? &dishMask_.flow : nullptr);
    bacteria_.dish = dish_;
    cyano_.dish = dish_;
    fungi_.dish = dish_;
    plasmo_.dish = dish_;
    for (std::size_t i = 0; i < config.detritus; ++i) {
        Rng rng(config.seed, rng_stream::kSpawn, spawnCounter_++, 0);
        addDetritus(rng, randomPoint(rng));
    }
    scatterSediment();
    bacteria_.spawn(config.bacteria, width_, height_, config.seed);
    seedBdellovibrio(config.bacteria / 300);
    cyano_.params.tMin = config.cyanoTMin;
    cyano_.params.tMax = config.cyanoTMax;
    cyano_.params.temperature = medium_.temperature;
    cyano_.spawn(config.cyanoFilaments, config.cyanoCells, width_, height_, config.seed);
    cyano_.spawn(config.oscillatoriaFilaments, config.oscillatoriaCells, width_, height_, config.seed, true);
    if (config.patchiness > 0.0f)
        scatterPatches();
    for (std::size_t k = 0; k < inflowPattern_.v.size(); ++k)
        inflowPattern_.v[k] *= dishMask_.isWater(k) ? 1.0f : 0.0f; // sin aporte dentro del vidrio
    for (ChemField* f : {&oxygen_, &nutrients_, &phosphate_, &nitrogen_, &silicate_, &iron_, &vitamin_, &co2_, &toxin_, &autoinducer_, &phage_})
        f->setMask(&dishMask_);
    bacteria_.resetMemory(nutrients_.grid());
    fluid_.solve(fft_);
    computeLight();
}

void World::refreshMedium() {
    temperatureFactor_ = std::pow(2.0f, (medium_.temperature - 20.0f) / 10.0f);
    o2Saturation_ = medium_.anoxic ? 0.0f : o2SaturationAt(medium_.temperature);
    viscositySpeed_ = waterViscosity(20.0f) / waterViscosity(medium_.temperature);
    cyano_.params.temperature = medium_.temperature;
    for (auto e : registry_.view<Heredity>())
        applyTraits(e); // la viscosidad cambia la velocidad de nado
}

Vec2 World::randomPoint(Rng& rng, float margin) const {
    const float u = rng.uniform();
    const float v = rng.uniform();
    if (!dish_.active())
        return {u * width_, v * height_};
    return dish_.sample(u, v, margin);
}

void World::scatterPatches() {
    // Gota real: la materia orgánica no está repartida pareja (restos de hojas, excrementos,
    // un alga muerta): manchas suaves de ruido de valor con detritos agrupados.
    constexpr int kLx = 8, kLy = 4;
    float lattice[kLx][kLy];
    for (int i = 0; i < kLx; ++i)
        for (int j = 0; j < kLy; ++j) {
            Rng rng(config_.seed, rng_stream::kSpawn + 70, static_cast<std::uint64_t>(i * kLy + j), 0);
            lattice[i][j] = rng.uniform();
        }
    auto noise = [&](float x, float y) {
        const float u = x / width_ * kLx, v = y / height_ * kLy;
        const int i0 = static_cast<int>(std::floor(u)), j0 = static_cast<int>(std::floor(v));
        const float fu = u - static_cast<float>(i0), fv = v - static_cast<float>(j0);
        const float su = fu * fu * (3.0f - 2.0f * fu), sv = fv * fv * (3.0f - 2.0f * fv);
        auto at = [&](int i, int j) { return lattice[wrapIndex(i, kLx)][wrapIndex(j, kLy)]; };
        return (at(i0, j0) * (1 - su) + at(i0 + 1, j0) * su) * (1 - sv) + (at(i0, j0 + 1) * (1 - su) + at(i0 + 1, j0 + 1) * su) * sv;
    };
    Grid2D& food = nutrients_.grid();
    double total = 0.0;
    for (int j = 0; j < food.ny; ++j)
        for (int i = 0; i < food.nx; ++i) {
            const float n = noise((static_cast<float>(i) + 0.5f) * food.dx, (static_cast<float>(j) + 0.5f) * food.dx);
            food.at(i, j) += config_.patchiness * 20.0f * std::max(0.0f, n - 0.55f); // [µM]
            // El aporte de afuera también llega en manchas (donde están las hojas).
            inflowPattern_.at(i, j) = (1.0f - config_.patchiness) + config_.patchiness * 2.0f * n;
            total += inflowPattern_.at(i, j);
        }
    const auto mean = static_cast<float>(total / static_cast<double>(inflowPattern_.v.size()));
    for (float& v : inflowPattern_.v)
        v /= std::max(1e-3f, mean);
    const auto extra = static_cast<std::size_t>(config_.patchiness * 80.0f);
    for (std::size_t k = 0; k < extra; ++k) {
        Rng rng(config_.seed, rng_stream::kSpawn + 71, k, 0);
        Vec2 p = randomPoint(rng);
        for (int tries = 0; tries < 20 && noise(p.x, p.y) < 0.6f; ++tries)
            p = randomPoint(rng);
        addDetritus(rng, p);
    }
}

void World::step(float dt) {
    PROFILE_SCOPE("World::step");
    // Muestra fijada: las células están muertas y entrecruzadas, sin vida que avance. Se simplifica:
    // tampoco hay corrientes ni movimiento browniano (en un preparado fijado y cubierto casi no hay).
    if (fixed_)
        return;
    // Orden de AGENTS.md: fluido → química → partículas → organismos.
    fluid_.clearForces();
    applyStir();
    fluid_.solve(fft_);
    fluid_.keepAsBackground();
    // Nadadores y filtradores mueven el agua; afecta a todo lo demás (bacterias, partículas),
    // pero las células móviles sienten solo el flujo de fondo (no el propio).
    bool pushed = false;
    for (auto [e, body, loco] : registry_.view<RingBody, Locomotion>().each()) {
        if (registry_.any_of<Cyst, Captured>(e))
            continue;
        const bool oral = speciesInfo(speciesOfEntity(registry_, e)).feeding == FeedingMode::OralGroove;
        if (!oral && !(loco.type == LocomotionType::Flagellum && loco.speed > 0.0f))
            continue;
        applySwimmerForces(body, bodies_, loco, locomotionParams_, fluid_, oral);
        pushed = true;
    }
    if (pushed)
        fluid_.solve(fft_);

    // Los cuerpos blandos se lanzan ya (en la GPU, si hay): mientras tanto avanzan la química,
    // la luz, las bacterias, las cianobacterias y los hongos, que no los mueven.
    beginBodies(dt);

    oxygen_.advect(fluid_, dt);
    nutrients_.advect(fluid_, dt);
    phosphate_.advect(fluid_, dt);
    for (ChemField* f : {&nitrogen_, &silicate_, &iron_, &vitamin_, &co2_, &toxin_, &autoinducer_, &phage_})
        f->advect(fluid_, dt);
    updateChemistrySources(dt);
    updateWaterChemistry(dt);
    stepAmbient(dt); // deposita nutrientes de los detritos y su sombra
    oxygen_.diffuse(fft_, dt);
    nutrients_.diffuse(fft_, dt);
    // Los campos lentos (nutrientes inorgánicos, señales, fagos) difunden cada 4 pasos con el
    // tiempo acumulado: la difusión espectral es exacta para cualquier paso.
    slowDiffusion_ += dt;
    if (step_ % 4 == 3) {
        phosphate_.diffuse(fft_, slowDiffusion_);
        for (ChemField* f : {&nitrogen_, &silicate_, &iron_, &vitamin_, &toxin_, &autoinducer_, &phage_})
            f->diffuse(fft_, slowDiffusion_);
        slowDiffusion_ = 0.0f;
    }
    co2_.diffuse(fft_, dt);
    computeLight();
    computePH();

    bacteria_.step(fluid_, nutrients_.grid(), dt, config_.seed, step_, width_, height_);
    growBacteria(dt);
    stepCyanobacteria(dt);
    stepFungi(dt);
    stepPlasmodium(dt);
    finishBodies();
    updateFeeding(dt);       // eventos de topología (fagocitosis): al final del paso
    updateLifecycle(dt);     // metabolismo, división y muerte
    tickDefenses(dt);
    excludeFromCells();

    spawnDetritus(dt);
    spawnDust(dt);
    // Marcas de lisis (solo visuales): duran unos segundos.
    std::erase_if(lysisMarks_, [&](const LysisMark& m) { return simTime_ - m.time > 3.0f; });
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
    const float inflow = medium_.organicInflow * dt * bioScale() / 3600.0f; // [µM]
    // B₁₂: la sintetizan las bacterias (proporcional a su número) y la degrada la luz.
    const float bioHours = dt * bioScale() / 3600.0f;
    const float vitaminSource = medium_.vitaminSourcePerHour * bioHours *
                                static_cast<float>(bacteria_.size()) / 50000.0f;
    const float vitaminKeep = std::exp(-medium_.vitaminDecayPerHour * medium_.surfaceLight * bioHours);
    auto& b12 = vitamin_.grid().v;
    for (std::size_t k = 0; k < o2.size(); ++k) {
        if (!dishMask_.isWater(k))
            continue; // vidrio: sus celdas fantasma no intercambian con el aire
        o2[k] = o2Saturation_ + (o2[k] - o2Saturation_) * o2Keep;
        food[k] = food[k] * nutrientKeep + inflow * inflowPattern_.v[k];
        b12[k] = b12[k] * vitaminKeep + vitaminSource;
    }
}

void World::stepAmbient(float dt) {
    PROFILE_SCOPE("stepAmbient");
    const auto n = static_cast<std::ptrdiff_t>(ambient_.size());

    // Sin inercia: velocidad = la del agua + browniano (Stokes–Einstein según el radio).
#pragma omp parallel for schedule(static)
    for (std::ptrdiff_t i = 0; i < n; ++i) {
        if (ambient_.kind[i] == ParticleKind::Mineral && ambient_.radius[i] >= kSettledRadius) {
            ambient_.age[i] += dt; // asentada en el fondo: solo la mueven las células
            continue;
        }
        Rng rng(config_.seed, rng_stream::kAmbient, static_cast<std::uint64_t>(i), step_);
        const float s = std::sqrt(2.0f * units::stokesEinsteinDiffusion(ambient_.radius[i]) * dt);
        const Vec2 u = fluid_.velocityAt({ambient_.x[i], ambient_.y[i]});
        ambient_.x[i] = wrapCoord(ambient_.x[i] + u.x * dt + s * rng.normal(), width_);
        ambient_.y[i] = wrapCoord(ambient_.y[i] + u.y * dt + s * rng.normal(), height_);
        dish_.bounce(ambient_.x[i], ambient_.y[i], nullptr, ambient_.radius[i]);
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
    // La descomposición la hacen bacterias y hongos: corre en tiempo biológico.
    const float decayed = 1.0f - std::exp(-medium_.detritusDecay * dt * bioScale());
    const float frustuleDissolved = 1.0f - std::exp(-medium_.frustuleDissolutionPerHour * dt * bioScale() / 3600.0f);
    // Junto a las hifas, las exoenzimas de los mohos de agua la aceleran.
    const float decayedNearHyphae =
        1.0f - std::exp(-medium_.detritusDecay * fungi_.params.detritusDecayBoost * dt * bioScale());

    for (std::size_t i = 0; i < ambient_.size();) {
        if (ambient_.kind[i] == ParticleKind::Ink) {
            if (ambient_.age[i] > medium_.inkLifetime) {
                ambient_.swapRemove(i);
                continue;
            }
        } else if (ambient_.kind[i] == ParticleKind::Mineral) {
            // Inerte: no se descompone; las opacas dan sombra.
            const int cx = food.cellX(ambient_.x[i]), cy = food.cellY(ambient_.y[i]);
            if (ambient_.mineral[i] == MineralKind::Frustule && ambient_.mass[i] > 0.0f) {
                // La sílice biogénica (opal) se redisuelve despacio y vuelve a estar disponible.
                const float dissolved = ambient_.mass[i] * frustuleDissolved;
                ambient_.mass[i] -= dissolved;
                silicate_.grid().at(cx, cy) += dissolved * microMolarPerPmol;
                ledger_.add(Element::Si, Flow::Dissolution, dissolved);
                if (ambient_.mass[i] < 1e-9f) {
                    silicate_.grid().at(cx, cy) += ambient_.mass[i] * microMolarPerPmol;
                    ledger_.add(Element::Si, Flow::Dissolution, ambient_.mass[i]);
                    ambient_.swapRemove(i);
                    continue;
                }
            }
            shading_.at(cx, cy) += mineralOpacity(ambient_.mineral[i]) * units::kPi * ambient_.radius[i] *
                                   ambient_.radius[i] / cellArea;
        } else {
            const int cx = food.cellX(ambient_.x[i]), cy = food.cellY(ambient_.y[i]);
            const float released = ambient_.mass[i] * (hyphaCover_.at(cx, cy) > 0.0f ? decayedNearHyphae : decayed);
            ambient_.mass[i] -= released;
            food.at(cx, cy) += released * microMolarPerPmol;
            ledger_.add(Element::C, Flow::Excretion, 6.0 * released); // descomposición: 6 C por glucosa
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
        addDetritus(rng, randomPoint(rng));
    }
}

void World::addDetritus(Rng& rng, Vec2 position) {
    const float radius = rng.uniform(1.0f, 5.0f);
    position = dish_.clampInside(position, radius);
    ambient_.push(ParticleKind::Detritus, wrapCoord(position.x, width_),
                  wrapCoord(position.y, height_), radius, organicMassPmol(radius));
}

void World::dropInk(Vec2 position, float radius, int count) {
    Rng rng(config_.seed, rng_stream::kTools, toolCounter_++, step_);
    for (int k = 0; k < count; ++k) {
        const float r = radius * std::sqrt(rng.uniform());
        const float a = rng.uniform(0.0f, 2.0f * units::kPi);
        const Vec2 p = dish_.clampInside(position + Vec2{r * std::cos(a), r * std::sin(a)}, kInkRadius);
        ambient_.push(ParticleKind::Ink, wrapCoord(p.x, width_), wrapCoord(p.y, height_), kInkRadius, 0.0f);
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
    hashVector(h, bodies_.x);
    hashVector(h, bodies_.y);
    hashVector(h, fungi_.spores.x);
    hashVector(h, fungi_.spores.y);
    for (const Mycelium& m : fungi_.mycelia)
        hashVector(h, m.x);
    for (const Plasmodium& pl : plasmo_.plasmodia) {
        hashVector(h, pl.x);
        hashVector(h, pl.y);
    }
    hashVector(h, plasmo_.flakes.pmol);
    return h;
}

// ---------------------------------------------------------------------------
// Células (cuerpos blandos)
// ---------------------------------------------------------------------------

entt::entity World::spawnCell(Vec2 center) {
    Rng rng(config_.seed, rng_stream::kCells, cellCounter_, 0);
    return spawnCell(randomCellSpec(rng, center));
}

entt::entity World::spawnCell(const CellSpec& spec) {
    Rng rng(config_.seed, rng_stream::kCells, cellCounter_++, 1);
    CellSpec wrapped = spec;
    wrapped.center = {wrapCoord(spec.center.x, width_), wrapCoord(spec.center.y, height_)};
    return createCell(registry_, bodies_, wrapped, rng);
}

void World::spawnTestScene(int count) {
    // Grilla holgada: las células más grandes (110 µm) no se tocan al aparecer.
    constexpr float kSpacing = 125.0f;
    const int cols = std::max(1, static_cast<int>(std::ceil(std::sqrt(2.0f * count))));
    const int rows = (count + cols - 1) / cols;
    const Vec2 origin = {0.5f * width_ - 0.5f * (cols - 1) * kSpacing,
                         0.5f * height_ - 0.5f * (rows - 1) * kSpacing};
    for (int k = 0; k < count; ++k)
        spawnCell(origin + Vec2{(k % cols) * kSpacing, static_cast<float>(k / cols) * kSpacing});
}

entt::entity World::cellAt(Vec2 position) const {
    for (const auto [e, body] : registry_.view<RingBody>().each()) {
        if (registry_.all_of<Rock>(e))
            continue; // los granos no son células (ver rockAt)
        const Vec2 q = body.centroid + Vec2{minImage(position.x - body.centroid.x, width_),
                                            minImage(position.y - body.centroid.y, height_)};
        if (polygon::contains(body.poly.data(), static_cast<int>(body.poly.size()), q))
            return e;
    }
    return entt::null;
}

void World::removeCellAt(Vec2 position) {
    const entt::entity e = cellAt(position);
    if (e != entt::null && registry_.all_of<Species>(e) && registry_.all_of<RingBody>(e))
        noteDeath(e, registry_.get<Species>(e).id, registry_.get<RingBody>(e).centroid, "borrada con la herramienta");
    if (e == entt::null) {
        if (const entt::entity rock = rockAt(position); rock != entt::null) {
            destroyCell(registry_, bodies_, rock);
            rebuildFlowMask();
        }
        return;
    }
    if (grabSlot_ != kNoGrab && bodies_.owner[grabSlot_] == entt::to_integral(e))
        endGrab();
    if (e == player_)
        player_ = entt::null;
    if (registry_.valid(player_))
        if (const PhagocyticCup* cup = registry_.try_get<PhagocyticCup>(player_);
            cup && cup->preyId == entt::to_integral(e))
            registry_.remove<PhagocyticCup>(player_);
    destroyCell(registry_, bodies_, e);
}

void World::setSqueeze(Vec2 center, float speed) {
    squeezeActive_ = true;
    squeezeCenter_ = center;
    squeezeSpeed_ = speed;
}

Vec2 World::grabGoal() const {
    // En µm: un poco más que el radio de contacto de la membrana, para que cada sub-paso no
    // salte de un lado al otro de otra membrana.
    constexpr float kGrabReach = 6.0f;
    const Vec2 at = {bodies_.x[grabSlot_], bodies_.y[grabSlot_]};
    const Vec2 to = {minImage(grabTarget_.x - at.x, width_), minImage(grabTarget_.y - at.y, height_)};
    const float len = length(to);
    return len > kGrabReach ? at + to * (kGrabReach / len) : at + to;
}

bool World::beginGrab(Vec2 position) {
    constexpr float kGrabRadius = 20.0f; // [µm]
    float best = kGrabRadius * kGrabRadius;
    grabSlot_ = kNoGrab;
    for (std::size_t s = 0; s < bodies_.capacity(); ++s) {
        if (!bodies_.alive[s] || bodies_.kind[s] != BodyKind::Membrane)
            continue;
        const float dx = minImage(bodies_.x[s] - position.x, width_);
        const float dy = minImage(bodies_.y[s] - position.y, height_);
        if (dx * dx + dy * dy < best) {
            best = dx * dx + dy * dy;
            grabSlot_ = static_cast<std::uint32_t>(s);
        }
    }
    grabTarget_ = position;
    return grabSlot_ != kNoGrab;
}

void World::collectBodies() {
    bodyList_.clear();
    for (auto [e, body] : registry_.view<RingBody>().each())
        bodyList_.push_back(&body);
    std::sort(bodyList_.begin(), bodyList_.end(),
              [](const RingBody* a, const RingBody* b) { return a->id < b->id; });
}

void World::beginBodies(float dt) {
    PROFILE_SCOPE("World::beginBodies");
    gpuPending_ = false;
    collectBodies();
    if (bodyList_.empty())
        return;
    const auto nb = static_cast<std::ptrdiff_t>(bodyList_.size());

    // Velocidad externa del paso: arrastre del agua + herramienta de compresión.
    // Los gránulos además tienen browniano (Stokes–Einstein).
#pragma omp parallel for schedule(dynamic, 1)
    for (std::ptrdiff_t b = 0; b < nb; ++b) {
        RingBody& body = *bodyList_[b];
        // La compresión traslada cada célula entera hacia el punto (no la estira por dentro).
        Vec2 push;
        if (squeezeActive_) {
            const Vec2 d = {minImage(squeezeCenter_.x - body.centroid.x, width_),
                            minImage(squeezeCenter_.y - body.centroid.y, height_)};
            const float len = length(d);
            if (len > 1.0f)
                push = d * (squeezeSpeed_ / len);
        }
        if (registry_.all_of<Rock>(static_cast<entt::entity>(body.id)))
            continue; // el grano está apoyado en el vidrio: ni el agua ni la compresión lo mueven
        // Los nadadores sienten solo el flujo de fondo.
        const bool swimmer = registry_.all_of<Locomotion>(static_cast<entt::entity>(body.id));
        std::function<void(std::uint32_t)> external = [&](std::uint32_t s) {
            const Vec2 p = {bodies_.x[s], bodies_.y[s]};
            const Vec2 v = (swimmer ? fluid_.backgroundAt(p) : fluid_.velocityAt(p)) + push;
            bodies_.vx[s] = v.x;
            bodies_.vy[s] = v.y;
        };
        // Membrana plástica (ameba): el córtex de actina es ~1000 veces más rígido que los
        // esfuerzos viscosos de una corriente de agua, así que el flujo traslada y rota a la
        // célula entera; no estira cada punto por separado (como una línea de tinta, se
        // plegaría y cruzaría). Se ajusta el flujo a un movimiento rígido.
        const bool rigidFlow = body.membraneReservoirRate > 0.0f;
        Vec2 flowMean, flowCenter;
        float flowOmega = 0.0f;
        if (rigidFlow) {
            Vec2 sumV, sumP;
            for (std::uint32_t s : body.ring) {
                const Vec2 p = {bodies_.x[s], bodies_.y[s]};
                sumP = sumP + p;
                sumV = sumV + (swimmer ? fluid_.backgroundAt(p) : fluid_.velocityAt(p));
            }
            const float inv = 1.0f / static_cast<float>(body.ring.size());
            flowMean = sumV * inv;
            flowCenter = sumP * inv;
            float torque = 0.0f, inertia = 0.0f;
            for (std::uint32_t s : body.ring) {
                const Vec2 p = {bodies_.x[s], bodies_.y[s]};
                const Vec2 r = p - flowCenter;
                torque += cross(r, (swimmer ? fluid_.backgroundAt(p) : fluid_.velocityAt(p)) - flowMean);
                inertia += dot(r, r);
            }
            flowOmega = inertia > 1e-6f ? torque / inertia : 0.0f;
            external = [&](std::uint32_t s) {
                const Vec2 r = Vec2{bodies_.x[s], bodies_.y[s]} - flowCenter;
                const Vec2 v = flowMean + Vec2{-r.y, r.x} * flowOmega + push;
                bodies_.vx[s] = v.x;
                bodies_.vy[s] = v.y;
            };
        }
        for (std::uint32_t s : body.ring)
            external(s);
        // Ciclosis: los gránulos giran alrededor del centro de la célula.
        const auto ent = static_cast<entt::entity>(body.id);
        const Species* sp = registry_.try_get<Species>(ent);
        const float cyclosis = sp && !registry_.all_of<Cyst>(ent) ? speciesInfo(sp->id).cyclosis : 0.0f;
        const float radius = 0.25f * ((body.aabbMax.x - body.aabbMin.x) + (body.aabbMax.y - body.aabbMin.y));
        const float omega = radius > 1.0f ? cyclosis / radius : 0.0f;
        for (std::size_t k = 0; k < body.interior.size(); ++k) {
            const std::uint32_t s = body.interior[k];
            external(s);
            if (omega != 0.0f && k >= body.organelleCount) {
                bodies_.vx[s] -= omega * (bodies_.y[s] - body.centroid.y);
                bodies_.vy[s] += omega * (bodies_.x[s] - body.centroid.x);
            }
            if (k >= body.organelleCount) {
                Rng rng(config_.seed, rng_stream::kBodies, s, step_);
                const float sd = std::sqrt(2.0f * units::stokesEinsteinDiffusion(bodies_.radius[s]) * dt);
                bodies_.x[s] += sd * rng.normal();
                bodies_.y[s] += sd * rng.normal();
            }
        }
    }

    applyMotion(dt);
    applyDivisionForces();
    for (RingBody* body : bodyList_) {
        soft_body::adaptPlastic(*body, bodies_, dt);
        soft_body::updateMembraneTension(*body, bodies_);
    }

    members_.clear();
    memberBody_.clear();
    float maxRadius = 0.0f;
    for (std::uint32_t bi = 0; bi < bodyList_.size(); ++bi) {
        const RingBody* body = bodyList_[bi];
        const auto n = static_cast<std::uint32_t>(body->ring.size());
        const auto first = static_cast<std::uint32_t>(members_.size());
        const std::uint32_t group = body->id;
        for (std::uint32_t i = 0; i < n; ++i) {
            members_.push_back({body->ring[i], first + (i + 1) % n, body->id, i, n, group,
                                static_cast<std::uint8_t>(body->membraneReservoirRate > 0.0f ? 1 : 0)});
            memberBody_.push_back(bi);
            maxRadius = std::max(maxRadius, bodies_.radius[body->ring[i]]);
        }
    }

    const int substeps = std::max(1, softTuning_.substeps);
    classifyLod();
    markTouching(2.0f * maxRadius + 1.0f, dt);
    limitApproach(dt, 2.0f * maxRadius + 1.0f);
    const int stride = substeps % std::max(1, lodStride) == 0 ? std::max(1, lodStride) : 1;
    memberActive_.resize(members_.size());
    // Fase 14: el bucle de sub-pasos puede correr entero en la GPU.
    if (substepSolver_) {
        PROFILE_SCOPE("World::gpuSubsteps");
        linkConstraints_.clear();
        for (const AdhesionLink& l : links_)
            linkConstraints_.push_back({l.a, l.b, l.rest});
        SubstepInput in;
        in.bodies = &bodyList_;
        in.lod = &bodyNear_;
        in.touch = &bodyTouch_;
        in.members = &members_;
        in.memberBody = &memberBody_;
        in.links = &linkConstraints_;
        in.tuning = &softTuning_;
        in.grabSlot = grabSlot_;
        if (grabSlot_ != kNoGrab)
            in.grabTarget = grabGoal();
        in.dt = dt;
        in.width = width_;
        in.height = height_;
        in.maxRadius = maxRadius;
        in.stride = stride;
        in.dish = dish_;
        gpuPending_ = substepSolver_->submit(bodies_, in);
    }
    pendingDt_ = dt;
    pendingStride_ = stride;
    pendingMaxRadius_ = maxRadius;
    if (!gpuPending_)
        cpuSubsteps(dt, stride, maxRadius);
}

// Bucle de sub-pasos de XPBD en la CPU (predicción, restricciones, contactos y uniones).
void World::cpuSubsteps(float dt, int stride, float maxRadius) {
    PROFILE_SCOPE("World::cpuSubsteps");
    const auto nb = static_cast<std::ptrdiff_t>(bodyList_.size());
    const int substeps = std::max(1, softTuning_.substeps);
    const float h = dt / static_cast<float>(substeps);
    const float grabFraction = 1.0f - std::exp(-300.0f * h);
    for (int sub = 0; sub < substeps; ++sub) {
        // Las lejanas solo se resuelven en uno de cada `stride` sub-pasos, con paso × stride.
        const bool farTurn = (sub + 1) % stride == 0;
        const float farScale = static_cast<float>(stride);
        const bool lastSub = sub == substeps - 1;
        auto solving = [&](std::ptrdiff_t b) { return bodyNear_[b] == 2 || (bodyNear_[b] == 1 && farTurn); };
#pragma omp parallel for schedule(dynamic, 1)
        for (std::ptrdiff_t b = 0; b < nb; ++b) {
            if (bodyNear_[b] == 2)
                soft_body::predict(*bodyList_[b], bodies_, h);
            else if (bodyNear_[b] == 1 && farTurn)
                soft_body::predict(*bodyList_[b], bodies_, h * farScale);
            else if (bodyNear_[b] == 0 && lastSub) // estadístico: un solo avance rígido por paso
                soft_body::predict(*bodyList_[b], bodies_, dt);
        }
        for (std::size_t k = 0; k < members_.size(); ++k)
            memberActive_[k] = solving(static_cast<std::ptrdiff_t>(memberBody_[k])) && bodyTouch_[memberBody_[k]];

        if (grabSlot_ != kNoGrab) {
            const std::uint32_t s = grabSlot_;
            const Vec2 goal = grabGoal();
            bodies_.x[s] += (goal.x - bodies_.x[s]) * grabFraction;
            bodies_.y[s] += (goal.y - bodies_.y[s]) * grabFraction;
        }

        // XPBD: restricciones internas y contactos se intercalan en cada iteración, para que
        // ninguno de los dos tenga siempre la última palabra.
        // Cada iteración aplica su fracción del sub-paso: las tasas [1/s] no cambian de
        // significado con la cantidad de iteraciones.
        const int iterations = std::max(1, softTuning_.iterations);
        const float hIter = h / static_cast<float>(iterations);
        contacts_.prepare(bodies_, members_, width_, height_, maxRadius, &memberActive_);
        for (int it = 0; it < iterations; ++it) {
            {
                PROFILE_SCOPE("soft_body::solve");
#pragma omp parallel for schedule(dynamic, 1)
                for (std::ptrdiff_t b = 0; b < nb; ++b)
                    if (solving(b)) {
                        soft_body::solve(*bodyList_[b], bodies_, softTuning_, bodyNear_[b] == 2 ? hIter : hIter * farScale);
                        soft_body::confineToDish(*bodyList_[b], bodies_, dish_);
                    }
            }
            if (it >= iterations - std::max(1, softTuning_.contactPasses))
                contacts_.iterate(bodies_, members_, width_, height_, &memberActive_);
            solveLinks(hIter);
        }
    }
}

void World::finishBodies() {
    PROFILE_SCOPE("World::finishBodies");
    if (bodyList_.empty())
        return;
    if (gpuPending_) {
        gpuPending_ = false;
        if (!substepSolver_ || !substepSolver_->collect(bodies_))
            cpuSubsteps(pendingDt_, pendingStride_, pendingMaxRadius_);
    }
    const auto nb = static_cast<std::ptrdiff_t>(bodyList_.size());
    resolvePenetrations();

    // Remallado de membranas plásticas: asigna slots en el pool compartido, por eso es serial.
    for (RingBody* body : bodyList_)
        soft_body::remesh(*body, bodies_, kMembraneSpacing);

#pragma omp parallel for schedule(dynamic, 1)
    for (std::ptrdiff_t b = 0; b < nb; ++b) {
        // Estadísticas (fuera de pantalla): no resuelven restricciones, así que el crecimiento
        // se refleja con un escalado uniforme alrededor del centroide.
        if (bodyNear_[b] == 0)
            soft_body::scaleToRestArea(*bodyList_[b], bodies_);
        // Pared de la placa: los estadísticos se trasladan enteros, los demás ya la respetan
        // en cada iteración y esto solo limpia lo que hayan dejado los contactos.
        if (bodyNear_[b] == 0)
            soft_body::pushIntoDish(*bodyList_[b], bodies_, dish_);
        else
            soft_body::confineToDish(*bodyList_[b], bodies_, dish_);
        soft_body::confineGranules(*bodyList_[b], bodies_);
        soft_body::refresh(*bodyList_[b], bodies_);
        soft_body::wrap(*bodyList_[b], bodies_, width_, height_);
    }
}

void World::classifyLod() {
    // Niveles: 2 = en detalle (en la vista o cerca), 1 = simplificado (sub-pasos espaciados),
    // 0 = estadístico (fuera de pantalla: se traslada como un sólido, sin XPBD ni contactos).
    // 3 = fijo (granos de arena): sin predicción ni XPBD, pero sus aristas siguen en la grilla de
    // contactos, así las células chocan contra ellos (CPU y GPU).
    const std::size_t nb = bodyList_.size();
    bodyNear_.assign(nb, 2);
    nearCount_ = 0;
    const bool lod = focusActive_ && lodStride > 1;
    for (std::size_t b = 0; b < nb; ++b) {
        const RingBody& body = *bodyList_[b];
        const auto e = static_cast<entt::entity>(body.id);
        if (registry_.all_of<Rock>(e)) {
            bodyNear_[b] = 3;
            continue;
        }
        if (!lod) {
            ++nearCount_;
            continue;
        }
        const Vec2 half = (body.aabbMax - body.aabbMin) * 0.5f;
        const float dx = std::abs(minImage(body.centroid.x - focusCenter_.x, width_));
        const float dy = std::abs(minImage(body.centroid.y - focusCenter_.y, height_));
        // Margen: lo que entra a la vista ya está asentado. Los eventos de topología y las
        // interacciones físicas finas (división, fagocitosis, conjugación) siempre en detalle.
        bool near = dx < focusHalf_.x + half.x + kLodNearMargin && dy < focusHalf_.y + half.y + kLodNearMargin;
        near = near || e == player_ || registry_.any_of<Dividing, PhagocyticCup, Conjugating, Captured, Lichenized>(e);
        const bool mid = dx < focusHalf_.x + half.x + kLodMidMargin && dy < focusHalf_.y + half.y + kLodMidMargin;
        bodyNear_[b] = near ? 2 : (mid ? 1 : 0);
        nearCount_ += near ? 1 : 0;
    }
}

void World::markTouching(float contactRange, float dt) {
    // Una célula sola en el agua no necesita contactos: solo las que están cerca de otra (o las
    // de membrana plástica, que pueden plegarse sobre sí mismas). Así se ahorra la mayor parte
    // del trabajo de colisiones.
    PROFILE_SCOPE("World::markTouching");
    rebuildBuckets();
    const std::size_t nb = bodyList_.size();
    bodyTouch_.assign(nb, 0);
    // Cuánto puede acercarse cada una en este paso: su nado más el agua (con holgura).
    bodyReach_.resize(nb);
    for (std::size_t b = 0; b < nb; ++b) {
        const auto e = static_cast<entt::entity>(bodyList_[b]->id);
        const Locomotion* loco = registry_.try_get<Locomotion>(e);
        const float speed = (loco ? std::abs(loco->speed) : 0.0f) + length(fluid_.velocityAt(bodyList_[b]->centroid)) + 5.0f;
        bodyReach_[b] = 1.5f * speed * dt;
    }
    float maxReach = 0.0f;
    for (float r : bodyReach_)
        maxReach = std::max(maxReach, r);
    for (std::size_t b = 0; b < nb; ++b) {
        const RingBody& body = *bodyList_[b];
        if (body.membraneReservoirRate > 0.0f)
            bodyTouch_[b] = 1;
        const float margin = contactRange + bodyReach_[b] + maxReach;
        const int x0 = static_cast<int>(std::floor((body.aabbMin.x - margin) / bucketSize_));
        const int x1 = static_cast<int>(std::floor((body.aabbMax.x + margin) / bucketSize_));
        const int y0 = static_cast<int>(std::floor((body.aabbMin.y - margin) / bucketSize_));
        const int y1 = static_cast<int>(std::floor((body.aabbMax.y + margin) / bucketSize_));
        for (int by = y0; by <= std::min(y1, y0 + bucketsY_ - 1); ++by)
            for (int bx = x0; bx <= std::min(x1, x0 + bucketsX_ - 1); ++bx)
                for (std::uint32_t o : buckets_[static_cast<std::size_t>(wrapIndex(by, bucketsY_)) * bucketsX_ + wrapIndex(bx, bucketsX_)]) {
                    if (o <= b || (bodyTouch_[b] && bodyTouch_[o]))
                        continue;
                    const RingBody& other = *bodyList_[o];
                    const Vec2 shift = imageShift(other, body.centroid);
                    const float m = contactRange + bodyReach_[b] + bodyReach_[o];
                    if (other.aabbMax.x + shift.x < body.aabbMin.x - m || other.aabbMin.x + shift.x > body.aabbMax.x + m ||
                        other.aabbMax.y + shift.y < body.aabbMin.y - m || other.aabbMin.y + shift.y > body.aabbMax.y + m)
                        continue;
                    if (other.aabbMax.x + shift.x < body.aabbMin.x - margin || other.aabbMin.x + shift.x > body.aabbMax.x + margin ||
                        other.aabbMax.y + shift.y < body.aabbMin.y - margin || other.aabbMin.y + shift.y > body.aabbMax.y + margin)
                        continue;
                    bodyTouch_[b] = bodyTouch_[o] = 1;
                }
    }
}

void World::limitApproach(float dt, float contactRange) {
    PROFILE_SCOPE("World::limitApproach");
    const std::size_t nb = bodyList_.size();
    // Movilidad de cada cuerpo (0 = no se mueve: granos, sésiles) y grupo (partes de un mismo
    // organismo no se empujan entre sí).
    bodyMobility_.assign(nb, 0.0f);
    bodySkip_.assign(nb, 0);
    for (std::size_t b = 0; b < nb; ++b) {
        const RingBody& body = *bodyList_[b];
        const auto e = static_cast<entt::entity>(body.id);
        // Presas sujetas, copas, divisiones y conjugaciones se tocan a propósito.
        bodySkip_[b] = bodyNear_[b] != 2 || registry_.any_of<PhagocyticCup, Captured, Dividing, Conjugating, Lichenized>(e);
        if (registry_.any_of<Rock, Anchored>(e))
            continue;
        float inv = 0.0f;
        for (std::uint32_t s : body.ring)
            inv += bodies_.invMass[s];
        bodyMobility_[b] = inv / static_cast<float>(body.ring.size() * body.ring.size());
    }
    // Cotas para descartar puntos sin buscar su arista más cercana (no cambia el resultado): un
    // punto de P solo frena si gap ≤ acercamiento·dt, con gap = d − (rP + rQ) y acercamiento ≤
    // |vP| + |vQ|. Así d ≤ rP + rQ + (|vP| + |vQ|)·dt, y d nunca es menor que la distancia a la caja
    // del contorno de Q.
    bodyPolyMin_.resize(nb);
    bodyPolyMax_.resize(nb);
    bodyRadiusMax_.resize(nb);
    bodySpeedMax_.resize(nb);
    bodyRingMin_.resize(nb);
    bodyRingMax_.resize(nb);
    constexpr float kHuge = std::numeric_limits<float>::max();
    for (std::size_t b = 0; b < nb; ++b) {
        const RingBody& body = *bodyList_[b];
        Vec2 lo = {kHuge, kHuge}, hi = {-kHuge, -kHuge};
        for (const Vec2& q : body.poly) {
            lo = {std::min(lo.x, q.x), std::min(lo.y, q.y)};
            hi = {std::max(hi.x, q.x), std::max(hi.y, q.y)};
        }
        Vec2 rlo = {kHuge, kHuge}, rhi = {-kHuge, -kHuge};
        float radius = 0.0f, speed2 = 0.0f;
        for (std::uint32_t s : body.ring) {
            rlo = {std::min(rlo.x, bodies_.x[s]), std::min(rlo.y, bodies_.y[s])};
            rhi = {std::max(rhi.x, bodies_.x[s]), std::max(rhi.y, bodies_.y[s])};
            radius = std::max(radius, bodies_.radius[s]);
            speed2 = std::max(speed2, bodies_.vx[s] * bodies_.vx[s] + bodies_.vy[s] * bodies_.vy[s]);
        }
        bodyPolyMin_[b] = lo;
        bodyPolyMax_[b] = hi;
        bodyRingMin_[b] = rlo; // las posiciones no cambian acá (solo las velocidades)
        bodyRingMax_[b] = rhi;
        bodyRadiusMax_[b] = radius;
        bodySpeedMax_[b] = std::sqrt(speed2);
    }
    const float scanRange = contactRange + 3.0f * 600.0f * dt;
    // 1) Pares candidatos, en el orden de siempre (un par puede repetirse si el otro cuerpo
    //    ocupa varias celdas: se conserva).
    approachPairs_.clear();
    for (std::size_t b = 0; b < nb; ++b) {
        if (bodySkip_[b] || !bodyTouch_[b])
            continue;
        const RingBody& A = *bodyList_[b];
        const float reachA = bodyReach_[b];
        const int x0 = static_cast<int>(std::floor((A.aabbMin.x - contactRange - reachA) / bucketSize_));
        const int x1 = static_cast<int>(std::floor((A.aabbMax.x + contactRange + reachA) / bucketSize_));
        const int y0 = static_cast<int>(std::floor((A.aabbMin.y - contactRange - reachA) / bucketSize_));
        const int y1 = static_cast<int>(std::floor((A.aabbMax.y + contactRange + reachA) / bucketSize_));
        for (int by = y0; by <= std::min(y1, y0 + bucketsY_ - 1); ++by)
            for (int bx = x0; bx <= std::min(x1, x0 + bucketsX_ - 1); ++bx)
                for (std::uint32_t o : buckets_[static_cast<std::size_t>(wrapIndex(by, bucketsY_)) * bucketsX_ + wrapIndex(bx, bucketsX_)]) {
                    if (o <= b || bodySkip_[o])
                        continue;
                    if (bodyMobility_[b] + bodyMobility_[o] <= 0.0f)
                        continue;
                    const RingBody& B = *bodyList_[o];
                    const Vec2 shift = imageShift(B, A.centroid);
                    const float m = contactRange + reachA + bodyReach_[o];
                    if (B.aabbMax.x + shift.x < A.aabbMin.x - m || B.aabbMin.x + shift.x > A.aabbMax.x + m ||
                        B.aabbMax.y + shift.y < A.aabbMin.y - m || B.aabbMin.y + shift.y > A.aabbMax.y + m)
                        continue;
                    approachPairs_.push_back({static_cast<std::uint32_t>(b), o, shift});
                }
    }

    // 2) En serie y en orden: cada par lee las velocidades que dejaron los anteriores. (Medido:
    //    ~30 % de los pares empujan y forman cadenas de cientos de niveles; paralelizar por
    //    niveles o por grupos conexos no gana nada y el resultado tiene que ser este.)
    for (const ApproachPair& pr : approachPairs_)
        resolveApproach(pr, dt, scanRange, approachScratch_);
}

// Un par de limitApproach: quita la velocidad de acercamiento de A y B y la reparte.
void World::resolveApproach(const ApproachPair& pr, float dt, float scanRange, ApproachScratch& sc) {
    const std::size_t b = pr.a, o = pr.b;
    const RingBody& A = *bodyList_[b];
    const RingBody& B = *bodyList_[o];
    const float muA = bodyMobility_[b], muB = bodyMobility_[o];
    const ApproachWorst ab = approachScan(b, o, pr.shift, dt, scanRange, sc);         // A contra B (n: de B hacia A)
    const ApproachWorst ba = approachScan(o, b, pr.shift * -1.0f, dt, scanRange, sc); // B contra A (n: de A hacia B)
    // Se mueve la que corresponde: A se aleja de B por `ab.n`; B se aleja de A por `ba.n`.
    // El vidrio de la placa tampoco cede: si una de las dos está contra la pared en
    // la dirección del empuje, la otra absorbe todo el acercamiento.
    auto againstGlass = [&](const RingBody& body, Vec2 dir) {
        if (!dish_.active() || dot(dir, dir) < 1e-6f)
            return false;
        const Vec2 step = dir * (4.0f / length(dir));
        for (std::uint32_t s : body.ring) {
            const Vec2 q = Vec2{bodies_.x[s], bodies_.y[s]} + step;
            const Vec2 c = dish_.clampInside(q, bodies_.radius[s]);
            if (c.x != q.x || c.y != q.y)
                return true;
        }
        return false;
    };
    const Vec2 awayA = ab.need > 0.0f ? ab.n : ba.n * -1.0f; // hacia dónde se aleja A
    float shareA = muA / (muA + muB);
    if (againstGlass(A, awayA))
        shareA = 0.0f;
    else if (againstGlass(B, awayA * -1.0f))
        shareA = 1.0f;
    const float shareB = 1.0f - shareA;
    Vec2 dA, dB;
    if (ab.need > 0.0f) {
        dA = dA + ab.n * (shareA * ab.need);
        dB = dB - ab.n * (shareB * ab.need);
    }
    if (ba.need > 0.0f) {
        dB = dB + ba.n * (shareB * ba.need);
        dA = dA - ba.n * (shareA * ba.need);
    }
    if (dot(dA, dA) == 0.0f && dot(dB, dB) == 0.0f)
        return;
    // Traslación rígida: todo el cuerpo (membrana y orgánulos).
    auto drive = [&](const RingBody& body, Vec2 dv) {
        for (std::uint32_t s : body.ring) {
            bodies_.vx[s] += dv.x;
            bodies_.vy[s] += dv.y;
        }
        for (std::uint32_t s : body.interior) {
            bodies_.vx[s] += dv.x;
            bodies_.vy[s] += dv.y;
        }
    };
    drive(A, dA);
    drive(B, dB);
    // La cota de rapidez sigue valiendo para los pares que faltan: |v + dv| ≤ |v| + |dv|.
    bodySpeedMax_[b] += length(dA);
    bodySpeedMax_[o] += length(dB);
}

// Peor acercamiento de los puntos de P contra el contorno de Q (Q desplazado `qShift`): cuánta
// velocidad hay que quitar y la normal (de Q hacia P).
World::ApproachWorst World::approachScan(std::size_t pi, std::size_t qi, Vec2 qShift, float dt, float scanRange,
                                         ApproachScratch& sc) const {
    constexpr float kHuge = std::numeric_limits<float>::max();
    const RingBody& P = *bodyList_[pi];
    const RingBody& Q = *bodyList_[qi];
    ApproachWorst w;
    const int qn = static_cast<int>(Q.poly.size());
    // Cota de la distancia útil, con holgura para el redondeo.
    const float bound = std::min(scanRange, bodyRadiusMax_[pi] + bodyRadiusMax_[qi] +
                                                (bodySpeedMax_[pi] + bodySpeedMax_[qi]) * dt);
    const float reach = bound * 1.001f + 0.05f;
    const float reach2 = reach * reach;
    // Solo las aristas de Q al alcance de algún punto de P: contienen la más cercana de todo
    // punto que pueda frenar (y sus empates).
    const Vec2 rlo = bodyRingMin_[pi] - qShift, rhi = bodyRingMax_[pi] - qShift;
    const Vec2 qlo = bodyPolyMin_[qi], qhi = bodyPolyMax_[qi];
    if (qlo.x > rhi.x + reach || qhi.x < rlo.x - reach || qlo.y > rhi.y + reach || qhi.y < rlo.y - reach)
        return w; // ninguna arista de Q está al alcance de P
    sc.nearEdges.clear();
    sc.nearLo.clear();
    sc.nearHi.clear();
    for (int i = 0; i < qn; ++i) {
        const Vec2 a = Q.poly[static_cast<std::size_t>(i)];
        const Vec2 c = Q.poly[static_cast<std::size_t>((i + 1) % qn)];
        const Vec2 elo = {std::min(a.x, c.x), std::min(a.y, c.y)};
        const Vec2 ehi = {std::max(a.x, c.x), std::max(a.y, c.y)};
        if (elo.x > rhi.x + reach || ehi.x < rlo.x - reach || elo.y > rhi.y + reach || ehi.y < rlo.y - reach)
            continue;
        sc.nearEdges.push_back(i);
        sc.nearLo.push_back(elo);
        sc.nearHi.push_back(ehi);
    }
    if (sc.nearEdges.empty())
        return w;
    // Lo mismo por tramos de puntos consecutivos de P (cajas más chicas). Los puntos se recorren
    // en el orden de siempre: el resultado no cambia.
    constexpr std::size_t kChunk = 8;
    const std::size_t pn = P.ring.size();
    for (std::size_t c0 = 0; c0 < pn; c0 += kChunk) {
        const std::size_t c1 = std::min(pn, c0 + kChunk);
        Vec2 clo = {kHuge, kHuge}, chi = {-kHuge, -kHuge};
        for (std::size_t k = c0; k < c1; ++k) {
            const std::uint32_t s = P.ring[k];
            clo = {std::min(clo.x, bodies_.x[s]), std::min(clo.y, bodies_.y[s])};
            chi = {std::max(chi.x, bodies_.x[s]), std::max(chi.y, bodies_.y[s])};
        }
        clo = clo - qShift;
        chi = chi - qShift;
        sc.chunkEdges.clear();
        sc.chunkLo.clear();
        sc.chunkHi.clear();
        Vec2 lo = {kHuge, kHuge}, hi = {-kHuge, -kHuge}; // caja de esas aristas
        for (std::size_t k = 0; k < sc.nearEdges.size(); ++k) {
            const Vec2 elo = sc.nearLo[k], ehi = sc.nearHi[k];
            if (elo.x > chi.x + reach || ehi.x < clo.x - reach || elo.y > chi.y + reach || ehi.y < clo.y - reach)
                continue;
            sc.chunkEdges.push_back(sc.nearEdges[k]);
            sc.chunkLo.push_back(elo);
            sc.chunkHi.push_back(ehi);
            lo = {std::min(lo.x, elo.x), std::min(lo.y, elo.y)};
            hi = {std::max(hi.x, ehi.x), std::max(hi.y, ehi.y)};
        }
        const int ne = static_cast<int>(sc.chunkEdges.size());
        if (ne == 0)
            continue;
        for (std::size_t k = c0; k < c1; ++k) {
            const std::uint32_t s = P.ring[k];
            const Vec2 p = Vec2{bodies_.x[s], bodies_.y[s]} - qShift;
            const float bx = std::max({lo.x - p.x, p.x - hi.x, 0.0f});
            const float by = std::max({lo.y - p.y, p.y - hi.y, 0.0f});
            if (bx * bx + by * by > reach2)
                continue;
            // Cota superior de la distancia mínima: el vértice más cercano. Solo se evalúan las
            // aristas cuya caja está más cerca que eso (con holgura muy por encima del redondeo),
            // en orden creciente: la arista elegida, aun en un empate, es la misma.
            float upper = kHuge;
            for (int k = 0; k < ne; ++k) {
                const Vec2 a = Q.poly[static_cast<std::size_t>(sc.chunkEdges[static_cast<std::size_t>(k)])];
                upper = std::min(upper, dot(p - a, p - a));
            }
            const int last = sc.chunkEdges[static_cast<std::size_t>(ne - 1)];
            const Vec2 z = Q.poly[static_cast<std::size_t>((last + 1) % qn)];
            upper = std::min(upper, dot(p - z, p - z)) * 1.0001f + 1e-6f;
            polygon::Closest cl;
            for (int k = 0; k < ne; ++k) {
                const Vec2 elo = sc.chunkLo[static_cast<std::size_t>(k)], ehi = sc.chunkHi[static_cast<std::size_t>(k)];
                const float ex = std::max({elo.x - p.x, p.x - ehi.x, 0.0f});
                const float ey = std::max({elo.y - p.y, p.y - ehi.y, 0.0f});
                if (ex * ex + ey * ey <= upper)
                    polygon::closestOnEdge(Q.poly.data(), qn, sc.chunkEdges[static_cast<std::size_t>(k)], p, cl);
            }
            const float d = std::sqrt(cl.dist2);
            if (d < 1e-4f || d > scanRange)
                continue;
            const Vec2 out = polygon::inwardNormal(cl.edgeDir, Q.restArea) * -1.0f;
            const Vec2 toP = p - cl.point;
            if (dot(toP, out) <= 0.0f)
                continue; // ya está adentro: lo resuelve la pasada de penetraciones
            const Vec2 n = toP * (1.0f / d);
            const std::uint32_t qa = Q.ring[static_cast<std::size_t>(cl.edge)];
            const std::uint32_t qb = Q.ring[static_cast<std::size_t>((cl.edge + 1) % qn)];
            const Vec2 vq = Vec2{bodies_.vx[qa], bodies_.vy[qa]} * (1.0f - cl.t) + Vec2{bodies_.vx[qb], bodies_.vy[qb]} * cl.t;
            const float approach = -dot(Vec2{bodies_.vx[s], bodies_.vy[s]} - vq, n);
            const float gap = d - (bodies_.radius[s] + bodies_.radius[qa]);
            if (approach <= 0.0f || gap > approach * dt)
                continue;
            const float need = approach - std::max(gap, 0.0f) / dt;
            if (need > w.need)
                w = {need, n};
        }
    }
    return w;
}

void World::rebuildBuckets() {
    for (auto& bucket : buckets_)
        bucket.clear();
    for (std::uint32_t b = 0; b < bodyList_.size(); ++b) {
        const RingBody& body = *bodyList_[b];
        const int x0 = static_cast<int>(std::floor(body.aabbMin.x / bucketSize_));
        const int x1 = static_cast<int>(std::floor(body.aabbMax.x / bucketSize_));
        const int y0 = static_cast<int>(std::floor(body.aabbMin.y / bucketSize_));
        const int y1 = static_cast<int>(std::floor(body.aabbMax.y / bucketSize_));
        for (int by = y0; by <= y1; ++by)
            for (int bx = x0; bx <= x1; ++bx)
                buckets_[static_cast<std::size_t>(wrapIndex(by, bucketsY_)) * bucketsX_ +
                         wrapIndex(bx, bucketsX_)]
                    .push_back(b);
    }
}

// Red de seguridad: si una partícula de membrana quedó dentro de otra célula (compresión
// extrema), se la saca al borde más cercano. Jacobi: se calcula todo y después se aplica.
void World::resolvePenetrations() {
    PROFILE_SCOPE("World::resolvePenetrations");
    for (RingBody* body : bodyList_)
        soft_body::refresh(*body, bodies_);
    rebuildBuckets();
    const auto n = static_cast<std::ptrdiff_t>(members_.size());
    penetration_.assign(members_.size(), Vec2{});
#pragma omp parallel for schedule(static)
    for (std::ptrdiff_t k = 0; k < n; ++k) {
        const ContactMember& me = members_[k];
        // Un cuerpo que no se acerca a ningún otro no puede haber quedado adentro de otro.
        if (memberBody_.size() == members_.size() && bodyTouch_.size() == bodyList_.size() && !bodyTouch_[memberBody_[k]])
            continue;
        if (memberBody_.size() == members_.size() && bodyNear_.size() == bodyList_.size() && bodyNear_[memberBody_[k]] == 3)
            continue; // el grano no se mueve: la célula es la que sale
        const float x = bodies_.x[me.slot], y = bodies_.y[me.slot];
        const int bx = wrapIndex(static_cast<int>(std::floor(wrapCoord(x, width_) / bucketSize_)), bucketsX_);
        const int by = wrapIndex(static_cast<int>(std::floor(wrapCoord(y, height_) / bucketSize_)), bucketsY_);
        for (std::uint32_t b : buckets_[static_cast<std::size_t>(by) * bucketsX_ + bx]) {
            const RingBody& other = *bodyList_[b];
            if (other.id == me.owner || members_[k].group == other.id)
                continue;
            const Vec2 q = other.centroid + Vec2{minImage(x - other.centroid.x, width_),
                                                 minImage(y - other.centroid.y, height_)};
            if (q.x < other.aabbMin.x || q.x > other.aabbMax.x || q.y < other.aabbMin.y ||
                q.y > other.aabbMax.y)
                continue;
            const int pn = static_cast<int>(other.poly.size());
            if (!polygon::contains(other.poly.data(), pn, q))
                continue;
            const polygon::Closest cl = polygon::closestOnBoundary(other.poly.data(), pn, q);
            const float r = bodies_.radius[me.slot] + bodies_.radius[other.ring[0]];
            penetration_[k] = cl.point - polygon::inwardNormal(cl.edgeDir, other.restArea) * r - q;
            break;
        }
    }
    for (std::size_t k = 0; k < members_.size(); ++k) {
        bodies_.x[members_[k].slot] += penetration_[k].x;
        bodies_.y[members_[k].slot] += penetration_[k].y;
    }
}

void World::excludeFromCells() {
    PROFILE_SCOPE("World::excludeFromCells");
    collectBodies(); // la fagocitosis pudo destruir cuerpos
    if (bodyList_.empty())
        return;
    rebuildBuckets();

    const auto nb = static_cast<std::ptrdiff_t>(bacteria_.size());
#pragma omp parallel for schedule(static)
    for (std::ptrdiff_t i = 0; i < nb; ++i)
        pushOutOfCells(bacteria_.x[i], bacteria_.y[i]);
    const auto na = static_cast<std::ptrdiff_t>(ambient_.size());
#pragma omp parallel for schedule(static)
    for (std::ptrdiff_t i = 0; i < na; ++i)
        pushOutOfCells(ambient_.x[i], ambient_.y[i]);
    // Las cuentas de las cianobacterias tampoco entran en las células (cadena contigua: se
    // trabaja con una copia envuelta y se aplica el desplazamiento).
    const auto nf = static_cast<std::ptrdiff_t>(cyano_.filaments.size());
#pragma omp parallel for schedule(dynamic, 4)
    for (std::ptrdiff_t f = 0; f < nf; ++f) {
        CyanoFilament& fil = cyano_.filaments[f];
        for (std::size_t k = 0; k < fil.x.size(); ++k) {
            float wx = wrapCoord(fil.x[k], width_), wy = wrapCoord(fil.y[k], height_);
            const float ox = wx, oy = wy;
            pushOutOfCells(wx, wy);
            fil.x[k] += minImage(wx - ox, width_);
            fil.y[k] += minImage(wy - oy, height_);
        }
    }
}

void World::pushOutOfCells(float& x, float& y) const {
    const int bx = wrapIndex(static_cast<int>(std::floor(x / bucketSize_)), bucketsX_);
    const int by = wrapIndex(static_cast<int>(std::floor(y / bucketSize_)), bucketsY_);
    for (std::uint32_t b : buckets_[static_cast<std::size_t>(by) * bucketsX_ + bx]) {
        const RingBody& body = *bodyList_[b];
        // Imagen del punto más cercana a la célula (las células no se envuelven por partícula).
        const Vec2 q = body.centroid + Vec2{minImage(x - body.centroid.x, width_),
                                            minImage(y - body.centroid.y, height_)};
        if (q.x < body.aabbMin.x || q.x > body.aabbMax.x || q.y < body.aabbMin.y ||
            q.y > body.aabbMax.y)
            continue;
        const int n = static_cast<int>(body.poly.size());
        if (!polygon::contains(body.poly.data(), n, q))
            continue;
        const polygon::Closest cl = polygon::closestOnBoundary(body.poly.data(), n, q);
        const float margin = bodies_.radius[body.ring[0]] + 0.5f;
        const Vec2 out = cl.point - polygon::inwardNormal(cl.edgeDir, body.restArea) * margin;
        x = wrapCoord(out.x, width_);
        y = wrapCoord(out.y, height_);
        return;
    }
}

// ---------------------------------------------------------------------------
// Jugador: locomoción y alimentación
// ---------------------------------------------------------------------------

CellSpec speciesSpec(LocomotionType type) { return speciesInfo(playerSpecies(type)).spec; }

namespace {
// Biomasa de nacimiento de la especie con un gen de tamaño dado (masa ∝ tamaño³).
float targetBirthBiomassOf(SpeciesId species, float size) {
    const CellSpec base = speciesInfo(species).spec;
    return cellMassPmol(units::kPi * base.radiusX * base.radiusY) * size * size * size *
           speciesInfo(species).dryMassScale;
}

SpeciesId speciesOfEntity(const entt::registry& reg, entt::entity e) {
    if (const Species* sp = reg.try_get<Species>(e))
        return sp->id;
    return playerSpecies(reg.get<Locomotion>(e).type);
}
} // namespace

void World::applyTraits(entt::entity e) {
    const Heredity& h = registry_.get<Heredity>(e);
    const SpeciesInfo& info = speciesInfo(speciesOfEntity(registry_, e));
    Locomotion& loco = registry_.get<Locomotion>(e);
    loco.power = h.traits.speed * info.speedScale * viscositySpeed_;
    registry_.get<Digestion>(e).rateScale = h.traits.digestion;
    Metabolism& meta = registry_.get<Metabolism>(e);
    meta.photosynthetic = h.traits.photosynthesis > 0.0f;
    meta.targetBirthBiomass = targetBirthBiomassOf(speciesOfEntity(registry_, e), h.traits.sizeScale);
    meta.fissions = info.fissions;
    CellShape& shape = registry_.get<CellShape>(e);
    shape.shapeRate = info.spec.shapeRate * h.traits.rigidity;
    RingBody& body = registry_.get<RingBody>(e);
    if (body.membraneReservoirRate <= 0.0f && !registry_.any_of<Dividing, Cyst>(e))
        body.shapeRate = shape.shapeRate;
}

entt::entity World::spawnOrganism(SpeciesId species, Vec2 center, const Genome& genome, std::uint32_t run,
                                  float angle) {
    const SpeciesInfo& info = speciesInfo(species);
    CellSpec spec = info.spec;
    spec.center = center;
    spec.angle = angle;
    const Traits traits = traitsOf(genome, info.locomotion);
    spec.radiusX *= traits.sizeScale;
    spec.radiusY *= traits.sizeScale;
    spec.shapeRate *= traits.rigidity;
    const entt::entity e = spawnCell(spec);
    RingBody& body = registry_.get<RingBody>(e);
    Locomotion loco;
    loco.type = info.locomotion;
    // Citostoma: en el ciliado, en el surco oral (lado ventral); en el flagelado, en el polo
    // anterior junto al flagelo.
    if (info.feeding == FeedingMode::OralGroove)
        loco.mouthRing = static_cast<std::uint32_t>(0.15f * static_cast<float>(body.ring.size()));
    if (info.mouthAtRear)
        loco.mouthRing = static_cast<std::uint32_t>(body.ring.size() / 2);
    if (info.locomotion == LocomotionType::Pseudopod)
        soft_body::makePlastic(body, bodies_, 1.0f);
    Metabolism meta;
    meta.biomass = meta.birthBiomass = cellMassPmol(body.restArea) * info.dryMassScale;
    meta.reserves = 0.3f * meta.biomass;
    registry_.emplace<Species>(e, Species{species});
    registry_.emplace<Locomotion>(e, loco);
    registry_.emplace<Digestion>(e);
    registry_.emplace<Metabolism>(e, meta);
    registry_.emplace<Lineage>(e, Lineage{run, 0});
    registry_.emplace<Heredity>(e, Heredity{genome, traits});
    applyTraits(e);
    if (Metabolism& m = registry_.get<Metabolism>(e); m.photosynthetic) {
        m.phosphorus = -exchange(phosphate_, center, -metabolismParams_.phosphorusRatio * m.biomass, Flow::Spawn);
        m.nitrogen = -exchange(nitrogen_, center, -metabolismParams_.nitrogenRatio * m.biomass, Flow::Spawn);
    }
    seedMicronutrients(registry_.get<Metabolism>(e), info, center);
    Rng rng(config_.seed, rng_stream::kOrganelles, body.id, step_);
    refillOrganelles(body, bodies_, genome, rng);
    return e;
}

entt::entity World::spawnOrganism(LocomotionType type, Vec2 center, const Genome& genome) {
    return spawnOrganism(playerSpecies(type), center, genome, run_);
}

entt::entity World::spawnPlayer(LocomotionType type, Vec2 center, const Genome* genome) {
    const entt::entity e = spawnOrganism(playerSpecies(type), center, genome ? *genome : speciesGenome(type), run_);
    registry_.emplace<Player>(e);
    player_ = e;
    return e;
}

void World::populateEcosystem(float scale) {
    // Especies silvestres (linaje 0), con algo de variación genética y tipos de apareamiento
    // mezclados. Se ubican sin superponerse y lejos del centro, donde nace el jugador.
    const Vec2 middle = {0.5f * width_, 0.5f * height_};
    std::uint64_t k = 0;
    // Las más grandes primero (las colonias no entrarían entre las demás).
    int order[kSpeciesCount];
    for (int sp = 0; sp < kSpeciesCount; ++sp)
        order[sp] = sp;
    std::stable_sort(order, order + kSpeciesCount, [](int a, int b) {
        return speciesInfo(static_cast<SpeciesId>(a)).spec.radiusX > speciesInfo(static_cast<SpeciesId>(b)).spec.radiusX;
    });
    for (int sp : order) {
        const auto id = static_cast<SpeciesId>(sp);
        const SpeciesInfo& info = speciesInfo(id);
        const int count = static_cast<int>(std::round(static_cast<float>(info.initialCount) * scale * config_.speciesWeight[sp]));
        if (info.filament) {
            for (int f = 0; f < count; ++f) {
                Rng rng(config_.seed, rng_stream::kPopulate, 5000 + k++, 0);
                // El filamento entero dentro de la placa: el inicio, a su largo de la pared.
                const float length = static_cast<float>(info.filamentCells) * (2.0f * info.spec.radiusX + 4.0f);
                const Vec2 start = randomPoint(rng, length + info.spec.radiusY + 4.0f);
                spawnFilament(id, start, rng.uniform(0.0f, 6.2831853f), info.filamentCells, 0);
            }
            continue;
        }
        for (int placed = 0, attempts = 0; placed < count && attempts < 200 * count; ++attempts) {
            Rng rng(config_.seed, rng_stream::kPopulate, 1000 + k++, 0);
            const Vec2 p = randomPoint(rng, std::max(info.spec.radiusX, info.spec.radiusY) * 1.5f + 10.0f);
            if (std::abs(minImage(p.x - middle.x, width_)) < 250.0f && std::abs(minImage(p.y - middle.y, height_)) < 200.0f)
                continue;
            // Lugar libre: un círculo del tamaño de la nueva célula (más un margen) que no toque
            // a ninguna otra (prueba contra el contorno real, no contra su caja).
            const float clearance = std::max(info.spec.radiusX, info.spec.radiusY) + 6.0f;
            bool free = true;
            for (auto [e, b] : registry_.view<RingBody>().each()) {
                const Vec2 q = b.centroid + Vec2{minImage(p.x - b.centroid.x, width_), minImage(p.y - b.centroid.y, height_)};
                if (q.x < b.aabbMin.x - clearance || q.x > b.aabbMax.x + clearance || q.y < b.aabbMin.y - clearance ||
                    q.y > b.aabbMax.y + clearance)
                    continue;
                const int pn = static_cast<int>(b.poly.size());
                if (polygon::contains(b.poly.data(), pn, q) ||
                    polygon::closestOnBoundary(b.poly.data(), pn, q).dist2 < clearance * clearance) {
                    free = false;
                    break;
                }
            }
            if (!free)
                continue;
            spawnWild(id, p, rng, true);
            ++placed;
        }
    }
    seedFungi();
}

entt::entity World::spawnWild(SpeciesId id, Vec2 p, Rng& rng, bool randomStage) {
    const SpeciesInfo& info = speciesInfo(id);
    Genome g = speciesGenome(id);
    mutate(g, info.locomotion, rng, 0.5f);
    g[Gene::MatingType] = rng.uniform() < 0.5f ? 0.0f : 1.0f;
    const entt::entity e = spawnOrganism(id, p, g, 0, rng.uniform(0.0f, 2.0f * units::kPi));
    if (randomStage) {
        // Población en distintas etapas del ciclo celular (si no, todas se dividirían juntas).
        Metabolism& m = registry_.get<Metabolism>(e);
        const float stage = rng.uniform(1.0f, info.fissions > 1 ? 3.5f : 1.8f);
        m.biomass *= stage;
        if (m.photosynthetic) { // el P y el N de la biomasa extra salen del agua
            m.phosphorus += -exchange(phosphate_, p, -(stage - 1.0f) * m.phosphorus, Flow::Spawn);
            m.nitrogen += -exchange(nitrogen_, p, -(stage - 1.0f) * m.nitrogen, Flow::Spawn);
        }
        growMicronutrients(m, p, stage);
    }
    if (info.speedScale > 0.0f)
        registry_.emplace<Autopilot>(e);
    if (info.sessile)
        anchorHere(e);
    return e;
}

void World::spawnBeing(Being being, Vec2 at) {
    PROFILE_SCOPE("World::spawnBeing");
    Rng rng(config_.seed, rng_stream::kTools, toolCounter_++, step_);
    const float angle = rng.uniform(0.0f, 2.0f * units::kPi);
    switch (being.kind) {
    case BeingKind::Species: {
        const SpeciesInfo& info = speciesInfo(being.species);
        if (info.filament) { // el filamento arranca en el cursor y entra entero en la placa
            const float length = static_cast<float>(info.filamentCells) * (2.0f * info.spec.radiusX + 4.0f);
            spawnFilament(being.species, dish_.clampInside(at, length + info.spec.radiusY + 4.0f), angle,
                          info.filamentCells, 0);
        } else {
            const float reach = std::max(info.spec.radiusX, info.spec.radiusY) * 1.5f + 10.0f;
            spawnWild(being.species, dish_.clampInside(at, reach), rng, false);
        }
        break;
    }
    case BeingKind::Bacterium:
    case BeingKind::Bacillus:
    case BeingKind::Violaceum: // un racimo: una sola bacteria apenas se ve
        for (int k = 0; k < 30; ++k) {
            const float a = rng.uniform(0.0f, 2.0f * units::kPi), r = 12.0f * std::sqrt(rng.uniform());
            const Vec2 p = dish_.clampInside({at.x + r * std::cos(a), at.y + r * std::sin(a)}, 2.0f);
            bacteria_.add(p.x, p.y, rng.uniform(0.0f, 2.0f * units::kPi), rng.uniform(1.0f, 2.0f),
                          being.kind == BeingKind::Violaceum ? 1 : (being.kind == BeingKind::Bacillus ? 2 : 0));
        }
        break;
    case BeingKind::Bdellovibrio:
        for (int k = 0; k < 10; ++k) {
            const float a = rng.uniform(0.0f, 2.0f * units::kPi), r = 10.0f * std::sqrt(rng.uniform());
            const Vec2 p = dish_.clampInside({at.x + r * std::cos(a), at.y + r * std::sin(a)}, 2.0f);
            bdello_.push(p.x, p.y, rng.uniform(0.0f, 2.0f * units::kPi));
        }
        break;
    case BeingKind::Cyanobacterium:
    case BeingKind::Oscillatoria: {
        const bool osc = being.kind == BeingKind::Oscillatoria;
        const int cells = std::max(8, osc ? config_.oscillatoriaCells : config_.cyanoCells);
        const Vec2 start = dish_.clampInside(at, cyano_.params.spacing * static_cast<float>(cells) + 4.0f);
        cyano_.addFilament(start, angle, cells, rng, osc);
        break;
    }
    case BeingKind::Chytrid: spawnFungus(FungusKind::Chytrid, dish_.clampInside(at, 10.0f), angle, 30000 + toolCounter_); break;
    case BeingKind::Saprolegnia: spawnFungus(FungusKind::Saprolegnia, dish_.clampInside(at, 20.0f), angle, 0); break;
    case BeingKind::Allomyces: spawnFungus(FungusKind::Allomyces, dish_.clampInside(at, 20.0f), angle, 0); break;
    case BeingKind::Lichen: spawnFungus(FungusKind::Mycobiont, dish_.clampInside(at, 20.0f), angle, 0); break;
    case BeingKind::Physarum: spawnPlasmodium(dish_.clampInside(at, 20.0f), angle); break;
    case BeingKind::OatFlake: spawnOatFlake(dish_.clampInside(at, 20.0f)); break;
    case BeingKind::Phage: {
        // Un lisado: ~10⁸ viriones/mL en las 3×3 celdas alrededor del cursor.
        Grid2D& g = phage_.grid();
        const int cx = g.cellX(at.x), cy = g.cellY(at.y);
        for (int j = -1; j <= 1; ++j)
            for (int i = -1; i <= 1; ++i) {
                const int x = wrapIndex(cx + i, g.nx), y = wrapIndex(cy + j, g.ny);
                if (dishMask_.isWater(static_cast<std::size_t>(y) * g.nx + x))
                    g.at(x, y) += 1e-4f;
            }
        break;
    }
    case BeingKind::Clay:
        for (int k = 0; k < 12; ++k) { // un flóculo: una sola partícula de arcilla apenas se ve
            const float a = rng.uniform(0.0f, 2.0f * units::kPi), r = 6.0f * std::sqrt(rng.uniform());
            spawnMineral(MineralKind::Clay, {at.x + r * std::cos(a), at.y + r * std::sin(a)}, rng);
        }
        break;
    case BeingKind::Quartz:
    case BeingKind::Mica:
    case BeingKind::IronOxide:
    case BeingKind::Sulfur:
    case BeingKind::Frustule: spawnMineral(beingMineral(being.kind), at, rng); break;
    case BeingKind::Rock: spawnRock(at, rng.uniform(40.0f, 90.0f), MineralKind::Quartz, rng); break;
    case BeingKind::Sediment: spawnSediment(at, 40, rng); break;
    case BeingKind::Count: break;
    }
}

entt::entity World::setPlayerSpecies(SpeciesId species, Vec2 fallback) {
    PROFILE_SCOPE("World::setPlayerSpecies");
    Vec2 at = fallback;
    float angle = 0.0f, mating = 0.0f;
    std::uint32_t generation = 0;
    if (registry_.valid(player_)) {
        const RingBody& body = registry_.get<RingBody>(player_);
        at = body.centroid;
        const Vec2 h = bodyHeading(body);
        angle = std::atan2(h.y, h.x);
        if (const Lineage* lin = registry_.try_get<Lineage>(player_))
            generation = lin->generation;
        if (const Heredity* her = registry_.try_get<Heredity>(player_))
            mating = her->genome[Gene::MatingType];
        if (grabSlot_ != kNoGrab && bodies_.owner[grabSlot_] == entt::to_integral(player_))
            endGrab();
        // Sin lisis ni evento: no es una muerte, es un cambio de forma de prueba. El fósforo y el
        // nitrógeno del cuerpo viejo vuelven al agua (se conservan).
        float phosphorus = 0.0f;
        if (const Metabolism* m = registry_.try_get<Metabolism>(player_))
            phosphorus += m->phosphorus;
        if (const Digestion* d = registry_.try_get<Digestion>(player_))
            for (const FoodVacuole& v : d->vacuoles)
                phosphorus += v.phosphorus;
        if (phosphorus > 0.0f)
            exchange(phosphate_, at, phosphorus, Flow::Death);
        if (const float nitrogen = carriedNitrogen(player_); nitrogen > 0.0f)
            exchange(nitrogen_, at, nitrogen, Flow::Death);
        if (const float si = releaseMicronutrients(player_, at); si > 0.0f)
            exchange(silicate_, at, si, Flow::Death);
        destroyCell(registry_, bodies_, player_);
        player_ = entt::null;
    }
    const SpeciesInfo& info = speciesInfo(species);
    Genome g = speciesGenome(species);
    g[Gene::MatingType] = mating;
    const float reach = std::max(info.spec.radiusX, info.spec.radiusY) * 1.5f + 10.0f;
    const entt::entity e = spawnOrganism(species, dish_.clampInside(at, reach), g, run_, angle);
    registry_.get<Lineage>(e).generation = generation;
    if (info.sessile)
        anchorHere(e);
    registry_.emplace<Player>(e);
    player_ = e;
    return e;
}

bool World::forceReproduction(entt::entity e) {
    if (!registry_.valid(e) || !registry_.all_of<Metabolism, Species>(e))
        return false;
    // Lo mismo que frena la reproducción en updateLifecycle (o ya está en marcha).
    if (registry_.any_of<Dividing, Conjugating, Cyst, ChytridInfection, Lichenized>(e))
        return false;
    Metabolism& m = registry_.get<Metabolism>(e);
    const MetabolismParams& mp = metabolismParams_;
    const float cost = registry_.all_of<Player>(e) ? mp.playerDivisionCost : mp.npcDivisionCost;
    const float birth = m.targetBirthBiomass > 0.0f ? m.targetBirthBiomass : m.birthBiomass;
    // Los mismos umbrales que readyToDivide (con un margen).
    float need = m.pendingFissions > 0 ? 1.8f * birth : static_cast<float>(1 << m.fissions) * birth;
    need *= 1.02f;
    if (m.biomass < need) {
        const float k = need / m.biomass;
        const Vec2 at = registry_.get<RingBody>(e).centroid;
        if (m.photosynthetic) { // el P y el N de la biomasa extra salen del agua (se conservan)
            m.phosphorus += -exchange(phosphate_, at, -(k - 1.0f) * m.phosphorus, Flow::Spawn);
            m.nitrogen += -exchange(nitrogen_, at, -(k - 1.0f) * m.nitrogen, Flow::Spawn);
        }
        growMicronutrients(m, at, k);
        m.biomass = need;
    }
    m.reserves = std::max(m.reserves, (cost + 0.05f) * m.biomass);
    return true;
}

std::vector<const RingBody*> World::preyFor(const RingBody& predator) const {
    std::vector<const RingBody*> prey;
    const auto predatorEntity = static_cast<entt::entity>(predator.id);
    const Species* predatorSpecies = registry_.try_get<Species>(predatorEntity);
    const Lineage* predatorLineage = registry_.try_get<Lineage>(predatorEntity);
    const float maxArea = feedingParams_.maxPreyAreaRatio * std::abs(predator.restArea);
    // Solapamiento de cajas (con margen de alcance), comparando CENTROS de caja: el
    // pseudópodo puede extenderse lejos del centroide.
    const Vec2 ext = (predator.aabbMax - predator.aabbMin) * 0.5f;
    const Vec2 center = (predator.aabbMax + predator.aabbMin) * 0.5f;
    const float margin = feedingParams_.wrapReach + feedingParams_.contactGap + 10.0f;
    for (auto [e, body] : registry_.view<RingBody>().each()) {
        if (&body == &predator || std::abs(body.restArea) > maxArea || registry_.all_of<Rock>(e))
            continue;
        // Una lórica o testa gruesa no se puede fagocitar.
        if (const Heredity* h = registry_.try_get<Heredity>(e); h && h->traits.armor >= 0.5f)
            continue;
        // Solo las especies de su dieta; nunca la propia especie ni el propio linaje.
        if (const Species* ps = registry_.try_get<Species>(e); ps && predatorSpecies) {
            if (ps->id == predatorSpecies->id || !(speciesInfo(predatorSpecies->id).preyMask & speciesBit(ps->id)))
                continue;
        }
        if (const Lineage* pl = registry_.try_get<Lineage>(e);
            pl && predatorLineage && pl->run > 0 && pl->run == predatorLineage->run)
            continue;
        if (registry_.all_of<Cyst>(e))
            continue; // la pared del quiste no se digiere
        const Vec2 pc = (body.aabbMax + body.aabbMin) * 0.5f;
        const Vec2 d = {minImage(pc.x - center.x, width_), minImage(pc.y - center.y, height_)};
        const Vec2 pe = (body.aabbMax - body.aabbMin) * 0.5f;
        if (std::abs(d.x) < ext.x + pe.x + margin && std::abs(d.y) < ext.y + pe.y + margin)
            prey.push_back(&body);
    }
    return prey;
}

Vec2 World::imageShift(const RingBody& of, Vec2 near) const {
    return Vec2{minImage(of.centroid.x - near.x, width_), minImage(of.centroid.y - near.y, height_)} +
           near - of.centroid;
}

void World::buildPerceptionGrid() {
    percGridX_ = std::max(1, static_cast<int>(width_ / kPerceptionCell));
    percGridY_ = std::max(1, static_cast<int>(height_ / kPerceptionCell));
    percGrid_.resize(static_cast<std::size_t>(percGridX_) * percGridY_);
    for (auto& cell : percGrid_)
        cell.clear();
    for (auto [e, body] : registry_.view<RingBody>().each()) {
        const int cx = wrapIndex(static_cast<int>(body.centroid.x / width_ * percGridX_), percGridX_);
        const int cy = wrapIndex(static_cast<int>(body.centroid.y / height_ * percGridY_), percGridY_);
        percGrid_[static_cast<std::size_t>(cy) * percGridX_ + cx].push_back(e);
    }
}

void World::applyMotion(float dt) {
    PROFILE_SCOPE("World::applyMotion");
    buildPerceptionGrid();
    // Controles: el jugador por input; el resto del linaje, por su piloto automático.
    for (auto [e, body, loco] : registry_.view<RingBody, Locomotion>().each()) {
        const bool engulfing = registry_.all_of<PhagocyticCup>(e);
        bool disturbed = false;
        if (e == player_) {
            loco.forward = playerInput_.forward;
            loco.turn = playerInput_.turn;
            loco.target = playerInput_.aim;
            loco.active = (loco.type == LocomotionType::Flagellum && playerInput_.propel) ||
                          (loco.type == LocomotionType::Pseudopod && playerInput_.pseudopod);
        } else if (Autopilot* pilot = registry_.try_get<Autopilot>(e)) {
            if (registry_.all_of<Cyst>(e))
                continue; // el quiste no se mueve
            Rng rng(config_.seed, rng_stream::kAutopilot, body.id, step_);
            const SpeciesId sp = speciesOfEntity(registry_, e);
            Perception sense = perceive(e, body, sp);
            if (isSatiated(e))
                sense.hasPrey = false; // saciada: no persigue presas
            steerAutopilot(*pilot, loco, body, speciesInfo(sp), sense, rng, dt);
            // Perturbación mecánica: un choque, una amenaza cerca o una corriente fuerte.
            disturbed = sense.bumped || (sense.hasThreat && sense.threatDistance < 40.0f) ||
                        length(fluid_.velocityAt(body.centroid)) > 40.0f;
        } else {
            continue;
        }
        // Sésiles: no nadan; el tallo las sostiene (la corona ciliar solo hace corriente).
        if (Anchored* anchor = registry_.try_get<Anchored>(e)) {
            loco.forward = loco.turn = 0.0f;
            loco.active = false;
            applyAnchor(e, body, *anchor, disturbed, dt);
        }
        // Atrapada por un suctorio: los tentáculos la sujetan y no puede nadar.
        if (const Captured* cap = registry_.try_get<Captured>(e); cap && registry_.valid(cap->by)) {
            loco.forward = loco.turn = 0.0f;
            loco.active = false;
            pullTogether(body, registry_.get<RingBody>(cap->by));
        }
        // Unida a un hongo liquenizante: el haustorio la mantiene en su lugar.
        if (const Lichenized* li = registry_.try_get<Lichenized>(e)) {
            loco.forward = loco.turn = 0.0f;
            loco.active = false;
            holdAt(body, li->point);
        }
        // Lacrymaria: mientras estira el cuello, el cuerpo se queda quieto.
        if (const Neck* neck = registry_.try_get<Neck>(e); neck && neck->length > 5.0f)
            loco.forward = loco.turn = 0.0f;
        // La ameba deja de desplazarse mientras forma una copa fagocítica.
        if (loco.type == LocomotionType::Pseudopod && engulfing)
            loco.active = false;
        // Durante la conjugación la pareja queda unida por la región oral y no nada.
        if (const Conjugating* cj = registry_.try_get<Conjugating>(e);
            cj && registry_.valid(cj->partner) && registry_.all_of<RingBody>(cj->partner)) {
            loco.forward = loco.turn = 0.0f;
            loco.active = false;
            pullTogether(body, registry_.get<RingBody>(cj->partner));
        }
        applyLocomotion(body, bodies_, loco, locomotionParams_, simTime_, width_, height_);
        if (loco.type == LocomotionType::Pseudopod)
            updateCup(e, body, dt);
    }
}

void World::updateCup(entt::entity e, RingBody& body, float dt) {
    PROFILE_SCOPE("World::updateCup");
    // Fagocitosis: el contacto con una presa elegible inicia una copa; la copa la forra.
    if (PhagocyticCup* cup = registry_.try_get<PhagocyticCup>(e)) {
        const auto preyEntity = static_cast<entt::entity>(cup->preyId);
        if (!registry_.valid(preyEntity) || !registry_.all_of<RingBody>(preyEntity)) {
            registry_.remove<PhagocyticCup>(e);
            return;
        }
        const RingBody& prey = registry_.get<RingBody>(preyEntity);
        const Vec2 shift = imageShift(prey, body.centroid);
        if (std::find(body.ring.begin(), body.ring.end(), cup->anchorSlot) == body.ring.end()) {
            const std::uint32_t again = findContact(body, bodies_, prey, shift, feedingParams_);
            if (again == ~0u) {
                releasePrey(prey, bodies_, *cup);
                registry_.remove<PhagocyticCup>(e); // la presa se soltó
                return;
            }
            cup->anchorSlot = again;
        }
        // Red de seguridad: si la membrana se cruzó, se desenreda y se abandona la copa.
        if (soft_body::untangle(body, bodies_)) {
            releasePrey(prey, bodies_, *cup);
            registry_.remove<PhagocyticCup>(e);
            return;
        }
        if (body.ring.size() > 1500) { // tope de seguridad: se abandona la copa
            releasePrey(prey, bodies_, *cup);
            registry_.remove<PhagocyticCup>(e);
            return;
        }
        // Otras células pegadas a la presa: los labios no entran entre ellas.
        cupObstacles_.clear();
        const Vec2 lo = prey.aabbMin + shift - Vec2{12.0f, 12.0f}, hi = prey.aabbMax + shift + Vec2{12.0f, 12.0f};
        for (const RingBody* other : bodyList_) {
            if (other == &body || other == &prey)
                continue;
            const Vec2 os = imageShift(*other, prey.centroid + shift);
            if (other->aabbMax.x + os.x < lo.x || other->aabbMin.x + os.x > hi.x || other->aabbMax.y + os.y < lo.y ||
                other->aabbMin.y + os.y > hi.y)
                continue;
            cupObstacles_.push_back({other, os});
        }
        growLips(body, bodies_, prey, shift, *cup, feedingParams_, dt, cupObstacles_);
        applyCup(body, bodies_, prey, shift, *cup, feedingParams_, dt, cupObstacles_);
        return;
    }
    if (isSatiated(e))
        return; // digiriendo una presa entera
    if (body.ring.size() > 1500)
        return; // tope de seguridad: la membrana no sigue creciendo
    for (const RingBody* prey : preyFor(body)) {
        if (smallPrey(*prey))
            continue; // las presas chicas se fagocitan por contacto (updateFeeding)
        if (registry_.any_of<PhagocyticCup>(static_cast<entt::entity>(prey->id)))
            continue;
        const auto preyEntity = static_cast<entt::entity>(prey->id);
        if (registry_.all_of<Discharge>(preyEntity))
            continue; // acaba de disparar sus tricocistos: el depredador retrocede
        const std::uint32_t contact =
            findContact(body, bodies_, *prey, imageShift(*prey, body.centroid), feedingParams_);
        if (contact == ~0u)
            continue;
        if (repelledByTrichocysts(e, preyEntity))
            continue;
        PhagocyticCup& cup = registry_.emplace<PhagocyticCup>(e, PhagocyticCup{prey->id, contact});
        holdPrey(*prey, bodies_, cup);
        return;
    }
}

void World::updateFeeding(float dt) {
    PROFILE_SCOPE("World::updateFeeding");
    auto feeders = registry_.view<RingBody, Locomotion, Digestion>();
    if (feeders.begin() == feeders.end())
        return;
    // Grilla de bacterias para las consultas de todas las bocas; las presas se marcan como
    // tomadas y se quitan todas juntas al final (quitar antes cambiaría los índices).
    bacteriaGrid_.build(bacteria_.x, bacteria_.y, width_, height_, 8.0f);
    takenBacteria_.assign(bacteria_.size(), 0);
    takenAmbient_.assign(ambient_.size(), 0);
    engulfs_.clear();

    swallows_.clear();
    for (auto [e, body, loco, dig] : feeders.each()) {
        if (registry_.all_of<Cyst>(e))
            continue;
        const FeedingMode mode = speciesInfo(speciesOfEntity(registry_, e)).feeding;
        if (mode == FeedingMode::None)
            continue;
        if (mode == FeedingMode::Raptorial) {
            hunt(e, body);
            continue;
        }
        if (mode == FeedingMode::Tentacles) {
            trapWithTentacles(e, body);
            continue;
        }
        if (mode == FeedingMode::NeckStrike) {
            strikeWithNeck(e, body, dt);
            continue;
        }
        if (mode == FeedingMode::Phagocytosis) {
            // Presas más chicas que la resolución de la membrana (~4 puntos): la copa no se
            // distingue a esta escala; se fagocitan por contacto, de a una.
            if (!isSatiated(e) && !registry_.all_of<PhagocyticCup>(e)) {
                for (const RingBody* prey : preyFor(body)) {
                    const auto pe = static_cast<entt::entity>(prey->id);
                    if (!smallPrey(*prey) || registry_.all_of<Discharge>(pe))
                        continue;
                    if (std::any_of(swallows_.begin(), swallows_.end(), [&](const auto& sw) { return sw.second == pe; }))
                        continue;
                    if (findContact(body, bodies_, *prey, imageShift(*prey, body.centroid), feedingParams_) == ~0u)
                        continue;
                    if (!repelledByTrichocysts(e, pe))
                        swallows_.push_back({e, pe});
                    break;
                }
            }
            if (PhagocyticCup* cup = registry_.try_get<PhagocyticCup>(e)) {
                const auto preyEntity = static_cast<entt::entity>(cup->preyId);
                if (registry_.valid(preyEntity) && registry_.all_of<RingBody>(preyEntity)) {
                    const RingBody& prey = registry_.get<RingBody>(preyEntity);
                    const Vec2 preyCenter = prey.centroid + imageShift(prey, body.centroid);
                    Closure closure;
                    if (findClosure(body, bodies_, preyCenter, feedingParams_, closure)) {
                        engulfs_.push_back({e, preyEntity, std::move(closure)});
                    } else if (cup->coverage > cup->bestCoverage + 0.02f) {
                        cup->bestCoverage = cup->coverage;
                        cup->stalled = 0.0f;
                    } else {
                        cup->stalled += dt;
                        if (cup->stalled > 10.0f) { // la copa dejó de avanzar: se abandona
                            releasePrey(prey, bodies_, *cup);
                            registry_.remove<PhagocyticCup>(e);
                        }
                    }
                }
            }
            feedByContact(body, dig, dt);
        } else {
            feedAtMouth(e, body, loco, dig, dt);
        }
    }
    // Las fagocitosis destruyen entidades: después de recorrer la vista.
    for (const auto& [predator, prey] : swallows_)
        if (registry_.valid(predator) && registry_.valid(prey))
            swallow(predator, prey);
    for (EngulfEvent& ev : engulfs_) {
        if (!registry_.valid(ev.predator) || !registry_.valid(ev.prey))
            continue;
        registry_.remove<PhagocyticCup>(ev.predator);
        engulf(ev.predator, ev.closure, ev.prey);
    }
    removeBacteria(eatenBacteria_);
    removeAmbient(eatenAmbient_);
}

void World::forBacteriaInBox(Vec2 lo, Vec2 hi, const std::function<void(std::size_t)>& fn) const {
    const int gw = bacteriaGrid_.cellsX(), gh = bacteriaGrid_.cellsY();
    const float cw = width_ / static_cast<float>(gw), ch = height_ / static_cast<float>(gh);
    const int x0 = static_cast<int>(std::floor(lo.x / cw)), x1 = static_cast<int>(std::floor(hi.x / cw));
    const int y0 = static_cast<int>(std::floor(lo.y / ch)), y1 = static_cast<int>(std::floor(hi.y / ch));
    const auto& order = bacteriaGrid_.order();
    for (int cy = y0; cy <= std::min(y1, y0 + gh - 1); ++cy)
        for (int cx = x0; cx <= std::min(x1, x0 + gw - 1); ++cx)
            for (std::uint32_t it = bacteriaGrid_.cellBegin(cx, cy); it < bacteriaGrid_.cellEnd(cx, cy); ++it)
                fn(order[it]);
}

void World::removeBacteria(std::vector<std::size_t>& indices) {
    std::sort(indices.rbegin(), indices.rend()); // de atrás hacia adelante: swap-and-pop seguro
    for (std::size_t i : indices)
        bacteria_.remove(i);
    indices.clear();
}

void World::removeAmbient(std::vector<std::size_t>& indices) {
    std::sort(indices.rbegin(), indices.rend());
    for (std::size_t i : indices)
        ambient_.swapRemove(i);
    indices.clear();
}

void World::engulf(entt::entity predator, const Closure& closure, entt::entity preyEntity) {
    PROFILE_SCOPE("World::engulf");
    const RingBody& predatorBefore = registry_.get<RingBody>(predator);
    const CellLook look = registry_.get<CellLook>(preyEntity);
    const RingBody& preyBody = registry_.get<RingBody>(preyEntity);
    const float preyArea = std::abs(preyBody.restArea);
    float preyMass = cellMassPmol(preyArea), preyPhosphorus = 0.0f;
    if (const Metabolism* m = registry_.try_get<Metabolism>(preyEntity)) {
        preyMass = m->biomass + m->reserves;
        preyPhosphorus = m->phosphorus;
    }
    if (const Digestion* pd = registry_.try_get<Digestion>(preyEntity))
        for (const FoodVacuole& v : pd->vacuoles)
            preyPhosphorus += v.phosphorus;
    if (const ChytridInfection* inf = registry_.try_get<ChytridInfection>(preyEntity))
        preyPhosphorus += inf->phosphorus; // el parásito se digiere con ella
    const float preyNitrogen = carriedNitrogen(preyEntity);
    const int pn = static_cast<int>(closure.pocket.size());

    // Todo lo que quedó dentro de la copa entra al fagosoma junto con la presa.
    float extraArea = 0.0f, extraPmol = 0.0f;
    int bacteriaInside = 0;
    for (std::size_t i = 0; i < bacteria_.size(); ++i) {
        if (takenBacteria_[i])
            continue;
        const Vec2 q = predatorBefore.centroid + Vec2{minImage(bacteria_.x[i] - predatorBefore.centroid.x, width_),
                                                      minImage(bacteria_.y[i] - predatorBefore.centroid.y, height_)};
        if (polygon::contains(closure.pocket.data(), pn, q)) {
            takenBacteria_[i] = 1;
            eatenBacteria_.push_back(i);
            extraArea += kBacteriumArea;
            extraPmol += kBacteriumPmol * bacteria_.mass[i];
            ++bacteriaInside;
        }
    }
    if (preyEntity == player_) {
        events_.push_back({WorldEvent::Type::PlayerDied, preyEntity, entt::null, "fagocitada"});
        player_ = entt::null;
        lastDeathCause_ = "fagocitada";
    }
    recordDeath(preyEntity, std::string("fagocitada por ") + speciesInfo(speciesOfEntity(registry_, predator)).name);
    // El predador pudo tener a la presa sujeta en otra copa: se quita al destruirla.
    if (const float si = releaseMicronutrients(preyEntity, preyBody.centroid); si > 0.0f)
        exchange(silicate_, preyBody.centroid, si, Flow::Death);
    destroyCell(registry_, bodies_, preyEntity);
    // Destruir compacta el almacenamiento de EnTT: se vuelven a obtener las referencias.
    RingBody& body = registry_.get<RingBody>(predator);
    Digestion& dig = registry_.get<Digestion>(predator);

    // Fusión de membranas: el tramo de la copa pasa a ser la membrana del fagosoma.
    for (std::uint32_t s : body.ring)
        if (std::find(closure.outerRing.begin(), closure.outerRing.end(), s) == closure.outerRing.end())
            bodies_.release(s);
    if (grabSlot_ != kNoGrab && !bodies_.alive[grabSlot_])
        endGrab();
    soft_body::replaceRing(body, bodies_, closure.outerRing);

    ingest(dig, body, bodies_, closure.preyCenter, preyArea + extraArea, preyMass + extraPmol,
           look.r, look.g, look.b, true, preyPhosphorus, 1.0f, preyNitrogen);
    // El fagosoma nace del tamaño que cabe dentro del anillo nuevo (y se infla en `digest`): el
    // cuello de la copa cortó parte del lugar de la presa.
    if (!dig.vacuoles.empty()) {
        const std::uint32_t slot = dig.vacuoles.back().slot;
        const polygon::Closest cl = polygon::closestOnBoundary(body.poly.data(), static_cast<int>(body.poly.size()),
                                                               {bodies_.x[slot], bodies_.y[slot]});
        bodies_.radius[slot] = std::clamp(0.8f * std::sqrt(cl.dist2), 1.5f, bodies_.radius[slot]);
    }
    soft_body::untangle(body, bodies_); // por si el cierre dejó las puntas de los labios cruzadas
    ++dig.cellsEaten;
    dig.handling = speciesInfo(speciesOfEntity(registry_, predator)).handlingHours * 3600.0f;
    dig.bacteriaEaten += bacteriaInside;
}

void World::feedAtMouth(entt::entity e, RingBody& body, const Locomotion& loco, Digestion& dig, float dt) {
    PROFILE_SCOPE("World::feedAtMouth");
    const std::size_t n = body.ring.size();
    const std::size_t i = loco.mouthRing % n;
    const Vec2 m = {bodies_.x[body.ring[i]], bodies_.y[body.ring[i]]};
    const Vec2 prev = {bodies_.x[body.ring[(i + n - 1) % n]], bodies_.y[body.ring[(i + n - 1) % n]]};
    const Vec2 next = {bodies_.x[body.ring[(i + 1) % n]], bodies_.y[body.ring[(i + 1) % n]]};
    const Vec2 t = (next - prev) * (1.0f / std::max(1e-4f, length(next - prev)));
    const Vec2 site = m + polygon::inwardNormal(t, body.restArea) * 4.0f; // la vacuola se forma adentro
    const float reach = feedingParams_.mouthRadius;
    const bool ciliate = loco.type == LocomotionType::Cilia;
    // Trichonympha traga astillas de madera grandes por su polo posterior.
    const float maxParticle = speciesInfo(speciesOfEntity(registry_, e)).mouthAtRear
                                  ? 6.0f
                                  : (ciliate ? feedingParams_.ciliateMaxParticle : feedingParams_.flagellateMaxParticle);
    const float maxRate = ciliate ? feedingParams_.mouthMaxRate : feedingParams_.flagellateMaxRate;

    // El citostoma tiene un caudal máximo: no traga todo lo que pasa cerca.
    dig.mouthBudget = std::min(dig.mouthBudget + maxRate * dt, maxRate);
    const Vec2 wm = {wrapCoord(m.x, width_), wrapCoord(m.y, height_)};
    forBacteriaInBox(wm - Vec2{reach, reach}, wm + Vec2{reach, reach}, [&](std::size_t k) {
        if (takenBacteria_[k] || dig.mouthBudget < 1.0f)
            return;
        const float dx = minImage(bacteria_.x[k] - m.x, width_);
        const float dy = minImage(bacteria_.y[k] - m.y, height_);
        if (dx * dx + dy * dy < reach * reach) {
            dig.mouthBudget -= 1.0f;
            takenBacteria_[k] = 1;
            eatenBacteria_.push_back(k);
            ++dig.bacteriaEaten;
            if (bacteria_.quorum[k]) // violaceína: tóxica para el protista que la come
                dig.toxin += kViolaceinDose;
            ingest(dig, body, bodies_, site, kBacteriumArea, kBacteriumPmol * bacteria_.mass[k], 170, 150, 140, false,
                   0.0f, bioScale());
        }
    });

    // Filtradores grandes (Stentor, Euplotes): la corriente oral también trae algas chicas, que
    // entran enteras por el citostoma.
    if (const std::uint32_t mask = speciesInfo(speciesOfEntity(registry_, e)).mouthPreyMask; mask && !isSatiated(e)) {
        for (const RingBody* other : bodyList_) {
            const auto oe = static_cast<entt::entity>(other->id);
            const Species* os = registry_.try_get<Species>(oe);
            if (!os || !(mask & speciesBit(os->id)) || !smallPrey(*other) || registry_.any_of<Cyst, Captured>(oe))
                continue;
            if (std::any_of(swallows_.begin(), swallows_.end(), [&](const auto& sw) { return sw.second == oe; }))
                continue;
            const Vec2 shift = imageShift(*other, m);
            if (polygon::closestOnBoundary(other->poly.data(), static_cast<int>(other->poly.size()), m - shift).dist2 <
                reach * reach) {
                swallows_.push_back({e, oe});
                break;
            }
        }
    }

    for (std::size_t k = 0; k < ambient_.size(); ++k) {
        if (takenAmbient_[k] || ambient_.kind[k] != ParticleKind::Detritus || ambient_.radius[k] > maxParticle)
            continue;
        const float dx = minImage(ambient_.x[k] - m.x, width_);
        const float dy = minImage(ambient_.y[k] - m.y, height_);
        const float r = reach + ambient_.radius[k];
        if (dx * dx + dy * dy < r * r) {
            takenAmbient_[k] = 1;
            eatenAmbient_.push_back(k);
            ++dig.detritusEaten;
            const float a = units::kPi * ambient_.radius[k] * ambient_.radius[k];
            ingest(dig, body, bodies_, site, a, ambient_.mass[k], 140, 112, 72, false, 0.0f, bioScale());
        }
    }
}

void World::feedByContact(RingBody& body, Digestion& dig, float dt) {
    PROFILE_SCOPE("World::feedByContact");
    const int n = static_cast<int>(body.poly.size());
    const float reach = bodies_.radius[body.ring[0]] + feedingParams_.contactGap;
    const float chance = 1.0f - std::exp(-feedingParams_.contactRate * dt);
    auto tryEat = [&](float x, float y, std::size_t index, Vec2& site) {
        const Vec2 q = body.centroid + Vec2{minImage(x - body.centroid.x, width_),
                                            minImage(y - body.centroid.y, height_)};
        if (q.x < body.aabbMin.x - reach || q.x > body.aabbMax.x + reach ||
            q.y < body.aabbMin.y - reach || q.y > body.aabbMax.y + reach)
            return false;
        const polygon::Closest cl = polygon::closestOnBoundary(body.poly.data(), n, q);
        if (cl.dist2 > reach * reach || polygon::contains(body.poly.data(), n, q))
            return false;
        Rng rng(config_.seed, rng_stream::kFeeding, index, step_);
        if (rng.uniform() >= chance)
            return false;
        site = cl.point + polygon::inwardNormal(cl.edgeDir, body.restArea) * 4.0f;
        return true;
    };

    // La maquinaria de fagocitosis tiene un caudal limitado.
    dig.mouthBudget = std::min(dig.mouthBudget + feedingParams_.contactMaxRate * dt,
                               feedingParams_.contactMaxRate);
    Vec2 site;
    // La caja del cuerpo puede salirse del mundo: se consulta su versión envuelta.
    const Vec2 lo = body.aabbMin - Vec2{reach, reach}, hi = body.aabbMax + Vec2{reach, reach};
    const Vec2 shift = {wrapCoord(lo.x, width_) - lo.x, wrapCoord(lo.y, height_) - lo.y};
    forBacteriaInBox(lo + shift, hi + shift, [&](std::size_t k) {
        if (takenBacteria_[k] || dig.mouthBudget < 1.0f)
            return;
        if (tryEat(bacteria_.x[k], bacteria_.y[k], k, site)) {
            dig.mouthBudget -= 1.0f;
            takenBacteria_[k] = 1;
            eatenBacteria_.push_back(k);
            ++dig.bacteriaEaten;
            if (bacteria_.quorum[k]) // violaceína: tóxica para el protista que la come
                dig.toxin += kViolaceinDose;
            ingest(dig, body, bodies_, site, kBacteriumArea, kBacteriumPmol * bacteria_.mass[k], 170, 150, 140, false,
                   0.0f, bioScale());
        }
    });

    for (std::size_t k = 0; k < ambient_.size() && dig.mouthBudget >= 1.0f; ++k) {
        if (takenAmbient_[k] || ambient_.kind[k] != ParticleKind::Detritus)
            continue;
        if (tryEat(ambient_.x[k], ambient_.y[k], bacteria_.size() + k, site)) {
            dig.mouthBudget -= 1.0f;
            takenAmbient_[k] = 1;
            eatenAmbient_.push_back(k);
            ++dig.detritusEaten;
            const float a = units::kPi * ambient_.radius[k] * ambient_.radius[k];
            ingest(dig, body, bodies_, site, a, ambient_.mass[k], 140, 112, 72, false, 0.0f, bioScale());
        }
    }
}

// ---------------------------------------------------------------------------
// Ciclo de vida: metabolismo, crecimiento, división y muerte
// ---------------------------------------------------------------------------

namespace {

float maxSpeedOf(const LocomotionParams& p, LocomotionType type) {
    switch (type) {
    case LocomotionType::Cilia: return p.ciliaSpeed;
    case LocomotionType::Flagellum: return p.flagellumSpeed;
    case LocomotionType::Pseudopod: return p.pseudopodSpeed;
    }
    return 1.0f;
}

// Citostoma del ciliado: en el surco oral, ~54° del polo anterior.
void placeMouth(const RingBody& body, Locomotion& loco) {
    if (loco.type != LocomotionType::Cilia)
        return;
    float best = 1e9f;
    for (std::size_t i = 0; i < body.restRing.size(); ++i) {
        const float a = std::atan2(body.restRing[i].y, body.restRing[i].x);
        const float d = std::abs(a - 0.3f * units::kPi);
        if (d < best) {
            best = d;
            loco.mouthRing = static_cast<std::uint32_t>(i);
        }
    }
}

} // namespace

float World::microMolarPerPmol() const {
    const double cellLiters = static_cast<double>(config_.cellSize) * config_.cellSize *
                              fluid_.gapHeight * units::kLitersPerCubicMicrometer;
    return static_cast<float>(1e-6 / cellLiters);
}

void World::seedMicronutrients(Metabolism& m, const SpeciesInfo& info, Vec2 at) {
    const MetabolismParams& p = metabolismParams_;
    m.needsSilicon = info.frustule;
    m.needsVitamin = info.vitaminB12;
    if (m.needsSilicon)
        m.silicon = -exchange(silicate_, at, -p.siliconRatio * m.biomass, Flow::Spawn);
    if (m.photosynthetic)
        m.iron = -exchange(iron_, at, -p.ironRatio * m.biomass, Flow::Spawn);
    if (m.needsVitamin)
        m.vitamin = -exchange(vitamin_, at, -p.vitaminRatio * m.biomass);
}

// La biomasa extra de una célula nueva (etapa del ciclo al azar) sale del agua.
void World::growMicronutrients(Metabolism& m, Vec2 at, float factor) {
    if (m.needsSilicon)
        m.silicon += -exchange(silicate_, at, -(factor - 1.0f) * m.silicon, Flow::Spawn);
    if (m.photosynthetic)
        m.iron += -exchange(iron_, at, -(factor - 1.0f) * m.iron, Flow::Spawn);
    if (m.needsVitamin)
        m.vitamin += -exchange(vitamin_, at, -(factor - 1.0f) * m.vitamin);
}

// Simplificación: al ser comida o lisada, el hierro y la B₁₂ vuelven disueltos de inmediato (en
// la realidad pasan por la digestión del depredador). El Si de una diatomea lisada queda en su
// frústula; si la comen, se disuelve en el agua.
float World::releaseMicronutrients(entt::entity e, Vec2 at) {
    const Metabolism* m = registry_.try_get<Metabolism>(e);
    if (!m)
        return 0.0f;
    if (m->iron > 0.0f)
        exchange(iron_, at, m->iron, Flow::Death);
    if (m->vitamin > 0.0f)
        exchange(vitamin_, at, m->vitamin);
    return m->silicon;
}

void World::dose(ChemField& field, Vec2 at, float radius, float micromolar) {
    Grid2D& g = field.grid();
    double added = 0.0; // [µM·celda]
    const int r = static_cast<int>(radius / g.dx) + 1;
    const int cx = g.cellX(at.x), cy = g.cellY(at.y);
    for (int dy = -r; dy <= r; ++dy)
        for (int dx = -r; dx <= r; ++dx) {
            if (static_cast<float>(dx * dx + dy * dy) * g.dx * g.dx > radius * radius)
                continue;
            const std::size_t k = static_cast<std::size_t>(((cy + dy) % g.ny + g.ny) % g.ny) * static_cast<std::size_t>(g.nx) +
                                  static_cast<std::size_t>(((cx + dx) % g.nx + g.nx) % g.nx);
            if (dishMask_.isWater(k)) {
                const float before = g.v[k];
                g.v[k] = std::max(0.0f, g.v[k] + micromolar);
                added += g.v[k] - before;
            }
        }
    Element e;
    float perPmol;
    if (fieldElement(field, e, perPmol)) {
        const double pmol = added / microMolarPerPmol() * perPmol;
        ledger_.add(e, Flow::Dose, pmol);
        ledger_.addExternal(e, pmol);
    }
}

double World::totalSilicon() const { return massBudget().total(Element::Si); }
double World::totalIron() const { return massBudget().total(Element::Fe); }

MassBudget World::massBudget() const {
    PROFILE_SCOPE("World::massBudget");
    MassBudget b;
    b.ledger = ledger_;
    b.time = simTime_;
    const double k = microMolarPerPmol();
    auto add = [&](Element e, Pool p, double pmol) { b.stock[static_cast<int>(e)][static_cast<int>(p)] += pmol; };
    constexpr double kC = 6.0; // la masa es glucosa equivalente: 6 C por pmol
    add(Element::P, Pool::Water, phosphate_.sum() / k);
    add(Element::N, Pool::Water, nitrogen_.sum() / k);
    add(Element::Si, Pool::Water, silicate_.sum() / k);
    add(Element::Fe, Pool::Water, iron_.sum() / k);
    add(Element::C, Pool::Water, kC * nutrients_.sum() / k);
    add(Element::C, Pool::Gas, co2_.sum() / k);
    for (auto [e, m] : registry_.view<Metabolism>().each()) {
        add(Element::P, Pool::Cells, m.phosphorus);
        add(Element::N, Pool::Cells, m.nitrogen);
        add(Element::Si, Pool::Cells, m.silicon);
        add(Element::Fe, Pool::Cells, m.iron);
        add(Element::C, Pool::Cells, kC * (m.biomass + m.reserves));
    }
    for (auto [e, d] : registry_.view<Digestion>().each())
        for (const FoodVacuole& v : d.vacuoles) {
            add(Element::P, Pool::Vacuoles, v.phosphorus);
            add(Element::N, Pool::Vacuoles, v.nitrogen);
            add(Element::C, Pool::Vacuoles, kC * v.contentPmol);
        }
    for (auto [e, inf] : registry_.view<ChytridInfection>().each()) {
        add(Element::P, Pool::Fungi, inf.phosphorus);
        add(Element::C, Pool::Fungi, kC * inf.mass);
    }
    add(Element::P, Pool::Fungi, fungi_.phosphorus());
    add(Element::C, Pool::Fungi, kC * fungi_.biomass());
    add(Element::P, Pool::Cyanobacteria, cyano_.phosphorus());
    add(Element::N, Pool::Cyanobacteria, cyano_.nitrogen());
    add(Element::C, Pool::Cyanobacteria, kC * cyano_.biomass());
    add(Element::P, Pool::Plasmodium, plasmo_.phosphorus());
    add(Element::C, Pool::Plasmodium, kC * plasmo_.biomass());
    double bacteria = 0.0;
    for (std::size_t i = 0; i < bacteria_.size(); ++i)
        bacteria += bacteria_.mass[i];
    add(Element::C, Pool::Bacteria, kC * kBacteriumPmol * bacteria);
    for (std::size_t i = 0; i < ambient_.size(); ++i) {
        if (ambient_.kind[i] == ParticleKind::Detritus)
            add(Element::C, Pool::Detritus, kC * ambient_.mass[i]);
        else if (ambient_.kind[i] == ParticleKind::Mineral && ambient_.mineral[i] == MineralKind::Frustule)
            add(Element::Si, Pool::Sediment, ambient_.mass[i]);
    }
    return b;
}

float World::meanConcentration(const ChemField& field) const {
    return static_cast<float>(field.sum() / static_cast<double>(std::max<std::size_t>(1, dishMask_.waterCells)));
}

void World::setMeanConcentration(ChemField& field, float target) {
    const double before = field.sum();
    const float mean = meanConcentration(field);
    if (mean > 1e-12f) {
        const float k = target / mean;
        for (float& v : field.grid().v)
            v *= k;
    } else {
        std::fill(field.grid().v.begin(), field.grid().v.end(), target);
    }
    Element e;
    float perPmol;
    if (fieldElement(field, e, perPmol)) {
        const double pmol = (field.sum() - before) / microMolarPerPmol() * perPmol;
        ledger_.add(e, Flow::Dose, pmol);
        ledger_.addExternal(e, pmol);
    }
}

void World::exchangeO2(Vec2 at, float pmol) { exchange(oxygen_, at, pmol); }

bool World::fieldElement(const ChemField& field, Element& element, float& perPmol) const {
    perPmol = 1.0f;
    if (&field == &phosphate_)
        element = Element::P;
    else if (&field == &nitrogen_)
        element = Element::N;
    else if (&field == &silicate_)
        element = Element::Si;
    else if (&field == &iron_)
        element = Element::Fe;
    else if (&field == &co2_)
        element = Element::C;
    else if (&field == &nutrients_) {
        element = Element::C;
        perPmol = 6.0f; // materia orgánica en equivalentes de glucosa: C₆
    } else
        return false;
    return true;
}

void World::record(const ChemField& field, Flow flow, double pmol) {
    Element e;
    float perPmol;
    if (fieldElement(field, e, perPmol))
        ledger_.add(e, flow, pmol * perPmol);
}

float World::exchange(ChemField& field, Vec2 at, float pmol, Flow flow) {
    const float moved = exchangeRaw(field, at, pmol);
    record(field, flow, moved);
    return moved;
}

float World::exchangeRaw(ChemField& field, Vec2 at, float pmol) {
    // Se reparte en las 3×3 celdas alrededor de la célula; lo que se quita no puede superar lo
    // que hay (así la masa se conserva).
    Grid2D& g = field.grid();
    const float k = microMolarPerPmol();
    const int cx = g.cellX(at.x), cy = g.cellY(at.y);
    float available = 0.0f;
    if (pmol < 0.0f) {
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                available += std::max(0.0f, g.at(wrapIndex(cx + dx, g.nx), wrapIndex(cy + dy, g.ny))) / k;
        pmol = std::max(pmol, -available);
        // Se quita de cada celda en proporción a lo que tiene (nunca más de lo que hay).
        const float f = available > 0.0f ? -pmol / available : 0.0f;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                float& v = g.at(wrapIndex(cx + dx, g.nx), wrapIndex(cy + dy, g.ny));
                v = std::max(0.0f, v) * (1.0f - f);
            }
        return pmol;
    }
    const float perCell = pmol * k / 9.0f;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            float& v = g.at(wrapIndex(cx + dx, g.nx), wrapIndex(cy + dy, g.ny));
            v = std::max(0.0f, v + perCell);
        }
    return pmol;
}

void World::growBacteria(float dt) {
    PROFILE_SCOPE("World::growBacteria");
    const float bioDt = dt * bioScale();
    const BacteriaParams& bp = bacteria_.params;
    bacteria_.grow(nutrients_.grid(), bioDt, bacterialDemand_);
    // Serial: cada bacteria toma de su celda lo que pide, si hay (conserva la masa).
    Grid2D& food = nutrients_.grid();
    const float k = microMolarPerPmol();
    const float maintenance = bp.maintenancePerHour * bioDt / 3600.0f;
    eatenBacteria_.clear();
    const std::size_t n = bacteria_.size();
    for (std::size_t i = 0; i < n; ++i) {
        float& c = food.at(food.cellX(bacteria_.x[i]), food.cellY(bacteria_.y[i]));
        const float taken = std::min(bacterialDemand_[i], std::max(0.0f, c) / k);
        c -= taken * k;
        float& m = bacteria_.mass[i];
        m += taken * bp.yield / kBacteriumPmol - maintenance * m;
        if (m < 0.5f) { // muere de hambre: su materia vuelve al agua
            c += m * kBacteriumPmol * k;
            eatenBacteria_.push_back(i);
        } else if (m >= 2.0f && bacteria_.size() < bp.maxCount) {
            // Fisión binaria: la hija sale al lado, con otra orientación.
            m *= 0.5f;
            Rng rng(config_.seed, rng_stream::kBacteria + 100, i, step_);
            const std::uint8_t resistant =
                bacteria_.resistant[i] || rng.uniform() < bp.resistanceMutation ? 1 : 0;
            bacteria_.add(wrapCoord(bacteria_.x[i] + std::cos(bacteria_.heading[i]), width_),
                          wrapCoord(bacteria_.y[i] + std::sin(bacteria_.heading[i]), height_),
                          rng.uniform(0.0f, 2.0f * units::kPi), m, bacteria_.strain[i], resistant);
        }
    }
    removeBacteria(eatenBacteria_);
    microbialInteractions(dt);
}


float World::carriedNitrogen(entt::entity e) const {
    float n = 0.0f;
    if (const Metabolism* m = registry_.try_get<Metabolism>(e))
        n += m->nitrogen;
    if (const Digestion* d = registry_.try_get<Digestion>(e))
        for (const FoodVacuole& v : d->vacuoles)
            n += v.nitrogen;
    return n;
}

double World::totalNitrogen() const { return massBudget().total(Element::N); }
double World::totalPhosphorus() const { return massBudget().total(Element::P); }

void World::updateLifecycle(float dt) {
    PROFILE_SCOPE("World::updateLifecycle");
    const float bioDt = dt * bioScale();
    const MetabolismParams& mp = metabolismParams_;
    dying_.clear();
    splits_.clear();
    conjugationsDone_.clear();

    for (auto [e, body, meta] : registry_.view<RingBody, Metabolism>().each()) {
        Digestion* dig = registry_.try_get<Digestion>(e);
        const Locomotion* loco = registry_.try_get<Locomotion>(e);
        MetabolicInput in;
        in.o2 = oxygen_.grid().sample(body.centroid.x, body.centroid.y);
        in.light = light_.sample(body.centroid.x, body.centroid.y);
        // La alimentación es mecánica y corre en tiempo REAL; el metabolismo, comprimido. Sin
        // compensar, la célula comería bioTimeScale veces menos por hora biológica que en la
        // realidad. Cada presa digerida rinde por todas las que habría recolectado en ese tiempo.
        Excreted excreted;
        in.digested = dig ? digest(*dig, body, bodies_, feedingParams_, dt, bioDt, excreted) : 0.0f;
        if (dig)
            dig->handling = std::max(0.0f, dig->handling - bioDt);
        if (speciesInfo(speciesOfEntity(registry_, e)).feeding == FeedingMode::Osmotroph) {
            // Levaduras: transportadores de la pared absorben lo disuelto (Monod; menos afines
            // que las bacterias, prosperan donde abunda la materia orgánica).
            const float c = std::max(0.0f, nutrients_.grid().sample(body.centroid.x, body.centroid.y));
            const float want = kOsmotrophUptakePerHour * meta.biomass * bioDt / 3600.0f * c / (c + kOsmotrophHalfSaturation);
            if (want > 0.0f)
                in.digested -= exchange(nutrients_, body.centroid, -want);
        }
        if (excreted.phosphorus > 0.0f)
            exchange(phosphate_, body.centroid, excreted.phosphorus, Flow::Excretion);
        if (excreted.nitrogen > 0.0f) // amonio excretado al digerir
            exchange(nitrogen_, body.centroid, excreted.nitrogen, Flow::Excretion);
        in.phosphate = phosphate_.grid().sample(body.centroid.x, body.centroid.y);
        in.nitrogen = nitrogen_.grid().sample(body.centroid.x, body.centroid.y);
        in.silicate = silicate_.grid().sample(body.centroid.x, body.centroid.y);
        in.iron = iron_.grid().sample(body.centroid.x, body.centroid.y);
        in.vitamin = vitamin_.grid().sample(body.centroid.x, body.centroid.y);
        in.activity = 0.0f;
        if (loco) {
            in.activity = loco->type == LocomotionType::Pseudopod
                              ? (loco->active ? 1.0f : 0.0f)
                              : std::min(1.0f, std::abs(loco->speed) / maxSpeedOf(locomotionParams_, loco->type));
        }
        in.bioDt = bioDt;
        const SpeciesInfo& info = speciesInfo(speciesOfEntity(registry_, e));
        Cyst* cyst = registry_.try_get<Cyst>(e);
        const Heredity* her = registry_.try_get<Heredity>(e);
        if (her) {
            in.growthScale = her->traits.growthMax;
            in.maintenanceScale = her->traits.maintenance;
            in.photosynthesisScale = her->traits.photosynthesis * info.photosynthesisScale;
            in.motilityCostScale = her->traits.motilityCost;
        }
        if (cyst) { // metabolismo mínimo dentro de la pared del quiste
            in.maintenanceScale *= 0.05f;
            in.activity = 0.0f;
            in.photosynthesisScale = 0.0f;
        }
        const char* stressCause = applyEnvironmentStress(e, body, info, in, bioDt);
        const float phosphorusBefore = meta.phosphorus, nitrogenBefore = meta.nitrogen;
        const float siliconBefore = meta.silicon, ironBefore = meta.iron, vitaminBefore = meta.vitamin;
        const MetabolicOutput out = updateMetabolism(meta, mp, in);
        if (out.phosphateTaken > 0.0f) {
            // El fosfato sale del agua cercana; si no alcanza, la célula toma lo que hay.
            const float taken = -exchange(phosphate_, body.centroid, -out.phosphateTaken, Flow::Uptake);
            meta.phosphorus = phosphorusBefore + taken;
        }
        if (out.nitrogenTaken > 0.0f) {
            const float taken = -exchange(nitrogen_, body.centroid, -out.nitrogenTaken, Flow::Uptake);
            meta.nitrogen = nitrogenBefore + taken;
        }
        if (out.siliconTaken > 0.0f)
            meta.silicon = siliconBefore - exchange(silicate_, body.centroid, -out.siliconTaken, Flow::Uptake);
        if (out.ironTaken > 0.0f)
            meta.iron = ironBefore - exchange(iron_, body.centroid, -out.ironTaken, Flow::Uptake);
        if (out.vitaminTaken > 0.0f)
            meta.vitamin = vitaminBefore - exchange(vitamin_, body.centroid, -out.vitaminTaken);
        // CO₂: lo que respira (y fermenta) sale; lo que fija entra. También a la tasa real.
        exchangeCO2(body.centroid, (out.o2Consumed + out.co2Fermented - out.o2Produced) / bioScale());
        // El intercambio de gases con el agua ocurre a la tasa REAL: comprimir el tiempo
        // biológico no debe acelerar la difusión ni el intercambio con el aire (agotaría o
        // sobresaturaría el O₂ alrededor de cada célula de forma irreal).
        exchangeO2(body.centroid, (out.o2Produced - out.o2Consumed) / bioScale());
        if (out.exudate > 0.0f) // bucle microbiano: las algas alimentan a las bacterias
            exchange(nutrients_, body.centroid, out.exudate);

        // Atrapada por un suctorio: los tentáculos le succionan el citoplasma (~1 h biológica).
        const char* deathCause = stressCause ? stressCause : "inanicion";
        if (const Captured* cap = registry_.try_get<Captured>(e)) {
            if (!registry_.valid(cap->by) || !registry_.all_of<Metabolism>(cap->by)) {
                released_.push_back(e);
            } else {
                const float f = 1.0f - std::exp(-bioDt / 3600.0f);
                const float fromReserves = meta.reserves * f, fromBiomass = meta.biomass * f;
                meta.reserves -= fromReserves;
                meta.biomass -= fromBiomass;
                registry_.get<Metabolism>(cap->by).reserves += fromReserves + fromBiomass;
                deathCause = "succionada por un suctorio";
            }
        }
        if (registry_.all_of<ChytridInfection>(e))
            deathCause = "parasitada por quitridios";
        if (Telotroch* tel = registry_.try_get<Telotroch>(e)) {
            tel->timer -= dt;
            if (tel->timer <= 0.0f)
                settling_.push_back(e);
        }
        if (Zoospore* zs = registry_.try_get<Zoospore>(e)) {
            zs->hoursLeft -= bioDt / 3600.0f;
            if (zs->hoursLeft <= 0.0f)
                zoosporeSettling_.push_back(e);
        }
        if (isDead(meta, mp)) {
            dying_.push_back({e, deathCause});
            continue;
        }

        // Tamaño: biomasa + lo que hay en las vacuolas. Los organelos crecen con la célula.
        const float target = cellAreaForMass(meta.biomass / info.dryMassScale) * (her ? her->traits.volume : 1.0f) +
                             (dig ? vacuoleContentArea(*dig) : 0.0f);
        const float before = std::abs(body.restArea);
        // Las células de un filamento crecen solo a lo largo (pared rígida de ancho fijo).
        if (info.filament && !registry_.all_of<Zoospore>(e))
            soft_body::setRestAreaAxial(body, body.restArea >= 0.0f ? target : -target);
        else
            soft_body::setRestArea(body, body.restArea >= 0.0f ? target : -target);
        if (before > 0.0f) {
            const float k = std::sqrt(target / before);
            const CellAnatomy& an = registry_.get<CellAnatomy>(e);
            bodies_.radius[an.nucleus] *= k;
            bodies_.radius[an.contractileVacuole] *= k;
            if (const Dividing* dv = registry_.try_get<Dividing>(e); dv && dv->nucleus2 != ~0u) {
                bodies_.radius[dv->nucleus2] *= k;
                bodies_.radius[dv->vacuole2] *= k;
            }
        }

        // Quistes (Didinium): se enquista con hambre y sale cuando vuelve a haber presas cerca.
        if (cyst) {
            cyst->age += dt;
            const Perception sense = perceive(e, body, speciesOfEntity(registry_, e));
            if (cyst->age > 30.0f && sense.hasPrey && sense.preyDistance < 300.0f)
                excysting_.push_back(e);
            continue;
        }
        // Se enquista cuando se acaba la comida, no si tiene presas cerca (recién salida del
        // quiste está hambrienta y tiene que cazar).
        if (info.encysts && meta.starving && !registry_.any_of<Dividing, Conjugating>(e)) {
            const Perception sense = perceive(e, body, speciesOfEntity(registry_, e));
            if (!(sense.hasPrey && sense.preyDistance < 300.0f)) {
                encysting_.push_back(e);
                continue;
            }
        }
        if (Conjugating* cj = registry_.try_get<Conjugating>(e)) {
            if (!registry_.valid(cj->partner)) { // la pareja murió: se separa
                abandonedConjugations_.push_back(e);
                continue;
            }
            cj->progress += bioDt / (kConjugationHours * 3600.0f);
            if (cj->progress >= 1.0f && e < cj->partner) // cada pareja se resuelve una sola vez
                conjugationsDone_.push_back({e, cj->partner});
            continue; // no se divide mientras conjuga
        }
        Dividing* dv = registry_.try_get<Dividing>(e);
        const float cost = registry_.all_of<Player>(e) ? mp.playerDivisionCost : mp.npcDivisionCost;
        if (registry_.any_of<ChytridInfection, Zoospore, Lichenized>(e))
            continue; // el parásito le impide reproducirse (y la zoóspora no se divide)
        if (info.colonyDaughters > 0) { // colonias: liberan colonias hijas en vez de dividirse
            if (readyToDivide(meta, cost))
                colonyReleases_.push_back(e);
            continue;
        }
        if (!dv && info.zoosporeChance > 0.0f && readyToDivide(meta, cost)) {
            // Oedogonium: a veces el protoplasto entero sale como zoóspora en vez de dividirse.
            Rng rng(config_.seed, rng_stream::kCells + 40, body.id, step_);
            if (rng.uniform() < info.zoosporeChance) {
                meta.reserves -= cost * meta.biomass;
                zoosporeReleases_.push_back(e);
                continue;
            }
        }
        if (!dv && readyToDivide(meta, cost)) {
            meta.reserves -= cost * meta.biomass;
            Dividing& d = registry_.emplace<Dividing>(e);
            d.motherRest = body.restRing;
            d.motherArea = std::abs(body.restArea);
        } else if (dv) {
            dv->progress += bioDt / (mp.divisionHours * 3600.0f);
            // El córtex se reorganiza: la forma de reposo pasa de la madre a dos hijas.
            if (body.membraneReservoirRate <= 0.0f && dv->motherArea > 0.0f && !info.filament) {
                const float t = std::clamp((dv->progress - 0.2f) / 0.8f, 0.0f, 1.0f);
                const float scale = std::sqrt(std::abs(body.restArea) / dv->motherArea);
                divisionRestShape(dv->motherRest, scale, t * t * (3.0f - 2.0f * t), body.restRing);
                const std::size_t rn = body.restRing.size();
                for (std::size_t i = 0; i < rn; ++i)
                    body.restEdge[i] = length(body.restRing[(i + 1) % rn] - body.restRing[i]);
            }
            if (dv->progress > 0.2f && dv->nucleus2 == ~0u)
                duplicateOrganelles(e);
            std::size_t ta, tb;
            if (info.filament) {
                // Spirogyra: no se estrangula; un tabique (septo) crece hacia adentro desde la
                // pared lateral y parte la célula en dos sin cambiar el ancho del filamento.
                if (dv->progress >= 1.0f && dv->nucleus2 != ~0u && septumTips(body, ta, tb))
                    splits_.push_back({e, ta, tb});
            } else if (dv->progress > 0.3f && furrowClosed(body, bodies_, ta, tb)) {
                splits_.push_back({e, ta, tb});
            }
        }
    }
    for (entt::entity e : encysting_) {
        registry_.emplace<Cyst>(e);
        RingBody& b = registry_.get<RingBody>(e);
        const float r = std::sqrt(std::abs(soft_body::area(b, bodies_)) / units::kPi);
        soft_body::reshapeRest(b, bodies_, r, r); // pared redonda
        b.shapeRate = 8.0f;
    }
    for (entt::entity e : excysting_) {
        registry_.remove<Cyst>(e);
        RingBody& b = registry_.get<RingBody>(e);
        const CellShape& shape = registry_.get<CellShape>(e);
        const float k = std::sqrt(std::abs(soft_body::area(b, bodies_)) / (units::kPi * shape.radiusX * shape.radiusY));
        soft_body::reshapeRest(b, bodies_, shape.radiusX * k, shape.radiusY * k, shape.squareness, shape.taper);
        b.shapeRate = shape.shapeRate;
    }
    encysting_.clear();
    excysting_.clear();
    for (entt::entity e : abandonedConjugations_)
        registry_.remove<Conjugating>(e);
    abandonedConjugations_.clear();
    for (const auto& [a, b] : conjugationsDone_)
        finishConjugation(a, b);
    for (const PendingSplit& sp : splits_)
        divide(sp.cell, sp.tipA, sp.tipB);
    for (entt::entity e : colonyReleases_)
        if (registry_.valid(e))
            releaseColonies(e);
    colonyReleases_.clear();
    for (entt::entity e : zoosporeReleases_)
        if (registry_.valid(e))
            releaseZoospore(e);
    zoosporeReleases_.clear();
    for (entt::entity e : zoosporeSettling_)
        if (registry_.valid(e))
            settleZoospore(e);
    zoosporeSettling_.clear();
    maintainLinks(bioDt);
    for (entt::entity e : released_)
        registry_.remove<Captured>(e);
    released_.clear();
    for (entt::entity e : settling_) {
        registry_.remove<Telotroch>(e);
        anchorHere(e);
    }
    settling_.clear();
    for (const auto& [e, cause] : dying_)
        if (registry_.valid(e))
            lyse(e, cause);
}

bool World::septumTips(const RingBody& body, std::size_t& tipA, std::size_t& tipB) const {
    // Los puntos de la pared lateral más cercanos al plano medio (x local = 0), uno de cada lado.
    float bestA = 1e9f, bestB = 1e9f;
    tipA = tipB = body.ring.size();
    for (std::size_t i = 0; i < body.ring.size(); ++i) {
        const std::uint32_t s = body.ring[i];
        const Vec2 q = soft_body::toLocal(body, {bodies_.x[s], bodies_.y[s]});
        float& best = q.y >= 0.0f ? bestA : bestB;
        if (std::abs(q.x) < best) {
            best = std::abs(q.x);
            (q.y >= 0.0f ? tipA : tipB) = i;
        }
    }
    return tipA < body.ring.size() && tipB < body.ring.size();
}

bool World::isSatiated(entt::entity e) const {
    const Metabolism* m = registry_.try_get<Metabolism>(e);
    const Digestion* d = registry_.try_get<Digestion>(e);
    if (!m)
        return false;
    if (d && (d->handling > 0.0f || satiated(*d, m->biomass)))
        return true;
    // Los suctorios succionan directo a sus reservas: con el citoplasma lleno no atrapan más.
    return speciesInfo(speciesOfEntity(registry_, e)).feeding == FeedingMode::Tentacles && m->reserves > 0.6f * m->biomass;
}

void World::duplicateOrganelles(entt::entity cell) {
    RingBody& body = registry_.get<RingBody>(cell);
    Dividing& dv = registry_.get<Dividing>(cell);
    const CellAnatomy& an = registry_.get<CellAnatomy>(cell);
    float half = 0.0f;
    for (const Vec2& q : body.restRing)
        half = std::max(half, std::abs(q.x));
    // El núcleo se divide y cada mitad viaja a un polo; lo mismo la vacuola contráctil.
    auto split = [&](std::uint32_t slot, Vec2 restA, Vec2 restB) {
        const float r = bodies_.radius[slot] / std::sqrt(2.0f);
        bodies_.radius[slot] = r;
        const std::uint32_t twin = bodies_.allocate(BodyKind::Interior, bodies_.x[slot] + 0.5f,
                                                    bodies_.y[slot], r, bodies_.invMass[slot], body.id);
        soft_body::addOrganelle(body, twin, restB, 3.0f);
        const auto it = std::find(body.interior.begin(), body.interior.end(), slot);
        const auto k = static_cast<std::size_t>(it - body.interior.begin());
        body.restInterior[k] = restA;
        body.anchorRate[k] = 3.0f;
        return twin;
    };
    dv.nucleus2 = split(an.nucleus, {0.45f * half, 0.0f}, {-0.45f * half, 0.0f});
    const Vec2 cv = soft_body::toLocal(body, {bodies_.x[an.contractileVacuole], bodies_.y[an.contractileVacuole]});
    dv.vacuole2 = split(an.contractileVacuole, {0.6f * half, cv.y}, {-0.6f * half, cv.y});
}

void World::applyDivisionForces() {
    for (auto [e, body, dv] : registry_.view<RingBody, Dividing>().each()) {
        if (dv.progress < 0.3f || speciesInfo(speciesOfEntity(registry_, e)).filament)
            continue;
        // Velocidad para cerrar el surco en el tiempo que queda (con un mínimo).
        const float remaining = std::max(0.5f, (1.0f - dv.progress) * metabolismParams_.divisionHours *
                                                   3600.0f / bioScale());
        const float speed = std::clamp(furrowHalfWidth(body, bodies_) / remaining, 1.0f, 30.0f);
        applyFurrow(body, bodies_, speed);
    }
}

void World::divide(entt::entity cell, std::size_t tipA, std::size_t tipB) {
    RingBody& body = registry_.get<RingBody>(cell);
    const std::size_t n = body.ring.size();
    auto segment = [&](std::size_t from, std::size_t to) {
        std::vector<std::uint32_t> out;
        for (std::size_t k = from;; k = (k + 1) % n) {
            out.push_back(body.ring[k]);
            if (k == to)
                break;
        }
        return out;
    };
    std::vector<std::uint32_t> ringA = segment(tipA, tipB);
    std::vector<std::uint32_t> ringB = segment(tipB, tipA);
    if (ringA.size() < 12 || ringB.size() < 12)
        return; // surco descentrado: sigue estrangulando

    const entt::entity child = registry_.create();
    const auto childId = static_cast<std::uint32_t>(entt::to_integral(child));
    // Las puntas del surco quedan en la hija A; la hija B recibe copias.
    auto duplicate = [&](std::uint32_t s) {
        return bodies_.allocate(BodyKind::Membrane, bodies_.x[s], bodies_.y[s], bodies_.radius[s],
                                bodies_.invMass[s], childId);
    };
    ringB.front() = duplicate(ringB.front());
    ringB.back() = duplicate(ringB.back());

    std::vector<Vec2> polyA(ringA.size());
    for (std::size_t i = 0; i < ringA.size(); ++i)
        polyA[i] = {bodies_.x[ringA[i]], bodies_.y[ringA[i]]};
    auto inA = [&](std::uint32_t s) {
        return polygon::contains(polyA.data(), static_cast<int>(polyA.size()), {bodies_.x[s], bodies_.y[s]});
    };

    RingBody child_body;
    child_body.id = childId;
    child_body.membraneReservoirRate = body.membraneReservoirRate;
    child_body.edgeScale = body.edgeScale;
    child_body.bendScale = body.bendScale;
    child_body.areaScale = body.areaScale;
    child_body.corticalTension = body.corticalTension;
    child_body.edgeTensionOnly = body.edgeTensionOnly;
    child_body.ring = ringB;

    // Reparte el interior según de qué lado del surco quedó (los organelos primero).
    RingBody keep = body; // copia del interior original
    body.interior.clear();
    body.restInterior.clear();
    body.anchorRate.clear();
    body.organelleCount = 0;
    for (std::size_t k = 0; k < keep.interior.size(); ++k) {
        const std::uint32_t s = keep.interior[k];
        RingBody& dst = inA(s) ? body : child_body;
        if (k < keep.organelleCount) {
            soft_body::addOrganelle(dst, s, keep.restInterior[k], keep.anchorRate[k]);
        } else {
            dst.interior.push_back(s);
            dst.restInterior.push_back(keep.restInterior[k]);
            dst.anchorRate.push_back(keep.anchorRate[k]);
        }
    }
    for (std::uint32_t s : child_body.ring)
        bodies_.owner[s] = childId;
    for (std::uint32_t s : child_body.interior)
        bodies_.owner[s] = childId;
    body.ring = ringA;

    // Cada hija vuelve a la forma de su especie, a su tamaño actual.
    const CellShape shape = registry_.get<CellShape>(cell);
    const bool filament = speciesInfo(speciesOfEntity(registry_, cell)).filament;
    auto reshape = [&](RingBody& b) {
        if (b.membraneReservoirRate > 0.0f) {
            soft_body::makePlastic(b, bodies_, b.membraneReservoirRate);
            return;
        }
        // Membrana con espaciado uniforme (como al nacer): si no, cada generación tendría la
        // mitad de puntos que la anterior.
        const std::size_t count = std::max<std::size_t>(
            48, static_cast<std::size_t>(soft_body::perimeter(b, bodies_) / kMembraneSpacing));
        soft_body::resampleRing(b, bodies_, count);
        const float a = std::abs(soft_body::area(b, bodies_));
        if (filament) { // mismo ancho, el largo que dé el área
            const float n = shape.squareness;
            const float g = std::tgamma(1.0f + 1.0f / n);
            const float unit = 4.0f * g * g / std::tgamma(1.0f + 2.0f / n); // área de la superelipse / (rx·ry)
            soft_body::reshapeRest(b, bodies_, a / (unit * shape.radiusY), shape.radiusY, n);
        } else {
            const float k = std::sqrt(a / (units::kPi * shape.radiusX * shape.radiusY));
            soft_body::reshapeRest(b, bodies_, shape.radiusX * k, shape.radiusY * k, shape.squareness, shape.taper);
        }
        b.shapeRate = shape.shapeRate;
    };
    reshape(body);
    reshape(child_body);
    const float areaA = std::abs(soft_body::area(body, bodies_));
    const float areaB = std::abs(soft_body::area(child_body, bodies_));
    const float fA = areaA / (areaA + areaB);

    // Componentes.
    const Dividing dv = registry_.get<Dividing>(cell);
    CellAnatomy anA = registry_.get<CellAnatomy>(cell);
    CellAnatomy anB{dv.nucleus2, dv.vacuole2};
    // Cada hija se queda con el núcleo y la vacuola que quedaron de su lado.
    if (!inA(anA.nucleus) && inA(anB.nucleus))
        std::swap(anA.nucleus, anB.nucleus);
    if (!inA(anA.contractileVacuole) && inA(anB.contractileVacuole))
        std::swap(anA.contractileVacuole, anB.contractileVacuole);
    registry_.replace<CellAnatomy>(cell, anA);
    registry_.remove<Dividing>(cell);

    Metabolism& meta = registry_.get<Metabolism>(cell);
    // Palintomía: las hijas siguen dividiéndose hasta completar el ciclo.
    meta.pendingFissions = meta.pendingFissions > 0 ? meta.pendingFissions - 1 : meta.fissions - 1;
    Metabolism childMeta = meta;
    childMeta.biomass = meta.biomass * (1.0f - fA);
    childMeta.reserves = meta.reserves * (1.0f - fA);
    childMeta.phosphorus = meta.phosphorus * (1.0f - fA);
    childMeta.nitrogen = meta.nitrogen * (1.0f - fA);
    childMeta.silicon = meta.silicon * (1.0f - fA);
    childMeta.iron = meta.iron * (1.0f - fA);
    childMeta.vitamin = meta.vitamin * (1.0f - fA);
    meta.biomass *= fA;
    meta.reserves *= fA;
    meta.phosphorus *= fA;
    meta.nitrogen *= fA;
    meta.silicon *= fA;
    meta.iron *= fA;
    meta.vitamin *= fA;
    meta.birthBiomass = meta.biomass;
    childMeta.birthBiomass = childMeta.biomass;
    meta.ageHours = childMeta.ageHours = 0.0f;

    Digestion childDig;
    if (Digestion* dig = registry_.try_get<Digestion>(cell)) {
        for (auto it = dig->vacuoles.begin(); it != dig->vacuoles.end();) {
            if (std::find(child_body.interior.begin(), child_body.interior.end(), it->slot) !=
                child_body.interior.end()) {
                childDig.vacuoles.push_back(*it);
                it = dig->vacuoles.erase(it);
            } else {
                ++it;
            }
        }
    }

    soft_body::refresh(body, bodies_);
    soft_body::refresh(child_body, bodies_);
    // Ojo: emplace puede realocar el almacenamiento de EnTT; se copia todo antes y no se usan
    // referencias a la madre tomadas antes de este punto.
    const CellLook look = registry_.get<CellLook>(cell);
    const Species* motherSpecies = registry_.try_get<Species>(cell);
    const bool hasSpecies = motherSpecies != nullptr;
    const Species species = hasSpecies ? *motherSpecies : Species{};
    const Locomotion* motherLoco = registry_.try_get<Locomotion>(cell);
    const bool motile = motherLoco != nullptr;
    Locomotion childLoco = motile ? *motherLoco : Locomotion{};
    registry_.emplace<RingBody>(child, std::move(child_body));
    registry_.emplace<CellLook>(child, look);
    if (hasSpecies)
        registry_.emplace<Species>(child, species);
    registry_.emplace<CellShape>(child, shape);
    registry_.emplace<CellAnatomy>(child, anB);
    registry_.emplace<Metabolism>(child, childMeta);
    registry_.emplace<Digestion>(child, std::move(childDig));
    if (motile) {
        childLoco.forward = childLoco.turn = childLoco.speed = 0.0f;
        childLoco.active = false;
        placeMouth(registry_.get<RingBody>(child), childLoco);
        registry_.emplace<Locomotion>(child, childLoco);
        placeMouth(registry_.get<RingBody>(cell), registry_.get<Locomotion>(cell));
        if (!hasSpecies || speciesInfo(species.id).speedScale > 0.0f)
            registry_.emplace<Autopilot>(child); // la hija que no controla el jugador deambula sola
        if (hasSpecies && speciesInfo(species.id).sessile)
            registry_.emplace<Telotroch>(child, Telotroch{60.0f}); // nada un rato y se fija
    }
    // Las dos hijas son una generación nueva del linaje.
    if (Lineage* lin = registry_.try_get<Lineage>(cell)) {
        ++lin->generation;
        const Lineage copy = *lin; // emplace puede realocar el almacenamiento de Lineage
        registry_.emplace<Lineage>(child, copy);
    }
    // Cada hija recibe una copia del genoma con sus propias mutaciones, y completa sus
    // organelos (se replican al crecer).
    if (const Heredity* mh = registry_.try_get<Heredity>(cell)) {
        const LocomotionType type = registry_.get<Locomotion>(cell).type;
        Heredity hb = *mh;
        Heredity ha = *mh;
        Rng ra(config_.seed, rng_stream::kMutation, entt::to_integral(cell) * 2ull, step_);
        Rng rb(config_.seed, rng_stream::kMutation, entt::to_integral(cell) * 2ull + 1, step_);
        mutate(ha.genome, type, ra);
        mutate(hb.genome, type, rb);
        ha.traits = traitsOf(ha.genome, type);
        hb.traits = traitsOf(hb.genome, type);
        registry_.replace<Heredity>(cell, ha);
        registry_.emplace<Heredity>(child, hb);
        applyTraits(cell);
        applyTraits(child);
        refillOrganelles(registry_.get<RingBody>(cell), bodies_, ha.genome, ra);
        refillOrganelles(registry_.get<RingBody>(child), bodies_, hb.genome, rb);
    }
    if (hasSpecies) {
        const SpeciesInfo& sinfo = speciesInfo(species.id);
        // Diatomeas: cada hija se queda con una valva de la madre y fabrica una nueva adentro,
        // así que una de ellas es más chica (regla de MacDonald–Pfitzer). Por debajo de ~75%
        // del tamaño forma una auxospora y recupera el tamaño máximo.
        if (sinfo.frustule && registry_.all_of<Heredity>(child)) {
            Heredity& h = registry_.get<Heredity>(child);
            h.genome[Gene::Size] *= 0.97f;
            if (h.genome[Gene::Size] < 0.75f)
                h.genome[Gene::Size] = 1.0f; // auxospora
            h.traits = traitsOf(h.genome, sinfo.locomotion);
            applyTraits(child);
        }
        if (sinfo.filament) {
            relinkAfterDivision(cell, child);
            ++observed_.filamentDivisions;
        }
    }
    if (cell == player_)
        events_.push_back({WorldEvent::Type::PlayerDivided, cell, child, ""});
    if (grabSlot_ != kNoGrab && bodies_.owner[grabSlot_] == childId)
        endGrab();
}

void World::lyseAt(Vec2 position) {
    const entt::entity e = cellAt(position);
    if (e != entt::null)
        lyse(e, "lisis provocada");
}

void World::recordDeath(entt::entity cell, const std::string& cause) {
    if (registry_.all_of<Species>(cell)) {
        ++deathTally_[{registry_.get<Species>(cell).id, cause}];
        const Vec2 at = registry_.all_of<RingBody>(cell) ? registry_.get<RingBody>(cell).centroid : Vec2{};
        noteDeath(cell, registry_.get<Species>(cell).id, at, cause);
    }
}

void World::noteDeath(entt::entity e, SpeciesId species, Vec2 at, const std::string& cause, entt::entity successor) {
    constexpr std::size_t kRecentDeaths = 64;
    if (recentDeaths_.size() >= kRecentDeaths)
        recentDeaths_.erase(recentDeaths_.begin());
    recentDeaths_.push_back({e, successor, species, cause, at, simTime_});
}

const DeathRecord* World::deathOf(entt::entity e) const {
    for (auto it = recentDeaths_.rbegin(); it != recentDeaths_.rend(); ++it)
        if (it->entity == e)
            return &*it;
    return nullptr;
}

void World::lyse(entt::entity cell, const char* cause) {
    recordDeath(cell, cause);
    {
        const RingBody& b = registry_.get<RingBody>(cell);
        const CellLook look = registry_.all_of<CellLook>(cell) ? registry_.get<CellLook>(cell) : CellLook{180, 180, 160};
        const float radius = 0.25f * ((b.aabbMax.x - b.aabbMin.x) + (b.aabbMax.y - b.aabbMin.y));
        lysisMarks_.push_back({b.centroid, radius, simTime_, look.r, look.g, look.b});
    }
    releaseChytridSpores(cell); // los esporangios maduran sobre el alga muerta
    const RingBody& body = registry_.get<RingBody>(cell);
    float total = cellMassPmol(body.restArea);
    if (const Metabolism* m = registry_.try_get<Metabolism>(cell))
        total = m->biomass + m->reserves;
    if (const Digestion* d = registry_.try_get<Digestion>(cell))
        for (const FoodVacuole& v : d->vacuoles)
            total += v.contentPmol;

    // La membrana revienta: ~40% del contenido se disuelve (aminoácidos, azúcares) y el resto
    // queda como detritos particulados, que las bacterias y los hongos descomponen.
    const int pn = static_cast<int>(body.poly.size());
    Grid2D& food = nutrients_.grid();
    std::vector<std::pair<int, int>> cells;
    for (float y = body.aabbMin.y; y <= body.aabbMax.y; y += food.dx)
        for (float x = body.aabbMin.x; x <= body.aabbMax.x; x += food.dx)
            if (polygon::contains(body.poly.data(), pn, {x, y}))
                cells.emplace_back(food.cellX(x), food.cellY(y));
    if (cells.empty())
        cells.emplace_back(food.cellX(body.centroid.x), food.cellY(body.centroid.y));
    const float dissolved = 0.4f * total * microMolarPerPmol() / static_cast<float>(cells.size());
    for (const auto& [cx, cy] : cells)
        food.at(cx, cy) += dissolved;

    // El fósforo de la célula y de sus vacuolas vuelve al agua.
    float phosphorus = 0.0f;
    if (const Metabolism* m = registry_.try_get<Metabolism>(cell))
        phosphorus += m->phosphorus;
    if (const Digestion* d = registry_.try_get<Digestion>(cell))
        for (const FoodVacuole& v : d->vacuoles)
            phosphorus += v.phosphorus;
    if (phosphorus > 0.0f)
        exchange(phosphate_, body.centroid, phosphorus, Flow::Death);
    if (const float nitrogen = carriedNitrogen(cell); nitrogen > 0.0f)
        exchange(nitrogen_, body.centroid, nitrogen, Flow::Death);
    const float silicon = releaseMicronutrients(cell, body.centroid); // el Si queda en la frústula
    // Las cianobacterias tóxicas no están en cuerpos blandos; acá no hay toxina que liberar.

    const float particulate = 0.6f * total;
    const int count = std::clamp(static_cast<int>(particulate / 0.05f), 4, 60);
    const float each = particulate / static_cast<float>(count);
    for (int k = 0; k < count; ++k) {
        Rng rng(config_.seed, rng_stream::kLysis, body.id * 64ull + static_cast<std::uint64_t>(k), step_);
        Vec2 p = body.centroid;
        for (int attempt = 0; attempt < 20; ++attempt) {
            const Vec2 q = {rng.uniform(body.aabbMin.x, body.aabbMax.x), rng.uniform(body.aabbMin.y, body.aabbMax.y)};
            if (polygon::contains(body.poly.data(), pn, q)) {
                p = q;
                break;
            }
        }
        ambient_.push(ParticleKind::Detritus, wrapCoord(p.x, width_), wrapCoord(p.y, height_),
                      std::min(4.0f, radiusFromMass(each)), each);
    }
    // Diatomeas: la frústula de sílice no se descompone; queda vacía en el sedimento (las que
    // se ven en cualquier muestra de agua son, en buena parte, frústulas de células muertas).
    if (const Species* sp = registry_.try_get<Species>(cell); sp && speciesInfo(sp->id).frustule) {
        float half = speciesInfo(sp->id).spec.radiusX;
        if (const CellShape* shape = registry_.try_get<CellShape>(cell))
            half = shape->radiusX * std::sqrt(std::abs(body.restArea) / (units::kPi * shape->radiusX * shape->radiusY));
        ambient_.push(ParticleKind::Mineral, wrapCoord(body.centroid.x, width_), wrapCoord(body.centroid.y, height_), half,
                      silicon, MineralKind::Frustule, body.angle);
    }

    if (cell == player_) {
        player_ = entt::null;
        lastDeathCause_ = cause;
        events_.push_back({WorldEvent::Type::PlayerDied, cell, entt::null, cause});
    }
    if (grabSlot_ != kNoGrab && bodies_.owner[grabSlot_] == entt::to_integral(cell))
        endGrab();
    destroyCell(registry_, bodies_, cell);
}

// ---------------------------------------------------------------------------
// Linaje
// ---------------------------------------------------------------------------

void World::setPlayer(entt::entity cell) {
    if (!registry_.valid(cell) || !registry_.all_of<Locomotion>(cell))
        return;
    if (registry_.valid(player_) && player_ != cell) {
        registry_.remove<Player>(player_);
        registry_.emplace_or_replace<Autopilot>(player_);
    }
    registry_.remove<Autopilot>(cell);
    registry_.emplace_or_replace<Player>(cell);
    player_ = cell;
}

std::vector<entt::entity> World::lineageMembers() const {
    std::vector<entt::entity> out;
    for (auto [e, lin] : registry_.view<Lineage>().each())
        if (lin.run == run_ && registry_.all_of<Metabolism>(e))
            out.push_back(e);
    std::sort(out.begin(), out.end());
    return out;
}

Census World::census() const {
    Census c;
    for (auto [e, body] : registry_.view<RingBody>().each()) {
        if (registry_.all_of<Rock>(e))
            continue; // los granos de arena no son biomasa
        const Metabolism* m = registry_.try_get<Metabolism>(e);
        const float mass = m ? m->biomass : cellMassPmol(body.restArea);
        c.totalBiomass += mass;
        if (const Species* sp = registry_.try_get<Species>(e))
            ++c.species[static_cast<int>(sp->id)];
        const Lineage* lin = registry_.try_get<Lineage>(e);
        if (lin && lin->run == run_) {
            c.lineageBiomass += mass;
            ++c.lineageCells;
            c.maxGeneration = std::max(c.maxGeneration, lin->generation);
        } else {
            ++c.otherCells;
        }
    }
    for (float m : bacteria_.mass)
        c.totalBiomass += m * kBacteriumPmol;
    c.totalBiomass += cyano_.biomass();
    c.cyanoCells = static_cast<int>(cyano_.cellCount());
    c.totalBiomass += fungi_.biomass();
    for (const Mycelium& m : fungi_.mycelia)
        ++c.mycelia[static_cast<int>(m.kind)];
    c.hyphaNodes = static_cast<int>(fungi_.nodeCount());
    c.restingSporangia = static_cast<int>(fungi_.resting.size());
    c.totalBiomass += plasmo_.biomass();
    c.plasmodia = static_cast<int>(plasmo_.plasmodia.size());
    c.plasmodiumNodes = static_cast<int>(plasmo_.nodeCount());
    c.plasmodiumEdges = static_cast<int>(plasmo_.edgeCount());
    c.sclerotia = static_cast<int>(plasmo_.sclerotia.size());
    c.oatFlakes = static_cast<int>(plasmo_.flakes.size());
    for (const Plasmodium& pl : plasmo_.plasmodia)
        c.plasmodiumLength += PlasmodiumSystem::totalLength(pl);
    c.lichenized = static_cast<int>(registry_.view<Lichenized>().size());
    c.zoospores = static_cast<int>(fungi_.spores.size());
    for (auto [e, inf] : registry_.view<ChytridInfection>().each()) {
        c.totalBiomass += inf.mass;
        ++c.infectedAlgae;
    }
    for (std::size_t i = 0; i < bacteria_.size(); ++i) {
        c.quorumBacteria += bacteria_.quorum[i];
        c.phageInfected += bacteria_.phage[i] >= 0.0f ? 1 : 0;
        c.bdelloplasts += bacteria_.bdello[i] >= 0.0f ? 1 : 0;
    }
    c.bdellovibrios = static_cast<int>(bdello_.size());
    double ph = 0.0;
    c.maxPH = 0.0f;
    for (std::size_t k = 0; k < ph_.v.size(); ++k) {
        if (!dishMask_.isWater(k))
            continue;
        ph += ph_.v[k];
        c.maxPH = std::max(c.maxPH, ph_.v[k]);
    }
    const auto cells = static_cast<double>(dishMask_.waterCells);
    c.meanPH = static_cast<float>(ph / cells);
    c.meanNitrogen = static_cast<float>(nitrogen_.sum() / cells);
    c.meanOxygen = static_cast<float>(oxygen_.sum() / cells);
    c.meanPhosphate = static_cast<float>(phosphate_.sum() / cells);
    c.meanSilicate = static_cast<float>(silicate_.sum() / cells);
    c.meanIron = static_cast<float>(iron_.sum() / cells);
    c.meanVitamin = static_cast<float>(vitamin_.sum() / cells);
    c.meanOrganic = static_cast<float>(nutrients_.sum() / cells);
    c.meanCO2 = static_cast<float>(co2_.sum() / cells);
    c.meanToxin = static_cast<float>(toxin_.sum() / cells);
    c.meanPhage = static_cast<float>(phage_.sum() / cells);
    c.bacteria = static_cast<int>(bacteria_.size());
    return c;
}

void World::populate(int cells) {
    // Grilla con jitter: las células no se superponen al aparecer y dejan libre el centro.
    constexpr float kSpacing = 180.0f;
    const int cols = static_cast<int>(width_ / kSpacing), rows = static_cast<int>(height_ / kSpacing);
    const Vec2 middle = {0.5f * width_, 0.5f * height_};
    int placed = 0;
    for (int k = 0; k < cols * rows && placed < cells; ++k) {
        Rng rng(config_.seed, rng_stream::kPopulate, static_cast<std::uint64_t>(k), 0);
        const int idx = static_cast<int>(rng.uniform() * static_cast<float>(cols * rows));
        const Vec2 p = {(static_cast<float>(idx % cols) + 0.5f) * kSpacing + rng.uniform(-30.0f, 30.0f),
                        (static_cast<float>(idx / cols) + 0.5f) * kSpacing + rng.uniform(-30.0f, 30.0f)};
        if (std::abs(p.x - middle.x) < 250.0f && std::abs(p.y - middle.y) < 200.0f)
            continue;
        if (cellAt(p) != entt::null)
            continue;
        spawnCell(randomCellSpec(rng, p));
        ++placed;
    }
}

// ---------------------------------------------------------------------------
// Conjugación (Paramecium)
// ---------------------------------------------------------------------------

void World::holdAt(RingBody& body, Vec2 point) {
    // El punto de membrana más cercano a la clavija queda pegado; el resto lo sigue por las
    // restricciones del cuerpo.
    std::uint32_t best = body.ring[0];
    float bd = 1e18f;
    for (std::uint32_t s : body.ring) {
        const Vec2 d = {minImage(point.x - bodies_.x[s], width_), minImage(point.y - bodies_.y[s], height_)};
        if (dot(d, d) < bd) {
            bd = dot(d, d);
            best = s;
        }
    }
    const Vec2 d = {minImage(point.x - bodies_.x[best], width_), minImage(point.y - bodies_.y[best], height_)};
    bodies_.vx[best] += d.x * 20.0f;
    bodies_.vy[best] += d.y * 20.0f;
}

void World::pullTogether(RingBody& body, const RingBody& partner) {
    // Adhesión de las regiones orales: los puntos cercanos a la pareja se acercan a ella.
    const Vec2 pc = partner.centroid + imageShift(partner, body.centroid) - partner.centroid;
    const Vec2 to = (partner.centroid + pc) - body.centroid;
    const float dist = length(to);
    if (dist < 1e-3f)
        return;
    const Vec2 dir = to * (1.0f / dist);
    for (std::uint32_t s : body.ring) {
        const Vec2 r = Vec2{bodies_.x[s], bodies_.y[s]} - body.centroid;
        if (dot(r, dir) <= 0.0f)
            continue;
        bodies_.vx[s] += dir.x * 3.0f;
        bodies_.vy[s] += dir.y * 3.0f;
    }
}

entt::entity World::conjugationPartner(entt::entity cell) const {
    if (!registry_.valid(cell) || registry_.any_of<Conjugating, Dividing>(cell))
        return entt::null;
    const Locomotion* loco = registry_.try_get<Locomotion>(cell);
    const Heredity* h = registry_.try_get<Heredity>(cell);
    if (!loco || !h || loco->type != LocomotionType::Cilia)
        return entt::null;
    const RingBody& body = registry_.get<RingBody>(cell);
    entt::entity best = entt::null;
    float bestDist = 40.0f; // [µm] entre membranas, aproximado por las cajas
    for (auto [e, other, ol, oh] : registry_.view<RingBody, Locomotion, Heredity>().each()) {
        if (e == cell || ol.type != LocomotionType::Cilia || registry_.any_of<Conjugating, Dividing>(e))
            continue;
        if (oh.genome[Gene::MatingType] == h->genome[Gene::MatingType])
            continue; // mismo tipo de apareamiento: no se reconocen como pareja
        const Vec2 d = {minImage(other.centroid.x - body.centroid.x, width_),
                        minImage(other.centroid.y - body.centroid.y, height_)};
        const float gap = length(d) - 0.5f * (std::min(body.aabbMax.x - body.aabbMin.x, body.aabbMax.y - body.aabbMin.y) +
                                                std::min(other.aabbMax.x - other.aabbMin.x, other.aabbMax.y - other.aabbMin.y));
        if (gap < bestDist) {
            bestDist = gap;
            best = e;
        }
    }
    return best;
}

bool World::tryConjugate(entt::entity cell) {
    const entt::entity partner = conjugationPartner(cell);
    if (partner == entt::null)
        return false;
    registry_.emplace<Conjugating>(cell, Conjugating{partner, 0.0f});
    registry_.emplace<Conjugating>(partner, Conjugating{cell, 0.0f});
    return true;
}

void World::finishConjugation(entt::entity a, entt::entity b) {
    if (!registry_.valid(a) || !registry_.valid(b))
        return;
    Heredity& ha = registry_.get<Heredity>(a);
    Heredity& hb = registry_.get<Heredity>(b);
    Rng rng(config_.seed, rng_stream::kMutation, entt::to_integral(a) * 7919ull + entt::to_integral(b), step_);
    // Intercambio recíproco de pronúcleos: los dos exconjugantes quedan con el mismo genotipo
    // (el tipo de apareamiento de cada uno se vuelve a determinar).
    const Genome child = recombine(ha.genome, hb.genome, rng);
    const LocomotionType type = registry_.get<Locomotion>(a).type;
    for (entt::entity e : {a, b}) {
        Heredity& h = registry_.get<Heredity>(e);
        h.genome = child;
        h.genome[Gene::MatingType] = rng.uniform() < 0.5f ? 0.0f : 1.0f;
        h.traits = traitsOf(h.genome, type);
        applyTraits(e);
        refillOrganelles(registry_.get<RingBody>(e), bodies_, h.genome, rng);
        registry_.remove<Conjugating>(e);
    }
    (void)ha;
    (void)hb;
    if (a == player_ || b == player_)
        events_.push_back({WorldEvent::Type::PlayerConjugated, a, b, ""});
}

void World::tickDefenses(float dt) {
    std::vector<entt::entity> expired;
    for (auto [e, d] : registry_.view<Discharge>().each()) {
        d.timer -= dt;
        if (d.timer <= 0.0f)
            expired.push_back(e);
    }
    for (entt::entity e : expired) // no se quita mientras se recorre la vista
        registry_.remove<Discharge>(e);
}

// ---------------------------------------------------------------------------
// Ecosistema: percepción, caza y defensas
// ---------------------------------------------------------------------------

Perception World::perceive(entt::entity e, const RingBody& body, SpeciesId species) const {
    PROFILE_SCOPE("World::perceive");
    Perception p;
    const SpeciesInfo& info = speciesInfo(species);
    const float maxPreyArea = feedingParams_.maxPreyAreaRatio * std::abs(body.restArea);
    const float myRadius = 0.25f * ((body.aabbMax.x - body.aabbMin.x) + (body.aabbMax.y - body.aabbMin.y));
    float bestPrey = 1e9f, bestThreat = 1e9f;
    // Solo los vecinos (grilla de centroides): nadie percibe más allá de ~300 µm.
    const int range = static_cast<int>(std::ceil(kPerceptionRange / kPerceptionCell));
    const int cx0 = static_cast<int>(body.centroid.x / width_ * percGridX_);
    const int cy0 = static_cast<int>(body.centroid.y / height_ * percGridY_);
    for (int gy = cy0 - range; gy <= cy0 + range && gy - (cy0 - range) < percGridY_; ++gy)
    for (int gx = cx0 - range; gx <= cx0 + range && gx - (cx0 - range) < percGridX_; ++gx)
    for (entt::entity oe : percGrid_[static_cast<std::size_t>(wrapIndex(gy, percGridY_)) * percGridX_ + wrapIndex(gx, percGridX_)]) {
        if (oe == e || !registry_.valid(oe) || !registry_.all_of<RingBody, Species>(oe))
            continue;
        const RingBody& other = registry_.get<RingBody>(oe);
        const Species& os = registry_.get<Species>(oe);
        const std::uint32_t bit = speciesBit(os.id);
        const bool prey = (info.preyMask & bit) && !registry_.any_of<Cyst, Discharge>(oe) &&
                          (info.feeding != FeedingMode::Phagocytosis || std::abs(other.restArea) <= maxPreyArea);
        const bool threat = (info.fleeMask & bit) && !registry_.all_of<Cyst>(oe);
        if (!prey && !threat)
            continue;
        const Vec2 d = {minImage(other.centroid.x - body.centroid.x, width_),
                        minImage(other.centroid.y - body.centroid.y, height_)};
        const float otherRadius = 0.25f * ((other.aabbMax.x - other.aabbMin.x) + (other.aabbMax.y - other.aabbMin.y));
        const float gap = std::max(0.0f, length(d) - myRadius - otherRadius);
        if (prey && gap < bestPrey) {
            bestPrey = gap;
            p.hasPrey = true;
            p.prey = body.centroid + d;
            p.preyDistance = gap;
        }
        if (threat && gap < bestThreat) {
            bestThreat = gap;
            p.hasThreat = true;
            p.threat = body.centroid + d;
            p.threatDistance = gap;
        }
    }
    p.food = std::log1p(std::max(0.0f, nutrients_.grid().sample(body.centroid.x, body.centroid.y)));
    p.acid = std::log1p(std::max(0.0f, co2_.grid().sample(body.centroid.x, body.centroid.y) - units::kCO2Saturation) /
                        units::kCO2Saturation);
    // Choque del polo anterior contra otra célula o contra la pared de la placa.
    {
        const Vec2 head = bodyHeading(body);
        float half = 0.0f;
        for (const Vec2& q : body.restRing)
            half = std::max(half, q.x);
        const Vec2 tip = body.centroid + head * half;
        p.bumped = !dish_.contains(tip, 6.0f);
        // Vecinos por la grilla de centroides (±2 celdas: alcanza a las células grandes).
        const int tx = static_cast<int>(wrapCoord(tip.x, width_) / width_ * percGridX_);
        const int ty = static_cast<int>(wrapCoord(tip.y, height_) / height_ * percGridY_);
        bool done = p.bumped;
        for (int gy = ty - 2; gy <= ty + 2 && !done; ++gy)
        for (int gx = tx - 2; gx <= tx + 2 && !done; ++gx)
        for (entt::entity oe : percGrid_[static_cast<std::size_t>(wrapIndex(gy, percGridY_)) * percGridX_ + wrapIndex(gx, percGridX_)]) {
            if (!registry_.valid(oe) || !registry_.all_of<RingBody>(oe))
                continue;
            const RingBody* other = &registry_.get<RingBody>(oe);
            if (other == &body)
                continue;
            const Vec2 shift = imageShift(*other, tip);
            const Vec2 t = tip - shift;
            if (t.x < other->aabbMin.x - 6.0f || t.x > other->aabbMax.x + 6.0f || t.y < other->aabbMin.y - 6.0f ||
                t.y > other->aabbMax.y + 6.0f)
                continue;
            if (polygon::closestOnBoundary(other->poly.data(), static_cast<int>(other->poly.size()), t).dist2 < 36.0f) {
                p.bumped = true;
                done = true;
                break;
            }
        }
    }
    // Luz a cada lado del polo anterior (estigma y fotorreceptor de los flagelados).
    const Vec2 h = bodyHeading(body), side = {-h.y, h.x};
    const Vec2 front = body.centroid + h * myRadius;
    const Vec2 l = front + side * 15.0f, r = front - side * 15.0f;
    p.lightLeft = light_.sample(l.x, l.y);
    p.lightRight = light_.sample(r.x, r.y);
    return p;
}

bool World::repelledByTrichocysts(entt::entity predator, entt::entity preyEntity) {
    // Defensa: al sentir el ataque, la presa dispara sus tricocistos (arpones de proteína que
    // eyecta en milisegundos) y puede rechazar al depredador.
    const Heredity* h = registry_.try_get<Heredity>(preyEntity);
    if (!h || h->traits.trichocystDefense <= 0.0f)
        return false;
    const RingBody& prey = registry_.get<RingBody>(preyEntity);
    Rng rng(config_.seed, rng_stream::kDefense, prey.id, step_);
    if (rng.uniform() >= h->traits.trichocystDefense)
        return false;
    registry_.emplace_or_replace<Discharge>(preyEntity, Discharge{5.0f});
    if (Metabolism* pm = registry_.try_get<Metabolism>(preyEntity))
        pm->reserves = std::max(0.0f, pm->reserves - 0.02f * pm->biomass); // rearmar cuesta
    RingBody& body = registry_.get<RingBody>(predator);
    const Vec2 preyCenter = prey.centroid + imageShift(prey, body.centroid) - prey.centroid + prey.centroid;
    const float reach = 0.8f * length(prey.aabbMax - prey.aabbMin) + 20.0f;
    for (std::uint32_t s : body.ring) { // los arpones empujan la membrana atacante
        const Vec2 d = Vec2{bodies_.x[s], bodies_.y[s]} - preyCenter;
        const float len = length(d);
        if (len < 1e-3f || len > reach)
            continue;
        bodies_.vx[s] += d.x / len * 20.0f;
        bodies_.vy[s] += d.y / len * 20.0f;
    }
    if (Autopilot* pilot = registry_.try_get<Autopilot>(predator))
        pilot->reverse = 1.5f; // el depredador retrocede
    return true;
}

void World::hunt(entt::entity predator, const RingBody& body) {
    PROFILE_SCOPE("World::hunt");
    // Didinium: la probóscide (polo anterior) toca a la presa, los toxicistos la paralizan y la
    // traga entera por el citostoma, que se dilata enormemente.
    std::size_t front = 0;
    for (std::size_t i = 1; i < body.restRing.size(); ++i)
        if (body.restRing[i].x > body.restRing[front].x)
            front = i;
    const Vec2 mouth = {bodies_.x[body.ring[front]], bodies_.y[body.ring[front]]};
    if (isSatiated(predator))
        return; // digiriendo: Didinium caza ~1 paramecio cada pocas horas
    const std::uint32_t preyMask = speciesInfo(speciesOfEntity(registry_, predator)).preyMask;
    for (const RingBody* other : bodyList_) {
        const auto oe = static_cast<entt::entity>(other->id);
        const Species* os = registry_.try_get<Species>(oe);
        if (!os || !(preyMask & speciesBit(os->id)) || registry_.any_of<Cyst, Discharge>(oe))
            continue;
        if (std::any_of(swallows_.begin(), swallows_.end(), [&](const auto& sw) { return sw.second == oe; }))
            continue;
        const Vec2 shift = imageShift(*other, mouth);
        const polygon::Closest cl = polygon::closestOnBoundary(other->poly.data(), static_cast<int>(other->poly.size()),
                                                               mouth - shift);
        if (cl.dist2 > 8.0f * 8.0f)
            continue;
        if (!repelledByTrichocysts(predator, oe))
            swallows_.push_back({predator, oe});
        return;
    }
}

void World::swallow(entt::entity predator, entt::entity preyEntity) {
    PROFILE_SCOPE("World::swallow");
    smallPreyCenter_.clear();
    {
        const RingBody& pb = registry_.get<RingBody>(predator);
        const RingBody& py = registry_.get<RingBody>(preyEntity);
        if (smallPrey(py)) {
            // Dentro de la membrana, hacia la presa.
            const Vec2 c = py.centroid + imageShift(py, pb.centroid) - py.centroid + py.centroid;
            const Vec2 d = c - pb.centroid;
            const float len = std::max(1e-3f, length(d));
            const float radius = 0.25f * ((pb.aabbMax.x - pb.aabbMin.x) + (pb.aabbMax.y - pb.aabbMin.y));
            smallPreyCenter_[preyEntity] = pb.centroid + d * (std::max(0.0f, std::min(len, radius) - 10.0f) / len);
        }
    }
    const CellLook look = registry_.get<CellLook>(preyEntity);
    const RingBody& prey = registry_.get<RingBody>(preyEntity);
    const float preyArea = std::abs(prey.restArea);
    float preyMass = cellMassPmol(preyArea), preyPhosphorus = 0.0f;
    if (const Metabolism* m = registry_.try_get<Metabolism>(preyEntity)) {
        preyMass = m->biomass + m->reserves;
        preyPhosphorus = m->phosphorus;
    }
    if (const Digestion* pd = registry_.try_get<Digestion>(preyEntity))
        for (const FoodVacuole& v : pd->vacuoles) {
            preyMass += v.contentPmol;
            preyPhosphorus += v.phosphorus;
        }
    if (const ChytridInfection* inf = registry_.try_get<ChytridInfection>(preyEntity))
        preyPhosphorus += inf->phosphorus;
    const float preyNitrogen = carriedNitrogen(preyEntity);
    if (preyEntity == player_) {
        events_.push_back({WorldEvent::Type::PlayerDied, preyEntity, entt::null, "devorada por Didinium"});
        player_ = entt::null;
        lastDeathCause_ = "devorada por Didinium";
    }
    if (grabSlot_ != kNoGrab && bodies_.owner[grabSlot_] == prey.id)
        endGrab();
    recordDeath(preyEntity, std::string("tragada por ") + speciesInfo(speciesOfEntity(registry_, predator)).name);
    if (const float si = releaseMicronutrients(preyEntity, prey.centroid); si > 0.0f)
        exchange(silicate_, prey.centroid, si, Flow::Death);
    destroyCell(registry_, bodies_, preyEntity);
    // Destruir la presa compacta el almacenamiento de EnTT: las referencias al depredador
    // tomadas antes pueden quedar colgadas. Se vuelven a obtener.
    RingBody& predatorBody = registry_.get<RingBody>(predator);
    Digestion& predatorDig = registry_.get<Digestion>(predator);
    // Didinium: la presa ocupa casi todo el depredador hinchado (vacuola central). Ameba con
    // presa chica: la vacuola se forma cerca de donde la tocó.
    const Vec2 site = smallPreyCenter_.count(preyEntity) ? smallPreyCenter_[preyEntity] : predatorBody.centroid;
    ingest(predatorDig, predatorBody, bodies_, site, preyArea, preyMass, look.r, look.g, look.b, true,
           preyPhosphorus, 1.0f, preyNitrogen);
    ++predatorDig.cellsEaten;
    predatorDig.handling = speciesInfo(speciesOfEntity(registry_, predator)).handlingHours * 3600.0f;
}

bool World::smallPrey(const RingBody& prey) const {
    // Espesor efectivo 2·área/perímetro: una Euglena es larga pero fina (~10 µm), un paramecio
    // es ancho (~35 µm). Por debajo de ~4 puntos de membrana del depredador, la copa no se
    // resuelve.
    const float perimeter = soft_body::perimeter(prey, bodies_);
    return perimeter > 0.0f && 2.0f * std::abs(prey.restArea) / perimeter < 12.0f;
}

// ---------------------------------------------------------------------------
// Sésiles, suctorios y Lacrymaria
// ---------------------------------------------------------------------------

namespace {
std::size_t poleIndex(const RingBody& body, bool anterior) {
    std::size_t best = 0;
    for (std::size_t i = 1; i < body.restRing.size(); ++i)
        if (anterior ? body.restRing[i].x > body.restRing[best].x : body.restRing[i].x < body.restRing[best].x)
            best = i;
    return best;
}
} // namespace

void World::anchorHere(entt::entity e) {
    // El tallo sale del polo posterior hacia atrás y se fija al vidrio.
    const RingBody& body = registry_.get<RingBody>(e);
    const SpeciesInfo& info = speciesInfo(speciesOfEntity(registry_, e));
    const std::uint32_t base = body.ring[poleIndex(body, false)];
    const Vec2 h = bodyHeading(body);
    const Vec2 hold = Vec2{bodies_.x[base], bodies_.y[base]} - h * info.stalkLength;
    registry_.emplace_or_replace<Anchored>(e, Anchored{{wrapCoord(hold.x, width_), wrapCoord(hold.y, height_)},
                                                       info.stalkLength});
}

void World::applyAnchor(entt::entity e, RingBody& body, Anchored& anchor, bool disturbed, float dt) {
    PROFILE_SCOPE("World::applyAnchor");
    const SpeciesInfo& info = speciesInfo(speciesOfEntity(registry_, e));
    // Espasmonema: ante una perturbación el tallo se enrolla en ~10 ms y se re-extiende en
    // unos segundos.
    if (info.contractileStalk && disturbed)
        anchor.contracted = 1.0f;
    anchor.contracted = std::max(0.0f, anchor.contracted - dt / 4.0f);
    anchor.stalk = info.stalkLength * (1.0f - 0.85f * anchor.contracted);

    const std::size_t n = body.ring.size();
    const std::size_t back = poleIndex(body, false), front = poleIndex(body, true);
    const Vec2 base = {bodies_.x[body.ring[back]], bodies_.y[body.ring[back]]};
    const Vec2 hold = base + Vec2{minImage(anchor.holdfast.x - base.x, width_), minImage(anchor.holdfast.y - base.y, height_)};
    const Vec2 out = base - hold;
    const float dist = std::max(1e-3f, length(out));
    const Vec2 dir = out * (1.0f / dist);
    // El tallo tira del polo posterior hasta su largo actual (rápido al contraerse).
    const Vec2 target = hold + dir * anchor.stalk;
    const float rate = anchor.contracted > 0.9f ? 60.0f : 6.0f;
    for (int k = -2; k <= 2; ++k) {
        const std::uint32_t s = body.ring[(back + n + static_cast<std::size_t>(k + static_cast<int>(n))) % n];
        bodies_.vx[s] += (target.x - base.x) * rate;
        bodies_.vy[s] += (target.y - base.y) * rate;
    }
    // El tallo mantiene la célula erguida: el polo anterior se aleja del punto de fijación.
    const std::uint32_t tip = body.ring[front];
    bodies_.vx[tip] += dir.x * 3.0f;
    bodies_.vy[tip] += dir.y * 3.0f;
}

void World::trapWithTentacles(entt::entity e, const RingBody& body) {
    PROFILE_SCOPE("World::trapWithTentacles");
    // Suctorio: los tentáculos capitados se adhieren a un ciliado que pasa (descargan
    // haptocistos) y lo sujetan mientras le succionan el citoplasma.
    for (auto [pe, cap] : registry_.view<Captured>().each())
        if (cap.by == e)
            return; // ya tiene una presa
    const SpeciesInfo& info = speciesInfo(speciesOfEntity(registry_, e));
    for (const RingBody* other : bodyList_) {
        const auto oe = static_cast<entt::entity>(other->id);
        const Species* os = registry_.try_get<Species>(oe);
        if (!os || !(info.preyMask & speciesBit(os->id)) || registry_.any_of<Cyst, Captured, Discharge>(oe))
            continue;
        const Vec2 shift = imageShift(*other, body.centroid);
        const float d2 = polygon::closestOnBoundary(other->poly.data(), static_cast<int>(other->poly.size()),
                                                    body.centroid - shift)
                             .dist2;
        if (d2 > info.tentacleReach * info.tentacleReach)
            continue;
        if (!repelledByTrichocysts(e, oe)) {
            registry_.emplace<Captured>(oe, Captured{e});
            registry_.get<Digestion>(e).handling = info.handlingHours * 3600.0f;
        }
        return;
    }
}

void World::strikeWithNeck(entt::entity e, const RingBody& body, float dt) {
    PROFILE_SCOPE("World::strikeWithNeck");
    // Lacrymaria olor: el cuello (lleno de microtúbulos) se estira hacia la presa y su extremo
    // ciliado con toxicistos la atrapa; después se retrae.
    Neck& neck = registry_.get_or_emplace<Neck>(e);
    const SpeciesInfo& info = speciesInfo(speciesOfEntity(registry_, e));
    const std::uint32_t front = body.ring[poleIndex(body, true)];
    const Vec2 head = {bodies_.x[front], bodies_.y[front]};
    const bool full = isSatiated(e);

    auto preyOk = [&](entt::entity oe) {
        if (!registry_.valid(oe) || !registry_.all_of<RingBody, Species>(oe) || registry_.any_of<Cyst, Captured>(oe))
            return false;
        return (info.preyMask & speciesBit(registry_.get<Species>(oe).id)) != 0;
    };
    auto imageCenter = [&](entt::entity oe) {
        const RingBody& o = registry_.get<RingBody>(oe);
        return o.centroid + imageShift(o, head) - o.centroid + o.centroid;
    };
    if (!full && (!preyOk(neck.target) || length(imageCenter(neck.target) - head) > info.neckReach)) {
        neck.target = entt::null;
        float best = info.neckReach;
        for (const RingBody* other : bodyList_) {
            const auto oe = static_cast<entt::entity>(other->id);
            if (oe == e || !preyOk(oe))
                continue;
            const float d = length(imageCenter(oe) - head);
            if (d < best) {
                best = d;
                neck.target = oe;
            }
        }
    }
    if (full || neck.target == entt::null) {
        neck.length = std::max(0.0f, neck.length - 200.0f * dt); // se retrae
        return;
    }
    const Vec2 to = imageCenter(neck.target) - head;
    neck.direction = to * (1.0f / std::max(1e-3f, length(to)));
    neck.length = std::min(info.neckReach, neck.length + 300.0f * dt);
    const Vec2 tip = head + neck.direction * neck.length;
    const RingBody& prey = registry_.get<RingBody>(neck.target);
    const Vec2 shift = imageShift(prey, tip);
    const int pn = static_cast<int>(prey.poly.size());
    if (polygon::contains(prey.poly.data(), pn, tip - shift) ||
        polygon::closestOnBoundary(prey.poly.data(), pn, tip - shift).dist2 < 16.0f) {
        if (!repelledByTrichocysts(e, neck.target))
            swallows_.push_back({e, neck.target});
        neck.target = entt::null;
    }
}

// ---------------------------------------------------------------------------
// Filamentos y colonias
// ---------------------------------------------------------------------------

std::vector<entt::entity> World::spawnFilament(SpeciesId species, Vec2 start, float angle, int cells, std::uint32_t run) {
    const SpeciesInfo& info = speciesInfo(species);
    const Vec2 dir = {std::cos(angle), std::sin(angle)};
    std::vector<entt::entity> chain;
    Rng rng(config_.seed, rng_stream::kPopulate, 9000 + registry_.view<RingBody>().size(), 0);
    for (int k = 0; k < cells; ++k) {
        Genome g = speciesGenome(species);
        mutate(g, info.locomotion, rng, 0.3f);
        const Vec2 p = start + dir * (static_cast<float>(k) * (2.0f * info.spec.radiusX + 4.0f));
        chain.push_back(spawnOrganism(species, {wrapCoord(p.x, width_), wrapCoord(p.y, height_)}, g, run, angle));
        if (k > 0)
            linkCells(chain[k - 1], chain[k]);
    }
    return chain;
}

void World::linkCells(entt::entity ea, entt::entity eb) {
    // Dos uniones (las esquinas) entre el extremo de A que mira a B y el de B que mira a A.
    const RingBody& a = registry_.get<RingBody>(ea);
    const RingBody& b = registry_.get<RingBody>(eb);
    const Vec2 bc = b.centroid + imageShift(b, a.centroid) - b.centroid + b.centroid;
    const Vec2 axis = (bc - a.centroid) * (1.0f / std::max(1e-3f, length(bc - a.centroid)));
    auto corner = [&](const RingBody& body, Vec2 c, Vec2 ax, float sideSign) {
        std::uint32_t best = body.ring[0];
        float score = -1e9f;
        for (std::uint32_t s : body.ring) {
            const Vec2 r = Vec2{bodies_.x[s], bodies_.y[s]} - c;
            const float side = cross(ax, r) * sideSign;
            if (side <= 0.0f)
                continue;
            const float sc = dot(r, ax) + 0.3f * side;
            if (sc > score) {
                score = sc;
                best = s;
            }
        }
        return best;
    };
    const Vec2 shift = bc - b.centroid;
    std::uint32_t ca[2], cb[2];
    for (int k = 0; k < 2; ++k) {
        const float sideSign = k == 0 ? 1.0f : -1.0f;
        ca[k] = corner(a, a.centroid, axis, sideSign);
        cb[k] = corner(b, b.centroid, axis * -1.0f, -sideSign);
    }
    auto add = [&](std::uint32_t sa, std::uint32_t sb) {
        const Vec2 pa = {bodies_.x[sa], bodies_.y[sa]}, pb = Vec2{bodies_.x[sb], bodies_.y[sb]} + shift;
        links_.push_back({sa, sb, ea, eb, std::max(length(pb - pa), 2.0f * bodies_.radius[sa])});
    };
    add(ca[0], cb[0]);
    add(ca[1], cb[1]);
}

void World::solveLinks(float h) {
    if (links_.empty())
        return;
    PROFILE_SCOPE("World::solveLinks");
    // Uniones de pared celular: casi rígidas. Serial: escriben en dos cuerpos.
    const float f = 1.0f - std::exp(-300.0f * h);
    for (const AdhesionLink& l : links_) {
        const Vec2 pa = {bodies_.x[l.a], bodies_.y[l.a]};
        const Vec2 pb = pa + Vec2{minImage(bodies_.x[l.b] - pa.x, width_), minImage(bodies_.y[l.b] - pa.y, height_)};
        const Vec2 d = pb - pa;
        const float len = length(d);
        const float w = bodies_.invMass[l.a] + bodies_.invMass[l.b];
        if (len < 1e-6f || w <= 0.0f)
            continue;
        const Vec2 corr = d * (f * (len - l.rest) / (len * w));
        bodies_.x[l.a] += corr.x * bodies_.invMass[l.a];
        bodies_.y[l.a] += corr.y * bodies_.invMass[l.a];
        bodies_.x[l.b] -= corr.x * bodies_.invMass[l.b];
        bodies_.y[l.b] -= corr.y * bodies_.invMass[l.b];
    }
}

void World::relinkAfterDivision(entt::entity mother, entt::entity child) {
    // Las uniones de la madre con sus vecinas pasan a la hija que quedó de ese lado, enganchadas
    // en su punto de membrana más cercano (el re-muestreo movió los puntos por el anillo).
    auto nearest = [&](entt::entity e, Vec2 to) {
        const RingBody& body = registry_.get<RingBody>(e);
        std::uint32_t best = body.ring[0];
        float bd = 1e18f;
        for (std::uint32_t s : body.ring) {
            const Vec2 d = {minImage(bodies_.x[s] - to.x, width_), minImage(bodies_.y[s] - to.y, height_)};
            if (dot(d, d) < bd) {
                bd = dot(d, d);
                best = s;
            }
        }
        return std::pair{best, bd};
    };
    for (AdhesionLink& l : links_) {
        for (int end = 0; end < 2; ++end) {
            std::uint32_t& slot = end == 0 ? l.a : l.b;
            entt::entity& owner = end == 0 ? l.ownerA : l.ownerB;
            if (owner != mother)
                continue;
            const Vec2 partner = end == 0 ? Vec2{bodies_.x[l.b], bodies_.y[l.b]} : Vec2{bodies_.x[l.a], bodies_.y[l.a]};
            const auto [sm, dm] = nearest(mother, partner);
            const auto [sc, dc] = nearest(child, partner);
            owner = dc < dm ? child : mother;
            slot = dc < dm ? sc : sm;
        }
    }
    linkCells(mother, child); // la pared nueva entre las dos hijas
}

void World::maintainLinks(float bioDt) {
    // Se cortan las uniones de células muertas, las muy estiradas y, de a poco, las que se
    // fragmentan solas (Spirogyra se reproduce también por fragmentación).
    std::size_t k = 0;
    for (std::size_t i = 0; i < links_.size(); ++i) {
        AdhesionLink l = links_[i];
        bool keep = registry_.valid(l.ownerA) && registry_.valid(l.ownerB) && bodies_.alive[l.a] && bodies_.alive[l.b] &&
                    bodies_.owner[l.a] == entt::to_integral(l.ownerA) && bodies_.owner[l.b] == entt::to_integral(l.ownerB);
        if (keep) {
            const float dx = minImage(bodies_.x[l.b] - bodies_.x[l.a], width_);
            const float dy = minImage(bodies_.y[l.b] - bodies_.y[l.a], height_);
            if (std::sqrt(dx * dx + dy * dy) > 4.0f * l.rest + 10.0f)
                keep = false;
        }
        if (keep && registry_.all_of<Species>(l.ownerA)) {
            const float rate = speciesInfo(registry_.get<Species>(l.ownerA).id).fragmentationPerHour;
            Rng rng(config_.seed, rng_stream::kFeeding + 100, l.a, step_);
            if (rate > 0.0f && rng.uniform() < 0.5f * rate * bioDt / 3600.0f) // dos uniones por pared
                keep = false;
        }
        if (keep)
            links_[k++] = l;
    }
    links_.resize(k);
}

void World::releaseColonies(entt::entity mother) {
    // Volvox: las colonias hijas (gonidios) crecen dentro y nacen rompiendo a la madre.
    // Pediastrum: cada ciclo, una colonia hija formada por zoosporas sale al lado.
    const SpeciesId sid = speciesOfEntity(registry_, mother);
    const SpeciesInfo& info = speciesInfo(sid);
    // Se copia todo antes de crear entidades (emplace puede realocar el almacenamiento).
    const RingBody& body = registry_.get<RingBody>(mother);
    const std::uint32_t bodyId = body.id;
    const Vec2 c = body.centroid;
    const float radius = 0.25f * ((body.aabbMax.x - body.aabbMin.x) + (body.aabbMax.y - body.aabbMin.y));
    const float motherBiomass = registry_.get<Metabolism>(mother).biomass;
    const Genome parent = registry_.all_of<Heredity>(mother) ? registry_.get<Heredity>(mother).genome : speciesGenome(sid);
    const std::uint32_t run = registry_.all_of<Lineage>(mother) ? registry_.get<Lineage>(mother).run : 0;
    const float share = motherBiomass / static_cast<float>(info.colonyDaughters + (info.motherDiesAtRelease ? 0 : 1));
    for (int k = 0; k < info.colonyDaughters; ++k) {
        Rng rng(config_.seed, rng_stream::kMutation + 50, bodyId * 16ull + static_cast<std::uint64_t>(k), step_);
        Genome g = parent;
        mutate(g, info.locomotion, rng);
        const float a = 6.2831853f * (static_cast<float>(k) + rng.uniform()) / static_cast<float>(info.colonyDaughters);
        const float dist = info.motherDiesAtRelease ? 0.5f * radius : (info.budding ? 1.7f * radius : 2.2f * radius);
        const Vec2 p = c + Vec2{std::cos(a), std::sin(a)} * dist;
        const entt::entity d = spawnOrganism(sid, {wrapCoord(p.x, width_), wrapCoord(p.y, height_)}, g, run, a);
        Metabolism& dm = registry_.get<Metabolism>(d);
        // La hija nace chica: escala su tamaño a la biomasa que le tocó.
        dm.birthBiomass = dm.biomass = std::min(dm.biomass, share);
        dm.reserves = 0.3f * dm.biomass;
        if (info.speedScale > 0.0f)
            registry_.emplace<Autopilot>(d);
    }
    ++(info.budding ? observed_.buds : observed_.colonyReleases);
    // Las hijas tomaron su fósforo del agua al nacer; la lisis devuelve el de la madre.
    if (info.motherDiesAtRelease) {
        lyse(mother, "libero sus colonias hijas");
    } else {
        Metabolism& meta = registry_.get<Metabolism>(mother);
        meta.biomass = share;
        meta.birthBiomass = share;
        meta.reserves *= 0.5f;
    }
}

// ---------------------------------------------------------------------------
// Zoósporas de Oedogonium
// ---------------------------------------------------------------------------

void World::releaseZoospore(entt::entity mother) {
    PROFILE_SCOPE("World::releaseZoospore");
    // El protoplasto se contrae, la pared se abre en un anillo y sale como una sola zoóspora
    // grande con una corona de flagelos (estefanoconte). Simplificación: la pared vacía no queda
    // en el filamento; el filamento se corta ahí.
    const SpeciesId sid = registry_.get<Species>(mother).id;
    const RingBody& mb = registry_.get<RingBody>(mother);
    const Vec2 at = mb.centroid;
    const float angle = mb.angle;
    const float area = std::abs(mb.restArea);
    const Metabolism m = registry_.get<Metabolism>(mother);
    const Genome genome = registry_.all_of<Heredity>(mother) ? registry_.get<Heredity>(mother).genome : speciesGenome(sid);
    const Lineage lin = registry_.all_of<Lineage>(mother) ? registry_.get<Lineage>(mother) : Lineage{0, 0};
    const bool wasPlayer = mother == player_;
    if (grabSlot_ != kNoGrab && bodies_.owner[grabSlot_] == entt::to_integral(mother))
        endGrab();
    releaseMicronutrients(mother, registry_.get<RingBody>(mother).centroid);
    destroyCell(registry_, bodies_, mother);
    if (wasPlayer)
        player_ = entt::null;

    const entt::entity z = spawnOrganism(sid, at, genome, lin.run, angle);
    registry_.get<Lineage>(z).generation = lin.generation;
    Metabolism& zm = registry_.get<Metabolism>(z);
    // spawnOrganism sacó del agua el P y el N de una célula nueva: vuelven, y la zoóspora se
    // lleva todo lo de la madre (se conservan).
    if (zm.phosphorus > 0.0f)
        exchange(phosphate_, at, zm.phosphorus, Flow::Spawn);
    if (zm.nitrogen > 0.0f)
        exchange(nitrogen_, at, zm.nitrogen, Flow::Spawn);
    zm.biomass = m.biomass;
    zm.reserves = m.reserves;
    zm.phosphorus = m.phosphorus;
    zm.nitrogen = m.nitrogen;
    zm.birthBiomass = m.birthBiomass;
    RingBody& zb = registry_.get<RingBody>(z);
    const float r = std::sqrt(area / units::kPi);
    soft_body::reshapeRest(zb, bodies_, 1.15f * r, 0.87f * r); // ovoide
    zb.shapeRate = 8.0f;
    Locomotion& loco = registry_.get<Locomotion>(z);
    loco.type = LocomotionType::Flagellum; // nada tirando de la corona de flagelos
    loco.power = viscositySpeed_;
    registry_.emplace<Autopilot>(z);
    Rng rng(config_.seed, rng_stream::kCells + 41, zb.id, step_);
    registry_.emplace<Zoospore>(z, Zoospore{rng.uniform(0.3f, 1.0f)}); // nada entre 20 min y 1 h
    if (wasPlayer)
        setPlayer(z);
    noteDeath(mother, sid, at, "la celula se convirtio en una zoospora", z);
    ++observed_.zoospores;
}

void World::settleZoospore(entt::entity e) {
    // La zoóspora se fija, pierde los flagelos y se alarga: es la primera célula de un filamento
    // nuevo, que crece por división intercalar.
    registry_.remove<Zoospore>(e);
    Locomotion& loco = registry_.get<Locomotion>(e);
    loco.type = speciesInfo(speciesOfEntity(registry_, e)).locomotion;
    loco.forward = loco.turn = loco.speed = 0.0f;
    loco.active = false;
    applyTraits(e); // sin nado (las células del filamento no se mueven)
    registry_.remove<Autopilot>(e);
    RingBody& b = registry_.get<RingBody>(e);
    const CellShape& shape = registry_.get<CellShape>(e);
    const float k = std::sqrt(std::abs(soft_body::area(b, bodies_)) / (units::kPi * shape.radiusX * shape.radiusY));
    soft_body::reshapeRest(b, bodies_, shape.radiusX * k, shape.radiusY * k, shape.squareness, shape.taper);
    b.shapeRate = shape.shapeRate;
    ++observed_.zoosporesSettled;
}
