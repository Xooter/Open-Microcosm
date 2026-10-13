// Inspección: elegir lo que hay bajo el cursor y armar la ficha de cada cosa.
#include "game/inspect.hpp"

#include "core/profiling.hpp"
#include "core/units.hpp"
#include "game/encyclopedia.hpp"
#include "game/world.hpp"
#include "sim/polygon.hpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace {

#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
std::string fmt(const char* f, ...) {
    char buf[320];
    va_list args;
    va_start(args, f);
    std::vsnprintf(buf, sizeof buf, f, args);
    va_end(args);
    return buf;
}

InspectSection& section(Inspection& ins, const char* title) {
    ins.sections.push_back({title, {}});
    return ins.sections.back();
}

void add(InspectSection& s, std::string label, std::string value) { s.lines.push_back({std::move(label), std::move(value)}); }

std::string speciesNames(std::uint32_t mask) {
    std::string out;
    for (int k = 0; k < kSpeciesCount; ++k)
        if (mask & speciesBit(static_cast<SpeciesId>(k))) {
            if (!out.empty())
                out += ", ";
            out += speciesInfo(static_cast<SpeciesId>(k)).name;
        }
    return out;
}

const char* feedingText(FeedingMode f) {
    switch (f) {
    case FeedingMode::OralGroove: return "filtra el agua por el surco oral (corriente hacia la boca)";
    case FeedingMode::Anterior: return "absorbe por el polo anterior (despacio)";
    case FeedingMode::Phagocytosis: return "fagocita: envuelve a la presa con la membrana";
    case FeedingMode::Raptorial: return "traga presas enteras por el polo anterior";
    case FeedingMode::Tentacles: return "succiona a sus presas con tentaculos";
    case FeedingMode::NeckStrike: return "ataca con un cuello extensible";
    case FeedingMode::Osmotroph: return "absorbe materia organica disuelta por la pared";
    case FeedingMode::None: return "autotrofa: solo fotosintesis";
    }
    return "";
}

const char* locomotionText(LocomotionType t) {
    switch (t) {
    case LocomotionType::Cilia: return "cilios";
    case LocomotionType::Flagellum: return "flagelo";
    case LocomotionType::Pseudopod: return "pseudopodos";
    case LocomotionType::Undulation: return "ondulacion del cuerpo";
    }
    return "";
}

const char* kindName(const SpeciesInfo& info) {
    if (info.multicellular)
        return "Animal pluricelular";
    if (info.filament)
        return "Alga filamentosa";
    if (info.budding && info.feeding == FeedingMode::Osmotroph)
        return "Levadura";
    if (info.glider && info.frustule)
        return "Diatomea";
    if (info.colonyDaughters > 0 && info.chloroplasts)
        return "Alga colonial";
    if (info.feeding == FeedingMode::None && info.chloroplasts)
        return "Alga unicelular";
    switch (info.locomotion) {
    case LocomotionType::Cilia: return "Ciliado";
    case LocomotionType::Flagellum: return "Flagelado";
    case LocomotionType::Pseudopod: return "Ameba";
    case LocomotionType::Undulation: return "Nematodo";
    }
    return "";
}

const char* intentText(Intent i, LocomotionType type, const SpeciesInfo& info) {
    switch (i) {
    case Intent::Wander:
        switch (type) {
        case LocomotionType::Cilia:
            return info.chemokinesis ? "nada con giros al azar; si la comida mejora, gira menos (quimiocinesis)"
                                     : "nada con giros al azar";
        case LocomotionType::Flagellum: return "nada hacia objetivos que cambian al azar";
        case LocomotionType::Pseudopod: return "extiende pseudopodos en direcciones al azar";
        case LocomotionType::Undulation: return "ondula el cuerpo y cambia de rumbo de a poco";
        }
        return "deambula";
    case Intent::Chase: return "persigue a una presa que detecta";
    case Intent::Flee: return "huye en linea recta de un depredador";
    case Intent::AvoidThreat: return "reaccion de evitacion: retrocede y gira ante un depredador cercano";
    case Intent::AvoidBump: return "reaccion de evitacion: choco con algo y retrocede (mecanorrecepcion)";
    case Intent::AcidTrap: return "trampa de acido (Jennings): vuelve a la zona con mas CO2";
    case Intent::Phototaxis: return "fototaxis: gira hacia el lado mas iluminado";
    case Intent::Glide: return "se desliza por el rafe, con inversiones de sentido";
    case Intent::Walk: return "camina sobre los cirros";
    case Intent::Pause: return "parada brusca";
    case Intent::Hop: return "salto: golpe de los apendices";
    case Intent::OmegaTurn: return "giro brusco (omega) despues de retroceder";
    }
    return "";
}

SpeciesId speciesOf(const entt::registry& reg, entt::entity e) {
    if (const Species* sp = reg.try_get<Species>(e))
        return sp->id;
    if (const Egg* egg = reg.try_get<Egg>(e))
        return egg->species;
    if (const Locomotion* l = reg.try_get<Locomotion>(e))
        return playerSpecies(l->type);
    return SpeciesId::Paramecium;
}

