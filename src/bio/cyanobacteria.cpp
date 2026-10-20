#include "bio/cyanobacteria.hpp"

#include "core/profiling.hpp"
#include "core/rng.hpp"
#include "core/units.hpp"
#include "sim/fluid.hpp"
#include "sim/grid2d.hpp"
#include "core/vec2.hpp"

#include <algorithm>
#include <cmath>

void Cyanobacteria::spawn(int count, int cells, float width, float height, std::uint64_t seed, bool oscillatoria) {
    for (int f = 0; f < count; ++f) {
        Rng rng(seed, rng_stream::kCyano, static_cast<std::uint64_t>(f) + (oscillatoria ? 50000u : 0u), 0);
        const float a = rng.uniform(0.0f, 2.0f * units::kPi);
        float x0 = rng.uniform(0.0f, width), y0 = rng.uniform(0.0f, height);
        if (dish.active()) { // el filamento entero dentro del agua
            const Vec2 p = dish.sample(x0 / width, y0 / height, params.spacing * static_cast<float>(cells) + 4.0f);
            x0 = p.x;
            y0 = p.y;
        }
        addFilament({x0, y0}, a, cells, rng, oscillatoria);
    }
}

void Cyanobacteria::addFilament(Vec2 start, float angle, int cells, Rng& rng, bool oscillatoria) {
    CyanoFilament fil;
    fil.oscillatoria = oscillatoria ? 1 : 0;
    for (int k = 0; k < cells; ++k) {
        fil.x.push_back(start.x + std::cos(angle) * params.spacing * static_cast<float>(k));
        fil.y.push_back(start.y + std::sin(angle) * params.spacing * static_cast<float>(k));
        fil.mass.push_back(rng.uniform(1.0f, 2.0f));
        fil.heterocyst.push_back(!oscillatoria && k % params.heterocystSpacing == params.heterocystSpacing / 2 ? 1 : 0);
    }
    fil.direction = rng.uniform() < 0.5f ? -1.0f : 1.0f;
    fil.reverseTimer = rng.uniform(10.0f, 40.0f);
    fil.phase = rng.uniform(0.0f, 6.2831853f);
    filaments.push_back(std::move(fil));
}

std::size_t Cyanobacteria::cellCount() const {
    std::size_t n = 0;
    for (const CyanoFilament& f : filaments)
        n += f.x.size();
    return n;
}

float Cyanobacteria::biomass() const {
    float m = 0.0f;
    for (const CyanoFilament& f : filaments)
        for (float v : f.mass)
            m += v * params.cellPmol;
    return m;
}

float Cyanobacteria::phosphorus() const {
    float p = 0.0f;
    for (const CyanoFilament& f : filaments)
        p += f.phosphorus;
    return p;
}

