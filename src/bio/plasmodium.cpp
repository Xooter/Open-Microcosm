#include "bio/plasmodium.hpp"

#include "core/profiling.hpp"
#include "core/rng.hpp"
#include "core/units.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr std::size_t kHashSize = 8192; // potencia de 2

std::size_t hashCell(int cx, int cy) {
    return (static_cast<std::size_t>(cx) * 73856093u ^ static_cast<std::size_t>(cy) * 19349663u) & (kHashSize - 1);
}

} // namespace

void OatFlakes::swapRemove(std::size_t i) {
    const std::size_t last = x.size() - 1;
    x[i] = x[last];
    y[i] = y[last];
    pmol[i] = pmol[last];
    phosphorus[i] = phosphorus[last];
    x.pop_back();
    y.pop_back();
    pmol.pop_back();
    phosphorus.pop_back();
}

void Sclerotia::swapRemove(std::size_t i) {
    const std::size_t last = x.size() - 1;
    x[i] = x[last];
    y[i] = y[last];
    ageHours[i] = ageHours[last];
    pmol[i] = pmol[last];
    phosphorus[i] = phosphorus[last];
    x.pop_back();
    y.pop_back();
    ageHours.pop_back();
    pmol.pop_back();
    phosphorus.pop_back();
}

// ---------------------------------------------------------------------------
// Consultas
// ---------------------------------------------------------------------------

std::uint32_t PlasmodiumSystem::spawn(Vec2 at, float heading, float reserves, float phosphorus) {
    if (plasmodia.size() >= params.maxPlasmodia)
        return 0;
    Plasmodium pl;
    pl.id = nextId_++;
    pl.x.push_back(at.x);
    pl.y.push_back(at.y);
    pl.heading.push_back(heading);
    pl.tip.push_back(1);
    pl.food.push_back(-1);
    pl.fed.push_back(1);
    pl.reserves = reserves;
    pl.phosphorus = phosphorus;
    plasmodia.push_back(std::move(pl));
    return plasmodia.back().id;
}

float PlasmodiumSystem::biomass() const {
    float b = 0.0f;
    for (const Plasmodium& pl : plasmodia)
        b += pl.biomass();
    for (float v : sclerotia.pmol)
        b += v;
    for (float v : flakes.pmol)
        b += v;
    return b;
}

float PlasmodiumSystem::phosphorus() const {
    float p = 0.0f;
    for (const Plasmodium& pl : plasmodia)
        p += pl.phosphorus;
    for (float v : sclerotia.phosphorus)
        p += v;
    for (float v : flakes.phosphorus)
        p += v;
    return p;
}

std::size_t PlasmodiumSystem::nodeCount() const {
    std::size_t n = 0;
    for (const Plasmodium& pl : plasmodia)
        n += pl.x.size();
    return n;
}

std::size_t PlasmodiumSystem::edgeCount() const {
    std::size_t n = 0;
    for (const Plasmodium& pl : plasmodia)
        n += pl.edges.size();
    return n;
}

const Plasmodium* PlasmodiumSystem::find(std::uint32_t id) const {
    for (const Plasmodium& pl : plasmodia)
        if (pl.id == id)
            return &pl;
    return nullptr;
}

Plasmodium* PlasmodiumSystem::find(std::uint32_t id) {
    for (Plasmodium& pl : plasmodia)
        if (pl.id == id)
            return &pl;
    return nullptr;
}

float PlasmodiumSystem::totalLength(const Plasmodium& pl) {
    float total = 0.0f;
    for (const Plasmodium::Edge& e : pl.edges)
        total += e.length;
    return total;
}

std::size_t PlasmodiumSystem::cycles(const Plasmodium& pl) {
    // Número ciclomático: aristas − nodos + componentes conexas.
    std::vector<std::int32_t> root(pl.x.size());
    for (std::size_t i = 0; i < root.size(); ++i)
        root[i] = static_cast<std::int32_t>(i);
    auto find = [&](std::int32_t i) {
        while (root[static_cast<std::size_t>(i)] != i) {
            root[static_cast<std::size_t>(i)] = root[static_cast<std::size_t>(root[static_cast<std::size_t>(i)])];
            i = root[static_cast<std::size_t>(i)];
        }
        return i;
    };
    std::size_t components = pl.x.size();
    for (const Plasmodium::Edge& e : pl.edges) {
        const std::int32_t a = find(e.a), b = find(e.b);
        if (a != b) {
            root[static_cast<std::size_t>(a)] = b;
            --components;
        }
    }
    const std::size_t e = pl.edges.size();
    const std::size_t v = pl.x.size();
    return e + components >= v ? e + components - v : 0;
}

