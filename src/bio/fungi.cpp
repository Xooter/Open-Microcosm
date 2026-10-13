#include "bio/fungi.hpp"

#include "core/profiling.hpp"
#include "core/rng.hpp"
#include "core/units.hpp"
#include "sim/fluid.hpp"

#include <algorithm>
#include <cmath>

const char* fungusName(FungusKind kind) {
    switch (kind) {
    case FungusKind::Chytrid:
        return "Rhizophydium";
    case FungusKind::Saprolegnia:
        return "Saprolegnia";
    case FungusKind::Zoophagus:
        return "Zoophagus";
    default:
        return "?";
    }
}

void Zoospores::push(FungusKind k, float px, float py, float h, float p) {
    x.push_back(px);
    y.push_back(py);
    heading.push_back(h);
    ageHours.push_back(0.0f);
    phosphorus.push_back(p);
    kind.push_back(k);
}

void Zoospores::swapRemove(std::size_t i) {
    const std::size_t last = x.size() - 1;
    x[i] = x[last];
    y[i] = y[last];
    heading[i] = heading[last];
    ageHours[i] = ageHours[last];
    phosphorus[i] = phosphorus[last];
    kind[i] = kind[last];
    x.pop_back();
    y.pop_back();
    heading.pop_back();
    ageHours.pop_back();
    phosphorus.pop_back();
    kind.pop_back();
}

std::uint32_t Fungi::spawnMycelium(FungusKind kind, Vec2 at, float heading, float reserves, float phosphorus) {
    Mycelium m;
    m.id = nextId_++;
    m.kind = kind;
    m.x.push_back(at.x);
    m.y.push_back(at.y);
    m.heading.push_back(heading);
    m.parent.push_back(-1);
    m.tip.push_back(1);
    m.peg.push_back(0);
    m.sporangium.push_back(0.0f);
    m.reserves = reserves;
    m.phosphorus = phosphorus;
    mycelia.push_back(std::move(m));
    return mycelia.back().id;
}

void Fungi::spawnSpores(FungusKind kind, Vec2 at, int count, float phosphorus, std::uint64_t seed,
                        std::uint64_t salt) {
    if (count <= 0)
        return;
    const float each = phosphorus / static_cast<float>(count);
    for (int k = 0; k < count; ++k) {
        Rng rng(seed, rng_stream::kFungi + 300, salt * 64ull + static_cast<std::uint64_t>(k), 0);
        const float a = rng.uniform(0.0f, 2.0f * units::kPi);
        const float r = rng.uniform(0.0f, 4.0f);
        spores.push(kind, at.x + r * std::cos(a), at.y + r * std::sin(a), a, each);
    }
}

std::size_t Fungi::nodeCount() const {
    std::size_t n = 0;
    for (const Mycelium& m : mycelia)
        n += m.x.size();
    return n;
}

float Fungi::biomass() const {
    float b = static_cast<float>(spores.size()) * params.sporePmol;
    for (const Mycelium& m : mycelia)
        b += m.biomass(params);
    return b;
}

float Fungi::phosphorus() const {
    float p = 0.0f;
    for (float v : spores.phosphorus)
        p += v;
    for (const Mycelium& m : mycelia)
        p += m.phosphorus;
    return p;
}

const Mycelium* Fungi::find(std::uint32_t id) const {
    for (const Mycelium& m : mycelia)
        if (m.id == id)
            return &m;
    return nullptr;
}

Mycelium* Fungi::find(std::uint32_t id) {
    for (Mycelium& m : mycelia)
        if (m.id == id)
            return &m;
    return nullptr;
}

void Fungi::feed(std::uint32_t id, float pmol, float phosphorus) {
    if (Mycelium* m = find(id)) {
        m->reserves += pmol;
        m->phosphorus += phosphorus;
    }
}

void Fungi::moveSpores(const Fluid& fluid, const std::vector<Vec2>& steer, float dt, std::uint64_t seed,
                       std::uint64_t step, float width, float height) {
    PROFILE_SCOPE("Fungi::moveSpores");
    const auto n = static_cast<std::ptrdiff_t>(spores.size());
#pragma omp parallel for schedule(static)
    for (std::ptrdiff_t i = 0; i < n; ++i) {
        Rng rng(seed, rng_stream::kFungi, static_cast<std::uint64_t>(i), step);
        float speed = 0.0f;
        switch (spores.kind[i]) {
        case FungusKind::Chytrid:
            speed = params.chytridSporeSpeed;
            break;
        case FungusKind::Saprolegnia:
            speed = params.saproSporeSpeed;
            break;
        default:
            break; // Zoophagus: esporas inmóviles que arrastra el agua
        }
        float& h = spores.heading[i];
        const Vec2 s = i < static_cast<std::ptrdiff_t>(steer.size()) ? steer[i] : Vec2{};
        if (s.x != 0.0f || s.y != 0.0f) {
            // Quimiotaxis: gira hacia el estímulo (exudados del alga, aminoácidos).
            float d = std::atan2(s.y, s.x) - h;
            d = std::remainder(d, 2.0f * units::kPi);
            h += std::clamp(d, -3.0f * dt, 3.0f * dt);
        }
        h += std::sqrt(2.0f * 0.5f * dt) * rng.normal(); // difusión rotacional
        const Vec2 u = fluid.velocityAt({spores.x[i], spores.y[i]});
        spores.x[i] = wrapCoord(spores.x[i] + (u.x + std::cos(h) * speed) * dt, width);
        spores.y[i] = wrapCoord(spores.y[i] + (u.y + std::sin(h) * speed) * dt, height);
    }
}