// ---------------------------------------------------------------------------
// Células
// ---------------------------------------------------------------------------

Inspection inspectGone(const World& w, entt::entity e) {
    Inspection ins;
    ins.valid = true;
    ins.dead = true;
    ins.entity = e;
    const DeathRecord* d = w.deathOf(e);
    if (!d) {
        ins.title = "Ya no esta";
        ins.subtitle = "Murio hace mucho";
        InspectSection& s = section(ins, "Muerte");
        add(s, "Causa", "desconocida (solo se recuerdan las ultimas 64 muertes)");
        return ins;
    }
    ins.title = fmt("%s (muerta)", speciesInfo(d->species).name);
    ins.subtitle = kindName(speciesInfo(d->species));
    ins.blurb = speciesBlurb(d->species);
    ins.at = d->at;
    ins.successor = d->successor;
    ins.headline = d->cause;
    InspectSection& s = section(ins, "Muerte");
    add(s, "Causa", d->cause);
    add(s, "Hace", fmt("%.0f s", std::max(0.0f, w.simTime() - d->time)));
    return ins;
}

Inspection inspectEgg(const World& w, entt::entity e, const Egg& egg) {
    Inspection ins;
    ins.valid = ins.live = true;
    ins.entity = e;
    const RingBody& body = w.registry().get<RingBody>(e);
    ins.at = body.centroid;
    ins.radius = 0.5f * length(body.aabbMax - body.aabbMin) + 4.0f;
    const SpeciesInfo& info = speciesInfo(egg.species);
    ins.title = fmt("Huevo de %s", info.name);
    ins.subtitle = "Huevo partenogenetico";
    ins.blurb = "Sin macho: la hembra pone un huevo diploide que lleva pegado a la caparazon hasta que eclosiona.";
    ins.headline = fmt("eclosiona en %.1f h bio", egg.hoursLeft);
    InspectSection& s = section(ins, "Estado");
    add(s, "", fmt("eclosiona en %.1f h biologicas (el embrion se va formando)", egg.hoursLeft));
    add(s, "Generacion", fmt("%u", egg.generation));
    return ins;
}

