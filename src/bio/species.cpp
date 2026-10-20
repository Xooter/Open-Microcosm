#include "bio/species.hpp"

#include "bio/silhouette.hpp"

#include "core/ids.hpp"
#include "core/pack_files.hpp"
#include "core/toml_reader.hpp"
#include "core/units.hpp"

#include <toml++/toml.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <deque>
#include <mutex>

// Registro de los tipos de ser (fase 20.0: el contenido como datos). Los seres vienen de los
// paquetes de contenido; los de fábrica, de assets/content/base/beings/. Se guardan en un
// deque: agregar los de un paquete no mueve los ya cargados (las referencias siguen valiendo).

namespace {

struct Entry {
    SpeciesInfo info;
    std::string key, name, description, plan, style, pack;
    std::string source; // el [[being]] como se leyó (para exportarlo)
    std::vector<std::string> legacyIds; // ids viejos (core/ids)
    // Referencias sin resolver (ids o etiquetas): se resuelven contra todo lo cargado.
    std::vector<std::string> prey, mouthPrey, flee, morphPrey, symbiontSource;
};

// Estado del registro en estáticos locales: otras unidades piden especies al inicializar sus
// propios globales (orden de inicialización entre archivos no definido).
struct Registry {
    std::deque<Entry> entries;
    std::vector<std::string> warnings;
    std::recursive_mutex mutex;
    bool baseLoaded = false;
    int batch = 0; // SpeciesLoadBatch abiertos: las referencias se resuelven al cerrarse el último
    // Ya cargado: se lee sin el candado (lo consultan los bucles paralelos de cada paso).
    std::atomic<bool> ready{false};
};
Registry& registry() {
    static Registry r;
    return r;
}
#define gEntries registry().entries
#define gWarnings registry().warnings
#define gMutex registry().mutex
#define gBaseLoaded registry().baseLoaded

void ensureBase();

const char* const kGeneKeys[] = {"size",      "motor",    "mitochondria", "chloroplasts", "lysosomes",
                                 "trichocysts", "armor",   "rigidity",      "mating_type"};
static_assert(sizeof(kGeneKeys) / sizeof(kGeneKeys[0]) == kGeneCount);

constexpr std::pair<const char*, LocomotionType> kLocomotion[] = {
    {"cilia", LocomotionType::Cilia}, {"flagellum", LocomotionType::Flagellum}, {"pseudopod", LocomotionType::Pseudopod},
    {"crawl", LocomotionType::Crawl}, {"none", LocomotionType::None}};
constexpr std::pair<const char*, FeedingMode> kFeeding[] = {
    {"oral_groove", FeedingMode::OralGroove}, {"anterior_pole", FeedingMode::Anterior},
    {"phagocytosis", FeedingMode::Phagocytosis}, {"raptorial", FeedingMode::Raptorial},
    {"tentacles", FeedingMode::Tentacles},  {"neck", FeedingMode::NeckStrike},
    {"osmotrophy", FeedingMode::Osmotroph},  {"axopodia", FeedingMode::Axopodia},
    {"perforation", FeedingMode::Perforation}, {"none", FeedingMode::None}};
constexpr std::pair<const char*, Storage> kStorage[] = {{"none", Storage::None},
                                                        {"starch", Storage::Starch},
                                                        {"glycogen", Storage::Glycogen},
                                                        {"paramylon", Storage::Paramylon},
                                                        {"chrysolaminarin", Storage::Chrysolaminarin}};
constexpr std::pair<const char*, CellWall> kWall[] = {{"none", CellWall::None},
                                                      {"cellulose", CellWall::Cellulose},
                                                      {"silica", CellWall::Silica},
                                                      {"yeast_glucan", CellWall::YeastGlucan}};

// Claves conocidas de un [[being]] y de su [being.shape]: una clave que no esté acá es casi siempre un
// error de tipeo, y se avisa (no se ignora en silencio).
constexpr const char* kBeingKeys[] = {
    "id", "legacy_ids", "name", "description", "body_plan", "tags", "locomotion", "feeding", "relative_speed", "helix_radius", "helix_period", "flagellar_gliding", "flagellum", "prey", "mouth_prey",
    "flees_from", "chemokinesis", "acid_trap", "phototaxis", "relative_photosynthesis", "fissions_per_cycle",
    "encysts", "conjugation", "initial_count", "sessile", "stalk_length", "spasmoneme", "walks", "tentacle_reach",
    "neck_reach", "handling_hours", "chloroplasts", "raphe_glider", "b12_auxotroph", "frustule",
    "filament", "filament_cells", "fragmentation_per_hour", "zoospores", "daughter_colonies", "colony_cells",
    "mother_dies_at_release", "relative_dry_mass", "budding", "anaerobe", "posterior_mouth", "mouth_width", "divides_in_cyst", "lorica_iron_ratio", "lorica_hours", "perforation_hours", "sliding_colony", "testate", "cyclosis", "ph",
    "temperature", "max_salinity", "storage", "wall", "genome", "shape", "appearance", "cell", "process", "response", "jump", "contraction", "shape_wave", "bioluminescence", "photosymbiont", "signal_toxicity", "stalk", "toxicysts", "histophagous", "release", "relay", "chemotaxis", "morph", "light_damage",
    "stress_pigment"};
constexpr const char* kProcessKeys[] = {"kind",  "at",    "length", "base_width", "tip_width",
                                        "curve", "count", "spread", "body",       "fillet"};
constexpr const char* kResponseKeys[] = {"field", "kind", "action", "threshold", "min_level", "memory", "refractory"};
constexpr std::pair<const char*, ResponseField> kResponseField[] = {
    {"light", ResponseField::Light}, {"oxygen", ResponseField::Oxygen}, {"shear", ResponseField::Shear},
    {"touch", ResponseField::Touch}};
constexpr std::pair<const char*, ResponseKind> kResponseKind[] = {
    {"step_up", ResponseKind::StepUp}, {"step_down", ResponseKind::StepDown}, {"above", ResponseKind::Above}};
constexpr std::pair<const char*, ResponseAction> kResponseAction[] = {
    {"avoid", ResponseAction::Avoid}, {"jump", ResponseAction::Jump}, {"contract", ResponseAction::Contract},
    {"flash", ResponseAction::Flash}};
constexpr const char* kSymbiontKeys[] = {"kind",           "source",          "per_prey", "max", "initial",
                                         "growth_per_hour", "decay_per_hour", "photosynthesis"};
constexpr const char* kBiolumKeys[] = {"flash_s", "recharge_s", "circadian", "day_capacity", "color"};
constexpr const char* kContractionKeys[] = {"extension",          "extension_s",        "profile", "foot_width",
                                           "flare",              "swimming_extension", "spontaneous_per_min",
                                           "swimming_squareness", "aboral_first"};
constexpr const char* kShapeWaveKeys[] = {"kind",  "amplitude",         "period_s",       "wavelength",
                                         "reach", "episodes_per_min", "episode_s"};
constexpr std::pair<const char*, ShapeWaveKind> kShapeWaveKind[] = {{"metaboly", ShapeWaveKind::Metaboly},
                                                                    {"sway", ShapeWaveKind::Sway}};
constexpr std::pair<const char*, ContractionProfile> kContractionProfile[] = {{"uniform", ContractionProfile::Uniform},
                                                                              {"trumpet", ContractionProfile::Trumpet},
                                                                              {"bell", ContractionProfile::Bell}};
constexpr const char* kToxicystKeys[] = {"reach", "paralysis_hours", "lethal"};
constexpr const char* kReleaseKeys[] = {"signal", "rate", "diffusion", "half_life_hours", "bio_time"};
constexpr const char* kRelayKeys[] = {"threshold", "pulse", "refractory_minutes", "pacemaker_fraction",
                                      "pacemaker_minutes", "competence_hours"};
constexpr const char* kMorphKeys[] = {"name",        "signal",       "threshold", "hours", "width_scale",
                                      "length_scale", "area_scale",  "feeding",   "prey",  "mouth_width"};
constexpr const char* kStalkKeys[] = {"kind",        "length",         "width",          "bending_rate",
                                      "growth_hours", "decay_hours",   "contracted_fraction", "extension_s",
                                      "spontaneous_per_min", "detaches", "colony", "max_zooids", "branch_angle",
                                      "shared_spasmoneme"};
constexpr std::pair<const char*, StalkKind> kStalkKind[] = {{"spasmoneme", StalkKind::Spasmoneme},
                                                            {"rigid", StalkKind::Rigid},
                                                            {"mucilage", StalkKind::Mucilage},
                                                            {"foot", StalkKind::Foot}};
constexpr const char* kJumpKeys[] = {"speed", "duration", "interval"};
constexpr const char* kLightDamageKeys[] = {"threshold", "stress"};
constexpr const char* kPigmentKeys[] = {"rate_per_hour", "decay_per_hour", "light_threshold", "protection", "color"};
constexpr std::pair<const char*, ProcessKind> kProcessKind[] = {
    {"horn", ProcessKind::Horn}, {"spine", ProcessKind::Spine}, {"arm", ProcessKind::Arm}, {"seta", ProcessKind::Seta},
    {"notch", ProcessKind::Notch}};
constexpr const char* kAnimalCellKeys[] = {"adherent", "rounded_radius", "contact_inhibition_locomotion",
                                           "contact_inhibition_proliferation", "min_serum",
                                           "apoptosis_without_serum_h", "divides", "fmlp_chemotaxis"};
constexpr const char* kLookKeys[] = {"style", "plastid_color", "pigment_stripes"};
constexpr const char* kShapeKeys[] = {"semi_major_axis", "semi_minor_axis", "shape_memory", "color",
                                      "nucleus",        "squareness",    "taper",  "taper_front", "bend", "thickness"};

using toml_io::known;
using toml_io::Reader;

// Ancho mínimo de Feret (calibre mínimo) de la silueta, o el eje menor de la elipse del cuerpo.
float minimumWidth(const SpeciesInfo& s) {
    if (s.outline.empty())
        return 2.0f * std::min(s.spec.radiusX, s.spec.radiusY);
    float best = 1e9f;
    for (int k = 0; k < 180; ++k) {
        const float a = units::kPi * static_cast<float>(k) / 180.0f;
        const Vec2 u = {std::cos(a), std::sin(a)};
        float lo = 1e9f, hi = -1e9f;
        for (const Vec2& q : s.outline) {
            lo = std::min(lo, dot(q, u));
            hi = std::max(hi, dot(q, u));
        }
        best = std::min(best, hi - lo);
    }
    return best;
}

// Un [[being]] → Entry. Devuelve false si tiene errores (se descarta).
bool readBeing(const toml::table& t, const std::string& source, const std::string& pack, Entry& e,
               std::vector<std::string>& warnings) {
    const auto idNode = t.get("id");
    const auto nameNode = t.get("name");
    if (!idNode || !idNode->value<std::string>() || !nameNode || !nameNode->value<std::string>()) {
        warnings.push_back(source + ": un [[being]] sin 'id' o sin 'name': se saltea");
        return false;
    }
    e.key = *idNode->value<std::string>();
    e.name = *nameNode->value<std::string>();
    e.pack = pack;
    if (const toml::node* d = t.get("description"))
        e.description = d->value<std::string>().value_or("");
    if (const toml::node* d = t.get("body_plan"))
        e.plan = d->value<std::string>().value_or("");
    Reader r{t, source + " (" + e.key + ")", {}};
    r.strings("legacy_ids", e.legacyIds);
    for (const auto& [k, v] : t)
        if (!known(k.str(), kBeingKeys))
            warnings.push_back(r.where + ": clave desconocida '" + std::string(k.str()) + "'");
    SpeciesInfo& s = e.info;
    r.choice("locomotion", kLocomotion, s.locomotion, true);
    r.choice("feeding", kFeeding, s.feeding);
    r.num("relative_speed", s.speedScale);
    r.count("helix_radius", s.helixRadius);
    r.count("helix_period", s.helixPeriod);
    r.flag("flagellar_gliding", s.flagellarGliding);
    if (const toml::node* fl = t.get("flagellum")) {
        const std::string where = fl->value<std::string>().value_or("");
        if (where == "posterior")
            s.flagellumPosterior = true;
        else if (where != "anterior")
            r.errors.push_back(r.where + ": 'flagellum' es \"anterior\" o \"posterior\"");
    }
    r.strings("prey", e.prey);
    r.strings("mouth_prey", e.mouthPrey);
    r.strings("flees_from", e.flee);
    r.strings("tags", s.tags);
    r.flag("chemokinesis", s.chemokinesis);
    r.flag("acid_trap", s.acidTrap);
    r.flag("phototaxis", s.phototaxis);
    r.num("relative_photosynthesis", s.photosynthesisScale);
    r.num("fissions_per_cycle", s.fissions);
    r.flag("encysts", s.encysts);
    r.flag("conjugation", s.conjugates);
    r.num("initial_count", s.initialCount);
    r.flag("sessile", s.sessile);
    r.num("stalk_length", s.stalkLength);
    r.flag("spasmoneme", s.contractileStalk);
    r.flag("walks", s.walker);
    r.num("tentacle_reach", s.tentacleReach);
    r.num("neck_reach", s.neckReach);
    r.num("handling_hours", s.handlingHours);
    r.flag("chloroplasts", s.chloroplasts);
    r.flag("raphe_glider", s.glider);
    r.flag("b12_auxotroph", s.vitaminB12);
    r.flag("frustule", s.frustule);
    r.flag("filament", s.filament);
    r.num("filament_cells", s.filamentCells);
    r.num("fragmentation_per_hour", s.fragmentationPerHour);
    r.num("zoospores", s.zoosporeChance);
    r.num("daughter_colonies", s.colonyDaughters);
    r.num("colony_cells", s.colonyCells);
    r.flag("mother_dies_at_release", s.motherDiesAtRelease);
    r.num("relative_dry_mass", s.dryMassScale);
    r.flag("budding", s.budding);
    r.flag("anaerobe", s.anaerobe);
    r.flag("posterior_mouth", s.mouthAtRear);
    r.flag("histophagous", s.histophagous);
    r.count("mouth_width", s.mouthWidth);
    r.flag("divides_in_cyst", s.dividesInCyst);
    r.count("lorica_iron_ratio", s.loricaIronRatio);
    r.count("lorica_hours", s.loricaHours);
    r.count("perforation_hours", s.perforationHours);
    if (const toml::node* tn = t.get("testate")) {
        const std::string kind = tn->value<std::string>().value_or("");
        s.testate = kind == "lobose" ? 1 : kind == "filose" ? 2 : 0;
        if (s.testate == 0)
            r.errors.push_back(r.where + ": 'testate' es \"lobose\" o \"filose\"");
    }
    if (const toml::table* sc = r.table("sliding_colony")) {
        constexpr const char* kKeys[] = {"max_cells", "amplitude", "period_s"};
        toml_io::warnUnknown(*sc, kKeys, r.where + " [sliding_colony]", warnings);
        Reader cr{*sc, r.where + " [sliding_colony]", {}};
        cr.count("max_cells", s.slidingColonyMax);
        cr.count("amplitude", s.slidingAmplitude);
        cr.count("period_s", s.slidingPeriod);
        if (s.slidingColonyMax < 2 || s.slidingPeriod <= 0.0f)
            cr.errors.push_back(cr.where + ": 'max_cells' al menos 2 y 'period_s' positivo");
        r.errors.insert(r.errors.end(), cr.errors.begin(), cr.errors.end());
    }
    r.num("cyclosis", s.cyclosis);
    r.range("ph", s.phMin, s.phMax);
    r.range("temperature", s.tMin, s.tMax);
    r.num("max_salinity", s.salinityMax);
    r.choice("storage", kStorage, s.chemistry.storage);
    r.choice("wall", kWall, s.chemistry.wall);
    if (const toml::node* g = t.get("genome")) {
        if (const toml::table* gt = g->as_table()) {
            for (const auto& [k, v] : *gt) {
                int gene = -1;
                for (std::size_t i = 0; i < kGeneCount; ++i)
                    if (k.str() == kGeneKeys[i])
                        gene = static_cast<int>(i);
                if (gene < 0 || !v.value<double>()) {
                    r.errors.push_back(r.where + ": gen desconocido o sin valor numérico en 'genome': " +
                                       std::string(k.str()));
                    continue;
                }
                s.genome.push_back({static_cast<Gene>(gene), static_cast<float>(*v.value<double>())});
            }
        } else {
            r.errors.push_back(r.where + ": 'genome' tiene que ser una tabla {gen = valor}");
        }
    }
    if (const toml::table* cell = r.table("cell")) {
        AnimalCellInfo& a = s.animalCell;
        a.present = true;
        toml_io::warnUnknown(*cell, kAnimalCellKeys, r.where + " [cell]", warnings);
        toml_io::Reader cr{*cell, r.where + " [cell]", {}};
        cr.flag("adherent", a.adherent);
        cr.count("rounded_radius", a.roundRadius);
        cr.flag("contact_inhibition_locomotion", a.contactLocomotion);
        cr.flag("contact_inhibition_proliferation", a.contactProliferation);
        cr.count("min_serum", a.serumMinimum);
        cr.count("apoptosis_without_serum_h", a.apoptosisWithoutSerumHours);
        cr.flag("divides", a.cycles);
        cr.flag("fmlp_chemotaxis", a.chemotaxisFmlp);
        if (a.roundRadius <= 0.0f)
            cr.errors.push_back(cr.where + ": 'rounded_radius' tiene que ser positivo");
        r.errors.insert(r.errors.end(), cr.errors.begin(), cr.errors.end());
    }
    if (const toml::node* pn = t.get("process")) {
        const toml::array* arr = pn->as_array();
        if (!arr) {
            r.errors.push_back(r.where + ": 'process' tiene que ser una lista ([[being.process]])");
        } else {
            for (const toml::node& item : *arr) {
                const toml::table* pt = item.as_table();
                if (!pt) {
                    r.errors.push_back(r.where + ": cada [[being.process]] tiene que ser una tabla");
                    continue;
                }
                toml_io::warnUnknown(*pt, kProcessKeys, r.where + " [process]", warnings);
                Reader pr{*pt, r.where + " [process]", {}};
                ProcessSpec p;
                float atDeg = 0.0f, curveDeg = 0.0f, spreadDeg = 0.0f;
                pr.choice("kind", kProcessKind, p.kind, true);
                pr.num("at", atDeg);
                pr.count("length", p.length);
                pr.count("base_width", p.baseWidth);
                pr.count("tip_width", p.tipWidth);
                pr.num("curve", curveDeg);
                pr.count("count", p.count);
                pr.count("spread", spreadDeg);
                // Por omisión, cuernos y brazos son del cuerpo y espinas y setas, de la cubierta.
                p.inBody = p.kind == ProcessKind::Horn || p.kind == ProcessKind::Arm;
                pr.flag("body", p.inBody);
                if (p.kind == ProcessKind::Notch)
                    p.inBody = true; // una escotadura es siempre del cuerpo
                pr.count("fillet", p.fillet);
                if (p.length <= 0.0f || p.baseWidth <= 0.0f || p.count < 1)
                    pr.errors.push_back(pr.where + ": 'length' y 'base_width' tienen que ser positivos y 'count' al menos 1");
                p.at = atDeg * units::kPi / 180.0f;
                p.curve = curveDeg * units::kPi / 180.0f;
                p.spread = spreadDeg * units::kPi / 180.0f;
                r.errors.insert(r.errors.end(), pr.errors.begin(), pr.errors.end());
                s.processes.push_back(p);
            }
        }
    }
    if (const toml::node* rn = t.get("response")) {
        const toml::array* arr = rn->as_array();
        if (!arr) {
            r.errors.push_back(r.where + ": 'response' tiene que ser una lista ([[being.response]])");
        } else {
            for (const toml::node& item : *arr) {
                const toml::table* rt = item.as_table();
                if (!rt) {
                    r.errors.push_back(r.where + ": cada [[being.response]] tiene que ser una tabla");
                    continue;
                }
                toml_io::warnUnknown(*rt, kResponseKeys, r.where + " [response]", warnings);
                Reader rr{*rt, r.where + " [response]", {}};
                ResponseSpec resp;
                rr.choice("field", kResponseField, resp.field, true);
                rr.choice("kind", kResponseKind, resp.kind, true);
                rr.choice("action", kResponseAction, resp.action);
                rr.count("threshold", resp.threshold);
                rr.count("min_level", resp.minLevel);
                rr.count("memory", resp.memory);
                rr.count("refractory", resp.refractory);
                if (resp.memory <= 0.0f)
                    rr.errors.push_back(rr.where + ": 'memory' tiene que ser positivo");
                r.errors.insert(r.errors.end(), rr.errors.begin(), rr.errors.end());
                s.responses.push_back(resp);
            }
            if (s.responses.size() > kMaxResponses)
                r.errors.push_back(r.where + ": a lo sumo " + std::to_string(kMaxResponses) + " [[being.response]]");
        }
    }
    if (const toml::table* jt = r.table("jump")) {
        toml_io::warnUnknown(*jt, kJumpKeys, r.where + " [jump]", warnings);
        Reader jr{*jt, r.where + " [jump]", {}};
        jr.count("speed", s.jump.speed);
        jr.count("duration", s.jump.duration);
        jr.count("interval", s.jump.interval);
        if (s.jump.speed <= 0.0f || s.jump.duration <= 0.0f)
            jr.errors.push_back(jr.where + ": 'speed' y 'duration' tienen que ser positivos");
        r.errors.insert(r.errors.end(), jr.errors.begin(), jr.errors.end());
    }
    if (const toml::table* rt = r.table("release")) {
        toml_io::warnUnknown(*rt, kReleaseKeys, r.where + " [release]", warnings);
        Reader rr{*rt, r.where + " [release]", {}};
        rr.text("signal", s.release.signal);
        rr.count("rate", s.release.rate);
        rr.count("diffusion", s.release.diffusion);
        rr.count("half_life_hours", s.release.halfLifeHours);
        rr.flag("bio_time", s.release.bioTime);
        if (s.release.signal.empty() || s.release.halfLifeHours <= 0.0f)
            rr.errors.push_back(rr.where + ": hace falta 'signal' y 'half_life_hours' positivo");
        r.errors.insert(r.errors.end(), rr.errors.begin(), rr.errors.end());
    }
    if (const toml::table* lt = r.table("relay")) {
        toml_io::warnUnknown(*lt, kRelayKeys, r.where + " [relay]", warnings);
        Reader lr{*lt, r.where + " [relay]", {}};
        RelaySpec& rl = s.relay;
        rl.present = true;
        lr.count("threshold", rl.threshold);
        lr.count("pulse", rl.pulse);
        lr.count("refractory_minutes", rl.refractoryMinutes);
        lr.count("pacemaker_fraction", rl.pacemakerFraction);
        lr.count("pacemaker_minutes", rl.pacemakerMinutes);
        lr.count("competence_hours", rl.competenceHours);
        if (s.release.signal.empty())
            lr.errors.push_back(lr.where + ": el relevo necesita un [being.release] con la señal");
        r.errors.insert(r.errors.end(), lr.errors.begin(), lr.errors.end());
    }
    if (const toml::table* ct = r.table("chemotaxis")) {
        constexpr const char* kKeys[] = {"signal"};
        toml_io::warnUnknown(*ct, kKeys, r.where + " [chemotaxis]", warnings);
        Reader cr{*ct, r.where + " [chemotaxis]", {}};
        cr.text("signal", s.chemotaxis.signal);
        r.errors.insert(r.errors.end(), cr.errors.begin(), cr.errors.end());
    }
    if (const toml::table* mt = r.table("morph")) {
        toml_io::warnUnknown(*mt, kMorphKeys, r.where + " [morph]", warnings);
        Reader mr{*mt, r.where + " [morph]", {}};
        MorphSpec& m = s.morph;
        m.present = true;
        mr.text("name", m.name);
        mr.text("signal", m.signal);
        mr.count("threshold", m.threshold);
        mr.count("hours", m.hours);
        mr.count("width_scale", m.widthScale);
        mr.count("length_scale", m.lengthScale);
        mr.count("area_scale", m.areaScale);
        if (mt->get("feeding")) {
            m.feedingSet = true;
            mr.choice("feeding", kFeeding, m.feeding);
        }
        if (mt->get("prey")) {
            m.preySet = true;
            mr.strings("prey", e.morphPrey);
        }
        if (mt->get("mouth_width"))
            mr.count("mouth_width", m.mouthWidth);
        if (m.signal.empty() || m.hours <= 0.0f || m.widthScale <= 0.0f || m.lengthScale <= 0.0f || m.areaScale <= 0.0f)
            mr.errors.push_back(mr.where + ": hace falta 'signal', y 'hours' y las escalas positivas");
        r.errors.insert(r.errors.end(), mr.errors.begin(), mr.errors.end());
    }
    if (const toml::table* tt = r.table("toxicysts")) {
        toml_io::warnUnknown(*tt, kToxicystKeys, r.where + " [toxicysts]", warnings);
        Reader tr{*tt, r.where + " [toxicysts]", {}};
        tr.count("reach", s.toxicysts.reach);
        tr.count("paralysis_hours", s.toxicysts.paralysisHours);
        tr.flag("lethal", s.toxicysts.lethal);
        if (s.toxicysts.reach <= 0.0f || s.toxicysts.paralysisHours <= 0.0f)
            tr.errors.push_back(tr.where + ": 'reach' y 'paralysis_hours' tienen que ser positivos");
        r.errors.insert(r.errors.end(), tr.errors.begin(), tr.errors.end());
    }
    if (const toml::table* tt = r.table("signal_toxicity")) {
        constexpr const char* kKeys[] = {"signal", "ec50"};
        toml_io::warnUnknown(*tt, kKeys, r.where + " [signal_toxicity]", warnings);
        Reader tr{*tt, r.where + " [signal_toxicity]", {}};
        tr.text("signal", s.signalToxicity.signal);
        tr.count("ec50", s.signalToxicity.ec50);
        if (s.signalToxicity.signal.empty() || s.signalToxicity.ec50 <= 0.0f)
            tr.errors.push_back(tr.where + ": hace falta 'signal' y 'ec50' positivo");
        r.errors.insert(r.errors.end(), tr.errors.begin(), tr.errors.end());
    }
    if (const toml::table* st = r.table("photosymbiont")) {
        toml_io::warnUnknown(*st, kSymbiontKeys, r.where + " [photosymbiont]", warnings);
        Reader sr{*st, r.where + " [photosymbiont]", {}};
        PhotosymbiontSpec& ps = s.photosymbiont;
        ps.present = true;
        std::string kind;
        sr.text("kind", kind);
        if (kind == "kleptoplasts")
            ps.kleptoplasts = true;
        else if (kind != "endosymbionts")
            sr.errors.push_back(sr.where + ": 'kind' es \"endosymbionts\" o \"kleptoplasts\"");
        sr.strings("source", e.symbiontSource);
        sr.count("per_prey", ps.perPrey);
        sr.count("max", ps.max);
        sr.count("initial", ps.initial);
        sr.count("growth_per_hour", ps.growthPerHour);
        sr.count("decay_per_hour", ps.decayPerHour);
        sr.count("photosynthesis", ps.photosynthesis);
        if (ps.max <= 0.0f || ps.initial > ps.max)
            sr.errors.push_back(sr.where + ": 'max' tiene que ser positivo y 'initial' no pasarlo");
        r.errors.insert(r.errors.end(), sr.errors.begin(), sr.errors.end());
    }
    if (const toml::table* bt = r.table("bioluminescence")) {
        toml_io::warnUnknown(*bt, kBiolumKeys, r.where + " [bioluminescence]", warnings);
        Reader br{*bt, r.where + " [bioluminescence]", {}};
        BioluminescenceSpec& bl = s.bioluminescence;
        bl.present = true;
        br.count("flash_s", bl.flashSeconds);
        br.count("recharge_s", bl.rechargeSeconds);
        br.flag("circadian", bl.circadian);
        br.count("day_capacity", bl.dayCapacity);
        if (const toml::node* c = bt->get("color")) {
            const toml::array* a = c->as_array();
            if (!a || a->size() != 3)
                br.errors.push_back(br.where + ": 'color' tiene que ser [r, g, b] (0–255)");
            else
                for (std::size_t i = 0; i < 3; ++i)
                    bl.color[i] = static_cast<std::uint8_t>(
                        std::clamp((*a)[i].value<std::int64_t>().value_or(0), std::int64_t{0}, std::int64_t{255}));
        }
        if (bl.flashSeconds <= 0.0f || bl.rechargeSeconds <= 0.0f)
            br.errors.push_back(br.where + ": 'flash_s' y 'recharge_s' tienen que ser positivos");
        r.errors.insert(r.errors.end(), br.errors.begin(), br.errors.end());
    }
    if (const toml::table* ct = r.table("contraction")) {
        toml_io::warnUnknown(*ct, kContractionKeys, r.where + " [contraction]", warnings);
        Reader cr{*ct, r.where + " [contraction]", {}};
        cr.count("extension", s.contraction.extension);
        cr.count("extension_s", s.contraction.extensionSeconds);
        cr.choice("profile", kContractionProfile, s.contraction.profile, false);
        cr.count("foot_width", s.contraction.footWidth);
        cr.count("flare", s.contraction.flare);
        cr.count("swimming_extension", s.contraction.swimmingExtension);
        cr.count("spontaneous_per_min", s.contraction.spontaneousPerMinute);
        cr.count("swimming_squareness", s.contraction.swimmingSquareness);
        cr.flag("aboral_first", s.contraction.aboralFirst);
        if (s.contraction.extension < 1.0f || s.contraction.extensionSeconds <= 0.0f)
            cr.errors.push_back(cr.where + ": 'extension' tiene que ser al menos 1 y 'extension_s' positivo");
        if (s.contraction.footWidth <= 0.0f || s.contraction.footWidth > 1.0f || s.contraction.flare < 0.0f || s.contraction.swimmingExtension < 0.0f ||
            s.contraction.swimmingExtension > 1.0f || s.contraction.spontaneousPerMinute < 0.0f)
            cr.errors.push_back(cr.where + ": 'foot_width' va de 0 a 1, 'flare' no es negativo, 'swimming_extension' de 0 a 1 y "
                                           "'spontaneous_per_min' no es negativo");
        r.errors.insert(r.errors.end(), cr.errors.begin(), cr.errors.end());
    }
    if (const toml::table* wt = r.table("shape_wave")) {
        toml_io::warnUnknown(*wt, kShapeWaveKeys, r.where + " [shape_wave]", warnings);
        Reader wr{*wt, r.where + " [shape_wave]", {}};
        ShapeWaveSpec& w = s.shapeWave;
        wr.choice("kind", kShapeWaveKind, w.kind, true);
        wr.count("amplitude", w.amplitude);
        wr.count("period_s", w.periodSeconds);
        wr.count("wavelength", w.wavelength);
        wr.count("reach", w.reach);
        wr.count("episodes_per_min", w.episodesPerMinute);
        wr.count("episode_s", w.episodeSeconds);
        if (w.amplitude < 0.0f || w.amplitude > 1.0f || w.periodSeconds <= 0.0f || w.wavelength <= 0.0f ||
            w.reach <= 0.0f || w.reach > 1.0f || w.episodesPerMinute < 0.0f || w.episodeSeconds <= 0.0f)
            wr.errors.push_back(wr.where + ": 'amplitude' y 'reach' van de 0 a 1; 'period_s', 'wavelength' y "
                                           "'episode_s' son positivos");
        if (w.kind != ShapeWaveKind::None && s.contraction.extension > 1.0f)
            wr.errors.push_back(wr.where + ": un ser con [being.contraction] no puede tener onda de forma");
        r.errors.insert(r.errors.end(), wr.errors.begin(), wr.errors.end());
    }
    // Tallo: las claves viejas (sessile, stalk_length, spasmoneme) dan un tallo con los valores por
    // omisión de su tipo; [being.stalk] lo describe entero.
    if (s.sessile) {
        s.stalk.kind = s.contractileStalk ? StalkKind::Spasmoneme : StalkKind::Rigid;
        s.stalk.length = s.stalkLength;
        if (s.contractileStalk) {
            s.stalk.contractedFraction = 0.35f;
            s.stalk.bendingRate = 3.0f;
        }
    }
    if (const toml::table* st = r.table("stalk")) {
        toml_io::warnUnknown(*st, kStalkKeys, r.where + " [stalk]", warnings);
        Reader sr{*st, r.where + " [stalk]", {}};
        StalkSpec& sp = s.stalk;
        sr.choice("kind", kStalkKind, sp.kind, true);
        sr.count("length", sp.length);
        sr.count("width", sp.width);
        sr.count("bending_rate", sp.bendingRate);
        sr.count("growth_hours", sp.growthHours);
        sr.count("decay_hours", sp.decayHours);
        sr.count("contracted_fraction", sp.contractedFraction);
        sr.count("extension_s", sp.extensionSeconds);
        sr.count("spontaneous_per_min", sp.spontaneousPerMinute);
        sr.flag("detaches", sp.detaches);
        sr.flag("colony", sp.colony);
        sr.count("max_zooids", sp.maxZooids);
        float branchDeg = sp.branchAngle * 180.0f / units::kPi;
        sr.count("branch_angle", branchDeg);
        sp.branchAngle = branchDeg * units::kPi / 180.0f;
        sr.flag("shared_spasmoneme", sp.sharedSpasmoneme);
        if (sp.kind != StalkKind::Foot && sp.length <= 0.0f)
            sr.errors.push_back(sr.where + ": un tallo necesita 'length' positivo");
        if (sp.contractedFraction <= 0.0f || sp.contractedFraction > 1.0f || sp.extensionSeconds <= 0.0f)
            sr.errors.push_back(sr.where + ": 'contracted_fraction' va de 0 a 1 y 'extension_s' es positivo");
        // El tallo implica ser sésil; los campos viejos quedan en sincronía (exportación, ficha).
        s.sessile = true;
        s.stalkLength = sp.kind == StalkKind::Foot ? 0.0f : sp.length;
        s.contractileStalk = sp.kind == StalkKind::Spasmoneme;
        r.errors.insert(r.errors.end(), sr.errors.begin(), sr.errors.end());
    }
    if (const toml::table* lt = r.table("light_damage")) {
        toml_io::warnUnknown(*lt, kLightDamageKeys, r.where + " [light_damage]", warnings);
        Reader lr{*lt, r.where + " [light_damage]", {}};
        lr.count("threshold", s.lightDamage.threshold);
        lr.count("stress", s.lightDamage.stress);
        if (s.lightDamage.threshold >= 1.0f)
            lr.errors.push_back(lr.where + ": 'threshold' tiene que ser menor que 1 (la lámpara al máximo)");
        r.errors.insert(r.errors.end(), lr.errors.begin(), lr.errors.end());
    }
    if (const toml::table* pt = r.table("stress_pigment")) {
        toml_io::warnUnknown(*pt, kPigmentKeys, r.where + " [stress_pigment]", warnings);
        Reader pr{*pt, r.where + " [stress_pigment]", {}};
        StressPigmentSpec& sp = s.stressPigment;
        pr.count("rate_per_hour", sp.ratePerHour);
        pr.count("decay_per_hour", sp.decayPerHour);
        pr.count("light_threshold", sp.lightThreshold);
        pr.count("protection", sp.protection);
        if (const toml::node* c = pt->get("color")) {
            const toml::array* a = c->as_array();
            if (!a || a->size() != 3)
                pr.errors.push_back(pr.where + ": 'color' tiene que ser [r, g, b] (0–255)");
            else
                for (std::size_t i = 0; i < 3; ++i)
                    sp.color[i] = static_cast<std::uint8_t>(
                        std::clamp((*a)[i].value<std::int64_t>().value_or(0), std::int64_t{0}, std::int64_t{255}));
        }
        if (sp.protection > 1.0f)
            pr.errors.push_back(pr.where + ": 'protection' es una fracción (0–1)");
        r.errors.insert(r.errors.end(), pr.errors.begin(), pr.errors.end());
    }
    if (const toml::table* look = t.get("appearance") ? t.get("appearance")->as_table() : nullptr) {
        for (const auto& [k, v] : *look)
            if (!known(k.str(), kLookKeys))
                warnings.push_back(r.where + ": clave desconocida en la apariencia '" + std::string(k.str()) + "'");
        if (const toml::node* st = look->get("style")) {
            e.style = st->value<std::string>().value_or("");
            const auto& styles = drawingStyles();
            if (std::find(styles.begin(), styles.end(), e.style) == styles.end()) {
                warnings.push_back(r.where + ": estilo de dibujo desconocido '" + e.style + "' (se usa el de su locomoción)");
                e.style.clear();
            }
        }
        if (const toml::node* ps = look->get("pigment_stripes")) {
            s.pigmentStripes = static_cast<int>(ps->value<std::int64_t>().value_or(0));
            if (s.pigmentStripes < 0 || s.pigmentStripes > 40)
                r.errors.push_back(r.where + ": 'pigment_stripes' va de 0 a 40");
        }
        if (const toml::node* c = look->get("plastid_color")) {
            const toml::array* a = c->as_array();
            if (!a || a->size() != 3) {
                r.errors.push_back(r.where + ": 'plastid_color' tiene que ser [r, g, b] (0–255)");
            } else {
                s.plastidColorSet = true;
                for (std::size_t i = 0; i < 3; ++i)
                    s.plastidColor[i] = static_cast<std::uint8_t>(std::clamp((*a)[i].value<std::int64_t>().value_or(0), std::int64_t{0}, std::int64_t{255}));
            }
        }
    }
    const toml::table* shape = t.get("shape") ? t.get("shape")->as_table() : nullptr;
    if (!shape) {
        r.errors.push_back(r.where + ": falta [being.shape]");
    } else {
        for (const auto& [k, v] : *shape)
            if (!known(k.str(), kShapeKeys))
                warnings.push_back(r.where + ": clave desconocida en la forma '" + std::string(k.str()) + "'");
        Reader f{*shape, r.where + " forma", {}};
        if (!shape->get("semi_major_axis") || !shape->get("semi_minor_axis"))
            f.errors.push_back(f.where + ": faltan 'semi_major_axis' y 'semi_minor_axis'");
        f.num("semi_major_axis", s.spec.radiusX);
        f.num("semi_minor_axis", s.spec.radiusY);
        f.num("shape_memory", s.spec.shapeRate);
        f.num("nucleus", s.spec.nucleusFraction);
        f.num("squareness", s.spec.squareness);
        f.num("taper", s.spec.taper.back);
        f.num("taper_front", s.spec.taper.front);
        float bendDeg = 0.0f;
        f.num("bend", bendDeg);
        s.spec.bend = bendDeg * units::kPi / 180.0f;
        f.num("thickness", s.thickness);
        if (const toml::node* c = shape->get("color")) {
            const toml::array* a = c->as_array();
            if (!a || a->size() != 3)
                f.errors.push_back(f.where + ": 'color' tiene que ser [r, g, b] (0–255)");
            else {
                std::uint8_t* rgb[3] = {&s.spec.r, &s.spec.g, &s.spec.b};
                for (std::size_t i = 0; i < 3; ++i)
                    *rgb[i] = static_cast<std::uint8_t>(std::clamp((*a)[i].value<std::int64_t>().value_or(0), std::int64_t{0}, std::int64_t{255}));
            }
        }
        if (s.spec.radiusX <= 0.0f || s.spec.radiusY <= 0.0f || s.thickness < 0.0f)
            f.errors.push_back(f.where + ": los semiejes tienen que ser positivos");
        if (s.spec.taper.back < 0.0f || s.spec.taper.back >= 1.0f || s.spec.taper.front < 0.0f || s.spec.taper.front >= 1.0f)
            f.errors.push_back(f.where + ": 'taper' y 'taper_front' van de 0 a menos de 1");
        if (std::abs(s.spec.bend) > 0.9f * units::kPi)
            f.errors.push_back(f.where + ": 'bend' va de -162° a 162°");
        r.errors.insert(r.errors.end(), f.errors.begin(), f.errors.end());
    }
    if (!r.errors.empty()) {
        warnings.insert(warnings.end(), r.errors.begin(), r.errors.end());
        warnings.push_back(r.where + ": el ser no se carga");
        return false;
    }
    // Con prolongaciones o escotaduras en el cuerpo, o el eje curvado, la célula entera es una silueta
    // (cuerpo + cuernos con redondeo − escotaduras).
    if (s.spec.bend != 0.0f ||
        std::any_of(s.processes.begin(), s.processes.end(), [](const ProcessSpec& p) { return p.inBody; })) {
        const Silhouette sil = buildSilhouette(s.spec, s.processes);
        if (sil.outline.empty()) {
            warnings.push_back(r.where + ": no se pudo armar la silueta con sus prolongaciones (se usa el cuerpo solo)");
        } else {
            s.outline = sil.outline;
            s.outlineArea = sil.area;
            s.outlineBodyCenter = sil.bodyCenter;
            s.outlineAreaFactor = sil.areaFactor;
            for (const Vec2& q : sil.outline)
                s.outlineReach = std::max(s.outlineReach, length(q));
        }
    }
    s.minWidth = minimumWidth(s);
    return true;
}

// Resuelve presas, huidas y nombres de todos los seres (ids o etiquetas) contra lo cargado.
void resolve() {
    auto setOf = [&](const Entry& owner, const std::vector<std::string>& refs, const char* what) {
        SpeciesSet set;
        for (const std::string& ref : refs) {
            bool found = false;
            for (std::size_t i = 0; i < gEntries.size(); ++i) {
                const Entry& other = gEntries[i];
                bool tagged = false;
                for (const std::string& tag : other.info.tags)
                    tagged = tagged || tag == ref;
                if (other.key == ref || tagged) {
                    set.set(i);
                    found = true;
                }
            }
            // Se resuelve todo en cada carga: el aviso de una referencia rota se da una sola vez.
            const std::string warning = owner.key + ": '" + ref + "' en " + what + " no es un ser ni una etiqueta conocida";
            if (!found && std::find(gWarnings.begin(), gWarnings.end(), warning) == gWarnings.end())
                gWarnings.push_back(warning);
        }
        return set;
    };
    for (Entry& e : gEntries) {
        e.info.key = e.key.c_str();
        e.info.name = e.name.c_str();
        e.info.description = e.description.c_str();
        e.info.plan = e.plan.c_str();
        e.info.style = e.style.c_str();
        e.info.prey = setOf(e, e.prey, "prey");
        e.info.mouthPrey = setOf(e, e.mouthPrey, "mouth_prey");
        e.info.flee = setOf(e, e.flee, "flees_from");
        if (e.info.photosymbiont.present)
            e.info.photosymbiont.source = setOf(e, e.symbiontSource, "photosymbiont.source");
        if (e.info.morph.preySet)
            e.info.morph.prey = setOf(e, e.morphPrey, "morph.prey");
    }
}

bool loadParsed(const toml::table& doc, const std::string& source, const std::string& pack) {
    const toml::array* beings = doc.get("being") ? doc.get("being")->as_array() : nullptr;
    if (!beings)
        return true; // el archivo no trae seres (minerales, medios…)
    for (const toml::node& n : *beings) {
        const toml::table* t = n.as_table();
        if (!t)
            continue;
        Entry e;
        if (!readBeing(*t, source, pack, e, gWarnings))
            continue;
        e.source = toml_io::snippet("being", *t);
        bool duplicate = false;
        for (const Entry& other : gEntries)
            duplicate = duplicate || other.key == e.key;
        if (duplicate) {
            gWarnings.push_back(source + ": el ser '" + e.key + "' ya existe (paquete '" + pack + "'): se saltea");
            continue;
        }
        if (gEntries.size() >= static_cast<std::size_t>(kMaxSpecies)) {
            gWarnings.push_back(source + ": se alcanzó el tope de tipos de ser; '" + e.key + "' no se carga");
            continue;
        }
        gEntries.push_back(std::move(e));
    }
    if (registry().batch == 0)
        resolve();
    return true;
}

void ensureBase() {
    if (registry().ready.load(std::memory_order_acquire))
        return;
    std::lock_guard<std::recursive_mutex> lock(gMutex);
    if (gBaseLoaded)
        return;
    gBaseLoaded = true;
    {
        SpeciesLoadBatch batch;
        for (const auto& path : packFolderFiles(basePackDir(), "beings")) {
            std::string error;
            if (!loadSpeciesFile(path.string(), "base", &error))
                gWarnings.push_back("no se pudo cargar el paquete de fábrica: " + error);
        }
    }
    registry().ready.store(true, std::memory_order_release);
}

} // namespace

