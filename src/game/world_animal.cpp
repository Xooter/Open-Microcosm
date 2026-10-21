// Células de organismos pluricelulares en cultivo (fase 20b): migración por arrastre, ciclo celular
// por fases con su punto de restricción, inhibición por contacto, metabolismo de glucosa y
// glutamina con secreción de lactato, apoptosis y quimiotaxis del neutrófilo. Todo en tiempo
// biológico (World::bioScale): la migración es polimerización de actina, no física del agua.
#include "game/world.hpp"

#include "bio/animal_cell.hpp"
#include "bio/division.hpp"
#include "bio/osmosis.hpp"
#include "bio/physiology.hpp"
#include "core/finite.hpp"
#include "game/reagent.hpp"
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
    case CellPhase::Dead: return "muerta";
    }
    return "";
}

const char* motilityGradeName(MotilityGrade grade) {
    switch (grade) {
    case MotilityGrade::Progressive: return "progresivo";
    case MotilityGrade::NonProgressive: return "no progresivo";
    case MotilityGrade::Immotile: return "inmovil";
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

// Una célula sembrada se adhiere y se extiende en ~30–60 min (Döbereiner et al. 2004, Phys. Rev.
// Lett. 93:108105: fibroblastos sobre fibronectina).
constexpr float kSpreadingHours = 1.0f;
// La célula que entra en mitosis se redondea en ~10–20 min (prometafase; Cramer y Mitchison 1997,
// Mol. Biol. Cell 8:109).
constexpr float kRoundingHours = 0.25f;

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
    // Llega redonda (de la suspensión de la siembra; spawnOrganism la crea así) y se extiende.
    if (info.animalCell.adherent)
        ac.spread = 0.0f;
    ac.polarity = rng.uniform(0.0f, 2.0f * units::kPi);
    // Asincrónicas, como las que vienen de un cultivo que crece en forma exponencial: la edad dentro
    // del ciclo sigue f(a) = 2·ln2/T · 2^(−a/T) (hay el doble de recién nacidas que de las que van a
    // dividirse) y la masa crece con la edad.
    if (info.animalCell.cycles) {
        const SpeciesPhysiology& phys = speciesPhysiology(speciesOfEntity(registry_, e));
        const CellPhase order[] = {CellPhase::G1, CellPhase::S, CellPhase::G2};
        float cycle = phaseHoursOf(phys, CellPhase::M);
        for (CellPhase ph : order)
            cycle += phaseHoursOf(phys, ph);
        const float age = -cycle * std::log2(1.0f - 0.5f * rng.uniform());
        float left = std::min(age, cycle - phaseHoursOf(phys, CellPhase::M) - 1e-3f); // no empieza a mitad de la mitosis
        for (CellPhase ph : order) {
            ac.phase = ph;
            ac.phaseHours = left;
            if (left < phaseHoursOf(phys, ph))
                break;
            left -= phaseHoursOf(phys, ph);
        }
        if (Metabolism* m = registry_.try_get<Metabolism>(e))
            m->biomass *= std::exp2(age / cycle);
    } else {
        ac.phase = CellPhase::G0;
    }
    // Una muestra de nadadores por clases (espermiograma): cada uno sale con su clase y su rapidez
    // propia. Los muertos de la muestra ya llegan muertos (enteros, inmóviles).
    if (info.motility.present) {
        const MotilityMix& mix = config_.motilitySet ? config_.motility : info.motility.mix;
        const float total = std::max(1e-6f, mix.progressive + mix.nonProgressive + mix.immotile + mix.dead);
        const float u = rng.uniform() * total;
        if (u < mix.progressive)
            ac.grade = MotilityGrade::Progressive;
        else if (u < mix.progressive + mix.nonProgressive)
            ac.grade = MotilityGrade::NonProgressive;
        else if (u < mix.progressive + mix.nonProgressive + mix.immotile)
            ac.grade = MotilityGrade::Immotile;
        else {
            ac.grade = MotilityGrade::Immotile;
            ac.phase = CellPhase::Dead;
        }
        ac.vigor = std::max(0.2f, 1.0f + info.motility.speedCv * rng.normal());
    }
    // Morfología: la forma de cada uno (criterios estrictos). La muestra puede tener otra fracción de
    // formas normales (WorldConfig::normalForms): las anormales conservan sus proporciones.
    if (!info.morphotypes.empty()) {
        float normal = 0.0f, total = 0.0f;
        for (const MorphotypeSpec& m : info.morphotypes) {
            total += m.fraction;
            normal += m.normal ? m.fraction : 0.0f;
        }
        const float wantNormal = config_.normalForms >= 0.0f ? std::min(1.0f, config_.normalForms) : normal / std::max(1e-6f, total);
        const float abnormalScale = total - normal > 1e-6f ? (1.0f - wantNormal) / (total - normal) : 0.0f;
        const float normalScale = normal > 1e-6f ? wantNormal / normal : 0.0f;
        float u = rng.uniform();
        for (std::size_t k = 0; k < info.morphotypes.size(); ++k) {
            const MorphotypeSpec& m = info.morphotypes[k];
            u -= m.fraction * (m.normal ? normalScale : abnormalScale);
            if (u < 0.0f || k + 1 == info.morphotypes.size()) {
                ac.morphotype = static_cast<std::uint8_t>(k);
                break;
            }
        }
        const MorphotypeSpec& m = info.morphotypes[ac.morphotype];
        if (CellShape* shape = registry_.try_get<CellShape>(e); shape && (m.lengthScale != 1.0f || m.widthScale != 1.0f || m.taper >= 0.0f)) {
            shape->radiusX *= m.lengthScale;
            shape->radiusY *= m.widthScale;
            if (m.taper >= 0.0f)
                shape->taper.back = m.taper;
            if (RingBody* body = registry_.try_get<RingBody>(e))
                soft_body::reshapeRestEven(*body, bodies_, shape->radiusX, shape->radiusY, shape->squareness, shape->taper);
            // El área de reposo sigue a la biomasa: una cabeza más chica o más grande tiene menos o más masa.
            if (Metabolism* meta = registry_.try_get<Metabolism>(e)) {
                const float k = m.lengthScale * m.widthScale;
                meta->biomass *= k;
                meta->birthBiomass *= k;
                meta->reserves *= k;
            }
        }
        if (m.speedScale != 1.0f)
            ac.vigor *= m.speedScale;
    }
    registry_.emplace_or_replace<AnimalCell>(e, ac);
    if (info.animalCell.adherent)
        registry_.emplace_or_replace<Adherent>(e);
    if (info.motility.present)
        applyTraits(e); // la rapidez propia y la de los no progresivos
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
    // Un cuerpo que la red de seguridad está rearmando no tiene dirección que seguir.
    if (!finiteNumber(body.centroid.x) || !finiteNumber(body.centroid.y)) {
        loco.active = false;
        return;
    }
    if (!finiteNumber(ac->polarity))
        ac->polarity = rng.uniform(0.0f, 2.0f * units::kPi);
    // Paseo aleatorio persistente: la dirección del frente difunde con D_r = 1/P (P: tiempo de
    // persistencia medido). Su desplazamiento cuadrático medio es el de Fürth–Ornstein:
    // <d²> = 2 S² P [t − P (1 − e^(−t/P))] (Gail & Boone 1970).
    const float persistenceS = (phys.persistence.present ? phys.persistence.value : 1.0f) * 3600.0f;
    ac->polarity += std::sqrt(2.0f * bioDt / persistenceS) * rng.normal();
    // Inhibición por contacto de la locomoción (Abercrombie y Heaysman 1954): al tocar a otra
    // célula, el lamelipodio de ese lado se retrae y la célula se repolariza hacia el lado libre.
    if (info.contactLocomotion && ac->contact > 0.05f && length(ac->contactDirection) > 0.1f) {
        const Vec2 away = ac->contactDirection * -1.0f;
        const float target = std::atan2(away.y, away.x);
        if (!ac->repolarized) { // el choque: el frente se retrae y la célula se da vuelta
            ac->polarity = target + 0.6f * rng.normal();
            ac->repolarized = true;
        } else { // mientras siga tocando, el lamelipodio crece solo del lado libre
            const float delta = std::remainder(target - ac->polarity, 2.0f * units::kPi);
            ac->polarity += delta * std::min(1.0f, bioDt / 600.0f);
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
    // En una monocapa densa la migración casi se detiene: sin borde libre no hay lamelipodio que
    // avance (Abercrombie y Heaysman 1954: la rapidez cae con los contactos).
    if (info.contactLocomotion) // sin borde libre (rodeada del todo) ya no avanza
        loco.speed *= std::clamp((1.0f - ac->contact) / 0.3f, 0.0f, 1.0f);
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
        if (!finiteNumber(body.centroid.x) || !finiteNumber(body.centroid.y)) {
            ac.contact = 0.0f;
            ac.contactDirection = {};
            continue;
        }
        for (const RingBody* other : allBodies_) {
            if (other == &body || registry_.all_of<Rock>(static_cast<entt::entity>(other->id)) ||
                !finiteNumber(other->centroid.x) || !finiteNumber(other->centroid.y))
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
    // Lo que pide: crecer a su tasa máxima (rendimiento 0,6) y su mantenimiento (~1 %/h). Quiescente
    // (G0) o muriendo, no crece: solo se mantiene.
    const AnimalCell* ac = registry_.try_get<AnimalCell>(e);
    const bool growing = ac && ac->phase != CellPhase::G0 && ac->phase != CellPhase::Apoptotic;
    const float mu = !growing ? 0.0f : p.growthMax.present ? p.growthMax.value : 0.03f;
    // La energía, con la misma cuenta que el metabolismo (bio/metabolism): mantenimiento, el gasto de
    // migrar y, si falta O₂, la glucólisis anaerobia, que pide mucha más glucosa.
    const MetabolismParams& mp = metabolismParams_;
    const float o2 = std::max(0.0f, oxygen_.grid().sample(c.x, c.y));
    const float aerobic = o2 / (o2 + mp.o2HalfSaturation);
    const Locomotion* loco = registry_.try_get<Locomotion>(e);
    const float activity = loco && loco->active ? 1.0f : 0.0f;
    const float maintenance = mp.maintenancePerHour * meta.biomass * (1.0f + mp.motilityCost * activity) *
                              (aerobic + (1.0f - aerobic) * mp.fermentationPenalty);
    const float demand = (meta.biomass * mu / 0.6f + maintenance) * hours * 1.1f;
    return fed * demand;
}

float World::animalTargetArea(entt::entity e, const AnimalCell& ac, float spreadArea) const {
    const AnimalCellInfo& info = speciesInfo(speciesOfEntity(registry_, e)).animalCell;
    const float round = units::kPi * info.roundRadius * info.roundRadius;
    // Extendida o redondeada (mitosis, apoptosis, recién sembrada): el área sigue a la forma.
    const float area = ac.phase == CellPhase::M || ac.phase == CellPhase::Apoptotic || ac.spread < 1.0f
                           ? round + (spreadArea - round) * ac.spread
                           : spreadArea;
    if (ac.phase == CellPhase::Apoptotic) // además se encoge (pierde agua)
        return area * std::max(0.4f, 1.0f - 0.6f * ac.apoptosis);
    return area;
}

bool World::updateAnimalCell(entt::entity e, RingBody& body, Metabolism& meta, float bioDt) {
    AnimalCell* acp = registry_.try_get<AnimalCell>(e);
    if (!acp)
        return false;
    AnimalCell& ac = *acp;
    if (ac.phase == CellPhase::Dead)
        return true; // con la membrana rota ya no se hincha (por eso la prueba hiposmótica la separa)
    const SpeciesId sid = speciesOfEntity(registry_, e);
    const SpeciesInfo& info = speciesInfo(sid);
    if (const OsmoticSwellingSpec& os = info.osmoticSwelling; os.present) {
        // La cola se hincha con el agua que entra (Boyle–van 't Hoff) y se enrolla; física, en tiempo real.
        const float osm = osmosis::mediumOsmolarity(medium_.salinity, medium_.osmolytes);
        const float v = osmosis::equilibriumVolume(os.isotonic, osm, os.inactive);
        const float full = osmosis::equilibriumVolume(os.isotonic, os.fullOsmolarity, os.inactive);
        const float target = std::clamp((v - 1.0f) / std::max(1e-3f, full - 1.0f), 0.0f, 1.0f);
        const float realDt = bioDt / bioScale();
        ac.swelling += (target - ac.swelling) * (1.0f - std::exp(-realDt / os.seconds));
    }
    const SpeciesPhysiology& phys = speciesPhysiology(sid);
    const float hours = bioDt / 3600.0f;
    ac.ageHours += hours;

    // Forma: se extiende sobre el fondo (recién sembrada o recién dividida) y se redondea para la
    // mitosis (~15 min) o la apoptosis. El cuerpo blando cambia de forma en tiempo real (física):
    // con el tiempo biológico muy comprimido no se le pide hacerlo en menos de ~1 s real (el
    // anillo se enredaría).
    const bool roundUp = ac.phase == CellPhase::M || ac.phase == CellPhase::Apoptotic;
    const float wanted = roundUp ? 0.0f : 1.0f;
    if (ac.spread != wanted && !registry_.all_of<Dividing>(e)) {
        const float realDt = bioDt / bioScale();
        const float tau = ac.phase == CellPhase::M ? kRoundingHours : ac.phase == CellPhase::Apoptotic ? 0.5f : kSpreadingHours;
        const float stepBy = std::min(hours / tau, realDt / 1.0f);
        ac.spread = wanted > ac.spread ? std::min(wanted, ac.spread + stepBy) : std::max(wanted, ac.spread - stepBy);
        body.shapeRate = roundUp ? 8.0f : body.shapeRate;
        if (const CellShape* shape = registry_.try_get<CellShape>(e)) {
            const float r = info.animalCell.roundRadius;
            const float k = std::sqrt(std::abs(body.restArea) / (units::kPi * shape->radiusX * shape->radiusY));
            const float t = ac.spread * ac.spread * (3.0f - 2.0f * ac.spread);
            // Por longitud de arco: de redonda a huso (o al revés) la proyección radial amontona los
            // puntos en los lados y la forma de reposo se cruza (la célula se empujaba sola).
            soft_body::reshapeRestEven(body, bodies_, r + (shape->radiusX * k - r) * t, r + (shape->radiusY * k - r) * t,
                                       2.0f + (shape->squareness - 2.0f) * t, Taper{shape->taper.back * t, shape->taper.front * t});
        }
    }
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
    const bool starved = ac.starvedHours > kStarvationApoptosisHours;
    bool apoptosis = starved || (info.animalCell.apoptosisWithoutSerumHours > 0.0f &&
                                 ac.serumlessHours > info.animalCell.apoptosisWithoutSerumHours);
    if (phys.lifespan.present) { // muerte por edad: riesgo constante con la vida media publicada
        Rng rng(config_.seed, rng_stream::kCells + 60, body.id, step_);
        apoptosis = apoptosis || rng.uniform() < 1.0f - std::exp(-std::log(2.0f) * hours / phys.lifespan.value);
    }
    // Capacitación: horas en un medio con bicarbonato y albúmina a ~37 °C; se revierte fuera de él (Visconti
    // et al. 2011). Capacitado, una fracción se hiperactiva y puede hacer la reacción acrosómica.
    if (const CapacitationSpec& cp = info.capacitation; cp.present && phys.capacitationTime.present) {
        const bool medium = medium_.alkalinity >= cp.minBicarbonate && medium_.serum >= cp.minAlbumin &&
                            medium_.temperature >= cp.minTemperature;
        const float before = ac.capacitation;
        ac.capacitation = std::clamp(ac.capacitation + (medium ? 1.0f : -1.0f) * hours / phys.capacitationTime.value, 0.0f, 1.0f);
        Rng rng(config_.seed, rng_stream::kCells + 63, body.id, step_);
        if (before < 1.0f && ac.capacitation >= 1.0f && rng.uniform() < cp.hyperactivatedFraction) {
            ac.hyperactive = true;
            applyTraits(e);
        } else if (ac.hyperactive && ac.capacitation < 0.5f) {
            ac.hyperactive = false; // de vuelta en un medio que no capacita
            applyTraits(e);
        }
        if (ac.capacitation >= 1.0f && !ac.acrosomeReacted) {
            float rate = cp.spontaneousReactionPerHour; // [1/h]
            if (!cp.inducer.empty() && reagentIndex_ >= 0 && reagent(reagentIndex_).key == cp.inducer) {
                const float c = std::max(0.0f, reagent_.grid().sample(body.centroid.x, body.centroid.y));
                rate += cp.inducedReactionPerHour * c / (c + cp.inducerEc50);
            }
            if (rng.uniform() < 1.0f - std::exp(-rate * hours))
                ac.acrosomeReacted = true;
        }
    }
    // Los que nadan pierden la motilidad antes que la vida (la vitalidad de una muestra siempre es mayor
    // que su motilidad; OMS 2021): un riesgo constante con la vida media de la motilidad.
    if (phys.motilityHalfLife.present && ac.grade != MotilityGrade::Immotile) {
        Rng rng(config_.seed, rng_stream::kCells + 62, body.id, step_);
        if (rng.uniform() < 1.0f - std::exp(-std::log(2.0f) * hours / phys.motilityHalfLife.value))
            ac.grade = MotilityGrade::Immotile;
    }
    if (apoptosis && info.animalCell.immotileDeath && !registry_.all_of<Dividing>(e)) {
        // El espermatozoide no se parte en cuerpos apoptóticos: deja de batir y queda entero, inmóvil
        // (en un espermiograma se cuenta como inmóvil, y la eosina lo tiñe porque su membrana ya no
        // la excluye). Sin metabolismo: lo que tiene queda en el cuerpo (el balance no cambia).
        const char* cause = starved ? "inanicion" : "edad (pierde la motilidad)";
        ac.phase = CellPhase::Dead;
        ac.grade = MotilityGrade::Immotile;
        ac.phaseHours = 0.0f;
        registry_.remove<Adherent>(e);
        recordDeath(e, cause);
        if (e == player_) {
            registry_.remove<Player>(e);
            player_ = entt::null;
            lastDeathCause_ = cause;
            events_.push_back({WorldEvent::Type::PlayerDied, e, entt::null, cause});
        }
        return true;
    }
    if (apoptosis && !registry_.all_of<Dividing>(e)) {
        ac.phase = CellPhase::Apoptotic;
        ac.phaseHours = ac.apoptosis = 0.0f;
        registry_.remove<Adherent>(e); // se despega (y se redondea de a poco, arriba)
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
            // Mitosis: primero se redondea (retrae las adhesiones, arriba); después el surco la parte.
        }
        break;
    case CellPhase::M: {
        // Catástrofe mitótica: si no logra dividirse (el huso o el surco fallan, por ejemplo
        // apretada entre vecinas), termina muriendo por apoptosis.
        if (ac.phaseHours > 6.0f * phaseHoursOf(phys, CellPhase::M)) {
            registry_.remove<Dividing>(e);
            ac.phase = CellPhase::Apoptotic;
            ac.phaseHours = ac.apoptosis = 0.0f;
            registry_.remove<Adherent>(e);
            ++observed_.apoptoses;
            break;
        }
        Dividing* dv = registry_.try_get<Dividing>(e);
        if (!dv) {
            if (ac.spread > 0.0f)
                break; // todavía redondeándose
            const float r = info.animalCell.roundRadius;
            // Redondeada desde el huso, los puntos de la membrana quedaron amontonados en los
            // costados: se reparten parejos antes del surco (si no, no se estrangula bien).
            // Par: un punto en cada extremo de la cintura, donde el surco cierra.
            soft_body::resampleRing(body, bodies_, std::max<std::size_t>(48, static_cast<std::size_t>(6.2831853f * r / kMembraneSpacing) & ~std::size_t{1}));
            // Círculo por longitud de arco: si el cuerpo todavía no terminó de redondearse, la
            // proyección radial dejaba la forma de reposo plegada y el surco no cerraba.
            soft_body::reshapeRestEven(body, bodies_, r, r, 2.0f, {}, true);
            // La célula mitótica es una esfera tensa (el córtex se contrae al redondearse): la
            // membrana se pone sobre su círculo, en el mismo orden. Al retraerse desde el huso podía
            // quedar un rulo cerrado alrededor del núcleo que la flexión no deshace (lo atrapaba).
            {
                const float c = std::cos(body.angle), sn = std::sin(body.angle);
                for (std::size_t i = 0; i < body.ring.size(); ++i) {
                    const Vec2 q = body.restRing[i];
                    bodies_.x[body.ring[i]] = body.centroid.x + q.x * c - q.y * sn;
                    bodies_.y[body.ring[i]] = body.centroid.y + q.x * sn + q.y * c;
                }
                soft_body::refresh(body, bodies_);
            }
            // Los organelos y gránulos se dimensionaron para la célula extendida (varias veces más
            // grande): en la célula redonda ocupan poco y se reparten a los dos lados del surco. Las
            // hijas rearman los suyos al nacer (refillOrganelles).
            const CellAnatomy& an = registry_.get<CellAnatomy>(e);
            for (std::size_t k = 0; k < body.interior.size(); ++k) {
                if (body.interior[k] != an.nucleus) {
                    bodies_.radius[body.interior[k]] *= 0.5f;
                } else { // metafase: el huso centra los cromosomas en la célula redonda
                    body.restInterior[k] = {};
                    body.anchorRate[k] = std::max(body.anchorRate[k], 3.0f);
                }
            }
            Dividing& d = registry_.emplace<Dividing>(e);
            d.motherRest = body.restRing;
            d.motherArea = units::kPi * r * r;
            dv = &d;
        }
        // El redondeo es parte de la mitosis: lo que queda de M es la división en sí.
        dv->progress += hours / std::max(0.25f, phaseHoursOf(phys, CellPhase::M) - kRoundingHours);
        // El surco entra entre los dos juegos de cromosomas ya separados: hasta que cada uno llega
        // a su mitad de la célula, la división no pasa de la anafase (si la cintura se cierra
        // antes, la membrana se pliega sobre un núcleo y la célula no se parte).
        if (dv->nucleus2 != ~0u && dv->progress > 0.35f) {
            const CellAnatomy& an = registry_.get<CellAnatomy>(e);
            const Vec2 a = soft_body::toLocal(body, {bodies_.x[an.nucleus], bodies_.y[an.nucleus]});
            const Vec2 b = soft_body::toLocal(body, {bodies_.x[dv->nucleus2], bodies_.y[dv->nucleus2]});
            const bool apart = a.x > bodies_.radius[an.nucleus] && b.x < -bodies_.radius[dv->nucleus2];
            if (!apart)
                dv->progress = 0.35f;
        }
        // Anafase B y citocinesis: la célula se alarga a medida que los cromosomas se separan (desde
        // que se duplican) y después el surco la estrangula; la forma de reposo pasa de la célula
        // redonda a dos hijas. Alargarse ya en la anafase fija el eje de la división.
        const float t = std::clamp((dv->progress - 0.3f) / 0.7f, 0.0f, 1.0f);
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
    case CellPhase::Apoptotic:
    case CellPhase::Dead: break;
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
    // Salen redondas de la citocinesis y se vuelven a extender en ~1 h.
    for (entt::entity e : {mother, child}) {
        registry_.get<AnimalCell>(e).spread = 0.0f;
        if (const CellShape* shape = registry_.try_get<CellShape>(e))
            registry_.get<RingBody>(e).shapeRate = shape->shapeRate;
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

void World::flattenSmear() {
    // Al secarse el frotis, cada leucocito se aplasta contra el vidrio: el neutrófilo, de ~8 µm en
    // suspensión, mide 12–15 µm en un frotis teñido (Bain, Blood Cells: A Practical Guide, 5ª ed.).
    // Las células sin datos de suspensión (linfocito, monocito) ya traen su tamaño de frotis.
    constexpr float kFlattening = 1.6f;
    for (auto [e, ac, body] : registry_.view<AnimalCell, RingBody>().each()) {
        const Vec2 c = body.centroid;
        auto spread = [&](std::uint32_t s) {
            bodies_.x[s] = c.x + (bodies_.x[s] - c.x) * kFlattening;
            bodies_.y[s] = c.y + (bodies_.y[s] - c.y) * kFlattening;
        };
        for (std::uint32_t s : body.ring)
            spread(s);
        for (std::uint32_t s : body.interior) {
            spread(s);
            bodies_.radius[s] *= kFlattening;
        }
        soft_body::refresh(body, bodies_);
        const float r = std::sqrt(std::abs(soft_body::area(body, bodies_)) / units::kPi);
        // Más membrana para el contorno más grande, con el espaciado de siempre.
        soft_body::resampleRing(body, bodies_, std::max<std::size_t>(48, static_cast<std::size_t>(6.2831853f * r / kMembraneSpacing) & ~std::size_t{1}));
        soft_body::reshapeRestEven(body, bodies_, r, r);
    }
}