float PlasmodiumSystem::tubeMassOf(const Plasmodium& pl) const {
    float m = 0.0f;
    for (const Plasmodium::Edge& e : pl.edges)
        m += params.nodePmol * (e.length / params.spacing) * std::sqrt(std::max(e.D, 0.0f));
    return m;
}

// ---------------------------------------------------------------------------
// Hash espacial de nodos (para fundir tubos y tocar copos)
// ---------------------------------------------------------------------------

void PlasmodiumSystem::hashBuild(const Plasmodium& pl) {
    hashHead_.assign(kHashSize, -1);
    hashNext_.assign(pl.x.size(), -1);
    for (std::size_t i = 0; i < pl.x.size(); ++i)
        hashInsert(pl, static_cast<std::int32_t>(i));
}

void PlasmodiumSystem::hashInsert(const Plasmodium& pl, std::int32_t node) {
    const auto i = static_cast<std::size_t>(node);
    if (hashNext_.size() <= i)
        hashNext_.resize(i + 1, -1);
    const float cell = std::max(params.spacing, params.flakeReach);
    const std::size_t h = hashCell(static_cast<int>(std::floor(pl.x[i] / cell)), static_cast<int>(std::floor(pl.y[i] / cell)));
    hashNext_[i] = hashHead_[h];
    hashHead_[h] = node;
}

template <typename F> void PlasmodiumSystem::hashNear(const Plasmodium& pl, Vec2 at, float radius, F&& fn) const {
    const float cell = std::max(params.spacing, params.flakeReach);
    const int x0 = static_cast<int>(std::floor((at.x - radius) / cell)), x1 = static_cast<int>(std::floor((at.x + radius) / cell));
    const int y0 = static_cast<int>(std::floor((at.y - radius) / cell)), y1 = static_cast<int>(std::floor((at.y + radius) / cell));
    for (int cy = y0; cy <= y1; ++cy)
        for (int cx = x0; cx <= x1; ++cx)
            for (std::int32_t n = hashHead_[hashCell(cx, cy)]; n >= 0; n = hashNext_[static_cast<std::size_t>(n)]) {
                const auto i = static_cast<std::size_t>(n);
                const float dx = pl.x[i] - at.x, dy = pl.y[i] - at.y;
                if (dx * dx + dy * dy <= radius * radius)
                    fn(n);
            }
}

// ---------------------------------------------------------------------------
// Flujo de citoplasma (Kirchhoff) y adaptación de tubos (Tero)
// ---------------------------------------------------------------------------

