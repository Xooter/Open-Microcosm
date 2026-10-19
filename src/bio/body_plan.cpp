#include "bio/body_plan.hpp"

#include "core/pack_files.hpp"
#include "core/toml_reader.hpp"

#include <toml++/toml.hpp>

#include <atomic>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>


namespace {

using K = StructureKind;
using S = StructureSource;
using A = Arrangement;

// Tamaños 0: los da la simulación (núcleo, vacuolas, organelos con partícula propia).
// Las estructuras declaradas eucariotas se muestran como UN representante junto a su ancla (un
// dictiosoma, unas cisternas de RE): la célula real tiene varios o muchos; el modelo no inventa su
// número ni su posición exacta.

// --- Bacterias -----------------------------------------------------------------------------

// Bacilo gramnegativo genérico (E. coli): envoltura de ~30 nm (membrana externa, periplasma con
// peptidoglicano fino, membrana interna; Silhavy et al. 2010), nucleoide, ~2·10⁴ ribosomas
// (7·10³ a 7·10⁴ según el crecimiento; Bremer & Dennis 2008), ~4 flagelos perítricos de 20 nm ×
// 5–10 µm (Turner & Berg 2000) y fimbrias tipo 1 (100–500, 7 nm × 1–2 µm).
constexpr StructureSpec kEcoli[] = {
    {K::OuterMembrane, S::Declared, A::Envelope, 0.0f, 0.007f, 1, false, "membrana externa"},
    {K::Peptidoglycan, S::Declared, A::Envelope, 0.0f, 0.004f, 1, false, "peptidoglicano (fino)"},
    {K::PlasmaMembrane, S::Declared, A::Envelope, 0.0f, 0.007f, 1, false, "membrana interna"},
    {K::Nucleoid, S::Declared, A::Interior, 1.0f, 0.5f, 1, false, "nucleoide (cromosoma circular)"},
    {K::Ribosomes, S::Declared, A::Interior, 0.02f, 0.02f, 20000, false, "ribosomas 70S"},
    {K::Flagellum, S::Simulated, A::Peritrichous, 7.0f, 0.02f, 4, false, "flagelos (haz al nadar, sueltos al girar)"},
    {K::Pili, S::Declared, A::Peritrichous, 1.0f, 0.007f, 200, false, "fimbrias tipo 1"},
};

// Chromobacterium violaceum: gramnegativa con flagelo polar. También tiene flagelos laterales,
// que el modelo todavía no representa (su número no está en los datos).
constexpr StructureSpec kViolaceum[] = {
    {K::OuterMembrane, S::Declared, A::Envelope, 0.0f, 0.007f, 1, false, "membrana externa"},
    {K::Peptidoglycan, S::Declared, A::Envelope, 0.0f, 0.004f, 1, false, "peptidoglicano (fino)"},
    {K::PlasmaMembrane, S::Declared, A::Envelope, 0.0f, 0.007f, 1, false, "membrana interna"},
    {K::Nucleoid, S::Declared, A::Interior, 1.0f, 0.5f, 1, false, "nucleoide"},
    {K::Ribosomes, S::Declared, A::Interior, 0.02f, 0.02f, 20000, false, "ribosomas 70S"},
    {K::Flagellum, S::Simulated, A::Polar, 6.0f, 0.02f, 1, false, "flagelo polar"},
};

// Bacillus subtilis: grampositiva, sin membrana externa; pared de peptidoglicano de 30–40 nm
// (20–80 nm en grampositivas) y ~26 flagelos perítricos (Guttenplan et al. 2013).
constexpr StructureSpec kBacillus[] = {
    {K::Peptidoglycan, S::Declared, A::Envelope, 0.0f, 0.035f, 1, false, "peptidoglicano (grueso)"},
    {K::PlasmaMembrane, S::Declared, A::Envelope, 0.0f, 0.007f, 1, false, "membrana plasmatica"},
    {K::Nucleoid, S::Declared, A::Interior, 1.0f, 0.5f, 1, false, "nucleoide"},
    {K::Ribosomes, S::Declared, A::Interior, 0.02f, 0.02f, 20000, false, "ribosomas 70S"},
    {K::Flagellum, S::Simulated, A::Peritrichous, 6.0f, 0.02f, 20, false, "flagelos peritricos"},
};

// Bdellovibrio bacteriovorus: vibrio de ~1 × 0,3 µm, gramnegativo, con un flagelo polar envainado
// (la vaina lo engrosa a ~28 nm). Ribosomas: tiene, pero su número no está en los datos (0: no se
// dibujan).
constexpr StructureSpec kBdello[] = {
    {K::OuterMembrane, S::Declared, A::Envelope, 0.0f, 0.007f, 1, false, "membrana externa"},
    {K::Peptidoglycan, S::Declared, A::Envelope, 0.0f, 0.004f, 1, false, "peptidoglicano (fino)"},
    {K::PlasmaMembrane, S::Declared, A::Envelope, 0.0f, 0.007f, 1, false, "membrana interna"},
    {K::Nucleoid, S::Declared, A::Interior, 0.5f, 0.2f, 1, false, "nucleoide"},
    {K::Ribosomes, S::Declared, A::Interior, 0.02f, 0.02f, 0, false, "ribosomas 70S"},
    {K::Flagellum, S::Simulated, A::Polar, 3.0f, 0.028f, 1, false, "flagelo polar envainado"},
};

// --- Virus ---------------------------------------------------------------------------------

// Fago T4 (Myoviridae): cabeza icosaédrica alargada de 78 × 111 nm con ~56 µm de ADN doble (169
// kpb) empaquetado, cola contráctil de 100 × 20 nm (al contraerse queda en ~40 nm e inyecta el
// ADN), placa basal de ~52 nm y 6 fibras largas de ~145 nm. Sin envoltura (Leiman et al. 2003).
constexpr StructureSpec kT4[] = {
    {K::Capsid, S::Simulated, A::Interior, 0.111f, 0.078f, 1, false, "capside (cabeza icosaedrica)"},
    {K::ViralGenome, S::Declared, A::Interior, 56.0f, 0.002f, 1, false, "ADN doble (169 kpb, empaquetado)"},
    {K::Tail, S::Declared, A::Polar, 0.1f, 0.02f, 1, false, "cola contractil"},
    {K::Baseplate, S::Declared, A::Polar, 0.052f, 0.01f, 1, false, "placa basal"},
    {K::TailFibers, S::Declared, A::Polar, 0.145f, 0.002f, 6, false, "fibras de la cola"},
};

// --- Eucariotas ----------------------------------------------------------------------------
// Los planes de protistas, algas, levaduras y células son datos (assets/content/base/body_plans/ y
// los paquetes del usuario). Si un ser pide un plan que no existe, se usa el mínimo (membrana y
// núcleo) y se avisa.
constexpr StructureSpec kMinimalEukaryote[] = {
    {K::PlasmaMembrane, S::Simulated, A::Envelope, 0.0f, 0.0f, 0, true, "membrana plasmatica"},
    {K::Nucleus, S::Simulated, A::Interior, 0.0f, 0.0f, 1, true, "nucleo"},
};


constexpr BodyPlan kBacteriaPlans[] = {
    {"bacilo gramnegativo (tipo E. coli)", Morphology::Rod, kEcoli},
    {"Chromobacterium violaceum", Morphology::Rod, kViolaceum},
    {"Bacillus subtilis", Morphology::Rod, kBacillus},
};
constexpr BodyPlan kBdelloPlan = {"Bdellovibrio bacteriovorus", Morphology::Vibrio, kBdello};
constexpr BodyPlan kVirusPlans[] = {
    {"fago tipo T4", Morphology::HeadTailPhage, kT4},
};

constexpr BodyPlan kMinimalPlan = {"eucariota", Morphology::Eukaryote, kMinimalEukaryote};

} // namespace

