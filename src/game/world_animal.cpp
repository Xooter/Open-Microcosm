// Células de organismos pluricelulares en cultivo (fase 20b): migración por arrastre, ciclo celular
// por fases con su punto de restricción, inhibición por contacto, metabolismo de glucosa y
// glutamina con secreción de lactato, apoptosis y quimiotaxis del neutrófilo. Todo en tiempo
// biológico (World::bioScale): la migración es polimerización de actina, no física del agua.
#include "game/world.hpp"

#include "bio/animal_cell.hpp"
#include "bio/division.hpp"
#include "bio/physiology.hpp"
#include "core/profiling.hpp"
#include "sim/polygon.hpp"

#include <algorithm>
#include <cmath>

const char* cellPhaseName(CellPhase phase) {
    switch (phase) {
    case CellPhase::G0: return "G0 (quiescente)";
    case CellPhase::G1: return "G1";
    case CellPhase::S: return "S (replica el ADN)";
    case CellPhase::G2: return "G2";
    case CellPhase::M: return "M (mitosis)";
    case CellPhase::Apoptotic: return "apoptosis";
    }
    return "";
}

namespace {

// Las células animales siempre tienen especie (como en game/world.cpp).
SpeciesId speciesOfEntity(const entt::registry& reg, entt::entity e) {
    if (const Species* sp = reg.try_get<Species>(e))
        return sp->id;
    return playerSpecies(reg.get<Locomotion>(e).type);
}

Vec2 unit(Vec2 v) {
    const float l = length(v);
    return l > 1e-6f ? v * (1.0f / l) : Vec2{};
}

// Sin dato de la especie: el ciclo de una célula humana típica que prolifera en cultivo (Alberts et
// al., Molecular Biology of the Cell, 6ª ed., fig. 17-4: G1 11 h, S 8 h, G2 4 h, M 1 h).
float phaseHoursOf(const SpeciesPhysiology& p, CellPhase phase) {
    auto pick = [](const SourcedValue& v, float fallback) { return v.present ? v.value : fallback; };
    switch (phase) {
    case CellPhase::G1: return pick(p.phaseG1, 11.0f);
    case CellPhase::S: return pick(p.phaseS, 8.0f);
    case CellPhase::G2: return pick(p.phaseG2, 4.0f);
    case CellPhase::M: return pick(p.phaseM, 1.0f);
    default: return 1.0f;
    }
}

// Duración de la apoptosis, del encogimiento a los cuerpos apoptóticos: ~1–3 h en cultivo (la
// fase de ejecución, con burbujeo de la membrana; Elmore 2007, Toxicol. Pathol. 35:495).
constexpr float kApoptosisHours = 2.0f;
// Sin energía (sin glucosa ni reservas) durante este tiempo, la célula entra en apoptosis.
constexpr float kStarvationApoptosisHours = 4.0f;
// Fracción del contorno ocupada por vecinas que detiene la proliferación en el punto de
// restricción (inhibición por contacto: con la periferia casi rodeada, la señal de Hippo/YAP
// deja la célula en G0; simplificación del efecto medido, no de la vía).
constexpr float kContactArrest = 0.5f;
// Constante de disociación del receptor de alta afinidad de fMLP (FPR1) del neutrófilo: ~1 nM.
constexpr float kFmlpKd = 1e-3f; // [µM]

} // namespace

bool World::isAnimalCell(entt::entity e) const { return registry_.all_of<AnimalCell>(e); }

void World::setupAnimalCell(entt::entity e, Rng& rng) {
    const SpeciesInfo& info = speciesInfo(speciesOfEntity(registry_, e));
    AnimalCell ac;
    ac.polarity = rng.uniform(0.0f, 2.0f * units::kPi);
    // Asincrónicas: cada una en un punto distinto de G1 (si no, todas se dividirían juntas).
    ac.phaseHours = rng.uniform(0.0f, phaseHoursOf(speciesPhysiology(speciesOfEntity(registry_, e)), CellPhase::G1));
    if (!info.animalCell.cycles)
        ac.phase = CellPhase::G0;
    registry_.emplace_or_replace<AnimalCell>(e, ac);
    if (info.animalCell.adherent)
        registry_.emplace_or_replace<Adherent>(e);
    cultureFields_ = true; // glutamina, lactato y fMLP empiezan a difundir
}