namespace {

// Saca un nodo hoja (sin hijos) del micelio. Devuelve false si no hay.
bool removeLeaf(Mycelium& m, Vec2& where) {
    const std::size_t n = m.x.size();
    if (n == 0)
        return false;
    std::vector<std::uint8_t> hasChild(n, 0);
    for (std::size_t i = 0; i < n; ++i)
        if (m.parent[i] >= 0)
            hasChild[static_cast<std::size_t>(m.parent[i])] = 1;
    std::size_t leaf = n;
    for (std::size_t i = n; i-- > 0;)
        if (!hasChild[i]) {
            leaf = i;
            break;
        }
    if (leaf == n)
        return false;
    where = {m.x[leaf], m.y[leaf]};
    // El padre de la hoja vuelve a ser punta (la hifa se retrae).
    if (m.parent[leaf] >= 0)
        m.tip[static_cast<std::size_t>(m.parent[leaf])] = 1;
    const std::size_t last = n - 1;
    if (leaf != last) {
        m.x[leaf] = m.x[last];
        m.y[leaf] = m.y[last];
        m.heading[leaf] = m.heading[last];
        m.parent[leaf] = m.parent[last];
        m.tip[leaf] = m.tip[last];
        m.peg[leaf] = m.peg[last];
        m.sporangium[leaf] = m.sporangium[last];
        for (std::int32_t& p : m.parent)
            if (p == static_cast<std::int32_t>(last))
                p = static_cast<std::int32_t>(leaf);
    }
    m.x.pop_back();
    m.y.pop_back();
    m.heading.pop_back();
    m.parent.pop_back();
    m.tip.pop_back();
    m.peg.pop_back();
    m.sporangium.pop_back();
    return true;
}

void addNode(Mycelium& m, std::size_t from, float heading, float spacing, bool peg) {
    m.x.push_back(m.x[from] + std::cos(heading) * spacing);
    m.y.push_back(m.y[from] + std::sin(heading) * spacing);
    m.heading.push_back(heading);
    m.parent.push_back(static_cast<std::int32_t>(from));
    m.tip.push_back(peg ? 0 : 1);
    m.peg.push_back(peg ? 1 : 0);
    m.sporangium.push_back(0.0f);
}

} // namespace