const char* structureName(StructureKind kind) {
    switch (kind) {
    case K::PlasmaMembrane: return "membrana plasmatica";
    case K::OuterMembrane: return "membrana externa";
    case K::Peptidoglycan: return "peptidoglicano";
    case K::CellWall: return "pared celular";
    case K::Capsule: return "capsula";
    case K::Nucleoid: return "nucleoide";
    case K::Ribosomes: return "ribosomas";
    case K::Flagellum: return "flagelo";
    case K::Pili: return "pili";
    case K::Nucleus: return "nucleo";
    case K::Nucleolus: return "nucleolo";
    case K::Mitochondrion: return "mitocondria";
    case K::Hydrogenosome: return "hidrogenosoma";
    case K::Chloroplast: return "cloroplasto";
    case K::Pyrenoid: return "pirenoide";
    case K::Eyespot: return "estigma";
    case K::EndoplasmicReticulum: return "reticulo endoplasmico";
    case K::Golgi: return "aparato de Golgi";
    case K::ContractileVacuole: return "vacuola contractil";
    case K::FoodVacuole: return "vacuola digestiva";
    case K::Lysosome: return "lisosoma";
    case K::StorageGranule: return "granulo de reserva";
    case K::Capsid: return "capside";
    case K::ViralGenome: return "genoma viral";
    case K::ViralEnvelope: return "envoltura viral";
    case K::Tail: return "cola";
    case K::Baseplate: return "placa basal";
    case K::TailFibers: return "fibras de la cola";
    case K::Count: break;
    }
    return "?";
}

