// Química del agua e interacciones microbianas: nitrógeno y cianobacterias fijadoras, CO₂ y pH,
// toxinas (microcistina, violaceína), quorum sensing, fagos y Bdellovibrio.
#include "game/world.hpp"

#include "bio/cell_physiology.hpp"
#include "core/profiling.hpp"
#include "core/rng.hpp"

#include <algorithm>
#include <cmath>


void World::stepCyanobacteria(float dt) {
    PROFILE_SCOPE("World::stepCyanobacteria");
    cyano_.move(fluid_, dt, config_.seed, step_, width_, height_);
    const float bioDt = dt * bioScale();
    cyano_.grow(light_, phosphate_.grid(), nitrogen_.grid(), ph_, bioDt, cyanoDemand_, cyanoDemandN_);
    // Fosfato y nitrógeno salen del agua cercana a cada filamento (conserva la masa).
    for (std::size_t f = 0; f < cyanoDemand_.size(); ++f) {
        const CyanoFilament& fil = cyano_.filaments[f];
        if (fil.x.empty())
            continue;
        const std::size_t mid = fil.x.size() / 2;
        const Vec2 at = wrapped({fil.x[mid], fil.y[mid]});
        if (cyanoDemand_[f] > 0.0f)
            cyanoDemand_[f] = -exchange(phosphate_, at, -cyanoDemand_[f], Flow::Uptake);
        if (cyanoDemandN_[f] > 0.0f)
            cyanoDemandN_[f] = -exchange(nitrogen_, at, -cyanoDemandN_[f], Flow::Uptake);
    }
    cyanoReleases_.clear();
    const double fixedBefore = cyano_.fixedTotal;
    cyano_.credit(cyanoDemand_, cyanoDemandN_, oxygen_.grid(), bioDt, config_.seed, step_, cyanoReleases_);
    if (cyano_.fixedTotal > fixedBefore) {
        observed_.nitrogenFixed += static_cast<float>(cyano_.fixedTotal - fixedBefore);
        ledger_.addExternal(Element::N, cyano_.fixedTotal - fixedBefore); // N₂ del aire
    }
    for (const CyanoRelease& r : cyanoReleases_) {
        const Vec2 at = wrapped({r.x, r.y});
        if (r.organic > 0.0f)
            exchange(nutrients_, at, r.organic, Flow::Death);
        if (r.phosphorus > 0.0f)
            exchange(phosphate_, at, r.phosphorus, Flow::Death);
        if (r.nitrogen > 0.0f)
            exchange(nitrogen_, at, r.nitrogen, Flow::Death);
        if (r.toxin > 0.0f)
            exchange(toxin_, at, r.toxin);
    }
    // La fotosíntesis de las cianobacterias también fija CO₂ (y sube el pH en un bloom).
    const float hours = bioDt / 3600.0f;
    for (const CyanoFilament& fil : cyano_.filaments) {
        if (fil.x.empty())
            continue;
        const std::size_t mid = fil.x.size() / 2;
        const float fixedGlucose = cyano_.params.growthPerHourMax * hours * cyano_.params.cellPmol *
                                   static_cast<float>(fil.x.size()) *
                                   std::clamp(light_.sample(fil.x[mid], fil.y[mid]), 0.0f, 1.0f);
        exchangeCO2(wrapped({fil.x[mid], fil.y[mid]}), -6.0f * fixedGlucose / bioTimeScale_); // tasa real (con Q10)
    }
}

void World::exchangeCO2(Vec2 at, float pmol) { exchange(co2_, at, pmol); }

void World::updateWaterChemistry(float dt) {
    PROFILE_SCOPE("World::updateWaterChemistry");
    const float hours = dt * bioScale() / 3600.0f;
    // CO₂: intercambio con el aire, como el O₂ (tasa real).
    const float co2Keep = std::exp(-medium_.o2AirExchange * dt);
    const float toxinKeep = std::exp(-medium_.toxinDecayPerHour * hours);
    const float phageKeep = std::exp(-medium_.phageDecayPerHour * hours);
    const float ahlBase = std::log(2.0f) / medium_.ahlHalfLifeHours;
    auto& co2 = co2_.grid().v;
    auto& tox = toxin_.grid().v;
    auto& ahl = autoinducer_.grid().v;
    auto& phg = phage_.grid().v;
    double fromAir = 0.0; // [µM·celda] de CO₂ que entra (o sale) por la superficie
    for (std::size_t k = 0; k < co2.size(); ++k) {
        if (!dishMask_.isWater(k))
            continue; // vidrio de la placa (celdas fantasma)
        const float before = co2[k];
        co2[k] = co2Saturation_ + (co2[k] - co2Saturation_) * co2Keep;
        fromAir += co2[k] - before;
        tox[k] *= toxinKeep;
        phg[k] *= phageKeep;
        // Lactonólisis: la vida media del AHL cae 10 veces por unidad de pH sobre 7.
        const float rate = ahlBase * std::pow(10.0f, std::clamp(ph_.v[k] - 7.0f, -1.0f, 3.0f));
        ahl[k] *= std::exp(-rate * hours);
    }
    // Balance de C: frontera abierta con el aire.
    const double pmol = fromAir / microMolarPerPmol();
    ledger_.add(Element::C, Flow::Air, pmol);
    ledger_.addExternal(Element::C, pmol);
}