Inspection inspectCell(const World& w, entt::entity e) {
    const entt::registry& reg = w.registry();
    if (!reg.valid(e) || !reg.all_of<RingBody>(e))
        return inspectGone(w, e);
    if (const Egg* egg = reg.try_get<Egg>(e))
        return inspectEgg(w, e, *egg);

    Inspection ins;
    ins.valid = ins.live = true;
    ins.entity = e;
    const RingBody& body = reg.get<RingBody>(e);
    const SpeciesId sid = speciesOf(reg, e);
    const SpeciesInfo& info = speciesInfo(sid);
    const Metabolism* meta = reg.try_get<Metabolism>(e);
    const Locomotion* loco = reg.try_get<Locomotion>(e);
    const Digestion* dig = reg.try_get<Digestion>(e);
    const Heredity* her = reg.try_get<Heredity>(e);
    const Lineage* lin = reg.try_get<Lineage>(e);
    const Undulation* und = reg.try_get<Undulation>(e);

    ins.at = body.centroid;
    ins.radius = 0.5f * std::max(body.aabbMax.x - body.aabbMin.x, body.aabbMax.y - body.aabbMin.y) + 4.0f;
    ins.title = info.name;
    const bool isPlayer = e == w.player();
    const bool mine = lin && lin->run > 0 && lin->run == w.run();
    ins.subtitle = fmt("%s - %s, generacion %u", kindName(info), isPlayer ? "tu celula" : mine ? "tu linaje" : "silvestre",
                       lin ? lin->generation : 0u);
    ins.blurb = speciesBlurb(sid);
    ins.hasGenome = her && loco;

    // --- Estado: lo que le esta pasando ahora.
    InspectSection state{"Estado", {}};
    auto now = [&](std::string text) { state.lines.push_back({"", std::move(text)}); };
    if (meta && meta->starving)
        now("en ayuno: consume sus reservas (autofagia)");
    if (const Dividing* dv = reg.try_get<Dividing>(e))
        now(fmt("%s (%.0f%%)", info.filament ? "formando un tabique para dividirse" : "dividiendose: el surco estrangula la celula",
                100.0f * std::min(1.0f, dv->progress)));
    if (const Conjugating* cj = reg.try_get<Conjugating>(e))
        now(fmt("conjugando con otra celula (%.0f%%): intercambian material genetico", 100.0f * cj->progress));
    if (reg.all_of<Cyst>(e))
        now("enquistada: pared de resistencia y metabolismo minimo");
    if (reg.all_of<PhagocyticCup>(e))
        now("envolviendo una presa: la membrana avanza por los labios de la copa");
    if (reg.all_of<Captured>(e))
        now("sujeta por los tentaculos de un suctorio, que le drena biomasa");
    if (reg.all_of<Stung>(e))
        now("paralizada por los nematocistos de Hydra");
    if (reg.all_of<Trapped>(e))
        now("pegada a una clavija de Zoophagus: sera digerida");
    if (const ChytridInfection* inf = reg.try_get<ChytridInfection>(e))
        now(fmt("infectada por quitridios: %zu esporangios sobre la pared (%.1f h bio)", inf->siteAngle.size(), inf->ageHours));
    if (const Telotroch* tel = reg.try_get<Telotroch>(e))
        now(fmt("nada como telotroco; se fija en %.0f s", tel->timer));
    if (const Zoospore* zs = reg.try_get<Zoospore>(e))
        now(fmt("zoospora: nada con su corona de flagelos ~%.1f h bio y despues se fija", zs->hoursLeft));
    if (const Anchored* an = reg.try_get<Anchored>(e))
        now(fmt("fija al vidrio por un tallo de %.0f um%s", an->stalk, an->contracted > 0.3f ? " (contraido)" : ""));
    if (reg.all_of<Discharge>(e))
        now("disparo tricocistos: el depredador retrocede");
    if (const Neck* neck = reg.try_get<Neck>(e); neck && neck->length > 5.0f)
        now(fmt("estirando el cuello (%.0f um)", neck->length));
    if (dig && dig->handling > 0.0f)
        now(fmt("manipulando una presa entera (%.1f h bio)", dig->handling / 3600.0f));
    else if (w.isSatiated(e))
        now("saciada: no persigue presas");
    if (state.lines.empty())
        now("sin eventos especiales");
    ins.headline = state.lines.front().value;
    ins.sections.push_back(std::move(state));

    // --- Comportamiento.
    {
        InspectSection& s = section(ins, "Comportamiento");
        std::string move;
        if (isPlayer)
            move = "la controlas vos";
        else if (reg.all_of<Anchored>(e))
            move = info.feeding == FeedingMode::OralGroove ? "no nada: el tallo la sostiene y la corona ciliar trae comida"
                                                           : "inmovil, sostenida por el tallo";
        else if (const Autopilot* pilot = reg.try_get<Autopilot>(e); pilot && loco)
            move = intentText(pilot->intent, loco->type, info);
        else if (reg.all_of<Segment>(e))
            move = "parte de un cuerpo compuesto";
        else
            move = "sin movimiento propio";
        add(s, "Movimiento", move);
        if (loco)
            add(s, "Locomocion", locomotionText(loco->type));
        if (ins.headline == "sin eventos especiales")
            ins.headline = move;
        add(s, "Come", feedingText(info.feeding));
        std::string prey = speciesNames(info.preyMask | info.mouthPreyMask);
        if (info.feeding == FeedingMode::OralGroove || info.feeding == FeedingMode::Anterior ||
            info.feeding == FeedingMode::Phagocytosis)
            prey = prey.empty() ? "bacterias y detritos" : "bacterias, detritos, " + prey;
        if (!prey.empty())
            add(s, "Presas", prey);
        if (info.fleeMask)
            add(s, "Huye de", speciesNames(info.fleeMask));
        if (her && her->traits.armor >= 0.5f)
            add(s, "Defensa", "coraza gruesa: no se la puede fagocitar");
        else if (info.frustule)
            add(s, "Defensa", "frustula de silice: cuesta romperla");
        const char* repro = info.eggHours > 0.0f         ? "pone huevos sin macho (partenogenesis)"
                            : info.budding              ? "gemacion: una yema crece y se separa"
                            : info.colonyDaughters > 0  ? "libera colonias hijas"
                            : info.filament             ? "division con tabique y fragmentacion del filamento"
                            : info.fissions > 1         ? "palintomia: se divide en 4 hijas"
                                                        : "fision binaria";
        add(s, "Reproduccion", repro);
        if (info.zoosporeChance > 0.0f)
            add(s, "", "tambien libera zoosporas nadadoras que se fijan y forman filamentos nuevos");
        if (sid == SpeciesId::Paramecium)
            add(s, "", "ademas se conjuga con otra celula de distinto tipo de apareamiento");
    }

    // --- Metabolismo.
    if (meta) {
        InspectSection& s = section(ins, "Metabolismo");
        if (info.anaerobe)
            add(s, "Respiracion", "anaerobia estricta: el O2 la danha");
        else
            add(s, "Respiracion", fmt("%s (%.0f%% aerobico)", meta->aerobic > 0.5f ? "respira O2" : "fermenta, sin O2", 100.0f * meta->aerobic));
        add(s, "O2 local", fmt("%.0f uM", meta->o2));
        if (meta->photosynthetic)
            add(s, "Luz local", fmt("%.0f%% (fotosintetiza)", 100.0f * meta->light));
        const float birth = meta->targetBirthBiomass > 0.0f ? meta->targetBirthBiomass : meta->birthBiomass;
        const float need = info.eggHours > 0.0f ? 1.4f * birth
                           : meta->pendingFissions > 0 ? 1.8f * birth
                                                       : static_cast<float>(1 << meta->fissions) * birth;
        add(s, "Crecimiento", fmt("%.0f%% de la biomasa para reproducirse", 100.0f * std::min(1.0f, meta->biomass / std::max(1e-6f, need))));
        const float cost = isPlayer ? w.metabolismParams().playerDivisionCost : w.metabolismParams().npcDivisionCost;
        add(s, "Reservas", fmt("%.0f%% de la biomasa (reproducirse pide %.0f%%)",
                               100.0f * meta->reserves / std::max(1e-6f, meta->biomass), 100.0f * cost));
        add(s, "Edad", fmt("%.1f h biologicas", meta->ageHours));
        if (meta->photosynthetic)
            add(s, "Fosforo y N", fmt("%.4f y %.4f pmol", meta->phosphorus, meta->nitrogen));
        if (meta->needsSilicon)
            add(s, "Silice", fmt("%.4f pmol (sin ella no arma la valva nueva y no se divide)", meta->silicon));
        if (meta->photosynthetic)
            add(s, "Hierro", fmt("%.5f pmol (fotosistemas y ferredoxina)", meta->iron));
        if (meta->needsVitamin)
            add(s, "Vitamina B12", fmt("%.2e pmol (auxotrofa: la aportan las bacterias)", meta->vitamin));
        if (dig)
            add(s, "Comio", fmt("%d bacterias, %d detritos, %d celulas (%zu vacuolas)", dig->bacteriaEaten, dig->detritusEaten,
                                dig->cellsEaten, dig->vacuoles.size()));
    }

    // --- Cuerpo.
    {
        InspectSection& s = section(ins, "Cuerpo");
        float half = 0.0f, width = 0.0f;
        for (const Vec2& q : body.restRing) {
            half = std::max(half, std::abs(q.x));
            width = std::max(width, std::abs(q.y));
        }
        if (und)
            half = 0.5f * und->length;
        add(s, "Tamano", fmt("%.0f x %.0f um (area %.0f um2)", 2.0f * half, 2.0f * width, std::abs(body.restArea)));
        if (loco)
            add(s, "Velocidad", fmt("%.0f um/s%s", std::abs(loco->speed), loco->speed < -1.0f ? " (hacia atras)" : ""));
        if (und)
            add(s, "Onda", fmt("%s: arrastre normal/tangencial = %.1f (agua libre %.1f)",
                               und->crowding > 0.5f ? "repta entre particulas" : "nada en agua libre", und->dragRatio,
                               w.locomotionParams().dragRatioWater));
        if (const Compound* comp = reg.try_get<Compound>(e))
            add(s, "Partes", fmt("%zu partes unidas por adhesiones (corona, tronco y pie)", comp->parts.size() + 1));
        if (her && sid == SpeciesId::Paramecium)
            add(s, "Apareamiento", her->genome[Gene::MatingType] > 0.5f ? "tipo II" : "tipo I");
    }

    // --- Medio local.
    {
        InspectSection& s = section(ins, "Medio local");
        const float temp = w.medium().temperature;
        const float ph = w.ph().sample(body.centroid.x, body.centroid.y);
        const bool hot = temp < info.tMin || temp > info.tMax, acid = ph < info.phMin || ph > info.phMax;
        const bool salty = w.medium().salinity > info.salinityMax;
        add(s, "Temperatura", fmt("%.0f C (tolera %.0f a %.0f)%s", temp, info.tMin, info.tMax, hot ? " - FUERA DE RANGO" : ""));
        add(s, "pH", fmt("%.1f (tolera %.1f a %.1f)%s", ph, info.phMin, info.phMax, acid ? " - FUERA DE RANGO" : ""));
        add(s, "Salinidad", fmt("%.1f PSU (maximo %.0f)%s", w.medium().salinity, info.salinityMax, salty ? " - FUERA DE RANGO" : ""));
        if (hot || acid || salty)
            add(s, "", "el estres se paga como mantenimiento extra");
    }
    return ins;
}