bool PlasmodiumSystem::solveFlow(Plasmodium& pl, std::int32_t source, std::int32_t sink, float inflow) {
    PROFILE_SCOPE("Plasmodium::solveFlow");
    const std::size_t n = pl.x.size();
    for (Plasmodium::Edge& e : pl.edges)
        e.flux = 0.0f;
    // sink = kAllTips: el frente de exploración consume (todas las puntas de la componente).
    if (source < 0 || (sink < 0 && sink != kAllTips) || source == sink || static_cast<std::size_t>(std::max(source, sink)) >= n)
        return false;

    // Adyacencia (CSR) y conductancias c = D/L.
    adjStart_.assign(n + 1, 0);
    for (const Plasmodium::Edge& e : pl.edges) {
        ++adjStart_[static_cast<std::size_t>(e.a) + 1];
        ++adjStart_[static_cast<std::size_t>(e.b) + 1];
    }
    for (std::size_t i = 0; i < n; ++i)
        adjStart_[i + 1] += adjStart_[i];
    adjList_.assign(static_cast<std::size_t>(adjStart_[n]), 0);
    queue_.assign(n, 0); // se usa de cursor de relleno y después de cola
    for (std::size_t k = 0; k < pl.edges.size(); ++k) {
        const Plasmodium::Edge& e = pl.edges[k];
        adjList_[static_cast<std::size_t>(adjStart_[static_cast<std::size_t>(e.a)] + queue_[static_cast<std::size_t>(e.a)]++)] =
            static_cast<std::int32_t>(k);
        adjList_[static_cast<std::size_t>(adjStart_[static_cast<std::size_t>(e.b)] + queue_[static_cast<std::size_t>(e.b)]++)] =
            static_cast<std::int32_t>(k);
    }
    conduct_.resize(pl.edges.size());
    for (std::size_t k = 0; k < pl.edges.size(); ++k)
        conduct_[k] = (pl.edges[k].D + 1e-4f) / std::max(pl.edges[k].length, 1e-3f);

    // Componente de la fuente (BFS). El sumidero tiene que estar en ella.
    inComp_.assign(n, 0);
    std::size_t head = 0, tail = 0;
    queue_[tail++] = source;
    inComp_[static_cast<std::size_t>(source)] = 1;
    while (head < tail) {
        const auto u = static_cast<std::size_t>(queue_[head++]);
        for (std::int32_t it = adjStart_[u]; it < adjStart_[u + 1]; ++it) {
            const Plasmodium::Edge& e = pl.edges[static_cast<std::size_t>(adjList_[static_cast<std::size_t>(it)])];
            const auto v = static_cast<std::size_t>(e.a == static_cast<std::int32_t>(u) ? e.b : e.a);
            if (!inComp_[v]) {
                inComp_[v] = 1;
                queue_[tail++] = static_cast<std::int32_t>(v);
            }
        }
    }
    // Sumideros: el nodo pedido, o todas las puntas alcanzables (el primero es la tierra).
    sinks_.clear();
    if (sink == kAllTips) {
        for (std::size_t i = 0; i < n; ++i)
            if (inComp_[i] && pl.tip[i] && static_cast<std::int32_t>(i) != source)
                sinks_.push_back(static_cast<std::int32_t>(i));
    } else if (inComp_[static_cast<std::size_t>(sink)]) {
        sinks_.push_back(sink);
    }
    if (sinks_.empty())
        return false;
    sink = sinks_[0];

    // Gradiente conjugado con precondicionador de Jacobi sobre los nodos de la componente menos
    // el sumidero (presión 0). Serial y en orden fijo: determinista.
    cg_p_.assign(n, 0.0f);
    cg_r_.assign(n, 0.0f);
    cg_z_.assign(n, 0.0f);
    cg_q_.assign(n, 0.0f);
    cg_diag_.assign(n, 0.0f);
    cg_b_.assign(n, 0.0f);
    std::vector<float>& pres = cg_p_;
    for (std::size_t k = 0; k < pl.edges.size(); ++k) {
        const Plasmodium::Edge& e = pl.edges[k];
        cg_diag_[static_cast<std::size_t>(e.a)] += conduct_[k];
        cg_diag_[static_cast<std::size_t>(e.b)] += conduct_[k];
    }
    cg_b_[static_cast<std::size_t>(source)] = inflow;
    for (std::size_t s = 1; s < sinks_.size(); ++s) // la tierra absorbe el resto
        cg_b_[static_cast<std::size_t>(sinks_[s])] = -inflow / static_cast<float>(sinks_.size());
    const auto sinkI = static_cast<std::size_t>(sink);
    auto active = [&](std::size_t i) { return inComp_[i] && i != sinkI; };
    // y = A·v restringido a los nodos activos.
    auto apply = [&](const std::vector<float>& v, std::vector<float>& y) {
        for (std::size_t i = 0; i < n; ++i)
            y[i] = active(i) ? cg_diag_[i] * v[i] : 0.0f;
        for (std::size_t k = 0; k < pl.edges.size(); ++k) {
            const Plasmodium::Edge& e = pl.edges[k];
            const auto a = static_cast<std::size_t>(e.a), b = static_cast<std::size_t>(e.b);
            if (active(a) && active(b)) {
                y[a] -= conduct_[k] * v[b];
                y[b] -= conduct_[k] * v[a];
            }
        }
    };
    // Precondicionador: r = b (p = 0 al principio).
    std::vector<float>& r = cg_r_;
    std::vector<float>& z = cg_z_;
    std::vector<float>& q = cg_q_;
    std::vector<float>& dir = cg_d_;
    dir.assign(n, 0.0f);
    double rz = 0.0, bnorm = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        if (!active(i))
            continue;
        r[i] = cg_b_[i];
        z[i] = r[i] / std::max(cg_diag_[i], 1e-9f);
        dir[i] = z[i];
        rz += static_cast<double>(r[i]) * static_cast<double>(z[i]);
        bnorm += static_cast<double>(r[i]) * static_cast<double>(r[i]);
    }
    const std::size_t maxIter = std::min<std::size_t>(400, 2 * n + 10);
    for (std::size_t it = 0; it < maxIter && rz > 1e-14 * std::max(bnorm, 1e-30); ++it) {
        apply(dir, q);
        double dq = 0.0;
        for (std::size_t i = 0; i < n; ++i)
            dq += static_cast<double>(dir[i]) * static_cast<double>(q[i]);
        if (dq <= 1e-30)
            break;
        const auto alpha = static_cast<float>(rz / dq);
        double rzNew = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            if (!active(i))
                continue;
            pres[i] += alpha * dir[i];
            r[i] -= alpha * q[i];
            z[i] = r[i] / std::max(cg_diag_[i], 1e-9f);
            rzNew += static_cast<double>(r[i]) * static_cast<double>(z[i]);
        }
        const auto beta = static_cast<float>(rzNew / std::max(rz, 1e-300));
        for (std::size_t i = 0; i < n; ++i)
            dir[i] = active(i) ? z[i] + beta * dir[i] : 0.0f;
        rz = rzNew;
    }
    // Flujos por tubo, normalizados por el caudal de entrada.
    for (std::size_t k = 0; k < pl.edges.size(); ++k) {
        Plasmodium::Edge& e = pl.edges[k];
        const auto a = static_cast<std::size_t>(e.a), b = static_cast<std::size_t>(e.b);
        if (inComp_[a] && inComp_[b])
            e.flux = std::abs(conduct_[k] * (pres[a] - pres[b])) / std::max(inflow, 1e-9f);
    }
    return true;
}

