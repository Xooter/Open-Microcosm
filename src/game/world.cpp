#include "game/world.hpp"

#include "core/profiling.hpp"
#include "core/rng.hpp"
#include "core/units.hpp"
#include "sim/polygon.hpp"

#include <cmath>
#include <algorithm>

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
constexpr float kMinDetritusMass = 0.0003f; // [pmol] debajo de esto se disolvió del todo

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
    bucketsX_ = std::max(1, static_cast<int>(width_ / bucketSize_));
    bucketsY_ = std::max(1, static_cast<int>(height_ / bucketSize_));
    buckets_.resize(static_cast<std::size_t>(bucketsX_) * bucketsY_);
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
    fluid_.keepAsBackground();
    if (registry_.valid(player_)) {
        // El agua que mueve el nadador afecta a todo lo demás, pero no a él mismo.
        applySwimmerForces(registry_.get<RingBody>(player_), bodies_,
                           registry_.get<Locomotion>(player_), locomotionParams_, fluid_);
        fluid_.solve(fft_);
    }

    oxygen_.advect(fluid_, dt);
    nutrients_.advect(fluid_, dt);
    updateChemistrySources(dt);
    stepAmbient(dt); // deposita nutrientes de los detritos y su sombra
    oxygen_.diffuse(fft_, dt);
    nutrients_.diffuse(fft_, dt);
    computeLight();

    bacteria_.step(fluid_, nutrients_.grid(), dt, config_.seed, step_, width_, height_);
    stepBodies(dt);
    updateFeeding(dt);       // eventos de topología (fagocitosis): al final del paso
    updateLifecycle(dt);     // metabolismo, división y muerte
    tickDefenses(dt);
    excludeFromCells();

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
    hashVector(h, bodies_.x);
    hashVector(h, bodies_.y);
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
        const Vec2 q = body.centroid + Vec2{minImage(position.x - body.centroid.x, width_),
                                            minImage(position.y - body.centroid.y, height_)};
        if (polygon::contains(body.poly.data(), static_cast<int>(body.poly.size()), q))
            return e;
    }
    return entt::null;
}