float World::crawlSpeed(entt::entity e) const {
    const SpeciesPhysiology& p = speciesPhysiology(speciesOfEntity(registry_, e));
    // [µm/h a 20 °C] → µm por segundo real: la migración corre en tiempo biológico.
    return p.migrationSpeed.present ? p.migrationSpeed.value / 3600.0f * bioScale() : 0.0f;
}

void World::steerCrawler(entt::entity e, const RingBody& body, Locomotion& loco, float dt, bool player) {
    AnimalCell* ac = registry_.try_get<AnimalCell>(e);
    if (!ac)
        return;
    // La célula en mitosis se redondea y deja de migrar; en apoptosis, también.
    const bool still = ac->phase == CellPhase::M || ac->phase == CellPhase::Apoptotic;
    loco.speed = still ? 0.0f : crawlSpeed(e);
    if (player) { // controlada: el frente va hacia donde apunta el usuario
        loco.active = loco.active && !still;
        return;
    }
    const SpeciesId sid = speciesOfEntity(registry_, e);
    const SpeciesPhysiology& phys = speciesPhysiology(sid);
    const AnimalCellInfo& info = speciesInfo(sid).animalCell;
    const float bioDt = dt * bioScale();
    Rng rng(config_.seed, rng_stream::kAutopilot + 50, body.id, step_);
    // Paseo aleatorio persistente: la dirección del frente difunde con D_r = 1/P (P: tiempo de
    // persistencia medido). Su desplazamiento cuadrático medio es el de Fürth–Ornstein:
    // <d²> = 2 S² P [t − P (1 − e^(−t/P))] (Gail & Boone 1970).
    const float persistenceS = (phys.persistence.present ? phys.persistence.value : 1.0f) * 3600.0f;
    ac->polarity += std::sqrt(2.0f * bioDt / persistenceS) * rng.normal();
    // Inhibición por contacto de la locomoción (Abercrombie y Heaysman 1954): al tocar a otra
    // célula, el lamelipodio de ese lado se retrae y la célula se repolariza hacia el lado libre.
    if (info.contactLocomotion && ac->contact > 0.05f && length(ac->contactDirection) > 0.1f) {
        if (!ac->repolarized) {
            const Vec2 away = ac->contactDirection * -1.0f;
            ac->polarity = std::atan2(away.y, away.x) + 0.6f * rng.normal();
            ac->repolarized = true;
        }
    } else {
        ac->repolarized = false;
    }
    // Quimiotaxis del neutrófilo: compara la concentración de fMLP entre el frente y la cola; con
    // su receptor (FPR1) lo bastante ocupado, detecta ~1 % de diferencia a lo largo del cuerpo
    // (Zigmond 1977, J. Cell Biol. 75:606).
    if (info.chemotaxisFmlp && cultureFields_) {
        const Vec2 h = {std::cos(ac->polarity), std::sin(ac->polarity)};
        const float half = 0.5f * std::max(body.aabbMax.x - body.aabbMin.x, body.aabbMax.y - body.aabbMin.y);
        auto conc = [&](Vec2 p) { return std::max(0.0f, fmlp_.grid().sample(p.x, p.y)); };
        const Vec2 side = {-h.y, h.x};
        const float mean = conc(body.centroid);
        const float dx = conc(body.centroid + h * half) - conc(body.centroid - h * half);
        const float dy = conc(body.centroid + side * half) - conc(body.centroid - side * half);
        const Vec2 grad = {dx * h.x + dy * side.x, dx * h.y + dy * side.y};
        const float g = length(grad);
        // Con menos de ~1 % de los receptores ocupados (c < Kd/100) no hay señal que leer.
        if (mean > 0.01f * kFmlpKd && g > 0.01f * mean) { // sobre el umbral: gira hacia el gradiente
            const float target = std::atan2(grad.y, grad.x);
            const float delta = std::remainder(target - ac->polarity, 2.0f * units::kPi);
            ac->polarity += delta * std::min(1.0f, bioDt / 60.0f); // se reorienta en ~1 min
        }
    }
    const Vec2 dir = {std::cos(ac->polarity), std::sin(ac->polarity)};
    loco.target = body.centroid + dir * 100.0f;
    loco.active = !still && loco.speed > 0.0f;
}

