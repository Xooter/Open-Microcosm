// Señales químicas entre seres (M3) y morfos inducidos (M4) de docs/mechanics/chemical-signals.md: la kairomona de
// Lembadion hace que Euplotes octocarinatus arme alas; lo que suelta Tetrahymena hace que T. vorax pase
// a macrostoma.
#include "core/profiling.hpp"
#include "game/world.hpp"

#include <algorithm>
#include <cmath>

namespace {
constexpr std::size_t kMaxSignals = 4; // una FFT más por señal activa
} // namespace

void World::setupSignals(const WorldConfig& config) {
    // Una señal por id que libere algún ser cargado (la primera definición manda).
    for (int k = 0; k < speciesCount() && signals_.size() < kMaxSignals; ++k) {
        const ReleaseSpec& r = speciesInfo(static_cast<SpeciesId>(k)).release;
        if (r.signal.empty() || signalIndex(r.signal) >= 0)
            continue;
        signals_.push_back({r.signal, ChemField(config.gridX, config.gridY, config.cellSize, r.diffusion, 0.0f),
                            r.halfLifeHours, false, r.diffusion, r.bioTime});
        signals_.back().field.setMask(&dishMask_);
    }
}

int World::signalIndex(std::string_view id) const {
    for (std::size_t i = 0; i < signals_.size(); ++i)
        if (signals_[i].id == id)
            return static_cast<int>(i);
    return -1;
}

float World::signalAt(int index, Vec2 p) const {
    if (index < 0 || !signals_[static_cast<std::size_t>(index)].active)
        return 0.0f;
    return std::max(0.0f, signals_[static_cast<std::size_t>(index)].field.grid().sample(p.x, p.y));
}

void World::updateSignals(float dt) {
    PROFILE_SCOPE("World::updateSignals");
    if (signals_.empty())
        return;
    const float bioDt = dt * bioScale();
    const float perPmol = microMolarPerPmol();
    for (SignalField& s : signals_) // la de un proceso biológico difunde en tiempo biológico
        s.field.diffusion = s.baseDiffusion * (s.bioTime ? bioTimeScale_ : 1.0f);
    const float hours = bioDt / 3600.0f;
    for (auto [e, body, sp] : registry_.view<RingBody, Species>().each()) {
        const SpeciesInfo& info = speciesInfo(sp.id);
        const ReleaseSpec& r = info.release;
        if (info.relay.present && !registry_.any_of<Cyst>(e)) {
            // Relevo (Dictyostelium): con hambre se vuelve competente; dispara al sentir la señal (o solo,
            // si es marcapasos) y queda refractaria.
            const RelaySpec& rl = info.relay;
            Relay& relay = registry_.get_or_emplace<Relay>(e);
            const Metabolism* m = registry_.try_get<Metabolism>(e);
            relay.starvingHours = m && m->starving ? relay.starvingHours + hours : 0.0f;
            if (!relay.competent && relay.starvingHours >= rl.competenceHours) {
                relay.competent = true;
                Rng rng(config_.seed, rng_stream::kCells + 70, body.id, step_);
                relay.pacemaker = rng.uniform() < rl.pacemakerFraction;
                relay.pacemakerHours = rng.uniform(0.0f, rl.pacemakerMinutes / 60.0f);
            }
            relay.refractoryHours = std::max(0.0f, relay.refractoryHours - hours);
            if (relay.pacemaker)
                relay.pacemakerHours -= hours;
            const int i = signalIndex(r.signal);
            if (relay.competent && relay.refractoryHours <= 0.0f && i >= 0 &&
                (signalAt(i, body.centroid) >= rl.threshold || (relay.pacemaker && relay.pacemakerHours <= 0.0f))) {
                SignalField& s = signals_[static_cast<std::size_t>(i)];
                Grid2D& g = s.field.grid();
                g.at(g.cellX(body.centroid.x), g.cellY(body.centroid.y)) += rl.pulse * perPmol;
                s.active = true;
                relay.refractoryHours = rl.refractoryMinutes / 60.0f;
                if (relay.pacemaker)
                    relay.pacemakerHours = rl.pacemakerMinutes / 60.0f;
            }
        }
        if (r.rate <= 0.0f || registry_.any_of<Cyst>(e))
            continue;
        const int i = signalIndex(r.signal);
        if (i < 0)
            continue;
        SignalField& s = signals_[static_cast<std::size_t>(i)];
        Grid2D& g = s.field.grid();
        g.at(g.cellX(body.centroid.x), g.cellY(body.centroid.y)) += r.rate * bioDt / 3600.0f * perPmol;
        s.active = true;
    }
    for (SignalField& s : signals_) { // se degrada (proteasas, bacterias): vida media de la especie
        if (!s.active)
            continue;
        const float keep = std::exp(-bioDt * 0.69314718f / (s.halfLifeHours * 3600.0f));
        for (float& v : s.field.grid().v)
            v *= keep;
    }
}