void PlasmodiumSystem::adapt(Plasmodium& pl, float hours) const {
    // dD/dt = f(|Q|) − D (μ = 1), con f(Q) = Qᵞ/(1+Qᵞ): el tubo que lleva flujo se ensancha;
    // el que no, se angosta (Tero et al. 2010).
    const float alpha = 1.0f - std::exp(-params.adaptRate * hours);
    for (Plasmodium::Edge& e : pl.edges) {
        const float g = std::pow(e.flux, params.gamma);
        const float f = g / (1.0f + g);
        e.D = std::clamp(e.D + (f - e.D) * alpha, 0.0f, 1.0f);
        // El frente de exploración (ectoplasma) no se retrae mientras la punta avanza.
        if ((pl.tip[static_cast<std::size_t>(e.a)] && pl.fed[static_cast<std::size_t>(e.a)]) ||
            (pl.tip[static_cast<std::size_t>(e.b)] && pl.fed[static_cast<std::size_t>(e.b)]))
            e.D = std::max(e.D, params.tipFloorD);
    }
}

// ---------------------------------------------------------------------------
// Crecimiento
// ---------------------------------------------------------------------------

void PlasmodiumSystem::compact(Plasmodium& pl) {
    // Quita los tubos retraídos y los nodos que quedaron sueltos (salvo puntas y nodos con comida).
    const std::size_t n = pl.x.size();
    pl.edges.erase(std::remove_if(pl.edges.begin(), pl.edges.end(), [&](const Plasmodium::Edge& e) { return e.D < params.pruneD; }),
                   pl.edges.end());
    remap_.assign(n, 0);
    for (const Plasmodium::Edge& e : pl.edges) {
        remap_[static_cast<std::size_t>(e.a)] = 1;
        remap_[static_cast<std::size_t>(e.b)] = 1;
    }
    std::int32_t next = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const bool keep = remap_[i] || (pl.tip[i] && pl.fed[i]) || pl.food[i] >= 0;
        if (!keep) {
            remap_[i] = -1;
            continue;
        }
        remap_[i] = next;
        pl.x[static_cast<std::size_t>(next)] = pl.x[i];
        pl.y[static_cast<std::size_t>(next)] = pl.y[i];
        pl.heading[static_cast<std::size_t>(next)] = pl.heading[i];
        pl.tip[static_cast<std::size_t>(next)] = pl.tip[i];
        pl.food[static_cast<std::size_t>(next)] = pl.food[i];
        pl.fed[static_cast<std::size_t>(next)] = pl.fed[i];
        ++next;
    }
    const auto keptNodes = static_cast<std::size_t>(next);
    for (Plasmodium::Edge& e : pl.edges) {
        e.a = remap_[static_cast<std::size_t>(e.a)];
        e.b = remap_[static_cast<std::size_t>(e.b)];
    }
    pl.x.resize(keptNodes);
    pl.y.resize(keptNodes);
    pl.heading.resize(keptNodes);
    pl.tip.resize(keptNodes);
    pl.food.resize(keptNodes);
    pl.fed.resize(keptNodes);
    // El par fuente/sumidero sigue siendo el mismo si sus nodos sobrevivieron.
    auto remapped = [&](std::int32_t i) { return i >= 0 && static_cast<std::size_t>(i) < n ? remap_[static_cast<std::size_t>(i)] : -1; };
    pl.source = remapped(pl.source);
    pl.sink = pl.sink == kAllTips ? kAllTips : remapped(pl.sink);
}

