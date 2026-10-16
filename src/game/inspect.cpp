// Inspección: elegir lo que hay bajo el cursor y armar la ficha de cada cosa.
#include "game/inspect.hpp"

#include "bio/body_plan.hpp"
#include "bio/physiology.hpp"
#include "core/profiling.hpp"
#include "core/units.hpp"
#include "game/encyclopedia.hpp"
#include "game/world.hpp"
#include "sim/polygon.hpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <utility>

namespace {

#if defined(__GNUC__)
#if defined(__MINGW32__)
__attribute__((format(gnu_printf, 1, 2))) // con __USE_MINGW_ANSI_STDIO, printf es el de C99
#else
__attribute__((format(printf, 1, 2)))
#endif
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

// Texto de un archivo de datos (UTF-8) llevado al ASCII de la ficha: acentos sin tilde, ° y µ
// como en el resto de la ficha ("26 C", "uM"), superíndices como dígitos.
std::string asciiFold(const std::string& text) {
    static const std::pair<const char*, const char*> kFold[] = {
        {"á", "a"}, {"é", "e"}, {"í", "i"}, {"ó", "o"}, {"ú", "u"}, {"ü", "u"}, {"ñ", "n"},
        {"Á", "A"}, {"É", "E"}, {"Í", "I"}, {"Ó", "O"}, {"Ú", "U"}, {"Ñ", "N"}, {"°", ""},
        {"µ", "u"}, {"±", "+/-"}, {"⁻", "-"}, {"¹", "1"}, {"²", "2"}, {"³", "3"}, {"–", "-"},
        {"₂", "2"}, {"₃", "3"}, {"₄", "4"}, {"—", "-"},
    };
    std::string out;
    for (std::size_t i = 0; i < text.size();) {
        if (static_cast<unsigned char>(text[i]) < 128) {
            out += text[i++];
            continue;
        }
        bool folded = false;
        for (const auto& [from, to] : kFold)
            if (text.compare(i, std::strlen(from), from) == 0) {
                out += to;
                i += std::strlen(from);
                folded = true;
                break;
            }
        if (!folded) { // otro carácter: se saltea la secuencia UTF-8 entera
            out += '?';
            for (++i; i < text.size() && (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80; ++i) {
            }
        }
    }
    return out;
}

// El índice de una partícula con id estable: primero prueba el de la foto del click.
bool resolveIndex(const std::vector<std::uint32_t>& ids, Picked& p) {
    if (p.index < ids.size() && ids[p.index] == p.id)
        return true;
    const auto it = std::find(ids.begin(), ids.end(), p.id);
    if (it == ids.end())
        return false;
    p.index = static_cast<std::size_t>(it - ids.begin());
    return true;
}

// Estructuras del plan corporal, con su tamaño real y si se ven en campo claro o solo en el modelo.
void addStructures(Inspection& ins, const BodyPlan& plan, const char* title = "Estructuras") {
    InspectSection& s = section(ins, title);
    for (const StructureSpec& st : plan.structures) {
        std::string size;
        if (st.width > 0.0f) {
            const float d = st.kind == StructureKind::ViralGenome ? st.length : std::max(st.length, st.width);
            size = d >= 1.0f ? fmt("%.3g um", static_cast<double>(d)) : fmt("%.0f nm", static_cast<double>(d * 1000.0f));
        }
        // Lo simulado tiene estado propio; lo declarado existe en el organismo real pero el modelo
        // lo ubica sin simularlo.
        const std::string seen = std::string(st.source == StructureSource::Simulated ? "simulada; " : "declarada; ") +
                                 (visibleInBrightfield(st) ? "se ve al microscopio" : "solo en la vista de modelo");
        std::string value = size.empty() ? seen : size + ", " + seen;
        if (st.count > 1)
            value = fmt("%d, ", st.count) + value;
        else if (st.perCubicMicron > 0.0f)
            value = fmt("~%.0f por um3, ", static_cast<double>(st.perCubicMicron)) + value;
        add(s, st.name, value);
    }
}

std::string speciesNames(const SpeciesSet& mask) {
    std::string out;
    for (int k = 0; k < speciesCount(); ++k)
        if (mask.test(static_cast<std::size_t>(k))) {
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
    case LocomotionType::Crawl: return "arrastre sobre el fondo (lamelipodio)";
    case LocomotionType::None: return "ninguna (no se mueve sola)";
    }
    return "";
}

const char* kindName(const SpeciesInfo& info) {
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
    case LocomotionType::Crawl: return info.animalCell.present ? "Celula humana en cultivo" : "Celula que se arrastra";
    case LocomotionType::None: return "Celula de un organismo pluricelular";
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
        case LocomotionType::Crawl: return "migra sobre el fondo en un paseo al azar persistente";
        case LocomotionType::None: return "queda quieta";
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
    }
    return "";
}

SpeciesId speciesOf(const entt::registry& reg, entt::entity e) {
    if (const Species* sp = reg.try_get<Species>(e))
        return sp->id;
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

Inspection inspectCell(const World& w, entt::entity e) {
    const entt::registry& reg = w.registry();
    if (!reg.valid(e) || !reg.all_of<RingBody>(e))
        return inspectGone(w, e);

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
    if (const AnimalCell* ac = reg.try_get<AnimalCell>(e)) {
        // Célula humana: dónde está de su ciclo y por qué (fase 20b).
        const AnimalCellInfo& ai = info.animalCell;
        if (ac->phase == CellPhase::Apoptotic)
            now(fmt("en apoptosis (%.0f%%): se encoge, burbujea y se partira en cuerpos apoptoticos, sin reventar",
                    100.0f * ac->apoptosis));
        else if (ac->phase == CellPhase::G0)
            now(!ai.cycles ? "no se divide (celula diferenciada terminal)"
                : w.medium().serum < ai.serumMinimum ? "quiescente (G0): sin factores de crecimiento del suero no pasa el punto de restriccion"
                : ac->contact >= 0.5f && ai.contactProliferation ? "quiescente (G0): rodeada de vecinas (inhibicion por contacto)"
                                                                   : "quiescente (G0)");
        else
            now(fmt("ciclo celular: fase %s (%.1f h en la fase)", asciiFold(cellPhaseName(ac->phase)).c_str(),
                    ac->phaseHours * w.bioTimeScale() / w.bioScale())); // horas a su temperatura (sin el Q10)
        if (ac->contact > 0.05f)
            now(fmt("toca a otras celulas en el %.0f%% de su borde%s", 100.0f * ac->contact,
                    ai.contactLocomotion ? ": se repolariza hacia el lado libre (Abercrombie)" : ""));
        if (reg.all_of<Adherent>(e))
            now("adherida al fondo por adhesiones focales");
    }
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
        else
            move = "sin movimiento propio";
        add(s, "Movimiento", move);
        if (loco)
            add(s, "Locomocion", locomotionText(loco->type));
        if (ins.headline == "sin eventos especiales")
            ins.headline = move;
        add(s, "Come", feedingText(info.feeding));
        std::string prey = speciesNames(info.prey | info.mouthPrey);
        if (info.feeding == FeedingMode::OralGroove || info.feeding == FeedingMode::Anterior ||
            info.feeding == FeedingMode::Phagocytosis)
            prey = prey.empty() ? "bacterias y detritos" : "bacterias, detritos, " + prey;
        if (!prey.empty())
            add(s, "Presas", prey);
        if (info.flee.any())
            add(s, "Huye de", speciesNames(info.flee));
        if (her && her->traits.armor >= 0.5f)
            add(s, "Defensa", "coraza gruesa: no se la puede fagocitar");
        else if (info.frustule)
            add(s, "Defensa", "frustula de silice: cuesta romperla");
        const char* repro = info.budding                ? "gemacion: una yema crece y se separa"
                            : info.colonyDaughters > 0  ? "libera colonias hijas"
                            : info.filament             ? "division con tabique y fragmentacion del filamento"
                            : info.fissions > 1         ? "palintomia: se divide en 4 hijas"
                                                        : "fision binaria";
        add(s, "Reproduccion", repro);
        if (info.zoosporeChance > 0.0f)
            add(s, "", "tambien libera zoosporas nadadoras que se fijan y forman filamentos nuevos");
        if (info.conjugates)
            add(s, "", "ademas se conjuga con otra celula de su especie y de distinto tipo de apareamiento");
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
        const float need = meta->pendingFissions > 0 ? 1.8f * birth : static_cast<float>(1 << meta->fissions) * birth;
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
        add(s, "Tamano", fmt("%.0f x %.0f um (area %.0f um2)", 2.0f * half, 2.0f * width, std::abs(body.restArea)));
        if (loco)
            add(s, "Velocidad", fmt("%.0f um/s%s", std::abs(loco->speed), loco->speed < -1.0f ? " (hacia atras)" : ""));
        if (her && info.conjugates)
            add(s, "Apareamiento", her->genome[Gene::MatingType] > 0.5f ? "tipo II" : "tipo I");
    }

    // --- Tinciones: lo que cada colorante ve en esta especie (bioquimica real).
    {
        InspectSection& s = section(ins, "Tinciones");
        const SpeciesChemistry chem = speciesChemistry(sid);
        switch (chem.storage) {
        case Storage::Starch: add(s, "Lugol", "almidon verdadero en el cloroplasto: azul-negro"); break;
        case Storage::Glycogen: add(s, "Lugol", "glucogeno: marron rojizo"); break;
        case Storage::Paramylon: add(s, "Lugol", "paramilon: no reacciona con el yodo"); break;
        case Storage::Chrysolaminarin: add(s, "Lugol", "crisolaminarina (en una vacuola): no reacciona con el yodo"); break;
        case Storage::None: break;
        }
        add(s, "Gram", chem.wall == CellWall::YeastGlucan ? "pared gruesa de levadura: se tine como grampositiva (violeta)"
                                                          : "celula eucariota: toma solo la contratincion (rosa palido)");
        add(s, "DAPI / azul de metileno", "el ADN del nucleo");
        add(s, "Rojo neutro", "vacuolas digestivas y lisosomas (acidos)");
    }

    // --- Fisiología con su fuente (assets/content/base/fisiologia.toml): de dónde sale cada número.
    {
        const SpeciesPhysiology& phys = speciesPhysiology(sid);
        auto cite = [](InspectSection& s, const SourcedValue& v) {
            add(s, "", fmt("publicado: %g %s a %.0f C%s", v.published, asciiFold(v.unit).c_str(), v.temperature,
                           v.estimate ? " - ESTIMACION (sin verificar)" : ""));
            add(s, "", asciiFold(v.source));
        };
        bool any = false;
        for (const PhysiologyKey& k : physiologyKeys())
            any = any || (phys.*k.member).present;
        if (any) {
            InspectSection& s = section(ins, "Fisiologia y fuentes");
            for (const PhysiologyKey& k : physiologyKeys()) {
                const SourcedValue& v = phys.*k.member;
                if (!v.present)
                    continue;
                if (k.member == &SpeciesPhysiology::growthMax)
                    add(s, k.label, fmt("%.3f /h a 20 C (duplicacion %.0f h)", v.value, std::log(2.0f) / v.value));
                else if (std::string(k.unit) == "uM" || std::string(k.unit) == "1/cm2" || std::string(k.unit) == "mol/mol")
                    add(s, k.label, fmt("%g %s", v.value, k.unit));
                else
                    add(s, k.label, fmt("%.3g %s a 20 C", v.value, k.unit));
                cite(s, v);
            }
        } else {
            add(section(ins, "Fisiologia y fuentes"), "", "sin datos propios: usa los valores generales del metabolismo");
        }
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
    addStructures(ins, speciesPlan(sid));
    return ins;
}

// ---------------------------------------------------------------------------
// Granos, partículas y microbios
// ---------------------------------------------------------------------------

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
    ins.title = asciiFold(mineralInfo(rock.kind).grainName);
    ins.subtitle = "Mineral - no esta vivo";
    ins.blurb = asciiFold(mineralInfo(rock.kind).description);
    ins.headline = "inmovil, apoyado en el vidrio";
    InspectSection& s = section(ins, "Grano");
    add(s, "Tamano", fmt("%.0f x %.0f um", body.aabbMax.x - body.aabbMin.x, body.aabbMax.y - body.aabbMin.y));
    add(s, "Fijo", "apoyado en el vidrio: ni el agua ni las celulas lo mueven");
    add(s, "Agua", "no fluye adentro del grano: la corriente lo rodea");
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
        ins.title = asciiFold(mineralName(k));
        ins.subtitle = "Mineral - inerte";
        ins.blurb = asciiFold(mineralInfo(k).description);
        const bool settled = r >= 2.0f;
        ins.headline = settled ? "asentada en el fondo" : "en suspension";
        // Stokes: v = 2/9 * (rho_p - rho_w) * g * r^2 / mu  ->  0,00218 * delta_rho * r^2 [um/s, r en um].
        const float fall = 0.002182f * mineralInfo(k).densityGap * r * r;
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

Inspection inspectBacterium(const World& w, const Picked& picked) {
    Inspection ins;
    const Bacteria& b = w.bacteria();
    Picked p = picked;
    if (!resolveIndex(b.id, p)) {
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
    const bool vio = b.strain[i] == 1, bacillus = b.strain[i] == 2;
    ins.title = vio ? "Chromobacterium violaceum" : (bacillus ? "Bacillus subtilis" : "Bacteria (bacilo)");
    ins.subtitle = "Bacteria";
    ins.blurb = otherBlurb(vio ? OtherOrganism::Violaceum : (bacillus ? OtherOrganism::Bacillus : OtherOrganism::Bacteria));
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
    add(s, "Tincion de Gram", bacillus ? "grampositiva: pared gruesa de peptidoglicano, retiene el cristal violeta"
                                       : "gramnegativa: pared fina y membrana externa, pierde el cristal violeta y toma la safranina (rosa)");
    if (b.attached[i])
        add(s, "Biopelicula", "adherida al vidrio: no nada y los filtradores no la alcanzan (las amebas si)");
    if (b.resistant[i])
        add(s, "Defensa", "resistente a los fagos");
    if (vio)
        add(s, "Quorum", b.quorum[i] ? "alcanzado: produce violaceina (violeta, toxica)" : "no alcanzado: faltan vecinas");
    addStructures(ins, bacteriumPlan(b.strain[i]));
    if (b.phage[i] >= 0.0f)
        addStructures(ins, virusPlan(kSimulatedPhage), "Fago que la infecta");
    return ins;
}

Inspection inspectBdello(const World& w, const Picked& picked) {
    Inspection ins;
    const Bdellovibrios& d = w.bdellovibrios();
    Picked p = picked;
    if (!resolveIndex(d.id, p)) {
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
    addStructures(ins, bdellovibrioPlan());
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
    const FungusInfo& info = fungusInfo(kind);
    ins.blurb = otherBlurb(otherOf(kind));
    ins.headline = info.sense == SporeSense::Host ? "busca un alga hospedadora"
                   : info.sense == SporeSense::Food ? "sube el gradiente de materia organica"
                                                    : "espora inmovil: la lleva el agua";
    InspectSection& s = section(ins, "Zoospora");
    add(s, "Busca", info.sense == SporeSense::Host   ? "un alga: nada hacia la mas cercana (quimiotaxis)"
                    : info.sense == SporeSense::Food ? "materia organica disuelta (aminoacidos)"
                                                     : "un sustrato donde germinar");
    add(s, "Velocidad", info.sporeSpeed > 0.0f ? fmt("%.0f um/s", info.sporeSpeed) : std::string("0 (la arrastra el agua)"));
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
    std::size_t tips = 0, sporangia = 0;
    for (std::size_t k = 0; k < m->x.size(); ++k) {
        lo = std::min(lo, m->x[k]);
        hi = std::max(hi, m->x[k]);
        lo2 = std::min(lo2, m->y[k]);
        hi2 = std::max(hi2, m->y[k]);
        tips += m->tip[k];
        sporangia += m->sporangium[k] > 0.0f ? 1 : 0;
    }
    ins.radius = 0.5f * std::max(hi - lo, hi2 - lo2) + 10.0f;
    ins.title = fmt("Micelio de %s", fungusName(m->kind));
    ins.subtitle = fungusInfo(m->kind).group;
    ins.blurb = otherBlurb(otherOf(m->kind));
    ins.headline = fmt("%zu nodos, %zu puntas", m->x.size(), tips);
    InspectSection& s = section(ins, "Micelio");
    add(s, "Hifas", fmt("%zu nodos, %zu puntas en crecimiento", m->x.size(), tips));
    if (sporangia)
        add(s, "Zoosporangios", fmt("%zu madurando: liberaran zoosporas", sporangia));
    if (fungusInfo(m->kind).dichotomous)
        add(s, "Ramificacion", "dicotomica: la punta se parte en dos hifas");
    if (fungusInfo(m->kind).photobiont) {
        int algae = 0;
        for (const auto& [e, li] : w.registry().view<Lichenized>().each())
            algae += li.mycelium == m->id;
        add(s, "Fotobionte", algae > 0 ? fmt("%d algas verdes unidas: le dan azucar y reciben fosforo", algae)
                                       : std::string("ninguna: solo casi no crece, busca un alga con su quimiotropismo"));
    }
    add(s, "Biomasa", fmt("%.1f pmol (reservas %.1f)", m->biomass(fp), m->reserves));
    add(s, "Fosforo", fmt("%.3f pmol", m->phosphorus));
    if (m->starvingHours > 0.0f)
        add(s, "Hambre", fmt("%.1f h bio sin comer", m->starvingHours));
    return ins;
}

Inspection gone(const Picked& p, const char* subtitle) {
    Inspection ins;
    ins.valid = ins.dead = true;
    ins.title = "Ya no esta";
    ins.subtitle = subtitle;
    ins.at = p.at;
    return ins;
}

Inspection inspectPlasmodium(const World& w, const Picked& p) {
    const Plasmodium* pl = w.plasmodia().find(p.id);
    if (!pl || pl->x.empty())
        return gone(p, "El plasmodio se endurecio en esclerocio o murio");
    const PlasmodiumParams& pp = w.plasmodia().params;
    Inspection ins;
    const std::size_t node = std::min(p.node, pl->x.size() - 1);
    ins.valid = ins.live = true;
    ins.at = {pl->x[node], pl->y[node]};
    float lo = 1e30f, hi = -1e30f, lo2 = 1e30f, hi2 = -1e30f;
    std::size_t tips = 0, foods = 0;
    for (std::size_t k = 0; k < pl->x.size(); ++k) {
        lo = std::min(lo, pl->x[k]);
        hi = std::max(hi, pl->x[k]);
        lo2 = std::min(lo2, pl->y[k]);
        hi2 = std::max(hi2, pl->y[k]);
        tips += pl->tip[k];
        foods += pl->food[k] >= 0;
    }
    ins.radius = 0.5f * std::max(hi - lo, hi2 - lo2) + 10.0f;
    ins.title = "Physarum polycephalum";
    ins.subtitle = "Moho mucilaginoso (Amoebozoa): no es un hongo";
    ins.blurb = otherBlurb(OtherOrganism::Physarum);
    ins.headline = fmt("%zu nodos, %zu tubos, %zu ciclos", pl->x.size(), pl->edges.size(), PlasmodiumSystem::cycles(*pl));
    InspectSection& s = section(ins, "Plasmodio");
    add(s, "Red", fmt("%zu nodos y %zu tubos; %zu ciclos (anastomosis)", pl->x.size(), pl->edges.size(), PlasmodiumSystem::cycles(*pl)));
    add(s, "Largo total", fmt("%.0f um de tubos", PlasmodiumSystem::totalLength(*pl)));
    add(s, "Frente", fmt("%zu puntas explorando; %zu nodos tocando comida", tips, foods));
    add(s, "Biomasa", fmt("%.1f pmol (reservas %.1f)", pl->biomass(), pl->reserves));
    add(s, "Fosforo", fmt("%.3f pmol", pl->phosphorus));
    if (pl->starvingHours > 0.0f)
        add(s, "Hambre", fmt("%.1f h bio (a las %.0f h se endurece en esclerocio)", pl->starvingHours, pp.starveHoursToSclerotium));
    // El tubo más cercano al nodo elegido.
    float bestD = -1.0f, bestFlux = 0.0f;
    for (const Plasmodium::Edge& e : pl->edges)
        if ((static_cast<std::size_t>(e.a) == node || static_cast<std::size_t>(e.b) == node) && e.D > bestD) {
            bestD = e.D;
            bestFlux = e.flux;
        }
    if (bestD >= 0.0f)
        add(s, "Tubo elegido", fmt("conductividad %.2f (radio ~%.1f um), flujo %.2f del caudal", bestD, 5.0f * std::pow(bestD, 0.25f), bestFlux));
    add(s, "Como decide", "el tubo con mas flujo se ensancha; el que no lleva nada se retrae (Tero 2010)");
    return ins;
}

Inspection inspectFlake(const World& w, const Picked& p) {
    const OatFlakes& f = w.plasmodia().flakes;
    if (p.index >= f.size())
        return gone(p, "El copo se acabo");
    Inspection ins;
    ins.valid = true;
    ins.at = {f.x[p.index], f.y[p.index]};
    ins.radius = 12.0f;
    ins.title = "Copo de avena";
    ins.subtitle = "Cebo de almidon";
    ins.blurb = "El alimento clasico de los experimentos con Physarum: libera azucares al agua y el plasmodio lo envuelve y lo digiere.";
    ins.headline = fmt("%.1f pmol de almidon", f.pmol[p.index]);
    InspectSection& s = section(ins, "Copo");
    add(s, "Almidon", fmt("%.1f pmol", f.pmol[p.index]));
    add(s, "Fosforo", fmt("%.3f pmol", f.phosphorus[p.index]));
    return ins;
}

Inspection inspectResting(const World& w, const Picked& p) {
    const RestingSporangia& r = w.fungi().resting;
    if (p.index >= r.size())
        return gone(p, "El esporangio germino");
    Inspection ins;
    ins.valid = true;
    ins.at = {r.x[p.index], r.y[p.index]};
    ins.radius = 10.0f;
    ins.title = "Esporangio de resistencia";
    ins.subtitle = "Allomyces - pared gruesa y parda";
    ins.blurb = otherBlurb(OtherOrganism::Allomyces);
    ins.headline = fmt("duerme hace %.1f h bio", r.ageHours[p.index]);
    InspectSection& s = section(ins, "Esporangio");
    add(s, "Despierta", fmt("con mas de %.0f uM de materia organica", w.fungi().params.restingGerminateFood));
    add(s, "Reservas", fmt("%.2f pmol", r.pmol[p.index]));
    return ins;
}

Inspection inspectSclerotium(const World& w, const Picked& p) {
    const Sclerotia& s0 = w.plasmodia().sclerotia;
    if (p.index >= s0.size())
        return gone(p, "El esclerocio desperto");
    Inspection ins;
    ins.valid = true;
    ins.at = {s0.x[p.index], s0.y[p.index]};
    ins.radius = 12.0f;
    ins.title = "Esclerocio de Physarum";
    ins.subtitle = "Plasmodio endurecido y dormido";
    ins.blurb = "Con hambre o sequia el plasmodio se contrae en una masa dura y seca que puede vivir anios; con humedad y comida despierta y vuelve a explorar.";
    ins.headline = fmt("dormido hace %.1f h bio", s0.ageHours[p.index]);
    InspectSection& s = section(ins, "Esclerocio");
    add(s, "Despierta", fmt("con mas de %.0f uM de materia organica", w.plasmodia().params.sclerotiumGerminateFood));
    add(s, "Reservas", fmt("%.2f pmol", s0.pmol[p.index]));
    return ins;
}

} // namespace

// ---------------------------------------------------------------------------
// Elegir
// ---------------------------------------------------------------------------

Picked World::pick(Vec2 position, float tolerance, std::uint32_t skip) const {
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
        out = {};
        out.kind = Picked::Kind::Cell;
        out.entity = e;
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
    for (std::size_t i = 0; i < bacteria_.size() && !(skip & pickBit(Picked::Kind::Bacterium)); ++i)
        consider(Picked::Kind::Bacterium, i, 0, bacteria_.id[i], bacteria_.x[i], bacteria_.y[i], 1.0f);
    for (std::size_t i = 0; i < bdello_.size() && !(skip & pickBit(Picked::Kind::Bdellovibrio)); ++i)
        consider(Picked::Kind::Bdellovibrio, i, 0, bdello_.id[i], bdello_.x[i], bdello_.y[i], 0.8f);
    for (std::size_t i = 0; i < fungi_.spores.size(); ++i)
        consider(Picked::Kind::Zoospore, i, 0, 0, fungi_.spores.x[i], fungi_.spores.y[i], 2.5f);
    for (std::size_t f = 0; f < cyano_.filaments.size(); ++f) {
        const CyanoFilament& fil = cyano_.filaments[f];
        for (std::size_t k = 0; k < fil.x.size(); ++k)
            consider(Picked::Kind::Cyanobacteria, f, k, 0, fil.x[k], fil.y[k], 2.5f);
    }
    for (const Mycelium& m : fungi_.mycelia) {
        const float radius = 0.5f * fungusInfo(m.kind).hyphaWidth;
        for (std::size_t k = 0; k < m.x.size(); ++k)
            consider(Picked::Kind::Hypha, 0, k, m.id, m.x[k], m.y[k], radius);
    }
    for (std::size_t i = 0; i < fungi_.resting.size(); ++i)
        consider(Picked::Kind::Resting, i, 0, 0, fungi_.resting.x[i], fungi_.resting.y[i], 5.0f);
    for (const Plasmodium& pl : plasmo_.plasmodia)
        for (std::size_t k = 0; k < pl.x.size(); ++k)
            consider(Picked::Kind::Plasmodium, 0, k, pl.id, pl.x[k], pl.y[k], 4.0f);
    for (std::size_t i = 0; i < plasmo_.flakes.size(); ++i)
        consider(Picked::Kind::OatFlake, i, 0, 0, plasmo_.flakes.x[i], plasmo_.flakes.y[i], 9.0f);
    for (std::size_t i = 0; i < plasmo_.sclerotia.size(); ++i)
        consider(Picked::Kind::Sclerotium, i, 0, 0, plasmo_.sclerotia.x[i], plasmo_.sclerotia.y[i], 7.0f);
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
    case Picked::Kind::Plasmodium: return inspectPlasmodium(world, picked);
    case Picked::Kind::OatFlake: return inspectFlake(world, picked);
    case Picked::Kind::Resting: return inspectResting(world, picked);
    case Picked::Kind::Sclerotium: return inspectSclerotium(world, picked);
    case Picked::Kind::Particle: return inspectParticle(world, picked);
    case Picked::Kind::None: break;
    }
    return {};
}

bool locatePicked(const World& world, Picked& picked, Vec2& at) {
    const entt::registry& reg = world.registry();
    switch (picked.kind) {
    case Picked::Kind::Cell:
    case Picked::Kind::Rock:
        if (!reg.valid(picked.entity) || !reg.all_of<RingBody>(picked.entity))
            return false;
        at = reg.get<RingBody>(picked.entity).centroid;
        return true;
    case Picked::Kind::Bacterium:
        if (!resolveIndex(world.bacteria().id, picked))
            return false;
        at = {world.bacteria().x[picked.index], world.bacteria().y[picked.index]};
        return true;
    case Picked::Kind::Bdellovibrio:
        if (!resolveIndex(world.bdellovibrios().id, picked))
            return false;
        at = {world.bdellovibrios().x[picked.index], world.bdellovibrios().y[picked.index]};
        return true;
    default:
        return false;
    }
}
