#include "game/world.hpp"

#include "core/profiling.hpp"
#include "core/rng.hpp"
#include "core/units.hpp"
#include "bio/phagocytosis.hpp"
#include "sim/polygon.hpp"

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
    updatePlayerFeeding(dt); // eventos de topología (fagocitosis): al final del paso
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

    applyPlayerMotion(dt);
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

entt::entity World::spawnPlayer(LocomotionType type, Vec2 center) {
    CellSpec spec;
    spec.center = center;
    spec.angle = 0.0f;
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
    registry_.emplace<Player>(e);
    registry_.emplace<Locomotion>(e, loco);
    registry_.emplace<Digestion>(e);
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

void World::applyPlayerMotion(float dt) {
    if (!registry_.valid(player_))
        return;
    RingBody& body = registry_.get<RingBody>(player_);
    Locomotion& loco = registry_.get<Locomotion>(player_);
    loco.forward = playerInput_.forward;
    loco.turn = playerInput_.turn;
    loco.target = playerInput_.aim;
    // La ameba deja de desplazarse mientras forma una copa fagocítica.
    const bool engulfing = registry_.all_of<PhagocyticCup>(player_);
    loco.active = (loco.type == LocomotionType::Flagellum && playerInput_.propel) ||
                  (loco.type == LocomotionType::Pseudopod && playerInput_.pseudopod && !engulfing);
    applyLocomotion(body, bodies_, loco, locomotionParams_, simTime_, width_, height_);
    if (loco.type != LocomotionType::Pseudopod)
        return;

    // Fagocitosis: el contacto con una presa elegible inicia una copa; la copa la forra.
    if (PhagocyticCup* cup = registry_.try_get<PhagocyticCup>(player_)) {
        const auto preyEntity = static_cast<entt::entity>(cup->preyId);
        if (!registry_.valid(preyEntity) || !registry_.all_of<RingBody>(preyEntity)) {
            registry_.remove<PhagocyticCup>(player_);
            return;
        }
        const RingBody& prey = registry_.get<RingBody>(preyEntity);
        const Vec2 shift = imageShift(prey, body.centroid);
        if (std::find(body.ring.begin(), body.ring.end(), cup->anchorSlot) == body.ring.end()) {
            const std::uint32_t again = findContact(body, bodies_, prey, shift, feedingParams_);
            if (again == ~0u) {
                releasePrey(prey, bodies_, *cup);
                registry_.remove<PhagocyticCup>(player_); // la presa se soltó
                return;
            }
            cup->anchorSlot = again;
        }
        growLips(body, bodies_, prey, shift, *cup, feedingParams_, dt);
        applyCup(body, bodies_, prey, shift, *cup, feedingParams_, dt);
        return;
    }
    for (const RingBody* prey : preyFor(body)) {
        const std::uint32_t contact =
            findContact(body, bodies_, *prey, imageShift(*prey, body.centroid), feedingParams_);
        if (contact != ~0u) {
            PhagocyticCup& cup = registry_.emplace<PhagocyticCup>(player_, PhagocyticCup{prey->id, contact});
            holdPrey(*prey, bodies_, cup);
            return;
        }
    }
}

void World::updatePlayerFeeding(float dt) {
    PROFILE_SCOPE("World::updatePlayerFeeding");
    if (!registry_.valid(player_))
        return;
    RingBody& body = registry_.get<RingBody>(player_);
    const Locomotion& loco = registry_.get<Locomotion>(player_);
    Digestion& dig = registry_.get<Digestion>(player_);

    if (loco.type == LocomotionType::Pseudopod) {
        if (PhagocyticCup* cup = registry_.try_get<PhagocyticCup>(player_)) {
            const auto preyEntity = static_cast<entt::entity>(cup->preyId);
            const RingBody& prey = registry_.get<RingBody>(preyEntity);
            const Vec2 preyCenter = prey.centroid + imageShift(prey, body.centroid);
            Closure closure;
            if (findClosure(body, bodies_, preyCenter, feedingParams_, closure)) {
                registry_.remove<PhagocyticCup>(player_);
                engulf(closure, preyEntity);
            } else if (cup->coverage > cup->bestCoverage + 0.02f) {
                cup->bestCoverage = cup->coverage;
                cup->stalled = 0.0f;
            } else {
                cup->stalled += dt;
                if (cup->stalled > 10.0f) { // la copa dejó de avanzar: se abandona
                    releasePrey(prey, bodies_, *cup);
                    registry_.remove<PhagocyticCup>(player_);
                }
            }
        }
        feedByContact(body, dig, dt);
    } else {
        feedAtMouth(body, loco, dig, dt);
    }
    digest(dig, body, bodies_, feedingParams_, dt);
    soft_body::refresh(body, bodies_);
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

void World::engulf(const Closure& closure, entt::entity preyEntity) {
    RingBody& body = registry_.get<RingBody>(player_);
    Digestion& dig = registry_.get<Digestion>(player_);
    const CellLook look = registry_.get<CellLook>(preyEntity);
    const float preyArea = std::abs(registry_.get<RingBody>(preyEntity).restArea);
    const int pn = static_cast<int>(closure.pocket.size());

    // Todo lo que quedó dentro de la copa entra al fagosoma junto con la presa.
    float extraArea = 0.0f, extraPmol = 0.0f;
    for (std::size_t i = 0; i < bacteria_.size(); ++i) {
        const Vec2 q = body.centroid + Vec2{minImage(bacteria_.x[i] - body.centroid.x, width_),
                                            minImage(bacteria_.y[i] - body.centroid.y, height_)};
        if (polygon::contains(closure.pocket.data(), pn, q)) {
            eatenBacteria_.push_back(i);
            extraArea += kBacteriumArea;
            extraPmol += kBacteriumPmol;
        }
    }
    dig.bacteriaEaten += static_cast<int>(eatenBacteria_.size());
    removeBacteria(eatenBacteria_);

    destroyCell(registry_, bodies_, preyEntity);

    // Fusión de membranas: el tramo de la copa pasa a ser la membrana del fagosoma.
    for (std::uint32_t s : body.ring)
        if (std::find(closure.outerRing.begin(), closure.outerRing.end(), s) == closure.outerRing.end())
            bodies_.release(s);
    if (grabSlot_ != kNoGrab && !bodies_.alive[grabSlot_])
        endGrab();
    soft_body::replaceRing(body, bodies_, closure.outerRing);

    ingest(dig, body, bodies_, closure.preyCenter, preyArea + extraArea,
           cellMassPmol(preyArea) + extraPmol, look.r, look.g, look.b, true, false);
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
    for (std::size_t k = 0; k < bacteria_.size() && dig.mouthBudget >= 1.0f; ++k) {
        const float dx = minImage(bacteria_.x[k] - m.x, width_);
        const float dy = minImage(bacteria_.y[k] - m.y, height_);
        if (dx * dx + dy * dy < reach * reach) {
            dig.mouthBudget -= 1.0f;
            eatenBacteria_.push_back(k);
            ingest(dig, body, bodies_, site, kBacteriumArea, kBacteriumPmol, 170, 150, 140, false, true);
        }
    }
    dig.bacteriaEaten += static_cast<int>(eatenBacteria_.size());
    removeBacteria(eatenBacteria_);

    for (std::size_t k = 0; k < ambient_.size(); ++k) {
        if (ambient_.kind[k] != ParticleKind::Detritus || ambient_.radius[k] > maxParticle)
            continue;
        const float dx = minImage(ambient_.x[k] - m.x, width_);
        const float dy = minImage(ambient_.y[k] - m.y, height_);
        const float r = reach + ambient_.radius[k];
        if (dx * dx + dy * dy < r * r) {
            eatenAmbient_.push_back(k);
            const float a = units::kPi * ambient_.radius[k] * ambient_.radius[k];
            ingest(dig, body, bodies_, site, a, ambient_.mass[k], 140, 112, 72, false, true);
        }
    }
    dig.detritusEaten += static_cast<int>(eatenAmbient_.size());
    removeAmbient(eatenAmbient_);
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
    for (std::size_t k = 0; k < bacteria_.size() && dig.mouthBudget >= 1.0f; ++k) {
        if (tryEat(bacteria_.x[k], bacteria_.y[k], k, site)) {
            dig.mouthBudget -= 1.0f;
            eatenBacteria_.push_back(k);
            ingest(dig, body, bodies_, site, kBacteriumArea, kBacteriumPmol, 170, 150, 140, false, true);
        }
    }
    dig.bacteriaEaten += static_cast<int>(eatenBacteria_.size());
    removeBacteria(eatenBacteria_);

    for (std::size_t k = 0; k < ambient_.size() && dig.mouthBudget >= 1.0f; ++k) {
        if (ambient_.kind[k] != ParticleKind::Detritus)
            continue;
        if (tryEat(ambient_.x[k], ambient_.y[k], bacteria_.size() + k, site)) {
            dig.mouthBudget -= 1.0f;
            eatenAmbient_.push_back(k);
            const float a = units::kPi * ambient_.radius[k] * ambient_.radius[k];
            ingest(dig, body, bodies_, site, a, ambient_.mass[k], 140, 112, 72, false, true);
        }
    }
    dig.detritusEaten += static_cast<int>(eatenAmbient_.size());
    removeAmbient(eatenAmbient_);
}