void PlasmodiumSystem::grow(Plasmodium& pl, const PlasmodiumEnv& env, float hours, std::uint64_t seed, std::uint64_t step) {
    const PlasmodiumParams& p = params;
    Rng rng(seed, rng_stream::kFungi + 600, pl.id, step);
    const std::size_t n = pl.x.size();
    hashBuild(pl);

    // Copos: el nodo que toca uno se alimenta de él (digiere el almidón).
    std::fill(pl.food.begin(), pl.food.end(), -1);
    float absorbed = 0.0f;
    for (std::size_t f = flakes.size(); f-- > 0;) {
        std::int32_t touching = -1;
        hashNear(pl, {flakes.x[f], flakes.y[f]}, p.flakeReach, [&](std::int32_t node) {
            if (touching < 0)
                touching = node;
        });
        if (touching < 0)
            continue;
        pl.food[static_cast<std::size_t>(touching)] = static_cast<std::int32_t>(f);
        const float take = std::min(flakes.pmol[f], p.flakeUptakePerHour * hours);
        const float pShare = flakes.pmol[f] > 0.0f ? flakes.phosphorus[f] * take / flakes.pmol[f] : 0.0f;
        flakes.pmol[f] -= take;
        flakes.phosphorus[f] -= pShare;
        absorbed += take;
        pl.reserves += take;
        pl.phosphorus += pShare;
    }

    // Absorción de materia orgánica disuelta por todos los tubos (osmotrofia; lo real también
    // fagocita bacterias).
    const float perNode = p.uptakePerHour * p.nodePmol * 0.5f * hours;
    for (std::size_t k = 0; k < n; ++k) {
        const Vec2 at = {pl.x[k], pl.y[k]};
        const float c = std::max(0.0f, env.food(at));
        const float want = perNode * c / (c + p.halfSaturation);
        if (want > 1e-7f) {
            const float got = env.takeFood(at, want);
            absorbed += got;
            pl.reserves += got;
        }
    }
    const float maintenance = pl.tubeMass * p.maintenancePerHour * hours;
    pl.reserves -= maintenance;
    pl.starvingHours = absorbed < maintenance ? pl.starvingHours + hours : std::max(0.0f, pl.starvingHours - hours);

    // Slime: el plasmodio deja su rastro por donde pasa.
    if (env.markSlime)
        for (std::size_t k = 0; k < n; ++k)
            env.markSlime({pl.x[k], pl.y[k]});

    // Las puntas siguen explorando aunque toquen un copo: el plasmodio lo cubre y el frente
    // sigue buscando más comida (así une varios copos en una red).

    // Exploración: las puntas avanzan si hay reservas.
    const float tubeCost = p.nodePmol * std::sqrt(p.youngD) / p.growthYield;
    if (pl.reserves > 3.0f * tubeCost && pl.starvingHours < p.starveHoursToSclerotium * 0.5f)
        pl.tipCredit += p.tipSpeedPerHour * hours;
    while (pl.tipCredit >= p.spacing) {
        pl.tipCredit -= p.spacing;
        const std::size_t count = pl.x.size();
        for (std::size_t k = 0; k < count; ++k) {
            if (!pl.tip[k] || !pl.fed[k])
                continue;
            if (pl.reserves < tubeCost || pl.x.size() >= p.maxNodes || pl.edges.size() + 2 >= p.maxEdges)
                break;
            const Vec2 at = {pl.x[k], pl.y[k]};
            // Rumbo: sigue la pista (comida, rastro, luz) con inercia y algo de azar.
            float h = pl.heading[k] + rng.normal() * 0.18f;
            const Vec2 cue = env.cue ? env.cue(at) : Vec2{};
            if (cue.x != 0.0f || cue.y != 0.0f)
                h += std::clamp(std::remainder(std::atan2(cue.y, cue.x) - h, 2.0f * units::kPi), -0.6f, 0.6f);
            // Fósforo para la hifa nueva.
            const float needP = p.phosphorusRatio * (pl.tubeMass + p.nodePmol * std::sqrt(p.youngD));
            if (pl.phosphorus < needP)
                pl.phosphorus += env.takePhosphate(at, needP - pl.phosphorus);
            if (pl.phosphorus < needP)
                break;
            // El vidrio no se atraviesa: se bordea.
            Vec2 to = at + Vec2{std::cos(h), std::sin(h)} * p.spacing;
            if (!dish.contains(to, 8.0f)) {
                h += rng.uniform() < 0.5f ? -1.0f : 1.0f;
                to = at + Vec2{std::cos(h), std::sin(h)} * p.spacing;
                if (!dish.contains(to, 8.0f)) {
                    pl.heading[k] = h;
                    continue;
                }
            }
            pl.tip[k] = 0;
            // Anastomosis: si cae sobre otro tubo (que no sea el suyo), se funden.
            std::int32_t fuse = -1;
            hashNear(pl, to, p.fuseDistance, [&](std::int32_t node) {
                if (fuse >= 0 || node == static_cast<std::int32_t>(k))
                    return;
                for (const Plasmodium::Edge& e : pl.edges) // ya unidos: no
                    if ((e.a == static_cast<std::int32_t>(k) && e.b == node) || (e.b == static_cast<std::int32_t>(k) && e.a == node))
                        return;
                const float dx = pl.x[static_cast<std::size_t>(node)] - at.x, dy = pl.y[static_cast<std::size_t>(node)] - at.y;
                if (dx * dx + dy * dy < 0.25f * p.spacing * p.spacing)
                    return; // vecino inmediato (el mismo tubo)
                fuse = node;
            });
            if (fuse >= 0) {
                const auto fi = static_cast<std::size_t>(fuse);
                const float len = std::hypot(pl.x[fi] - at.x, pl.y[fi] - at.y);
                pl.edges.push_back({static_cast<std::int32_t>(k), fuse, std::max(len, 1.0f), p.youngD, 0.0f});
                ++stats.fusions;
                pl.reserves -= tubeCost;
                continue;
            }
            const auto node = static_cast<std::int32_t>(pl.x.size());
            pl.x.push_back(to.x);
            pl.y.push_back(to.y);
            pl.heading.push_back(h);
            pl.tip.push_back(1);
            pl.food.push_back(-1);
            pl.fed.push_back(1);
            pl.edges.push_back({static_cast<std::int32_t>(k), node, p.spacing, p.youngD, 0.0f});
            pl.reserves -= tubeCost;
            hashInsert(pl, node);
            // Ramificación del frente (abanico de exploración).
            if (rng.uniform() < p.branchProbability && pl.reserves >= tubeCost) {
                const float bh = h + (rng.uniform() < 0.5f ? -1.0f : 1.0f) * rng.uniform(0.5f, 1.0f);
                const Vec2 bto = at + Vec2{std::cos(bh), std::sin(bh)} * p.spacing;
                if (dish.contains(bto, 8.0f)) {
                    const auto bn = static_cast<std::int32_t>(pl.x.size());
                    pl.x.push_back(bto.x);
                    pl.y.push_back(bto.y);
                    pl.heading.push_back(bh);
                    pl.tip.push_back(1);
                    pl.food.push_back(-1);
                    pl.fed.push_back(1);
                    pl.edges.push_back({static_cast<std::int32_t>(k), bn, p.spacing, p.youngD, 0.0f});
                    pl.reserves -= tubeCost;
                    hashInsert(pl, bn);
                }
            }
        }
    }
}