void World::removeCellAt(Vec2 position) {
    const entt::entity e = cellAt(position);
    if (e == entt::null)
        return;
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

void World::stepBodies(float dt) {
    PROFILE_SCOPE("World::stepBodies");
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
        const bool swimmer = registry_.valid(player_) && body.id == entt::to_integral(player_);
        auto external = [&](std::uint32_t s) {
            const Vec2 p = {bodies_.x[s], bodies_.y[s]};
            const Vec2 v = (swimmer ? fluid_.backgroundAt(p) : fluid_.velocityAt(p)) + push;
            bodies_.vx[s] = v.x;
            bodies_.vy[s] = v.y;
        };
        for (std::uint32_t s : body.ring)
            external(s);
        for (std::size_t k = 0; k < body.interior.size(); ++k) {
            const std::uint32_t s = body.interior[k];
            external(s);
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
    for (RingBody* body : bodyList_)
        soft_body::adaptPlastic(*body, bodies_, dt);

    members_.clear();
    float maxRadius = 0.0f;
    for (const RingBody* body : bodyList_) {
        const auto n = static_cast<std::uint32_t>(body->ring.size());
        const auto first = static_cast<std::uint32_t>(members_.size());
        for (std::uint32_t i = 0; i < n; ++i) {
            members_.push_back({body->ring[i], first + (i + 1) % n, body->id, i, n});
            maxRadius = std::max(maxRadius, bodies_.radius[body->ring[i]]);
        }
    }

    const int substeps = std::max(1, softTuning_.substeps);
    const float h = dt / static_cast<float>(substeps);
    const float grabFraction = 1.0f - std::exp(-300.0f * h);
    for (int sub = 0; sub < substeps; ++sub) {
#pragma omp parallel for schedule(dynamic, 1)
        for (std::ptrdiff_t b = 0; b < nb; ++b)
            soft_body::predict(*bodyList_[b], bodies_, h);

        if (grabSlot_ != kNoGrab) {
            const std::uint32_t s = grabSlot_;
            bodies_.x[s] += minImage(grabTarget_.x - bodies_.x[s], width_) * grabFraction;
            bodies_.y[s] += minImage(grabTarget_.y - bodies_.y[s], height_) * grabFraction;
        }

        // XPBD: restricciones internas y contactos se intercalan en cada iteración, para que
        // ninguno de los dos tenga siempre la última palabra.
        // Cada iteración aplica su fracción del sub-paso: las tasas [1/s] no cambian de
        // significado con la cantidad de iteraciones.
        const int iterations = std::max(1, softTuning_.iterations);
        const float hIter = h / static_cast<float>(iterations);
        contacts_.prepare(bodies_, members_, width_, height_, maxRadius);
        for (int it = 0; it < iterations; ++it) {
#pragma omp parallel for schedule(dynamic, 1)
            for (std::ptrdiff_t b = 0; b < nb; ++b)
                soft_body::solve(*bodyList_[b], bodies_, softTuning_, hIter);
            contacts_.iterate(bodies_, members_, width_, height_);
        }
    }
    resolvePenetrations();

    // Remallado de membranas plásticas: asigna slots en el pool compartido, por eso es serial.
    for (RingBody* body : bodyList_)
        soft_body::remesh(*body, bodies_, kMembraneSpacing);

#pragma omp parallel for schedule(dynamic, 1)
    for (std::ptrdiff_t b = 0; b < nb; ++b) {
        soft_body::confineGranules(*bodyList_[b], bodies_);
        soft_body::refresh(*bodyList_[b], bodies_);
        soft_body::wrap(*bodyList_[b], bodies_, width_, height_);
    }
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
        const float x = bodies_.x[me.slot], y = bodies_.y[me.slot];
        const int bx = wrapIndex(static_cast<int>(std::floor(wrapCoord(x, width_) / bucketSize_)), bucketsX_);
        const int by = wrapIndex(static_cast<int>(std::floor(wrapCoord(y, height_) / bucketSize_)), bucketsY_);
        for (std::uint32_t b : buckets_[static_cast<std::size_t>(by) * bucketsX_ + bx]) {
            const RingBody& other = *bodyList_[b];
            if (other.id == me.owner)
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

CellSpec speciesSpec(LocomotionType type) {
    CellSpec spec;
    switch (type) {
    case LocomotionType::Cilia: // Paramecium caudatum: ~180 × 50 µm, película rígida
        spec.radiusX = 90.0f;
        spec.radiusY = 25.0f;
        spec.shapeRate = 8.0f;
        spec.r = 160; spec.g = 176; spec.b = 138;
        break;
    case LocomotionType::Flagellum: // Euglena gracilis: ~50 × 14 µm, verde por los cloroplastos
        spec.radiusX = 25.0f;
        spec.radiusY = 7.0f;
        spec.shapeRate = 4.0f;
        spec.r = 120; spec.g = 172; spec.b = 92;
        break;
    case LocomotionType::Pseudopod: // Amoeba proteus (chica): ~180 µm, sin forma propia
        spec.radiusX = 90.0f;
        spec.radiusY = 90.0f;
        spec.shapeRate = 0.0f;
        spec.nucleusFraction = 0.2f;
        spec.r = 188; spec.g = 176; spec.b = 160;
        break;
    }
    return spec;
}

namespace {
// Biomasa de nacimiento de la especie con un gen de tamaño dado (masa ∝ tamaño³).
float targetBirthBiomassOf(LocomotionType type, float size) {
    const CellSpec base = speciesSpec(type);
    return cellMassPmol(units::kPi * base.radiusX * base.radiusY) * size * size * size;
}
} // namespace

void World::applyTraits(entt::entity e) {
    const Heredity& h = registry_.get<Heredity>(e);
    Locomotion& loco = registry_.get<Locomotion>(e);
    loco.power = h.traits.speed;
    registry_.get<Digestion>(e).rateScale = h.traits.digestion;
    Metabolism& meta = registry_.get<Metabolism>(e);
    meta.photosynthetic = h.traits.photosynthesis > 0.0f;
    meta.targetBirthBiomass = targetBirthBiomassOf(loco.type, h.traits.sizeScale);
    CellShape& shape = registry_.get<CellShape>(e);
    shape.shapeRate = speciesSpec(loco.type).shapeRate * h.traits.rigidity;
    RingBody& body = registry_.get<RingBody>(e);
    if (body.membraneReservoirRate <= 0.0f && !registry_.all_of<Dividing>(e))
        body.shapeRate = shape.shapeRate;
}

entt::entity World::spawnOrganism(LocomotionType type, Vec2 center, const Genome& genome) {
    CellSpec spec = speciesSpec(type);
    spec.center = center;
    spec.angle = 0.0f;
    const Traits traits = traitsOf(genome, type);
    spec.radiusX *= traits.sizeScale;
    spec.radiusY *= traits.sizeScale;
    spec.shapeRate *= traits.rigidity;
    const entt::entity e = spawnCell(spec);
    RingBody& body = registry_.get<RingBody>(e);
    Locomotion loco;
    loco.type = type;
    // Citostoma: en el ciliado, en el surco oral (lado ventral); en el flagelado, en el polo
    // anterior junto al flagelo.
    if (type == LocomotionType::Cilia)
        loco.mouthRing = static_cast<std::uint32_t>(0.15f * static_cast<float>(body.ring.size()));
    if (type == LocomotionType::Pseudopod)
        soft_body::makePlastic(body, bodies_, 1.0f);
    Metabolism meta;
    meta.biomass = meta.birthBiomass = cellMassPmol(body.restArea);
    meta.reserves = 0.3f * meta.biomass;
    registry_.emplace<Locomotion>(e, loco);
    registry_.emplace<Digestion>(e);
    registry_.emplace<Metabolism>(e, meta);
    registry_.emplace<Lineage>(e, Lineage{run_, 0});
    registry_.emplace<Heredity>(e, Heredity{genome, traits});
    applyTraits(e);
    Rng rng(config_.seed, rng_stream::kOrganelles, body.id, step_);
    refillOrganelles(body, bodies_, genome, rng);
    return e;
}

entt::entity World::spawnPlayer(LocomotionType type, Vec2 center, const Genome* genome) {
    const entt::entity e = spawnOrganism(type, center, genome ? *genome : speciesGenome(type));
    registry_.emplace<Player>(e);
    player_ = e;
    return e;
}

void World::setPlayerType(LocomotionType type) {
    Vec2 center = {0.5f * width_, 0.5f * height_};
    if (registry_.valid(player_)) {
        center = registry_.get<RingBody>(player_).centroid;
        endGrab();
        destroyCell(registry_, bodies_, player_);
        player_ = entt::null;
    }
    spawnPlayer(type, center);
}

std::vector<const RingBody*> World::preyFor(const RingBody& predator) const {
    std::vector<const RingBody*> prey;
    const float maxArea = feedingParams_.maxPreyAreaRatio * std::abs(predator.restArea);
    // Solapamiento de cajas (con margen de alcance), comparando CENTROS de caja: el
    // pseudópodo puede extenderse lejos del centroide.
    const Vec2 ext = (predator.aabbMax - predator.aabbMin) * 0.5f;
    const Vec2 center = (predator.aabbMax + predator.aabbMin) * 0.5f;
    const float margin = feedingParams_.wrapReach + feedingParams_.contactGap + 10.0f;
    for (auto [e, body] : registry_.view<RingBody>().each()) {
        if (&body == &predator || std::abs(body.restArea) > maxArea)
            continue;
        // Una lórica o testa gruesa no se puede fagocitar.
        if (const Heredity* h = registry_.try_get<Heredity>(e); h && h->traits.armor >= 0.5f)
            continue;
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

void World::applyMotion(float dt) {
    // Controles: el jugador por input; el resto del linaje, por su piloto automático.
    for (auto [e, body, loco] : registry_.view<RingBody, Locomotion>().each()) {
        const bool engulfing = registry_.all_of<PhagocyticCup>(e);
        if (e == player_) {
            loco.forward = playerInput_.forward;
            loco.turn = playerInput_.turn;
            loco.target = playerInput_.aim;
            loco.active = (loco.type == LocomotionType::Flagellum && playerInput_.propel) ||
                          (loco.type == LocomotionType::Pseudopod && playerInput_.pseudopod);
        } else if (Autopilot* pilot = registry_.try_get<Autopilot>(e)) {
            Rng rng(config_.seed, rng_stream::kAutopilot, body.id, step_);
            steerAutopilot(*pilot, loco, body, rng, dt);
        } else {
            continue;
        }
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
        growLips(body, bodies_, prey, shift, *cup, feedingParams_, dt);
        applyCup(body, bodies_, prey, shift, *cup, feedingParams_, dt);
        return;
    }
    for (const RingBody* prey : preyFor(body)) {
        if (registry_.all_of<Lineage>(static_cast<entt::entity>(prey->id)) &&
            registry_.all_of<Lineage>(e) &&
            registry_.get<Lineage>(static_cast<entt::entity>(prey->id)).run == registry_.get<Lineage>(e).run)
            continue; // no se come a su propio linaje
        if (registry_.any_of<PhagocyticCup>(static_cast<entt::entity>(prey->id)))
            continue;
        const auto preyEntity = static_cast<entt::entity>(prey->id);
        if (registry_.all_of<Discharge>(preyEntity))
            continue; // acaba de disparar sus tricocistos: el depredador retrocede
        const std::uint32_t contact =
            findContact(body, bodies_, *prey, imageShift(*prey, body.centroid), feedingParams_);
        if (contact == ~0u)
            continue;
        // Defensa: al sentir el ataque, la presa dispara sus tricocistos (arpones de proteína
        // que eyecta en milisegundos) y puede rechazar al depredador.
        if (const Heredity* h = registry_.try_get<Heredity>(preyEntity); h && h->traits.trichocystDefense > 0.0f) {
            Rng rng(config_.seed, rng_stream::kDefense, prey->id, step_);
            if (rng.uniform() < h->traits.trichocystDefense) {
                registry_.emplace<Discharge>(preyEntity, Discharge{5.0f});
                if (Metabolism* pm = registry_.try_get<Metabolism>(preyEntity))
                    pm->reserves = std::max(0.0f, pm->reserves - 0.02f * pm->biomass); // rearmar cuesta
                const Vec2 pc = prey->centroid + imageShift(*prey, body.centroid) - prey->centroid;
                for (std::uint32_t s : body.ring) { // los arpones empujan la membrana atacante
                    const Vec2 d = Vec2{bodies_.x[s], bodies_.y[s]} - (prey->centroid + pc);
                    const float len = length(d);
                    if (len < 1e-3f || len > 0.8f * length(prey->aabbMax - prey->aabbMin) + 20.0f)
                        continue;
                    bodies_.vx[s] += d.x / len * 20.0f;
                    bodies_.vy[s] += d.y / len * 20.0f;
                }
                continue;
            }
        }
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

    for (auto [e, body, loco, dig] : feeders.each()) {
        if (loco.type == LocomotionType::Pseudopod) {
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
            feedAtMouth(body, loco, dig, dt);
        }
    }
    // Las fagocitosis destruyen entidades: después de recorrer la vista.
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
    RingBody& body = registry_.get<RingBody>(predator);
    Digestion& dig = registry_.get<Digestion>(predator);
    const CellLook look = registry_.get<CellLook>(preyEntity);
    const RingBody& preyBody = registry_.get<RingBody>(preyEntity);
    const float preyArea = std::abs(preyBody.restArea);
    float preyMass = cellMassPmol(preyArea);
    if (const Metabolism* m = registry_.try_get<Metabolism>(preyEntity))
        preyMass = m->biomass + m->reserves;
    const int pn = static_cast<int>(closure.pocket.size());

    // Todo lo que quedó dentro de la copa entra al fagosoma junto con la presa.
    float extraArea = 0.0f, extraPmol = 0.0f;
    int bacteriaInside = 0;
    for (std::size_t i = 0; i < bacteria_.size(); ++i) {
        if (takenBacteria_[i])
            continue;
        const Vec2 q = body.centroid + Vec2{minImage(bacteria_.x[i] - body.centroid.x, width_),
                                            minImage(bacteria_.y[i] - body.centroid.y, height_)};
        if (polygon::contains(closure.pocket.data(), pn, q)) {
            takenBacteria_[i] = 1;
            eatenBacteria_.push_back(i);
            extraArea += kBacteriumArea;
            extraPmol += kBacteriumPmol;
            ++bacteriaInside;
        }
    }
    dig.bacteriaEaten += bacteriaInside;

    if (preyEntity == player_) {
        events_.push_back({WorldEvent::Type::PlayerDied, preyEntity, entt::null, "fagocitada"});
        player_ = entt::null;
        lastDeathCause_ = "fagocitada";
    }
    // El predador pudo tener a la presa sujeta en otra copa: se quita al destruirla.
    destroyCell(registry_, bodies_, preyEntity);

    // Fusión de membranas: el tramo de la copa pasa a ser la membrana del fagosoma.
    for (std::uint32_t s : body.ring)
        if (std::find(closure.outerRing.begin(), closure.outerRing.end(), s) == closure.outerRing.end())
            bodies_.release(s);
    if (grabSlot_ != kNoGrab && !bodies_.alive[grabSlot_])
        endGrab();
    soft_body::replaceRing(body, bodies_, closure.outerRing);

    ingest(dig, body, bodies_, closure.preyCenter, preyArea + extraArea, preyMass + extraPmol,
           look.r, look.g, look.b, true);
    ++dig.cellsEaten;
}

void World::feedAtMouth(RingBody& body, const Locomotion& loco, Digestion& dig, float dt) {
    const std::size_t n = body.ring.size();
    const std::size_t i = loco.mouthRing % n;
    const Vec2 m = {bodies_.x[body.ring[i]], bodies_.y[body.ring[i]]};
    const Vec2 prev = {bodies_.x[body.ring[(i + n - 1) % n]], bodies_.y[body.ring[(i + n - 1) % n]]};
    const Vec2 next = {bodies_.x[body.ring[(i + 1) % n]], bodies_.y[body.ring[(i + 1) % n]]};
    const Vec2 t = (next - prev) * (1.0f / std::max(1e-4f, length(next - prev)));
    const Vec2 site = m + polygon::inwardNormal(t, body.restArea) * 4.0f; // la vacuola se forma adentro
    const float reach = feedingParams_.mouthRadius;
    const bool ciliate = loco.type == LocomotionType::Cilia;
    const float maxParticle = ciliate ? feedingParams_.ciliateMaxParticle : feedingParams_.flagellateMaxParticle;
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
            ingest(dig, body, bodies_, site, kBacteriumArea, kBacteriumPmol, 170, 150, 140, false);
        }
    });

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
            ingest(dig, body, bodies_, site, a, ambient_.mass[k], 140, 112, 72, false);
        }
    }
}

void World::feedByContact(RingBody& body, Digestion& dig, float dt) {
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
            ingest(dig, body, bodies_, site, kBacteriumArea, kBacteriumPmol, 170, 150, 140, false);
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
            ingest(dig, body, bodies_, site, a, ambient_.mass[k], 140, 112, 72, false);
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

void World::exchangeO2(Vec2 at, float pmol) {
    // Se reparte en las 3×3 celdas alrededor de la célula.
    Grid2D& g = oxygen_.grid();
    const float perCell = pmol * microMolarPerPmol() / 9.0f;
    const int cx = g.cellX(at.x), cy = g.cellY(at.y);
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            float& v = g.at(wrapIndex(cx + dx, g.nx), wrapIndex(cy + dy, g.ny));
            v = std::max(0.0f, v + perCell);
        }
}

void World::updateLifecycle(float dt) {
    PROFILE_SCOPE("World::updateLifecycle");
    const float bioDt = dt * bioTimeScale_;
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
        in.digested = dig ? digest(*dig, body, bodies_, feedingParams_, dt, bioDt) * bioTimeScale_ : 0.0f;
        in.activity = 0.0f;
        if (loco) {
            in.activity = loco->type == LocomotionType::Pseudopod
                              ? (loco->active ? 1.0f : 0.0f)
                              : std::min(1.0f, std::abs(loco->speed) / maxSpeedOf(locomotionParams_, loco->type));
        }
        in.bioDt = bioDt;
        const Heredity* her = registry_.try_get<Heredity>(e);
        if (her) {
            in.growthScale = her->traits.growthMax;
            in.maintenanceScale = her->traits.maintenance;
            in.photosynthesisScale = her->traits.photosynthesis;
            in.motilityCostScale = her->traits.motilityCost;
        }
        const MetabolicOutput out = updateMetabolism(meta, mp, in);
        // El intercambio de gases con el agua ocurre a la tasa REAL: comprimir el tiempo
        // biológico no debe acelerar la difusión ni el intercambio con el aire (agotaría o
        // sobresaturaría el O₂ alrededor de cada célula de forma irreal).
        exchangeO2(body.centroid, (out.o2Produced - out.o2Consumed) / bioTimeScale_);

        if (isDead(meta, mp)) {
            dying_.push_back(e);
            continue;
        }

        // Tamaño: biomasa + lo que hay en las vacuolas. Los organelos crecen con la célula.
        const float target = cellAreaForMass(meta.biomass) * (her ? her->traits.volume : 1.0f) +
                             (dig ? vacuoleContentArea(*dig) : 0.0f);
        const float before = std::abs(body.restArea);
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
        if (!dv && readyToDivide(meta, cost)) {
            meta.reserves -= cost * meta.biomass;
            Dividing& d = registry_.emplace<Dividing>(e);
            d.motherRest = body.restRing;
            d.motherArea = std::abs(body.restArea);
        } else if (dv) {
            dv->progress += bioDt / (mp.divisionHours * 3600.0f);
            // El córtex se reorganiza: la forma de reposo pasa de la madre a dos hijas.
            if (body.membraneReservoirRate <= 0.0f && dv->motherArea > 0.0f) {
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
            if (dv->progress > 0.3f && furrowClosed(body, bodies_, ta, tb))
                splits_.push_back({e, ta, tb});
        }
    }
    for (entt::entity e : abandonedConjugations_)
        registry_.remove<Conjugating>(e);
    abandonedConjugations_.clear();
    for (const auto& [a, b] : conjugationsDone_)
        finishConjugation(a, b);
    for (const PendingSplit& sp : splits_)
        divide(sp.cell, sp.tipA, sp.tipB);
    for (entt::entity e : dying_)
        lyse(e, "inanicion");
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
        if (dv.progress < 0.3f)
            continue;
        // Velocidad para cerrar el surco en el tiempo que queda (con un mínimo).
        const float remaining = std::max(0.5f, (1.0f - dv.progress) * metabolismParams_.divisionHours *
                                                   3600.0f / bioTimeScale_);
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
        const float k = std::sqrt(a / (units::kPi * shape.radiusX * shape.radiusY));
        soft_body::reshapeRest(b, bodies_, shape.radiusX * k, shape.radiusY * k);
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
    Metabolism childMeta = meta;
    childMeta.biomass = meta.biomass * (1.0f - fA);
    childMeta.reserves = meta.reserves * (1.0f - fA);
    meta.biomass *= fA;
    meta.reserves *= fA;
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
    const Locomotion* motherLoco = registry_.try_get<Locomotion>(cell);
    const bool motile = motherLoco != nullptr;
    Locomotion childLoco = motile ? *motherLoco : Locomotion{};
    registry_.emplace<RingBody>(child, std::move(child_body));
    registry_.emplace<CellLook>(child, look);
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
        registry_.emplace<Autopilot>(child); // la hija que no controla el jugador deambula sola
    }
    // Las dos hijas son una generación nueva del linaje.
    if (Lineage* lin = registry_.try_get<Lineage>(cell)) {
        ++lin->generation;
        registry_.emplace<Lineage>(child, *lin);
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

void World::lyse(entt::entity cell, const char* cause) {
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
        const Metabolism* m = registry_.try_get<Metabolism>(e);
        const float mass = m ? m->biomass : cellMassPmol(body.restArea);
        c.totalBiomass += mass;
        const Lineage* lin = registry_.try_get<Lineage>(e);
        if (lin && lin->run == run_) {
            c.lineageBiomass += mass;
            ++c.lineageCells;
            c.maxGeneration = std::max(c.maxGeneration, lin->generation);
        } else {
            ++c.otherCells;
        }
    }
    c.totalBiomass += static_cast<float>(bacteria_.size()) * kBacteriumPmol;
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