// ---------------------------------------------------------------------------
// Granos, partículas y microbios
// ---------------------------------------------------------------------------

const char* mineralBlurb(MineralKind k) {
    switch (k) {
    case MineralKind::Clay:
        return "Arcilla: laminas de silicato de menos de 4 um. Tan chicas que el movimiento browniano supera a la sedimentacion: quedan en suspension y enturbian el agua.";
    case MineralKind::Quartz:
        return "Cuarzo (SiO2): el mineral mas comun de la arena. Su indice de refraccion (1,54) es mayor que el del agua (1,33): se ve por el relieve, con un borde oscuro grueso y una linea brillante de Becke por dentro.";
    case MineralKind::Mica:
        return "Mica: escamas hexagonales finisimas y brillantes. Por su forma plana se asientan despacio.";
    case MineralKind::IronOxide:
        return "Oxidos de hierro: opacos y pardo rojizos, hacen sombra. Dan su color al barro y al suelo.";
    case MineralKind::Sulfur:
        return "Globulos de azufre elemental: se forman donde hay sulfuros (fuentes termales). Muy refringentes: borde negro y centro brillante.";
    case MineralKind::Frustule:
        return "Frustula vacia de diatomea: la caja de silice (opalo) que queda cuando muere la celula. No se descompone, a diferencia del resto del organismo; el rafe y las estrias siguen intactos.";
    case MineralKind::Count: break;
    }
    return "";
}