void PlasmodiumSystem::labelFed(Plasmodium& pl) {
    // Un pedazo de red separado de toda comida no recibe flujo: deja de explorar y se retrae.
    // Mientras ningún nodo toque comida (recién inoculado), toda la red explora.
    const std::size_t n = pl.x.size();
    bool anyFood = false;
    for (std::size_t i = 0; i < n; ++i)
        anyFood = anyFood || pl.food[i] >= 0;
    if (!anyFood) {
        std::fill(pl.fed.begin(), pl.fed.end(), 1);
        return;
    }
    adjStart_.assign(n + 1, 0);
    for (const Plasmodium::Edge& e : pl.edges) {
        ++adjStart_[static_cast<std::size_t>(e.a) + 1];
        ++adjStart_[static_cast<std::size_t>(e.b) + 1];
    }
    for (std::size_t i = 0; i < n; ++i)
        adjStart_[i + 1] += adjStart_[i];
    adjList_.assign(static_cast<std::size_t>(adjStart_[n]), 0);
    queue_.assign(n, 0);
    for (const Plasmodium::Edge& e : pl.edges) {
        adjList_[static_cast<std::size_t>(adjStart_[static_cast<std::size_t>(e.a)] + queue_[static_cast<std::size_t>(e.a)]++)] = e.b;
        adjList_[static_cast<std::size_t>(adjStart_[static_cast<std::size_t>(e.b)] + queue_[static_cast<std::size_t>(e.b)]++)] = e.a;
    }
    std::fill(pl.fed.begin(), pl.fed.end(), 0);
    std::size_t head = 0, tail = 0;
    queue_.assign(n, 0);
    for (std::size_t i = 0; i < n; ++i)
        if (pl.food[i] >= 0 && !pl.fed[i]) {
            pl.fed[i] = 1;
            queue_[tail++] = static_cast<std::int32_t>(i);
        }
    while (head < tail) {
        const auto u = static_cast<std::size_t>(queue_[head++]);
        for (std::int32_t it = adjStart_[u]; it < adjStart_[u + 1]; ++it) {
            const auto v = static_cast<std::size_t>(adjList_[static_cast<std::size_t>(it)]);
            if (!pl.fed[v]) {
                pl.fed[v] = 1;
                queue_[tail++] = static_cast<std::int32_t>(v);
            }
        }
    }
}