void World::computePH() {
    // Sistema carbonato con alcalinidad fija: pH = pK′₁ + log₁₀([HCO₃⁻]/[CO₂]) (Henderson–
    // Hasselbalch). La fotosíntesis consume CO₂ y sube el pH (hasta 9–10 en un bloom); la
    // respiración lo baja; la incubadora lo fija con su CO₂. Simplificación: sin el carbonato
    // (pK′₂ ~10), que frena la suba del pH por encima de ~9.
    const auto& co2 = co2_.grid().v;
    const float pk = medium_.carbonatePK;
    if (!cultureFields_) {
        for (std::size_t k = 0; k < co2.size(); ++k)
            ph_.v[k] = std::clamp(pk + std::log10(medium_.alkalinity / std::max(0.05f, co2[k])), 4.0f, 11.0f);
        return;
    }
    // El ácido láctico que secretan las células (pKa 3,9: todo disociado) se lleva una base por
    // lactato: el bicarbonato baja y el medio se acidifica (el rojo fenol vira a naranja y amarillo).
    // El CO₂ que libera esa reacción se va al aire de la incubadora.
    const auto& lac = lactate_.grid().v;
    for (std::size_t k = 0; k < co2.size(); ++k) {
        const float bicarbonate = std::max(1.0f, medium_.alkalinity - std::max(0.0f, lac[k]));
        ph_.v[k] = std::clamp(pk + std::log10(bicarbonate / std::max(0.05f, co2[k])), 4.0f, 11.0f);
    }
}

const char* World::applyEnvironmentStress(entt::entity e, const RingBody& body, const SpeciesInfo& info,
                                          MetabolicInput& in, float bioDt) {
    CellSurroundings around;
    around.ph = ph_.sample(body.centroid.x, body.centroid.y);
    around.o2 = oxygen_.grid().sample(body.centroid.x, body.centroid.y);
    around.microcystin = toxin_.grid().sample(body.centroid.x, body.centroid.y);
    around.temperature = medium_.temperature;
    around.salinity = medium_.salinity;
    Digestion* d = registry_.try_get<Digestion>(e);
    return ::applyEnvironmentStress(info, around, d ? &d->toxin : nullptr, bioDt, in);
}

void World::seedBdellovibrio(std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        Rng rng(config_.seed, rng_stream::kBacteria + 400, i, 0);
        const Vec2 p = randomPoint(rng, 2.0f);
        bdello_.push(p.x, p.y, rng.uniform(0.0f, 2.0f * units::kPi));
    }
}