// Diferencia de densidad con el agua [kg/m3] para la ley de Stokes.
float mineralDensityGap(MineralKind k) {
    switch (k) {
    case MineralKind::Clay: return 1650.0f;
    case MineralKind::Quartz: return 1650.0f;
    case MineralKind::Mica: return 1800.0f;
    case MineralKind::IronOxide: return 4000.0f;
    case MineralKind::Sulfur: return 1000.0f;
    case MineralKind::Frustule: return 600.0f; // opalo hueco
    case MineralKind::Count: break;
    }
    return 1650.0f;
}

Inspection inspectRock(const World& w, entt::entity e) {
    const entt::registry& reg = w.registry();
    if (!reg.valid(e) || !reg.all_of<RingBody, Rock>(e)) {
        Inspection ins;
        ins.valid = ins.dead = true;
        ins.entity = e;
        ins.title = "Ya no esta";
        ins.subtitle = "El grano se borro";
        return ins;
    }
    const RingBody& body = reg.get<RingBody>(e);
    const Rock& rock = reg.get<Rock>(e);
    Inspection ins;
    ins.valid = ins.live = true;
    ins.entity = e;
    ins.at = body.centroid;
    ins.radius = 0.5f * std::max(body.aabbMax.x - body.aabbMin.x, body.aabbMax.y - body.aabbMin.y) + 4.0f;
    ins.title = rock.kind == MineralKind::IronOxide ? "Grano de oxido de hierro" : "Grano de arena (cuarzo)";
    ins.subtitle = "Mineral - no esta vivo";
    ins.blurb = mineralBlurb(rock.kind);
    ins.headline = "inmovil, apoyado en el vidrio";
    InspectSection& s = section(ins, "Grano");
    add(s, "Tamano", fmt("%.0f x %.0f um", body.aabbMax.x - body.aabbMin.x, body.aabbMax.y - body.aabbMin.y));
    add(s, "Fijo", "apoyado en el vidrio: ni el agua ni las celulas lo mueven");
    add(s, "Agua", "no fluye adentro del grano: la corriente lo rodea");
    add(s, "Entre granos", "da apoyo lateral: los nematodos reptan mucho mejor entre ellos");
    add(s, "Quimica", "simplificacion: los gases y nutrientes disueltos lo atraviesan");
    return ins;
}

Inspection inspectParticle(const World& w, const Picked& p) {
    const AmbientParticles& a = w.ambient();
    Inspection ins;
    ins.at = p.at;
    if (p.index >= a.size()) {
        ins.valid = ins.dead = true;
        ins.title = "Ya no esta";
        ins.subtitle = "La particula desaparecio";
        return ins;
    }
    ins.valid = true;
    ins.at = {a.x[p.index], a.y[p.index]};
    ins.radius = std::max(6.0f, 2.0f * a.radius[p.index]);
    const float r = a.radius[p.index];
    InspectSection& s = section(ins, "Particula");
    add(s, "Tamano", fmt("%.1f um de radio", r));
    if (a.kind[p.index] == ParticleKind::Ink) {
        ins.title = "Tinta china";
        ins.subtitle = "Carbono - trazador del flujo";
        ins.blurb = "Particulas de carbono de ~0,2 um: no se mueven solas, asi que muestran como se mueve el agua (corrientes, remolinos de los nadadores).";
        ins.headline = "sigue el agua";
        add(s, "Movimiento", "sigue el agua mas un browniano (Stokes-Einstein)");
        add(s, "Difusion", fmt("%.2f um2/s", units::stokesEinsteinDiffusion(r)));
        add(s, "Edad", fmt("%.0f s (se diluye con el tiempo)", a.age[p.index]));
    } else if (a.kind[p.index] == ParticleKind::Detritus) {
        ins.title = "Detrito";
        ins.subtitle = "Materia organica muerta";
        ins.blurb = "Bacterias y hongos la descomponen y liberan nutrientes disueltos; los ciliados filtradores la comen. Da sombra a lo que tiene debajo.";
        ins.headline = "se descompone";
        add(s, "Masa", fmt("%.4f pmol de glucosa equivalente", a.mass[p.index]));
        add(s, "Descomposicion", "libera nutrientes al agua (mas rapido junto a hifas de hongos)");
        add(s, "Edad", fmt("%.0f s", a.age[p.index]));
        add(s, "Difusion", fmt("%.2f um2/s", units::stokesEinsteinDiffusion(r)));
    } else {
        const MineralKind k = a.mineral[p.index];
        ins.title = mineralName(k);
        ins.subtitle = "Mineral - inerte";
        ins.blurb = mineralBlurb(k);
        const bool settled = r >= 2.0f;
        ins.headline = settled ? "asentada en el fondo" : "en suspension";
        // Stokes: v = 2/9 * (rho_p - rho_w) * g * r^2 / mu  ->  0,00218 * delta_rho * r^2 [um/s, r en um].
        const float fall = 0.002182f * mineralDensityGap(k) * r * r;
        add(s, "Estado", settled ? "asentada en el fondo: solo la mueven las celulas" : "en suspension: sigue el agua con browniano");
        add(s, "Sedimentacion", fmt("caeria a ~%.1f um/s (ley de Stokes)%s", fall, settled ? ": toca el vidrio en segundos" : ""));
        add(s, "No se descompone", "no se come ni se descompone; hace sombra solo si es opaca");
    }
    return ins;
}