void World::measureAnimalContacts() {
    PROFILE_SCOPE("World::measureAnimalContacts");
    // Qué parte del contorno de cada célula animal toca a otra, y hacia dónde. Escribe solo en su
    // célula: en paralelo. Se mide cada tantos pasos (cambia en minutos biológicos).
    animalList_.clear();
    for (auto [e, ac, body] : registry_.view<AnimalCell, RingBody>().each())
        animalList_.push_back(e);
    allBodies_.clear();
    for (auto [e, body] : registry_.view<RingBody>().each())
        allBodies_.push_back(&body);
    const auto n = static_cast<std::ptrdiff_t>(animalList_.size());
#pragma omp parallel for schedule(dynamic, 4)
    for (std::ptrdiff_t i = 0; i < n; ++i) {
        const entt::entity e = animalList_[static_cast<std::size_t>(i)];
        const RingBody& body = registry_.get<RingBody>(e);
        AnimalCell& ac = registry_.get<AnimalCell>(e);
        constexpr float kRange = 3.0f; // [µm] membranas a esta distancia se tocan
        int touching = 0;
        Vec2 toward;
        for (const RingBody* other : allBodies_) {
            if (other == &body || registry_.all_of<Rock>(static_cast<entt::entity>(other->id)))
                continue;
            const Vec2 shift = imageShift(*other, body.centroid);
            if (other->aabbMax.x + shift.x < body.aabbMin.x - kRange || other->aabbMin.x + shift.x > body.aabbMax.x + kRange ||
                other->aabbMax.y + shift.y < body.aabbMin.y - kRange || other->aabbMin.y + shift.y > body.aabbMax.y + kRange)
                continue;
            const int pn = static_cast<int>(other->poly.size());
            for (const Vec2& p : body.poly) {
                const Vec2 q = p - shift;
                if (polygon::contains(other->poly.data(), pn, q) ||
                    polygon::closestOnBoundary(other->poly.data(), pn, q).dist2 < kRange * kRange) {
                    ++touching;
                    toward = toward + (p - body.centroid);
                }
            }
        }
        ac.contact = body.poly.empty() ? 0.0f : std::min(1.0f, static_cast<float>(touching) / static_cast<float>(body.poly.size()));
        ac.contactDirection = touching > 0 && length(toward) > 1e-3f ? unit(toward) : Vec2{};
    }
}