void Cyanobacteria::move(const Fluid& fluid, float dt, std::uint64_t seed, std::uint64_t step, float width,
                         float height) {
    PROFILE_SCOPE("Cyanobacteria::move");
    const auto nf = static_cast<std::ptrdiff_t>(filaments.size());
#pragma omp parallel for schedule(dynamic, 4)
    for (std::ptrdiff_t fi = 0; fi < nf; ++fi) {
        CyanoFilament& f = filaments[fi];
        const std::size_t n = f.x.size();
        if (n == 0)
            continue;
        Rng rng(seed, rng_stream::kCyano + 100, static_cast<std::uint64_t>(fi), step);
        // Deslizamiento: el filamento avanza a lo largo de su propio eje e invierte cada tanto.
        f.reverseTimer -= dt;
        if (f.reverseTimer <= 0.0f) {
            f.direction = -f.direction;
            f.reverseTimer = rng.uniform(10.0f, 40.0f);
        }
        const float glide = (f.oscillatoria ? params.oscillatoriaGlide : params.glideSpeed) * f.direction;
        // Oscillatoria: el tricoma rota mientras se desliza y la punta que avanza barre de lado a
        // lado; el vaivén se apaga hacia atrás (las células de atrás siguen el camino).
        f.phase += 6.2831853f * dt / params.swayPeriod;
        const float sway = f.oscillatoria ? params.swayAmplitude * std::sin(f.phase) : 0.0f;
        for (std::size_t k = 0; k < n; ++k) {
            const std::size_t a = k == 0 ? 0 : k - 1, b = k + 1 < n ? k + 1 : n - 1;
            Vec2 t = {f.x[b] - f.x[a], f.y[b] - f.y[a]};
            const float len = length(t);
            t = len > 1e-4f ? t * (1.0f / len) : Vec2{1.0f, 0.0f};
            const Vec2 u = fluid.velocityAt({f.x[k], f.y[k]});
            const float fromTip = static_cast<float>(f.direction > 0.0f ? n - 1 - k : k);
            const float lateral = sway * std::exp(-fromTip / 3.0f);
            f.x[k] += (u.x + t.x * glide - t.y * lateral) * dt;
            f.y[k] += (u.y + t.y * glide + t.x * lateral) * dt;
        }
        // Cadena: distancias fijas entre células y algo de rigidez de flexión.
        for (int it = 0; it < 4; ++it) {
            for (std::size_t k = 0; k + 1 < n; ++k) {
                const Vec2 d = {f.x[k + 1] - f.x[k], f.y[k + 1] - f.y[k]};
                const float len = std::max(1e-4f, length(d));
                const Vec2 c = d * (0.5f * (len - params.spacing) / len);
                f.x[k] += c.x;
                f.y[k] += c.y;
                f.x[k + 1] -= c.x;
                f.y[k + 1] -= c.y;
            }
            for (std::size_t k = 1; k + 1 < n; ++k) {
                const Vec2 mid = {0.5f * (f.x[k - 1] + f.x[k + 1]), 0.5f * (f.y[k - 1] + f.y[k + 1])};
                f.x[k] += (mid.x - f.x[k]) * 0.1f;
                f.y[k] += (mid.y - f.y[k]) * 0.1f;
            }
        }
        // Pared de la placa: las células vuelven adentro y, si la punta que avanza chocó, el
        // filamento invierte el deslizamiento (como ante cualquier obstáculo).
        if (dish.active()) {
            bool hit = false;
            for (std::size_t k = 0; k < n; ++k)
                hit |= dish.bounce(f.x[k], f.y[k], nullptr, 2.0f) && (f.direction > 0.0f ? k == n - 1 : k == 0);
            if (hit)
                f.direction = -f.direction;
        }
        // El filamento entero se traslada si su primera célula salió del mundo toroidal.
        const float sx = f.x[0] < 0.0f ? width : (f.x[0] >= width ? -width : 0.0f);
        const float sy = f.y[0] < 0.0f ? height : (f.y[0] >= height ? -height : 0.0f);
        if (sx != 0.0f || sy != 0.0f)
            for (std::size_t k = 0; k < n; ++k) {
                f.x[k] += sx;
                f.y[k] += sy;
            }
    }
}

float Cyanobacteria::nitrogen() const {
    float n = 0.0f;
    for (const CyanoFilament& f : filaments)
        n += f.nitrogen;
    return n;
}

void Cyanobacteria::grow(const Grid2D& light, const Grid2D& phosphate, const Grid2D& nitrogen, const Grid2D& ph,
                         float bioDt, std::vector<float>& demandP, std::vector<float>& demandN) {
    demandP.assign(filaments.size(), 0.0f);
    demandN.assign(filaments.size(), 0.0f);
    potential_.assign(filaments.size(), 0.0f);
    const float hours = bioDt / 3600.0f;
    for (std::size_t fi = 0; fi < filaments.size(); ++fi) {
        const CyanoFilament& f = filaments[fi];
        if (f.x.empty())
            continue;
        float growing = 0.0f, mass = 0.0f;
        for (std::size_t k = 0; k < f.x.size(); ++k) {
            mass += f.mass[k];
            if (f.heterocyst[k] == kVegetativeCell)
                growing += f.mass[k] * std::clamp(light.sample(f.x[k], f.y[k]), 0.0f, 1.0f);
        }
        const std::size_t mid = f.x.size() / 2;
        const float phFactor = std::clamp((ph.sample(f.x[mid], f.y[mid]) - 6.0f) / 1.5f, 0.2f, 1.0f) *
                               (params.temperature < params.tMin || params.temperature > params.tMax ? 0.0f : 1.0f);
        const float biomass = mass * params.cellPmol;
        const float potential = params.growthPerHourMax * hours * growing * params.cellPmol * phFactor;
        potential_[fi] = potential;
        // Lo que falta para la biomasa nueva, descontando lo que ya tiene guardado.
        const float c = std::max(0.0f, phosphate.sample(f.x[mid], f.y[mid]));
        const float needP = potential * params.phosphorusRatio - (f.phosphorus - biomass * params.phosphorusRatio);
        demandP[fi] = std::max(0.0f, needP) * c / (c + params.phosphorusHalfSaturation);
        const float cn = std::max(0.0f, nitrogen.sample(f.x[mid], f.y[mid]));
        const float needN = potential * params.nitrogenRatio - (f.nitrogen - biomass * params.nitrogenRatio);
        demandN[fi] = std::max(0.0f, needN) * cn / (cn + params.nitrogenHalfSaturation);
    }
}