Inspection inspectCyano(const World& w, const Picked& p) {
    Inspection ins;
    const auto& fils = w.cyanobacteria().filaments;
    if (p.index >= fils.size() || fils[p.index].x.empty()) {
        ins.valid = ins.dead = true;
        ins.title = "Ya no esta";
        ins.subtitle = "El filamento se fragmento o murio";
        ins.at = p.at;
        return ins;
    }
    const CyanoFilament& f = fils[p.index];
    const CyanoParams& cp = w.cyanobacteria().params;
    const std::size_t node = std::min(p.node, f.x.size() - 1);
    ins.valid = true;
    ins.at = {f.x[node], f.y[node]};
    ins.radius = 0.5f * cp.spacing * static_cast<float>(f.x.size()) + 6.0f;
    std::size_t hetero = 0;
    float mass = 0.0f;
    for (std::size_t k = 0; k < f.x.size(); ++k) {
        hetero += f.heterocyst[k];
        mass += f.mass[k];
    }
    ins.title = f.oscillatoria ? "Oscillatoria" : "Anabaena";
    ins.subtitle = "Cianobacteria filamentosa";
    ins.blurb = f.oscillatoria
                    ? "Tricoma sin heterocistos (no fija nitrogeno): se desliza rotando sobre su eje, asi que la punta que avanza oscila de lado a lado."
                    : otherBlurb(OtherOrganism::Cyanobacteria);
    ins.headline = fmt("%zu celulas, desliza %.1f um/s", f.x.size(), f.oscillatoria ? cp.oscillatoriaGlide : cp.glideSpeed);
    InspectSection& s = section(ins, "Filamento");
    add(s, "Celulas", fmt("%zu (largo ~%.0f um)", f.x.size(), cp.spacing * static_cast<float>(f.x.size())));
    add(s, "Heterocistos", f.oscillatoria ? "ninguno: no fija N2" : fmt("%zu: fijan N2 donde el O2 no los inhibe", hetero));
    add(s, "Biomasa", fmt("%.3f pmol", mass * cp.cellPmol));
    add(s, "Fosforo y N", fmt("%.4f y %.4f pmol", f.phosphorus, f.nitrogen));
    add(s, "Deslizamiento", fmt("%.1f um/s, %s", f.oscillatoria ? cp.oscillatoriaGlide : cp.glideSpeed,
                                f.direction > 0.0f ? "hacia la ultima celula" : "hacia la primera celula"));
    add(s, "Al morir", "libera microcistina, toxica para quien la come");
    return ins;
}

Inspection inspectBacterium(const World& w, const Picked& p) {
    Inspection ins;
    const Bacteria& b = w.bacteria();
    if (p.index >= b.size()) {
        ins.valid = ins.dead = true;
        ins.title = "Ya no esta";
        ins.subtitle = "La bacteria se dividio o murio";
        ins.at = p.at;
        return ins;
    }
    const std::size_t i = p.index;
    ins.valid = true;
    ins.at = {b.x[i], b.y[i]};
    ins.radius = 6.0f;
    const bool vio = b.strain[i] == 1;
    ins.title = vio ? "Chromobacterium violaceum" : "Bacteria (bacilo)";
    ins.subtitle = "Bacteria";
    ins.blurb = otherBlurb(vio ? OtherOrganism::Violaceum : OtherOrganism::Bacteria);
    std::string state;
    if (b.bdello[i] >= 0.0f)
        state = fmt("bdeloplasto: un Bdellovibrio crece adentro (%.1f h bio)", b.bdello[i]);
    else if (b.phage[i] >= 0.0f)
        state = fmt("infectada por un fago (%.1f h bio): estallara", b.phage[i]);
    else
        state = b.tumbling[i] ? "dando un giro (tumble)" : "en corrida: nada recto";
    ins.headline = state;
    InspectSection& s = section(ins, "Bacteria");
    add(s, "Estado", state);
    add(s, "Masa", fmt("%.2f (se divide al llegar a 2)", b.mass[i]));
    add(s, "Alimento local", fmt("%.1f uM de materia organica", w.nutrients().grid().sample(b.x[i], b.y[i])));
    add(s, "Quimiotaxis", "compara la concentracion de ahora con la de hace ~1 s");
    if (b.resistant[i])
        add(s, "Defensa", "resistente a los fagos");
    if (vio)
        add(s, "Quorum", b.quorum[i] ? "alcanzado: produce violaceina (violeta, toxica)" : "no alcanzado: faltan vecinas");
    return ins;
}