float World::animalUptake(entt::entity e, const RingBody& body, const Metabolism& meta, float bioDt) {
    // Captación de glucosa (GLUT1) y glutamina (ASCT2) con Michaelis–Menten, con las Vmax y Km
    // de la especie; la glucólisis aeróbica (efecto Warburg) secreta lactato, que se lleva el
    // bicarbonato del medio y lo acidifica.
    //
    // Simplificación: la masa de una célula animal no sale de la glucosa (la proteína, ~60 % de la
    // masa seca, se arma con los aminoácidos del medio; los lípidos vienen en buena parte del
    // suero). Lo que entra al metabolismo es lo que pide crecer a su ritmo máximo y mantenerse,
    // escalado por cuánto consigue de su fuente de energía (glucosa y glutamina respecto de la
    // mitad de su Vmax): sin glucosa no hay energía y pasa hambre. Devuelve [pmol eq. de glucosa].
    const SpeciesId sid = speciesOfEntity(registry_, e);
    const SpeciesPhysiology& p = speciesPhysiology(sid);
    if (!p.glucoseVmax.present)
        return 0.0f;
    const Vec2 c = body.centroid;
    const float hours = bioDt / 3600.0f;
    const float glucose = std::max(0.0f, nutrients_.grid().sample(c.x, c.y));
    const float km = p.glucoseKm.present ? p.glucoseKm.value : 3000.0f;
    const float wantGlucose = p.glucoseVmax.value * 1e-3f * hours * glucose / (glucose + km); // fmol → pmol
    const float gotGlucose = wantGlucose > 0.0f ? -exchange(nutrients_, c, -wantGlucose, Flow::Uptake) : 0.0f;
    float energy = gotGlucose;
    if (p.lactatePerGlucose.present && gotGlucose > 0.0f) // cada glucosa a lactato da 2 lactatos
        exchange(lactate_, c, p.lactatePerGlucose.value * gotGlucose);
    if (p.glutamineVmax.present) {
        const float gln = std::max(0.0f, glutamine_.grid().sample(c.x, c.y));
        const float gkm = p.glutamineKm.present ? p.glutamineKm.value : 100.0f;
        const float want = p.glutamineVmax.value * 1e-3f * hours * gln / (gln + gkm);
        const float got = want > 0.0f ? -exchange(glutamine_, c, -want) : 0.0f;
        // Glutaminólisis: la glutamina entra al ciclo de Krebs (5 C ≈ 5/6 de una glucosa).
        energy += got * 5.0f / 6.0f;
    }
    const float fullEnergy = 0.5f * p.glucoseVmax.value * 1e-3f * hours;
    const float fed = fullEnergy > 0.0f ? std::clamp(energy / fullEnergy, 0.0f, 1.0f) : 0.0f;
    // Lo que pide: crecer a su tasa máxima (rendimiento 0,6) y su mantenimiento (~1 %/h).
    const float mu = p.growthMax.present ? p.growthMax.value : 0.03f;
    const float demand = meta.biomass * (mu / 0.6f + metabolismParams_.maintenancePerHour) * hours * 1.2f;
    return fed * demand;
}

float World::animalTargetArea(entt::entity e, const AnimalCell& ac, float spreadArea) const {
    const AnimalCellInfo& info = speciesInfo(speciesOfEntity(registry_, e)).animalCell;
    const float round = units::kPi * info.roundRadius * info.roundRadius;
    if (ac.phase == CellPhase::M) // se despega a medias y se redondea (aumenta su volumen, no su área)
        return round;
    if (ac.phase == CellPhase::Apoptotic) { // se encoge (pierde agua) y se redondea
        const float shrink = std::max(0.4f, 1.0f - 0.6f * ac.apoptosis);
        return std::min(spreadArea, round) * shrink;
    }
    return spreadArea;
}

