#include "game/world.hpp"

#include "core/profiling.hpp"
#include "core/rng.hpp"
#include "core/units.hpp"
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

    oxygen_.advect(fluid_, dt);
    nutrients_.advect(fluid_, dt);
    updateChemistrySources(dt);
    stepAmbient(dt); // deposita nutrientes de los detritos y su sombra
    oxygen_.diffuse(fft_, dt);
    nutrients_.diffuse(fft_, dt);
    computeLight();

    bacteria_.step(fluid_, nutrients_.grid(), dt, config_.seed, step_, width_, height_);
    stepBodies(dt);
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

void World::stepBodies(float dt) {
    PROFILE_SCOPE("World::stepBodies");
    bodyList_.clear();
    for (auto [e, body] : registry_.view<RingBody>().each())
        bodyList_.push_back(&body);
    if (bodyList_.empty())
        return;
    std::sort(bodyList_.begin(), bodyList_.end(),
              [](const RingBody* a, const RingBody* b) { return a->id < b->id; });
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
        auto external = [&](std::uint32_t s) {
            const Vec2 v = fluid_.velocityAt({bodies_.x[s], bodies_.y[s]}) + push;
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