Inspection inspectBdello(const World& w, const Picked& p) {
    Inspection ins;
    const Bdellovibrios& d = w.bdellovibrios();
    if (p.index >= d.size()) {
        ins.valid = ins.dead = true;
        ins.title = "Ya no esta";
        ins.subtitle = "El Bdellovibrio entro a una presa o murio de hambre";
        ins.at = p.at;
        return ins;
    }
    ins.valid = true;
    ins.at = {d.x[p.index], d.y[p.index]};
    ins.radius = 6.0f;
    ins.title = otherName(OtherOrganism::Bdellovibrio);
    ins.subtitle = "Bacteria depredadora";
    ins.blurb = otherBlurb(OtherOrganism::Bdellovibrio);
    ins.headline = "caza bacterias";
    InspectSection& s = section(ins, "Bdellovibrio");
    add(s, "Velocidad", fmt("%.0f um/s en linea recta, con cambios bruscos", w.bacteria().params.bdelloSpeed));
    add(s, "Hambre", fmt("%.1f h bio sin comer", d.hungerHours[p.index]));
    return ins;
}

Inspection inspectSpore(const World& w, const Picked& p) {
    Inspection ins;
    const Zoospores& z = w.fungi().spores;
    if (p.index >= z.size()) {
        ins.valid = ins.dead = true;
        ins.title = "Ya no esta";
        ins.subtitle = "La zoospora infecto un alga o se enquisto";
        ins.at = p.at;
        return ins;
    }
    const FungusKind kind = z.kind[p.index];
    ins.valid = true;
    ins.at = {z.x[p.index], z.y[p.index]};
    ins.radius = 6.0f;
    ins.title = fmt("Zoospora de %s", fungusName(kind));
    ins.subtitle = "Hongo - zoospora nadadora";
    ins.blurb = otherBlurb(kind == FungusKind::Chytrid ? OtherOrganism::Chytrid : OtherOrganism::Saprolegnia);
    ins.headline = kind == FungusKind::Chytrid ? "busca un alga hospedadora" : "sube el gradiente de materia organica";
    InspectSection& s = section(ins, "Zoospora");
    const FungiParams& fp = w.fungi().params;
    add(s, "Busca", kind == FungusKind::Chytrid ? "un alga: nada hacia la mas cercana (quimiotaxis)" : "materia organica disuelta (aminoacidos)");
    add(s, "Velocidad", fmt("%.0f um/s", kind == FungusKind::Chytrid ? fp.chytridSporeSpeed : fp.saproSporeSpeed));
    add(s, "Edad", fmt("%.1f h bio", z.ageHours[p.index]));
    add(s, "Fosforo", fmt("%.4f pmol", z.phosphorus[p.index]));
    return ins;
}

Inspection inspectHypha(const World& w, const Picked& p) {
    Inspection ins;
    const Mycelium* m = w.fungi().find(p.id);
    if (!m || m->x.empty()) {
        ins.valid = ins.dead = true;
        ins.title = "Ya no esta";
        ins.subtitle = "El micelio murio de hambre";
        ins.at = p.at;
        return ins;
    }
    const FungiParams& fp = w.fungi().params;
    const std::size_t node = std::min(p.node, m->x.size() - 1);
    ins.valid = ins.live = true;
    ins.at = {m->x[node], m->y[node]};
    float lo = 1e30f, hi = -1e30f, lo2 = 1e30f, hi2 = -1e30f;
    std::size_t tips = 0, sporangia = 0, pegs = 0;
    for (std::size_t k = 0; k < m->x.size(); ++k) {
        lo = std::min(lo, m->x[k]);
        hi = std::max(hi, m->x[k]);
        lo2 = std::min(lo2, m->y[k]);
        hi2 = std::max(hi2, m->y[k]);
        tips += m->tip[k];
        sporangia += m->sporangium[k] > 0.0f ? 1 : 0;
        pegs += m->peg[k];
    }
    ins.radius = 0.5f * std::max(hi - lo, hi2 - lo2) + 10.0f;
    ins.title = fmt("Micelio de %s", fungusName(m->kind));
    ins.subtitle = "Hongo";
    const OtherOrganism which = m->kind == FungusKind::Zoophagus ? OtherOrganism::Zoophagus : OtherOrganism::Saprolegnia;
    ins.blurb = otherBlurb(which);
    ins.headline = fmt("%zu nodos, %zu puntas", m->x.size(), tips);
    InspectSection& s = section(ins, "Micelio");
    add(s, "Hifas", fmt("%zu nodos, %zu puntas en crecimiento", m->x.size(), tips));
    if (sporangia)
        add(s, "Zoosporangios", fmt("%zu madurando: liberaran zoosporas", sporangia));
    if (m->kind == FungusKind::Zoophagus)
        add(s, "Clavijas", fmt("%zu clavijas adhesivas (atrapan rotiferos)", pegs));
    add(s, "Biomasa", fmt("%.1f pmol (reservas %.1f)", m->biomass(fp), m->reserves));
    add(s, "Fosforo", fmt("%.3f pmol", m->phosphorus));
    if (m->starvingHours > 0.0f)
        add(s, "Hambre", fmt("%.1f h bio sin comer", m->starvingHours));
    return ins;
}

} // namespace

// ---------------------------------------------------------------------------
// Elegir
// ---------------------------------------------------------------------------