float processAngle(const ProcessSpec& p, int k) {
    const float n = static_cast<float>(std::max(1, p.count));
    if (p.count <= 1)
        return p.at;
    if (p.spread > 0.0f) // abanico centrado en `at`
        return p.at - 0.5f * p.spread + p.spread * (static_cast<float>(k) + 0.5f) / n;
    return p.at + 2.0f * units::kPi * static_cast<float>(k) / n; // simetría radial (brazos)
}

Vec2 processAxisPoint(const ProcessSpec& p, float s, float scale) {
    // Arco de curvatura constante: la dirección gira de 0 a `curve` a lo largo del largo.
    const float len = p.length * scale;
    if (std::abs(p.curve) < 1e-4f)
        return {len * s, 0.0f};
    const float radius = len / p.curve;
    const float a = p.curve * s;
    return {radius * std::sin(a), radius * (1.0f - std::cos(a))};
}

const std::vector<std::string>& drawingStyles() {
    static const std::vector<std::string> styles = {
        "ciliate",          "flagellate",           "amoeboid",          "flagellar_rostrum",
        "girdles_and_proboscis", "helical_ribbon", "axial_plate",   "stellate_plastids",
        "reticulate_plastid", "frustule",           "spherical_colony",  "flat_colony",
        "peristomal_crown", "membranelle_crown", "capitate_tentacles", "cirri",
        "extensible_neck", "two_flagella",        "yeast",          "adherent_cell",
        "neutrophil",       "plant_protoplast", "elodea_leaf",      "squamous_cell",
        "lymphocyte",        "monocyte",            "coccoid",          "plate_colony",
        "compact_colony", "hollow_colony"};
    return styles;
}