void World::microbialInteractions(float dt) {
    PROFILE_SCOPE("World::microbialInteractions");
    const float bioDt = dt * bioScale();
    const float hours = bioDt / 3600.0f;
    const BacteriaParams& bp = bacteria_.params;
    const float k = microMolarPerPmol();
    // Volumen de agua de una celda [µm³] (para pasar viriones a concentración).
    const float cellVolume = phage_.grid().dx * phage_.grid().dx * fluid_.gapHeight;
    Grid2D& ahl = autoinducer_.grid();
    Grid2D& phg = phage_.grid();
    Grid2D& food = nutrients_.grid();
    eatenBacteria_.clear();
    const std::size_t n = bacteria_.size();
    for (std::size_t i = 0; i < n; ++i) {
        const int cx = ahl.cellX(bacteria_.x[i]), cy = ahl.cellY(bacteria_.y[i]);
        // Quorum sensing (C. violaceum): cada una libera autoinductor; cuando hay muchas cerca,
        // la concentración pasa el umbral y cambian de programa (violaceína, agregado).
        if (bacteria_.strain[i] == 1) {
            float& a = ahl.at(cx, cy);
            a += bp.ahlPerHour * hours * k;
            const std::uint8_t before = bacteria_.quorum[i];
            if (a > bp.quorumThreshold)
                bacteria_.quorum[i] = 1;
            else if (a < 0.5f * bp.quorumThreshold)
                bacteria_.quorum[i] = 0;
            if (!before && bacteria_.quorum[i])
                ++observed_.quorumSwitches;
        }
        // Fagos: adsorción a las sensibles; período latente; estallido.
        float& v = phg.at(cx, cy);
        if (bacteria_.phage[i] < 0.0f && bacteria_.bdello[i] < 0.0f && !bacteria_.resistant[i] && v > 0.0f) {
            Rng rng(config_.seed, rng_stream::kBacteria + 200, i, step_);
            if (rng.uniform() < bp.phageAdsorption * v * hours) {
                bacteria_.phage[i] = 0.0f;
                v = std::max(0.0f, v - 1.0f / cellVolume);
            }
        } else if (bacteria_.phage[i] >= 0.0f) {
            bacteria_.phage[i] += hours;
            if (bacteria_.phage[i] >= bp.phageLatentHours) {
                v += static_cast<float>(bp.phageBurst) / cellVolume;
                food.at(cx, cy) += bacteria_.mass[i] * kBacteriumPmol * k; // el contenido se derrama
                eatenBacteria_.push_back(i);
                ++observed_.phageLyses;
                continue;
            }
        }
        // Bdeloplasto: el Bdellovibrio crece adentro y sale con sus hijas.
        if (bacteria_.bdello[i] >= 0.0f) {
            bacteria_.bdello[i] += hours;
            if (bacteria_.bdello[i] >= bp.bdelloHours) {
                Rng rng(config_.seed, rng_stream::kBacteria + 300, i, step_);
                for (int c = 0; c < bp.bdelloProgeny; ++c)
                    bdello_.push(bacteria_.x[i], bacteria_.y[i], rng.uniform(0.0f, 2.0f * units::kPi));
                food.at(cx, cy) += 0.3f * bacteria_.mass[i] * kBacteriumPmol * k;
                eatenBacteria_.push_back(i);
                ++observed_.bdelloBursts;
            }
        }
    }
    removeBacteria(eatenBacteria_);

    // Bdellovibrio libres: nadan rápido en línea recta con cambios bruscos; mueren de hambre.
    for (std::size_t i = 0; i < bdello_.size();) {
        bdello_.hungerHours[i] += hours;
        if (bdello_.hungerHours[i] > bp.bdelloStarveHours) {
            bdello_.swapRemove(i);
            continue;
        }
        Rng rng(config_.seed, rng_stream::kBacteria + 500, i, step_);
        if (rng.uniform() < 0.5f * dt)
            bdello_.heading[i] += rng.uniform(-2.5f, 2.5f);
        const Vec2 u = fluid_.velocityAt({bdello_.x[i], bdello_.y[i]});
        bdello_.x[i] = wrapCoord(bdello_.x[i] + (u.x + std::cos(bdello_.heading[i]) * bp.bdelloSpeed) * dt, width_);
        bdello_.y[i] = wrapCoord(bdello_.y[i] + (u.y + std::sin(bdello_.heading[i]) * bp.bdelloSpeed) * dt, height_);
        dish_.bounce(bdello_.x[i], bdello_.y[i], &bdello_.heading[i], 1.0f);
        ++i;
    }
    if (bdello_.size() == 0)
        return;
    // Encuentros: grilla sobre los depredadores (pocos) y cada bacteria busca uno a < 1,5 µm.
    bdelloGrid_.build(bdello_.x, bdello_.y, width_, height_, 4.0f);
    std::vector<std::uint8_t> used(bdello_.size(), 0);
    std::vector<std::size_t> gone;
    const int gw = bdelloGrid_.cellsX(), gh = bdelloGrid_.cellsY();
    const float cw = width_ / static_cast<float>(gw), ch = height_ / static_cast<float>(gh);
    const auto& order = bdelloGrid_.order();
    for (std::size_t i = 0; i < bacteria_.size(); ++i) {
        if (bacteria_.bdello[i] >= 0.0f || bacteria_.phage[i] >= 0.0f)
            continue;
        const int cx = static_cast<int>(bacteria_.x[i] / cw), cy = static_cast<int>(bacteria_.y[i] / ch);
        bool caught = false;
        for (int oy = -1; oy <= 1 && !caught; ++oy)
            for (int ox = -1; ox <= 1 && !caught; ++ox)
                for (std::uint32_t it = bdelloGrid_.cellBegin(cx + ox, cy + oy); it < bdelloGrid_.cellEnd(cx + ox, cy + oy); ++it) {
                    const std::uint32_t b = order[it];
                    if (used[b])
                        continue;
                    const float dx = minImage(bdello_.x[b] - bacteria_.x[i], width_);
                    const float dy = minImage(bdello_.y[b] - bacteria_.y[i], height_);
                    if (dx * dx + dy * dy < 2.25f) {
                        used[b] = 1;
                        gone.push_back(b);
                        bacteria_.bdello[i] = 0.0f;
                        caught = true;
                        break;
                    }
                }
        (void)gh;
    }
    std::sort(gone.rbegin(), gone.rend());
    for (std::size_t b : gone)
        bdello_.swapRemove(b);
}