void Cyanobacteria::credit(const std::vector<float>& takenP, const std::vector<float>& takenN, const Grid2D& oxygen,
                           float bioDt, std::uint64_t seed, std::uint64_t step, std::vector<CyanoRelease>& releases) {
    PROFILE_SCOPE("Cyanobacteria::credit");
    const float hours = bioDt / 3600.0f;
    std::vector<CyanoFilament> fragments;
    std::size_t total = cellCount();
    for (std::size_t fi = 0; fi < filaments.size(); ++fi) {
        CyanoFilament& f = filaments[fi];
        if (f.x.empty())
            continue;
        f.phosphorus += takenP[fi];
        f.nitrogen += takenN[fi];
        float mass = 0.0f;
        std::size_t vegetative = 0, heterocysts = 0;
        for (std::size_t k = 0; k < f.x.size(); ++k) {
            mass += f.mass[k];
            heterocysts += f.heterocyst[k] == kHeterocyst ? 1 : 0;
            vegetative += f.heterocyst[k] == kVegetativeCell ? 1 : 0;
        }
        // Fijación de N₂ en los heterocistos; una parte se filtra al agua como amonio.
        if (heterocysts > 0) {
            const std::size_t h = [&] {
                for (std::size_t k = 0; k < f.x.size(); ++k)
                    if (f.heterocyst[k] == kHeterocyst)
                        return k;
                return std::size_t{0};
            }();
            const float o2 = std::max(0.0f, oxygen.sample(f.x[h], f.y[h]));
            const float fixed = params.fixationPerHeterocyst * static_cast<float>(heterocysts) * hours *
                                params.fixationO2Half / (params.fixationO2Half + o2);
            fixedTotal += fixed;
            f.nitrogen += fixed * (1.0f - params.fixedNLeak);
            releases.push_back({f.x[h], f.y[h], 0.0f, 0.0f, fixed * params.fixedNLeak, 0.0f});
        }
        // Acinetos: se forman junto a un heterocisto cuando el fósforo limita, germinan cuando vuelve.
        if (!f.oscillatoria) {
            const float biomassNow = mass * params.cellPmol;
            const bool pLimited = f.phosphorus < biomassNow * params.phosphorusRatio * 1.05f;
            Rng arng(seed, rng_stream::kCyano + 500, fi, step);
            for (std::size_t k = 0; k < f.x.size(); ++k) {
                const bool nextToHeterocyst = (k > 0 && f.heterocyst[k - 1] == kHeterocyst) ||
                                              (k + 1 < f.x.size() && f.heterocyst[k + 1] == kHeterocyst);
                if (f.heterocyst[k] == kVegetativeCell && pLimited && nextToHeterocyst &&
                    arng.uniform() < params.akinetePerHour * hours) {
                    f.heterocyst[k] = kAkinete;
                    --vegetative;
                } else if (f.heterocyst[k] == kAkinete && !pLimited && arng.uniform() < params.germinationPerHour * hours) {
                    f.heterocyst[k] = kVegetativeCell;
                    ++vegetative;
                }
            }
        }
        if (vegetative == 0)
            continue;
        // La biomasa nueva la limita el nutriente más escaso (ley de Liebig).
        const float biomass = mass * params.cellPmol;
        const float byP = std::max(0.0f, f.phosphorus / params.phosphorusRatio - biomass);
        const float byN = std::max(0.0f, f.nitrogen / params.nitrogenRatio - biomass);
        const float newBiomass = std::min({potential_.size() > fi ? potential_[fi] : 0.0f, byP, byN});
        if (newBiomass > 0.0f) {
            // Exudación: parte de lo fijado sale como materia orgánica (alimento de bacterias).
            const std::size_t mid = f.x.size() / 2;
            releases.push_back({f.x[mid], f.y[mid], params.exudation * newBiomass, 0.0f, 0.0f, 0.0f});
        }
        const float each = newBiomass / params.cellPmol / static_cast<float>(vegetative);
        for (std::size_t k = 0; k < f.x.size(); ++k) {
            if (f.heterocyst[k] != kVegetativeCell)
                continue;
            f.mass[k] += each;
            if (f.mass[k] >= 2.0f && total < params.maxCells) { // división intercalar
                f.mass[k] *= 0.5f;
                const std::size_t next = k + 1 < f.x.size() ? k + 1 : k;
                const float nx = 0.5f * (f.x[k] + f.x[next]) + (next == k ? params.spacing : 0.0f);
                const float ny = 0.5f * (f.y[k] + f.y[next]);
                f.x.insert(f.x.begin() + static_cast<std::ptrdiff_t>(k) + 1, nx);
                f.y.insert(f.y.begin() + static_cast<std::ptrdiff_t>(k) + 1, ny);
                f.mass.insert(f.mass.begin() + static_cast<std::ptrdiff_t>(k) + 1, f.mass[k]);
                f.heterocyst.insert(f.heterocyst.begin() + static_cast<std::ptrdiff_t>(k) + 1, kVegetativeCell);
                ++total;
                ++k;
            }
        }
        // Heterocistos: una célula se diferencia si no hay otro a menos de ~10 células
        // (Oscillatoria no los forma).
        int sinceLast = params.heterocystSpacing / 2;
        for (std::size_t k = 0; k < f.x.size() && !f.oscillatoria; ++k) {
            sinceLast = f.heterocyst[k] == kHeterocyst ? 0 : sinceLast + 1;
            if (sinceLast >= params.heterocystSpacing && f.heterocyst[k] == kVegetativeCell) {
                f.heterocyst[k] = kHeterocyst;
                sinceLast = 0;
            }
        }
        // Muerte de células sueltas: devuelven todo al agua y cortan el filamento.
        Rng rng(seed, rng_stream::kCyano + 300, fi, step);
        std::size_t cut = f.x.size();
        const float pDeath = params.mortalityPerHour * hours;
        for (std::size_t k = 0; k < f.x.size(); ++k) {
            if (rng.uniform() >= pDeath * (f.heterocyst[k] == kAkinete ? params.akineteMortality : 1.0f))
                continue;
            float cellMass = 0.0f;
            for (float m : f.mass)
                cellMass += m;
            const float share = f.mass[k] / std::max(1e-6f, cellMass);
            const float pmol = f.mass[k] * params.cellPmol;
            CyanoRelease r{f.x[k], f.y[k], pmol, f.phosphorus * share, f.nitrogen * share, pmol * params.toxinRatio};
            f.phosphorus -= r.phosphorus;
            f.nitrogen -= r.nitrogen;
            releases.push_back(r);
            cut = k;
            break; // una por paso y filamento alcanza (la probabilidad es chica)
        }
        if (cut < f.x.size()) {
            CyanoFilament part;
            const auto c = static_cast<std::ptrdiff_t>(cut);
            part.x.assign(f.x.begin() + c + 1, f.x.end());
            part.y.assign(f.y.begin() + c + 1, f.y.end());
            part.mass.assign(f.mass.begin() + c + 1, f.mass.end());
            part.heterocyst.assign(f.heterocyst.begin() + c + 1, f.heterocyst.end());
            const float before = static_cast<float>(f.x.size() - 1);
            const float share = before > 0.0f ? static_cast<float>(part.x.size()) / before : 0.0f;
            part.phosphorus = f.phosphorus * share;
            part.nitrogen = f.nitrogen * share;
            f.phosphorus -= part.phosphorus;
            f.nitrogen -= part.nitrogen;
            part.direction = -f.direction;
            part.oscillatoria = f.oscillatoria;
            part.phase = f.phase;
            part.reverseTimer = rng.uniform(10.0f, 40.0f);
            f.x.resize(cut);
            f.y.resize(cut);
            f.mass.resize(cut);
            f.heterocyst.resize(cut);
            if (!part.x.empty())
                fragments.push_back(std::move(part));
        }
        // Hormogonios: los filamentos largos se cortan por la mitad.
        if (f.x.size() > params.maxLength) {
            const std::size_t half = f.x.size() / 2;
            CyanoFilament part;
            const auto c = static_cast<std::ptrdiff_t>(half);
            part.x.assign(f.x.begin() + c, f.x.end());
            part.y.assign(f.y.begin() + c, f.y.end());
            part.mass.assign(f.mass.begin() + c, f.mass.end());
            part.heterocyst.assign(f.heterocyst.begin() + c, f.heterocyst.end());
            const float share = static_cast<float>(part.x.size()) / static_cast<float>(f.x.size());
            part.phosphorus = f.phosphorus * share;
            part.nitrogen = f.nitrogen * share;
            f.phosphorus -= part.phosphorus;
            f.nitrogen -= part.nitrogen;
            part.direction = -f.direction;
            part.oscillatoria = f.oscillatoria;
            part.phase = f.phase;
            part.reverseTimer = rng.uniform(10.0f, 40.0f);
            f.x.resize(half);
            f.y.resize(half);
            f.mass.resize(half);
            f.heterocyst.resize(half);
            fragments.push_back(std::move(part));
        }
    }
    // Filamentos vacíos (su última célula murió): lo que quedaba vuelve al agua.
    for (std::size_t fi = 0; fi < filaments.size();) {
        if (filaments[fi].x.empty()) {
            if (filaments[fi].phosphorus > 0.0f || filaments[fi].nitrogen > 0.0f)
                releases.push_back({0.0f, 0.0f, 0.0f, filaments[fi].phosphorus, filaments[fi].nitrogen, 0.0f});
            filaments[fi] = std::move(filaments.back());
            filaments.pop_back();
            continue;
        }
        ++fi;
    }
    for (CyanoFilament& p : fragments)
        filaments.push_back(std::move(p));
}