bool loadSpeciesFile(const std::string& path, const std::string& pack, std::string* error) {
    std::lock_guard<std::recursive_mutex> lock(gMutex);
    if (pack != "base")
        ensureBase();
    toml::table doc;
    try {
        doc = toml::parse_file(path);
    } catch (const toml::parse_error& e) {
        if (error)
            *error = path + ": " + std::string(e.description());
        return false;
    }
    return loadParsed(doc, path, pack);
}

bool loadSpeciesText(const std::string& text, const std::string& source, const std::string& pack, std::string* error) {
    std::lock_guard<std::recursive_mutex> lock(gMutex);
    ensureBase();
    toml::table doc;
    try {
        doc = toml::parse(text, source);
    } catch (const toml::parse_error& e) {
        if (error)
            *error = source + ": " + std::string(e.description());
        return false;
    }
    return loadParsed(doc, source, pack);
}

SpeciesLoadBatch::SpeciesLoadBatch() {
    std::lock_guard<std::recursive_mutex> lock(gMutex);
    ++registry().batch;
}

SpeciesLoadBatch::~SpeciesLoadBatch() {
    std::lock_guard<std::recursive_mutex> lock(gMutex);
    if (--registry().batch == 0)
        resolve();
}

const std::vector<std::string>& speciesLoadWarnings() {
    ensureBase();
    return gWarnings;
}