float World::morphAmount(entt::entity e) const {
    const Morph* m = registry_.try_get<Morph>(e);
    if (!m)
        return 0.0f;
    const float t = m->progress;
    return t * t * (3.0f - 2.0f * t);
}

FeedingMode World::feedingOf(entt::entity e) const {
    const SpeciesInfo& info = speciesOfCell(e);
    return info.morph.feedingSet && morphAmount(e) > 0.5f ? info.morph.feeding : info.feeding;
}

const SpeciesSet& World::preyOf(entt::entity e) const {
    const SpeciesInfo& info = speciesOfCell(e);
    return info.morph.preySet && morphAmount(e) > 0.5f ? info.morph.prey : info.prey;
}

float World::mouthWidthOf(entt::entity e) const {
    const SpeciesInfo& info = speciesOfCell(e);
    return info.morph.mouthWidth >= 0.0f && morphAmount(e) > 0.5f ? info.morph.mouthWidth : info.mouthWidth;
}

float World::morphAreaScale(entt::entity e) const {
    const SpeciesInfo& info = speciesOfCell(e);
    return info.morph.present ? 1.0f + (info.morph.areaScale - 1.0f) * morphAmount(e) : 1.0f;
}

void World::updateMorphs(float dt) {
    PROFILE_SCOPE("World::updateMorphs");
    const float bioDt = dt * bioScale();
    for (auto [e, body, sp] : registry_.view<RingBody, Species>().each()) {
        const MorphSpec& spec = speciesInfo(sp.id).morph;
        if (!spec.present || registry_.any_of<Dividing, Cyst>(e))
            continue;
        const bool induced = signalAt(signalIndex(spec.signal), body.centroid) >= spec.threshold && spec.threshold > 0.0f;
        Morph* m = registry_.try_get<Morph>(e);
        if (!m) {
            if (!induced)
                continue;
            m = &registry_.emplace<Morph>(e);
        }
        const float step = bioDt / (spec.hours * 3600.0f);
        m->progress = std::clamp(m->progress + (induced ? step : -step), 0.0f, 1.0f);
        // Forma: estiramiento afín de la forma de reposo (cada punto de membrana sigue siendo el mismo);
        // el área la ajusta la biomasa (con `area_scale`).
        const float t = morphAmount(e);
        const float sx = 1.0f + (spec.lengthScale - 1.0f) * t, sy = 1.0f + (spec.widthScale - 1.0f) * t;
        if (std::abs(sx - m->sx) > 1e-4f || std::abs(sy - m->sy) > 1e-4f) {
            for (Vec2& q : body.restRing) {
                q.x *= sx / m->sx;
                q.y *= sy / m->sy;
            }
            m->sx = sx;
            m->sy = sy;
        }
    }
}

void World::unmorphForDivision(entt::entity e, RingBody& body) {
    // Simplificación: la célula se divide con la forma de su especie y las hijas vuelven a inducirse si
    // la señal sigue (en la realidad las hijas de un Euplotes inducido nacen ya con alas).
    if (Morph* m = registry_.try_get<Morph>(e)) {
        for (Vec2& q : body.restRing) {
            q.x /= m->sx;
            q.y /= m->sy;
        }
        registry_.remove<Morph>(e);
    }
}