void PlasmodiumSystem::prune(Plasmodium& pl, const PlasmodiumEnv& env) {
    // Autólisis: sin reservas, el tubo más fino de las puntas se retrae y devuelve parte de su masa.
    while (pl.reserves < 0.0f && !pl.edges.empty()) {
        std::size_t weakest = 0;
        for (std::size_t k = 1; k < pl.edges.size(); ++k)
            if (pl.edges[k].D < pl.edges[weakest].D)
                weakest = k;
        const Plasmodium::Edge e = pl.edges[weakest];
        pl.reserves += 0.8f * params.nodePmol * (e.length / params.spacing) * std::sqrt(std::max(e.D, 0.01f));
        pl.edges.erase(pl.edges.begin() + static_cast<std::ptrdiff_t>(weakest));
    }
    compact(pl);
    labelFed(pl);
    pl.tubeMass = tubeMassOf(pl);
    // El fósforo que sobra de los tubos retraídos vuelve al agua.
    const float need = params.phosphorusRatio * pl.tubeMass;
    if (pl.phosphorus > 1.5f * need + 1e-5f && !pl.x.empty()) {
        const float excess = pl.phosphorus - 1.2f * need;
        pl.phosphorus -= excess;
        env.release({pl.x[0], pl.y[0]}, 0.0f, excess);
    }
}

// ---------------------------------------------------------------------------
// Paso
// ---------------------------------------------------------------------------