int speciesCount() {
    ensureBase();
    return static_cast<int>(gEntries.size());
}

const SpeciesInfo& speciesInfo(SpeciesId id) {
    ensureBase();
    return gEntries[static_cast<std::size_t>(id)].info;
}

const SpeciesChemistry& speciesChemistry(SpeciesId id) { return speciesInfo(id).chemistry; }

const std::string& speciesSource(SpeciesId id) {
    ensureBase();
    return gEntries[static_cast<std::size_t>(id)].source;
}

const std::string& speciesPack(SpeciesId id) {
    ensureBase();
    return gEntries[static_cast<std::size_t>(id)].pack;
}

std::optional<SpeciesId> findSpecies(std::string_view key) {
    ensureBase();
    const std::ptrdiff_t i = indexOfId(gEntries, key);
    if (i < 0)
        return std::nullopt;
    return static_cast<SpeciesId>(i);
}

SpeciesSet speciesWithTag(std::string_view tag) {
    ensureBase();
    SpeciesSet set;
    for (std::size_t i = 0; i < gEntries.size(); ++i)
        for (const std::string& t : gEntries[i].info.tags)
            if (t == tag)
                set.set(i);
    return set;
}

bool hasTag(SpeciesId id, std::string_view tag) {
    for (const std::string& t : speciesInfo(id).tags)
        if (t == tag)
            return true;
    return false;
}

SpeciesId playerSpecies(LocomotionType type) {
    // El ser que se controla al elegir solo una locomoción: el representativo de cada tipo.
    const char* key = type == LocomotionType::Flagellum ? "euglena" : type == LocomotionType::Pseudopod ? "amoeba" : "paramecium";
    return findSpecies(key).value_or(SpeciesId::Paramecium);
}

Genome speciesGenome(SpeciesId id) {
    const SpeciesInfo& info = speciesInfo(id);
    Genome g = speciesGenome(info.locomotion);
    g[Gene::Chloroplasts] = info.chloroplasts ? 1.0f : 0.0f;
    if (info.locomotion == LocomotionType::Cilia && info.chloroplasts)
        g[Gene::Trichocysts] = 0.0f; // algas: sin tricocistos
    for (const auto& [gene, value] : info.genome)
        g[gene] = value;
    return g;
}

float cellMassScale(const SpeciesInfo& info) {
    // Esfera de la misma sección: 4/3·π·(a·b)^(3/2); esferoide: 4/3·π·a·b·c.
    const float c = info.thickness > 0.0f ? info.thickness : info.spec.radiusY;
    return info.dryMassScale * c / std::sqrt(info.spec.radiusX * info.spec.radiusY);
}