void Fungi::grow(const FungiEnv& env, float bioDt, std::uint64_t seed, std::uint64_t step) {
    PROFILE_SCOPE("Fungi::grow");
    const float hours = bioDt / 3600.0f;
    const FungiParams& p = params;
    const float nodeCost = p.nodePmol / p.growthYield;
    std::size_t totalNodes = nodeCount();

    for (std::size_t mi = 0; mi < mycelia.size();) {
        Mycelium& m = mycelia[mi];
        Rng rng(seed, rng_stream::kFungi + 100, m.id, step);
        const std::size_t n = m.x.size();
        const float biomass = static_cast<float>(n) * p.nodePmol;

        // Absorción por toda la hifa (osmotrofia). Zoophagus vive sobre todo de sus presas.
        const float uptakeScale = m.kind == FungusKind::Zoophagus ? 0.3f : 1.0f;
        float absorbed = 0.0f;
        for (std::size_t k = 0; k < n; ++k) {
            const Vec2 at = {m.x[k], m.y[k]};
            const float c = std::max(0.0f, env.food(at));
            const float want = uptakeScale * p.uptakePerHour * p.nodePmol * hours * c / (c + p.halfSaturation);
            if (want > 1e-7f)
                absorbed += env.takeFood(at, want);
        }
        m.reserves += absorbed;
        const float maintenance = biomass * p.maintenancePerHour * hours;
        m.reserves -= maintenance;
        m.starvingHours = absorbed < maintenance ? m.starvingHours + hours : std::max(0.0f, m.starvingHours - hours);

        // Crecimiento apical: las puntas avanzan si hay reservas y fósforo para la hifa nueva.
        if (m.reserves > 2.0f * nodeCost && m.starvingHours < p.starvationHoursToSporulate)
            m.tipCredit += p.extensionPerHour * hours;
        while (m.tipCredit >= p.hyphaSpacing) {
            m.tipCredit -= p.hyphaSpacing;
            const std::size_t count = m.x.size();
            for (std::size_t k = 0; k < count; ++k) {
                if (!m.tip[k] || m.sporangium[k] > 0.0f)
                    continue;
                if (m.reserves < nodeCost || m.x.size() >= p.maxNodesPerMycelium || totalNodes >= p.maxNodes)
                    break;
                const Vec2 at = {m.x[k], m.y[k]};
                const float needP = p.nodePmol * p.phosphorusRatio;
                const float haveP = m.phosphorus - p.phosphorusRatio * static_cast<float>(m.x.size()) * p.nodePmol;
                if (haveP < needP)
                    m.phosphorus += env.takePhosphate(at, needP - std::max(0.0f, haveP));
                if (m.phosphorus < p.phosphorusRatio * static_cast<float>(m.x.size() + 1) * p.nodePmol)
                    break; // sin fósforo no hay hifa nueva
                m.reserves -= nodeCost;
                m.tip[k] = 0;
                const float h = m.heading[k] + rng.normal() * 0.25f;
                // Zoophagus: clavijas laterales cortas y adhesivas cada pocos nodos.
                const bool wantPeg = m.kind == FungusKind::Zoophagus && (m.x.size() % static_cast<std::size_t>(p.pegSpacing)) == 0;
                addNode(m, k, h, p.hyphaSpacing, false);
                ++totalNodes;
                if (wantPeg && m.reserves >= nodeCost) {
                    const float side = rng.uniform() < 0.5f ? -1.0f : 1.0f;
                    addNode(m, k, h + side * 1.4f, 0.6f * p.hyphaSpacing, true);
                    m.reserves -= 0.3f * nodeCost;
                    ++totalNodes;
                } else if (rng.uniform() < p.branchProbability && m.reserves >= nodeCost) {
                    const float side = rng.uniform() < 0.5f ? -1.0f : 1.0f;
                    addNode(m, k, h + side * rng.uniform(0.5f, 1.0f), p.hyphaSpacing, false);
                    m.reserves -= nodeCost;
                    ++totalNodes;
                }
            }
        }

        // Esporulación: sin comida, las puntas se hinchan en zoosporangios.
        if (m.kind != FungusKind::Chytrid && m.starvingHours >= p.starvationHoursToSporulate && m.x.size() >= 12) {
            bool any = false;
            for (float s : m.sporangium)
                any = any || s > 0.0f;
            if (!any)
                for (std::size_t k = m.x.size(); k-- > 0;)
                    if (m.tip[k]) {
                        m.sporangium[k] = 1e-4f;
                        break;
                    }
        }
        for (std::size_t k = 0; k < m.x.size(); ++k) {
            if (m.sporangium[k] <= 0.0f)
                continue;
            m.sporangium[k] += hours;
            if (m.sporangium[k] < p.sporangiumHours)
                continue;
            m.sporangium[k] = 0.0f;
            m.tip[k] = 0;
            // El citoplasma de la hifa pasa a las zoosporas: se pagan con reservas y, si no
            // alcanzan, con hifa (autólisis desde las puntas).
            const int count = std::min(p.sporesPerSporangium, static_cast<int>(m.biomass(p) * 0.4f / p.sporePmol));
            if (count <= 0 || spores.size() + static_cast<std::size_t>(count) > p.maxSpores)
                continue;
            const Vec2 at = {m.x[k], m.y[k]};
            m.reserves -= static_cast<float>(count) * p.sporePmol;
            const float share = std::min(m.phosphorus, static_cast<float>(count) * p.sporePmol * p.phosphorusRatio);
            m.phosphorus -= share;
            spawnSpores(m.kind, at, count, share, seed, step * 1024ull + m.id);
            m.starvingHours = 0.0f;
        }

        // Autólisis: sin reservas, la hifa se consume desde las puntas.
        while (m.reserves < 0.0f && !m.x.empty()) {
            const std::size_t before = m.x.size();
            Vec2 at;
            if (!removeLeaf(m, at))
                break;
            const float pShare = m.phosphorus / static_cast<float>(before);
            m.phosphorus -= pShare;
            m.reserves += 0.5f * p.nodePmol;
            env.release(at, 0.5f * p.nodePmol, pShare);
            --totalNodes;
        }
        if (m.x.empty()) {
            env.release({0.0f, 0.0f}, std::max(0.0f, m.reserves), m.phosphorus);
            mycelia[mi] = std::move(mycelia.back());
            mycelia.pop_back();
            continue;
        }
        ++mi;
    }

    // Zoosporas: envejecen; las de los mohos germinan sobre sustrato y las viejas mueren.
    for (std::size_t i = 0; i < spores.size();) {
        spores.ageHours[i] += hours;
        const Vec2 at = {spores.x[i], spores.y[i]};
        if (spores.ageHours[i] > p.sporeLifeHours) {
            env.release(at, p.sporePmol, spores.phosphorus[i]);
            spores.swapRemove(i);
            continue;
        }
        if (spores.kind[i] != FungusKind::Chytrid && spores.ageHours[i] > 0.25f && totalNodes < p.maxNodes &&
            env.substrate(at)) {
            Rng rng(seed, rng_stream::kFungi + 200, i, step);
            spawnMycelium(spores.kind[i], at, rng.uniform(0.0f, 2.0f * units::kPi), p.sporePmol, spores.phosphorus[i]);
            ++totalNodes;
            spores.swapRemove(i);
            continue;
        }
        ++i;
    }
}