void PlasmodiumSystem::step(const PlasmodiumEnv& env, float bioDt, std::uint64_t seed, std::uint64_t stepIndex) {
    PROFILE_SCOPE("Plasmodium::step");
    const float hours = bioDt / 3600.0f;
    const PlasmodiumParams& p = params;

    // Los copos se disuelven despacio en el agua (el gradiente que atrae al plasmodio).
    for (std::size_t f = flakes.size(); f-- > 0;) {
        const float leach = flakes.pmol[f] * (1.0f - std::exp(-p.flakeLeachPerHour * hours));
        flakes.pmol[f] -= leach;
        env.release({flakes.x[f], flakes.y[f]}, leach, 0.0f);
        if (flakes.pmol[f] < 0.02f) { // se acabó: queda el fósforo
            env.release({flakes.x[f], flakes.y[f]}, flakes.pmol[f], flakes.phosphorus[f]);
            flakes.swapRemove(f);
        }
    }

    for (std::size_t pi = 0; pi < plasmodia.size();) {
        Plasmodium& pl = plasmodia[pi];
        pl.ageHours += hours;
        grow(pl, env, hours, seed, stepIndex);
        pl.tubeMass = tubeMassOf(pl); // lo que costó la exploración ya se pagó en grow()

        // Fuente y sumidero (Tero): par de comidas, o la comida y una punta.
        pl.switchAccum += hours;
        if (pl.switchAccum >= p.switchHours || pl.source < 0 || pl.sink < 0) {
            pl.switchAccum = 0.0f;
            Rng rng(seed, rng_stream::kFungi + 700, pl.id, stepIndex);
            std::size_t foods = 0, tips = 0;
            for (std::size_t k = 0; k < pl.x.size(); ++k) {
                foods += pl.food[k] >= 0;
                tips += pl.tip[k];
            }
            pl.source = pl.sink = -1;
            auto pickNth = [&](auto pred, std::size_t nth) {
                for (std::size_t k = 0; k < pl.x.size(); ++k)
                    if (pred(k) && nth-- == 0)
                        return static_cast<std::int32_t>(k);
                return static_cast<std::int32_t>(-1);
            };
            auto isFood = [&](std::size_t k) { return pl.food[k] >= 0; };
            if (foods >= 2 && (tips == 0 || rng.uniform() < 0.5f)) {
                const std::size_t a = static_cast<std::size_t>(rng.uniform() * static_cast<float>(foods)) % foods;
                std::size_t b = static_cast<std::size_t>(rng.uniform() * static_cast<float>(foods - 1)) % (foods - 1);
                b += b >= a ? 1 : 0; // otra comida distinta
                pl.source = pickNth(isFood, a);
                pl.sink = pickNth(isFood, b);
            } else if (foods >= 1 && tips >= 1) {
                pl.source = pickNth(isFood, static_cast<std::size_t>(rng.uniform() * static_cast<float>(foods)) % foods);
                pl.sink = kAllTips;
            }
        }

        // Red: flujo y adaptación cada tanto (el solver es lo más caro).
        pl.solveAccum += hours;
        if (pl.solveAccum >= p.solveIntervalHours) {
            const float before = pl.tubeMass;
            oldD_.resize(pl.edges.size());
            for (std::size_t k = 0; k < pl.edges.size(); ++k)
                oldD_[k] = pl.edges[k].D;
            if (solveFlow(pl, pl.source, pl.sink, 1.0f)) {
                adapt(pl, pl.solveAccum);
                // Ensanchar un tubo cuesta masa: sin reservas alcanzan solo los que se pueden pagar.
                const float cost = (tubeMassOf(pl) - before) / p.growthYield;
                if (cost > pl.reserves) {
                    const float allowed = std::clamp(pl.reserves / cost, 0.0f, 1.0f);
                    for (std::size_t k = 0; k < pl.edges.size(); ++k)
                        if (pl.edges[k].D > oldD_[k])
                            pl.edges[k].D = oldD_[k] + allowed * (pl.edges[k].D - oldD_[k]);
                }
            } else {
                // Sin comida que conecte: los tubos se angostan despacio (la reserva se gasta).
                const float decay = std::exp(-0.2f * p.adaptRate * pl.solveAccum);
                for (Plasmodium::Edge& e : pl.edges)
                    e.D *= decay;
            }
            pl.solveAccum = 0.0f;
            pl.tubeMass = tubeMassOf(pl);
            // Ensanchar cuesta masa; angostar la devuelve (con pérdida).
            const float delta = pl.tubeMass - before;
            pl.reserves -= delta > 0.0f ? delta / p.growthYield : 0.5f * delta;
            prune(pl, env);
        }

        // Hambre prolongada: se endurece en esclerocio; sin nada, muere.
        const bool dead = pl.x.empty();
        if (dead || (pl.starvingHours >= p.starveHoursToSclerotium && pl.biomass() > 0.5f)) {
            const Vec2 where = pl.x.empty() ? Vec2{} : Vec2{pl.x[0], pl.y[0]};
            const float total = std::max(0.0f, pl.biomass());
            if (!dead && sclerotia.size() < 200) {
                const float kept = p.sclerotiumFraction * total;
                sclerotia.x.push_back(where.x);
                sclerotia.y.push_back(where.y);
                sclerotia.ageHours.push_back(0.0f);
                sclerotia.pmol.push_back(kept);
                sclerotia.phosphorus.push_back(pl.phosphorus);
                ++stats.sclerotiaFormed;
                env.release(where, total - kept, 0.0f);
            } else {
                env.release(where, total, pl.phosphorus);
            }
            plasmodia[pi] = std::move(plasmodia.back());
            plasmodia.pop_back();
            continue;
        }
        ++pi;
    }

    // Esclerocios: duermen hasta que haya comida disuelta cerca.
    for (std::size_t i = 0; i < sclerotia.size();) {
        sclerotia.ageHours[i] += hours;
        const Vec2 at = {sclerotia.x[i], sclerotia.y[i]};
        if (sclerotia.ageHours[i] >= p.sclerotiumMinHours && env.food(at) >= p.sclerotiumGerminateFood &&
            plasmodia.size() < p.maxPlasmodia) {
            Rng rng(seed, rng_stream::kFungi + 800, i, stepIndex);
            spawn(at, rng.uniform(0.0f, 2.0f * units::kPi), sclerotia.pmol[i], sclerotia.phosphorus[i]);
            sclerotia.swapRemove(i);
            ++stats.sclerotiaGerminated;
            continue;
        }
        ++i;
    }
}