const BodyPlan& bacteriumPlan(std::uint8_t strain) { return kBacteriaPlans[strain < 3 ? strain : 0]; }

const BodyPlan& bdellovibrioPlan() { return kBdelloPlan; }

const BodyPlan& virusPlan(VirusKind kind) { return kVirusPlans[static_cast<int>(kind)]; }

// --- Planes como datos (fase 20.0) -----------------------------------------------------------

namespace {

struct PlanEntry {
    std::string id, name, pack;
    std::string source; // el [[body_plan]] como se leyó (para exportarlo)
    std::deque<std::string> names; // nombres de las estructuras (direcciones estables)
    std::vector<StructureSpec> structures;
    BodyPlan plan{};
};

struct PlanRegistry {
    std::deque<PlanEntry> entries;
    std::vector<std::string> warnings;
    std::recursive_mutex mutex;
    bool baseLoaded = false;
    // Ya cargado: se lee sin el candado (lo consultan los bucles paralelos de cada paso).
    std::atomic<bool> ready{false};
};
PlanRegistry& plans() {
    static PlanRegistry r;
    return r;
}

constexpr std::pair<const char*, StructureKind> kKinds[] = {
    {"plasma_membrane", K::PlasmaMembrane}, {"outer_membrane", K::OuterMembrane},
    {"peptidoglycan", K::Peptidoglycan},       {"cell_wall", K::CellWall},
    {"capsule", K::Capsule},                    {"nucleoid", K::Nucleoid},
    {"ribosomes", K::Ribosomes},                {"flagellum", K::Flagellum},
    {"pili", K::Pili},                          {"nucleus", K::Nucleus},
    {"nucleolus", K::Nucleolus},                 {"mitochondrion", K::Mitochondrion},
    {"hydrogenosome", K::Hydrogenosome},        {"chloroplast", K::Chloroplast},
    {"pyrenoid", K::Pyrenoid},                 {"eyespot", K::Eyespot},
    {"endoplasmic_reticulum", K::EndoplasmicReticulum}, {"golgi", K::Golgi},
    {"contractile_vacuole", K::ContractileVacuole}, {"food_vacuole", K::FoodVacuole},
    {"lysosome", K::Lysosome},                  {"storage_granule", K::StorageGranule},
    {"capsid", K::Capsid},                     {"viral_genome", K::ViralGenome},
    {"viral_envelope", K::ViralEnvelope},      {"tail", K::Tail},
    {"baseplate", K::Baseplate},              {"tail_fibers", K::TailFibers}};
constexpr std::pair<const char*, StructureSource> kSources[] = {{"simulated", S::Simulated}, {"declared", S::Declared}};
constexpr std::pair<const char*, Arrangement> kArrangements[] = {
    {"envelope", A::Envelope},       {"interior", A::Interior},       {"polar", A::Polar},
    {"peritrichous", A::Peritrichous},   {"perinuclear", A::Perinuclear}, {"anterior", A::Anterior}};
constexpr std::pair<const char*, Morphology> kMorphologies[] = {
    {"rod", Morphology::Rod},          {"vibrio", Morphology::Vibrio},           {"eukaryote", Morphology::Eukaryote},
    {"tailed_phage", Morphology::HeadTailPhage}, {"icosahedral", Morphology::Icosahedral}, {"helical", Morphology::Helical}};

template <typename E, std::size_t N>
bool pick(const toml::table& t, const char* key, const std::pair<const char*, E> (&table)[N], E& out) {
    const auto v = t.get(key) ? t.get(key)->value<std::string>() : std::nullopt;
    if (!v)
        return false;
    for (const auto& [name, value] : table)
        if (*v == name) {
            out = value;
            return true;
        }
    return false;
}

void loadPlansDoc(const toml::table& doc, const std::string& source, const std::string& pack) {
    PlanRegistry& r = plans();
    const toml::array* list = doc.get("body_plan") ? doc.get("body_plan")->as_array() : nullptr;
    if (!list)
        return;
    for (const toml::node& n : *list) {
        const toml::table* t = n.as_table();
        if (!t)
            continue;
        const auto id = t->get("id") ? t->get("id")->value<std::string>() : std::nullopt;
        if (!id) {
            r.warnings.push_back(source + ": un [[body_plan]] sin 'id': se saltea");
            continue;
        }
        bool duplicate = false;
        for (const PlanEntry& e : r.entries)
            duplicate = duplicate || e.id == *id;
        if (duplicate) {
            r.warnings.push_back(source + ": el plan '" + *id + "' ya existe: se saltea");
            continue;
        }
        PlanEntry e;
        e.id = *id;
        e.pack = pack;
        e.source = toml_io::snippet("body_plan", *t);
        e.name = t->get("name") ? t->get("name")->value<std::string>().value_or(*id) : *id;
        Morphology morphology = Morphology::Eukaryote;
        if (t->get("morphology") && !pick(*t, "morphology", kMorphologies, morphology))
            r.warnings.push_back(source + " (" + *id + "): 'morphology' desconocida");
        bool ok = true;
        if (const toml::array* structures = t->get("structures") ? t->get("structures")->as_array() : nullptr) {
            for (const toml::node& sn : *structures) {
                const toml::table* st = sn.as_table();
                StructureSpec spec{K::Nucleus, S::Simulated, A::Interior, 0.0f, 0.0f, 0, false, ""};
                if (!st || !pick(*st, "kind", kKinds, spec.kind) || !pick(*st, "source", kSources, spec.source) ||
                    !pick(*st, "arrangement", kArrangements, spec.arrangement)) {
                    r.warnings.push_back(source + " (" + *id + "): una estructura sin tipo, fuente o disposición válidos");
                    ok = false;
                    continue;
                }
                auto num = [&](const char* key) { return static_cast<float>(st->get(key) ? st->get(key)->value<double>().value_or(0.0) : 0.0); };
                spec.length = num("length");
                spec.width = num("width");
                spec.perCubicMicron = num("per_um3");
                spec.count = static_cast<int>(st->get("count") ? st->get("count")->value<std::int64_t>().value_or(0) : 0);
                spec.brightfield = st->get("brightfield") ? st->get("brightfield")->value<bool>().value_or(false) : false;
                e.names.push_back(st->get("name") ? st->get("name")->value<std::string>().value_or("") : "");
                spec.name = e.names.back().c_str();
                e.structures.push_back(spec);
            }
        }
        if (!ok) {
            r.warnings.push_back(source + " (" + *id + "): el plan no se carga");
            continue;
        }
        r.entries.push_back(std::move(e));
        PlanEntry& stored = r.entries.back();
        stored.plan = {stored.name.c_str(), morphology, std::span<const StructureSpec>(stored.structures)};
    }
}

void ensurePlans() {
    PlanRegistry& r = plans();
    if (r.ready.load(std::memory_order_acquire))
        return;
    std::lock_guard<std::recursive_mutex> lock(r.mutex);
    if (r.baseLoaded)
        return;
    r.baseLoaded = true;
    for (const auto& path : packFolderFiles(basePackDir(), "body_plans")) {
        std::string error;
        if (!loadBodyPlansFile(path.string(), "base", &error))
            r.warnings.push_back("no se pudieron cargar los planes de fábrica: " + error);
    }
    r.ready.store(true, std::memory_order_release);
}

} // namespace