bool World::updateAnimalCell(entt::entity e, RingBody& body, Metabolism& meta, float bioDt) {
    AnimalCell* acp = registry_.try_get<AnimalCell>(e);
    if (!acp)
        return false;
    AnimalCell& ac = *acp;
    const SpeciesId sid = speciesOfEntity(registry_, e);
    const SpeciesInfo& info = speciesInfo(sid);
    const SpeciesPhysiology& phys = speciesPhysiology(sid);
    const float hours = bioDt / 3600.0f;
    ac.ageHours += hours;

    if (ac.phase == CellPhase::Apoptotic) {
        ac.apoptosis += hours / kApoptosisHours;
        if (ac.apoptosis >= 1.0f)
            apoptosisDone_.push_back(e);
        return true;
    }
    // Disparadores de la apoptosis: sin energía, sin factores de crecimiento (si la célula los
    // necesita para sobrevivir) o por edad (el neutrófilo vive horas: muere solo, sin inflamar).
    ac.starvedHours = meta.starving ? ac.starvedHours + hours : 0.0f;
    const bool serum = medium_.serum >= info.animalCell.serumMinimum;
    ac.serumlessHours = serum ? 0.0f : ac.serumlessHours + hours;
    bool apoptosis = ac.starvedHours > kStarvationApoptosisHours ||
                     (info.animalCell.apoptosisWithoutSerumHours > 0.0f &&
                      ac.serumlessHours > info.animalCell.apoptosisWithoutSerumHours);
    if (phys.lifespan.present) { // muerte por edad: riesgo constante con la vida media publicada
        Rng rng(config_.seed, rng_stream::kCells + 60, body.id, step_);
        apoptosis = apoptosis || rng.uniform() < 1.0f - std::exp(-std::log(2.0f) * hours / phys.lifespan.value);
    }
    if (apoptosis && !registry_.all_of<Dividing>(e)) {
        ac.phase = CellPhase::Apoptotic;
        ac.phaseHours = ac.apoptosis = 0.0f;
        registry_.remove<Adherent>(e); // se despega
        const float r = std::sqrt(std::abs(body.restArea) / units::kPi);
        soft_body::reshapeRest(body, bodies_, r, r);
        body.shapeRate = 8.0f;
        ++observed_.apoptoses;
        return true;
    }
    if (!info.animalCell.cycles)
        return true;

    // Ciclo celular por fases, en tiempo biológico (con Q10).
    ac.phaseHours += hours;
    switch (ac.phase) {
    case CellPhase::G0:
    case CellPhase::G1: {
        // Punto de restricción (Pardee 1974): pasar a S exige factores de crecimiento del suero,
        // energía y, si la célula tiene inhibición por contacto, la periferia libre.
        const bool free = !info.animalCell.contactProliferation || ac.contact < kContactArrest;
        const bool ready = serum && free && !meta.starving;
        if (ac.phase == CellPhase::G0) {
            if (ready) { // vuelve al ciclo: recorre G1 entero
                ac.phase = CellPhase::G1;
                ac.phaseHours = 0.0f;
            }
        } else if (ac.phaseHours >= phaseHoursOf(phys, CellPhase::G1)) {
            if (ready) {
                ac.phase = CellPhase::S;
                ac.phaseHours = 0.0f;
            } else {
                ac.phase = CellPhase::G0; // sale del ciclo: quiescente
                ac.phaseHours = 0.0f;
            }
        }
        break;
    }
    case CellPhase::S:
    case CellPhase::G2:
        // Comprometida: S y G2 siguen aunque falte el suero (el punto de restricción ya pasó).
        if (ac.phaseHours >= phaseHoursOf(phys, ac.phase)) {
            ac.phase = ac.phase == CellPhase::S ? CellPhase::G2 : CellPhase::M;
            ac.phaseHours = 0.0f;
            if (ac.phase == CellPhase::M) {
                // Mitosis: la célula se redondea (retrae las adhesiones) y el surco la parte.
                const float r = info.animalCell.roundRadius;
                soft_body::reshapeRest(body, bodies_, r, r);
                body.shapeRate = 8.0f;
                Dividing& d = registry_.emplace_or_replace<Dividing>(e);
                d.motherRest = body.restRing;
                d.motherArea = units::kPi * r * r;
            }
        }
        break;
    case CellPhase::M: {
        Dividing* dv = registry_.try_get<Dividing>(e);
        if (!dv)
            break;
        dv->progress += hours / phaseHoursOf(phys, CellPhase::M);
        // Citocinesis: la forma de reposo pasa de la célula redonda a dos hijas.
        const float t = std::clamp((dv->progress - 0.5f) / 0.5f, 0.0f, 1.0f);
        if (dv->motherArea > 0.0f) {
            const float scale = std::sqrt(std::abs(body.restArea) / dv->motherArea);
            divisionRestShape(dv->motherRest, scale, t * t * (3.0f - 2.0f * t), body.restRing);
            const std::size_t rn = body.restRing.size();
            for (std::size_t i = 0; i < rn; ++i)
                body.restEdge[i] = length(body.restRing[(i + 1) % rn] - body.restRing[i]);
        }
        if (dv->progress > 0.3f && dv->nucleus2 == ~0u)
            duplicateOrganelles(e);
        std::size_t ta, tb;
        if (dv->progress > 0.6f && dv->nucleus2 != ~0u && furrowClosed(body, bodies_, ta, tb))
            splits_.push_back({e, ta, tb});
        break;
    }
    case CellPhase::Apoptotic: break;
    }
    return true;
}