Picked World::pick(Vec2 position, float tolerance) const {
    PROFILE_SCOPE("World::pick");
    Picked out;

    // 1. Cuerpos blandos: lo de adentro vence a lo cercano; entre varios, el más chico.
    float bestScore = 1e30f;
    for (const auto [e, body] : registry_.view<RingBody>().each()) {
        if (registry_.all_of<Rock>(e))
            continue;
        const Vec2 q = body.centroid + Vec2{minImage(position.x - body.centroid.x, width_),
                                            minImage(position.y - body.centroid.y, height_)};
        if (q.x < body.aabbMin.x - tolerance || q.x > body.aabbMax.x + tolerance || q.y < body.aabbMin.y - tolerance ||
            q.y > body.aabbMax.y + tolerance)
            continue;
        const int pn = static_cast<int>(body.poly.size());
        const bool inside = polygon::contains(body.poly.data(), pn, q);
        if (!inside && polygon::closestOnBoundary(body.poly.data(), pn, q).dist2 > tolerance * tolerance)
            continue;
        const float score = (inside ? 0.0f : 1e20f) + std::abs(body.restArea);
        if (score >= bestScore)
            continue;
        bestScore = score;
        entt::entity who = e; // las partes de un cuerpo compuesto son el organismo
        if (const Segment* seg = registry_.try_get<Segment>(e); seg && registry_.valid(seg->organism))
            who = seg->organism;
        out = {};
        out.kind = Picked::Kind::Cell;
        out.entity = who;
        out.at = body.centroid;
    }
    if (out.kind != Picked::Kind::None)
        return out;

    // 2. Granos de arena.
    if (const entt::entity rock = rockAt(position); rock != entt::null) {
        out.kind = Picked::Kind::Rock;
        out.entity = rock;
        out.at = registry_.get<RingBody>(rock).centroid;
        return out;
    }

    // 3. Partículas y microbios: gana el que tiene su borde más cerca del cursor.
    float best = tolerance;
    auto consider = [&](Picked::Kind kind, std::size_t index, std::size_t node, std::uint32_t id, float x, float y,
                        float radius, float penalty = 0.0f) {
        const float dx = minImage(x - position.x, width_), dy = minImage(y - position.y, height_);
        const float reach = best + radius;
        if (std::abs(dx) > reach || std::abs(dy) > reach)
            return;
        const float d = std::sqrt(dx * dx + dy * dy) - radius + penalty;
        if (d > best)
            return;
        best = d;
        out = {};
        out.kind = kind;
        out.index = index;
        out.node = node;
        out.id = id;
        out.at = {x, y};
    };
    for (std::size_t i = 0; i < bacteria_.size(); ++i)
        consider(Picked::Kind::Bacterium, i, 0, 0, bacteria_.x[i], bacteria_.y[i], 1.0f);
    for (std::size_t i = 0; i < bdello_.size(); ++i)
        consider(Picked::Kind::Bdellovibrio, i, 0, 0, bdello_.x[i], bdello_.y[i], 0.8f);
    for (std::size_t i = 0; i < fungi_.spores.size(); ++i)
        consider(Picked::Kind::Zoospore, i, 0, 0, fungi_.spores.x[i], fungi_.spores.y[i], 2.5f);
    for (std::size_t f = 0; f < cyano_.filaments.size(); ++f) {
        const CyanoFilament& fil = cyano_.filaments[f];
        for (std::size_t k = 0; k < fil.x.size(); ++k)
            consider(Picked::Kind::Cyanobacteria, f, k, 0, fil.x[k], fil.y[k], 2.5f);
    }
    for (const Mycelium& m : fungi_.mycelia) {
        const float radius = m.kind == FungusKind::Zoophagus ? 2.0f : 5.5f;
        for (std::size_t k = 0; k < m.x.size(); ++k)
            consider(Picked::Kind::Hypha, 0, k, m.id, m.x[k], m.y[k], radius);
    }
    for (std::size_t i = 0; i < ambient_.size(); ++i) {
        // La tinta cubre zonas enteras: solo si se apunta directo a una partícula.
        const bool ink = ambient_.kind[i] == ParticleKind::Ink;
        consider(Picked::Kind::Particle, i, 0, 0, ambient_.x[i], ambient_.y[i], ambient_.radius[i], ink ? tolerance : 0.0f);
    }
    return out;
}

// ---------------------------------------------------------------------------
// La ficha
// ---------------------------------------------------------------------------

Inspection buildInspection(const World& world, const Picked& picked) {
    PROFILE_SCOPE("buildInspection");
    switch (picked.kind) {
    case Picked::Kind::Cell: return inspectCell(world, picked.entity);
    case Picked::Kind::Rock: return inspectRock(world, picked.entity);
    case Picked::Kind::Cyanobacteria: return inspectCyano(world, picked);
    case Picked::Kind::Bacterium: return inspectBacterium(world, picked);
    case Picked::Kind::Bdellovibrio: return inspectBdello(world, picked);
    case Picked::Kind::Zoospore: return inspectSpore(world, picked);
    case Picked::Kind::Hypha: return inspectHypha(world, picked);
    case Picked::Kind::Particle: return inspectParticle(world, picked);
    case Picked::Kind::None: break;
    }
    return {};
}