bool loadBodyPlansFile(const std::string& path, const std::string& pack, std::string* error) {
    std::lock_guard<std::recursive_mutex> lock(plans().mutex);
    if (pack != "base")
        ensurePlans();
    try {
        loadPlansDoc(toml::parse_file(path), path, pack);
    } catch (const toml::parse_error& e) {
        if (error)
            *error = path + ": " + std::string(e.description());
        return false;
    }
    return true;
}

bool loadBodyPlansText(const std::string& text, const std::string& source, const std::string& pack, std::string* error) {
    std::lock_guard<std::recursive_mutex> lock(plans().mutex);
    ensurePlans();
    try {
        loadPlansDoc(toml::parse(text, source), source, pack);
    } catch (const toml::parse_error& e) {
        if (error)
            *error = source + ": " + std::string(e.description());
        return false;
    }
    return true;
}

const BodyPlan* findBodyPlan(std::string_view id) {
    ensurePlans();
    for (const PlanEntry& e : plans().entries)
        if (e.id == id)
            return &e.plan;
    return nullptr;
}

std::string bodyPlanSource(std::string_view id, std::string* pack) {
    ensurePlans();
    PlanRegistry& r = plans();
    std::lock_guard<std::recursive_mutex> lock(r.mutex);
    for (const PlanEntry& e : r.entries)
        if (e.id == id) {
            if (pack)
                *pack = e.pack;
            return e.source;
        }
    return {};
}

const std::vector<std::string>& bodyPlanWarnings() {
    ensurePlans();
    return plans().warnings;
}

const BodyPlan& speciesPlan(SpeciesId id) {
    if (const BodyPlan* plan = findBodyPlan(speciesInfo(id).plan))
        return *plan;
    return kMinimalPlan;
}