void World::afterAnimalDivision(entt::entity mother, entt::entity child) {
    // Las dos hijas empiezan G1 y se vuelven a extender (se re-adhieren al fondo).
    AnimalCell& m = registry_.get<AnimalCell>(mother);
    m.phase = CellPhase::G1;
    m.phaseHours = 0.0f;
    AnimalCell c = m;
    c.polarity = m.polarity + units::kPi; // salen en direcciones opuestas
    c.contact = 0.0f;
    c.contactDirection = {};
    c.ageHours = 0.0f;
    registry_.emplace_or_replace<AnimalCell>(child, c);
    if (registry_.all_of<Adherent>(mother))
        registry_.emplace_or_replace<Adherent>(child);
    for (entt::entity e : {mother, child}) {
        if (!registry_.all_of<CellShape, RingBody>(e))
            continue;
        const CellShape& shape = registry_.get<CellShape>(e);
        RingBody& b = registry_.get<RingBody>(e);
        const float k = std::sqrt(std::abs(b.restArea) / (units::kPi * shape.radiusX * shape.radiusY));
        soft_body::reshapeRest(b, bodies_, shape.radiusX * k, shape.radiusY * k, shape.squareness, shape.taper);
        b.shapeRate = shape.shapeRate;
    }
}

void World::releaseFmlp(float dt) {
    // Las bacterias liberan péptidos formilados (fMLP: sus proteínas empiezan con
    // N-formilmetionina): la señal que guía a los neutrófilos hacia una infección. Tasa por bacteria:
    // estimación (sin medición directa; da concentraciones de nM junto a una colonia, el rango en que
    // responde el receptor).
    if (!cultureFields_ || bacteria_.size() == 0)
        return;
    // Marasco et al. (1984, J. Biol. Chem. 259:5430) aislaron el fMLP como el principal
    // quimiotáctico del sobrenadante de E. coli, a concentraciones de nM con ~10⁹ bacterias/mL:
    // ~5·10⁻⁶ fmol por bacteria y por hora (estimación a partir de ese orden de magnitud).
    constexpr float kFmlpPerBacteriumPerHour = 5e-6f; // [fmol/h]
    const float pmolEach = kFmlpPerBacteriumPerHour * 1e-3f * dt * bioScale() / 3600.0f;
    Grid2D& g = fmlp_.grid();
    // Simplificación: el resto de la química se toma bien mezclado en toda la profundidad; una fuente
    // en el fondo, en cambio, solo llena una capa del orden de la distancia lateral que importa a la
    // célula (~200 µm): en la película de una gota es lo mismo; en 2,5 mm de medio, no.
    const float layer = std::min(fluid_.gapHeight, 200.0f);
    const float perCell = pmolEach * microMolarPerPmol() * fluid_.gapHeight / layer;
    for (std::size_t i = 0; i < bacteria_.size(); ++i)
        g.at(g.cellX(bacteria_.x[i]), g.cellY(bacteria_.y[i])) += perCell;
    // Se degrada (peptidasas, adsorción): vida media de horas (estimación).
    const float keep = std::exp(-dt * bioScale() / (6.0f * 3600.0f));
    for (float& v : g.v)
        v *= keep;
}
