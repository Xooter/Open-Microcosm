#include "bio/species.hpp"

#include "core/paths.hpp"

#include <toml++/toml.hpp>

#include <algorithm>
#include <deque>
#include <mutex>

// Registro de los tipos de ser (fase 20.0: el contenido como datos). Los seres vienen de los
// paquetes de contenido; los de fábrica, de assets/content/base/seres.toml. Se guardan en un
// deque: agregar los de un paquete no mueve los ya cargados (las referencias siguen valiendo).

namespace {

struct Entry {
    SpeciesInfo info;
    std::string key, name, description, plan, style, pack;
    // Referencias sin resolver (ids o etiquetas): se resuelven contra todo lo cargado.
    std::vector<std::string> prey, mouthPrey, flee;
};

// Estado del registro en estáticos locales: otras unidades piden especies al inicializar sus
// propios globales (orden de inicialización entre archivos no definido).
struct Registry {
    std::deque<Entry> entries;
    std::vector<std::string> warnings;
    std::recursive_mutex mutex;
    bool baseLoaded = false;
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

const char* const kGeneKeys[] = {"tamano",      "motor",    "mitocondrias", "cloroplastos", "lisosomas",
                                 "tricocistos", "coraza",   "rigidez",      "apareamiento"};
static_assert(sizeof(kGeneKeys) / sizeof(kGeneKeys[0]) == kGeneCount);

template <typename E, std::size_t N>
bool parseEnum(std::string_view text, const std::pair<const char*, E> (&table)[N], E& out) {
    for (const auto& [name, value] : table)
        if (text == name) {
            out = value;
            return true;
        }
    return false;
}

constexpr std::pair<const char*, LocomotionType> kLocomotion[] = {
    {"cilios", LocomotionType::Cilia}, {"flagelo", LocomotionType::Flagellum}, {"pseudopodo", LocomotionType::Pseudopod}};
constexpr std::pair<const char*, FeedingMode> kFeeding[] = {
    {"surco_oral", FeedingMode::OralGroove}, {"polo_anterior", FeedingMode::Anterior},
    {"fagocitosis", FeedingMode::Phagocytosis}, {"raptorial", FeedingMode::Raptorial},
    {"tentaculos", FeedingMode::Tentacles},  {"cuello", FeedingMode::NeckStrike},
    {"osmotrofia", FeedingMode::Osmotroph},  {"ninguna", FeedingMode::None}};
constexpr std::pair<const char*, Storage> kStorage[] = {{"ninguna", Storage::None},
                                                        {"almidon", Storage::Starch},
                                                        {"glucogeno", Storage::Glycogen},
                                                        {"paramilon", Storage::Paramylon},
                                                        {"crisolaminarina", Storage::Chrysolaminarin}};
constexpr std::pair<const char*, CellWall> kWall[] = {{"ninguna", CellWall::None},
                                                      {"celulosa", CellWall::Cellulose},
                                                      {"silice", CellWall::Silica},
                                                      {"glucano_levadura", CellWall::YeastGlucan}};

// Claves conocidas de un [[ser]] y de su [ser.forma]: una clave que no esté acá es casi siempre un
// error de tipeo, y se avisa (no se ignora en silencio).
constexpr const char* kBeingKeys[] = {
    "id", "nombre", "descripcion", "plan", "etiquetas", "locomocion", "alimentacion", "velocidad_relativa", "presas", "presas_boca",
    "huye_de", "quimiocinesis", "trampa_acida", "fototaxis", "fotosintesis_relativa", "divisiones_por_ciclo",
    "enquista", "conjugacion", "cantidad_inicial", "sesil", "largo_tallo", "espasmonema", "camina", "alcance_tentaculos",
    "alcance_cuello", "manipulacion_horas", "cloroplastos", "desliza_por_rafe", "auxotrofa_b12", "frustula",
    "filamento", "celulas_filamento", "fragmentacion_por_hora", "zoosporas", "colonias_hijas",
    "madre_muere_al_liberar", "masa_seca_relativa", "gemacion", "anaerobia", "boca_posterior", "ciclosis", "ph",
    "temperatura", "salinidad_max", "reserva", "pared", "genoma", "forma", "apariencia"};
constexpr const char* kLookKeys[] = {"estilo", "color_plastos"};
constexpr const char* kShapeKeys[] = {"semieje_mayor", "semieje_menor", "memoria_forma", "color",
                                      "nucleo",        "cuadratura",    "afinamiento"};

template <std::size_t N>
bool known(std::string_view key, const char* const (&keys)[N]) {
    for (const char* k : keys)
        if (key == k)
            return true;
    return false;
}

// Lector de un ser: acumula errores (que descartan el ser) y avisos.
struct Reader {
    const toml::table& t;
    std::string where;
    std::vector<std::string> errors;

    template <typename T> void num(const char* key, T& out) {
        const toml::node* n = t.get(key);
        if (!n)
            return;
        if (auto v = n->value<double>())
            out = static_cast<T>(*v);
        else
            errors.push_back(where + ": '" + key + "' tiene que ser un número");
    }
    void flag(const char* key, bool& out) {
        const toml::node* n = t.get(key);
        if (!n)
            return;
        if (auto v = n->value<bool>())
            out = *v;
        else
            errors.push_back(where + ": '" + key + "' tiene que ser true o false");
    }
    void range(const char* key, float& lo, float& hi) {
        const toml::node* n = t.get(key);
        if (!n)
            return;
        const toml::array* a = n->as_array();
        if (!a || a->size() != 2 || !(*a)[0].value<double>() || !(*a)[1].value<double>()) {
            errors.push_back(where + ": '" + key + "' tiene que ser [mínimo, máximo]");
            return;
        }
        lo = static_cast<float>(*(*a)[0].value<double>());
        hi = static_cast<float>(*(*a)[1].value<double>());
    }
    void strings(const char* key, std::vector<std::string>& out) {
        const toml::node* n = t.get(key);
        if (!n)
            return;
        const toml::array* a = n->as_array();
        if (!a) {
            errors.push_back(where + ": '" + key + "' tiene que ser una lista de textos");
            return;
        }
        for (const toml::node& e : *a) {
            if (auto v = e.value<std::string>())
                out.push_back(*v);
            else
                errors.push_back(where + ": '" + key + "' tiene que ser una lista de textos");
        }
    }
    template <typename E, std::size_t N>
    void choice(const char* key, const std::pair<const char*, E> (&table)[N], E& out, bool required = false) {
        const toml::node* n = t.get(key);
        if (!n) {
            if (required)
                errors.push_back(where + ": falta '" + key + "'");
            return;
        }
        auto v = n->value<std::string>();
        if (!v || !parseEnum(*v, table, out)) {
            std::string options;
            for (const auto& [name, value] : table)
                options += std::string(options.empty() ? "" : ", ") + name;
            errors.push_back(where + ": '" + key + "' tiene que ser uno de: " + options);
        }
    }
};

// Un [[ser]] → Entry. Devuelve false si tiene errores (se descarta).
bool readBeing(const toml::table& t, const std::string& source, const std::string& pack, Entry& e,
               std::vector<std::string>& warnings) {
    const auto idNode = t.get("id");
    const auto nameNode = t.get("nombre");
    if (!idNode || !idNode->value<std::string>() || !nameNode || !nameNode->value<std::string>()) {
        warnings.push_back(source + ": un [[ser]] sin 'id' o sin 'nombre': se saltea");
        return false;
    }
    e.key = *idNode->value<std::string>();
    e.name = *nameNode->value<std::string>();
    e.pack = pack;
    if (const toml::node* d = t.get("descripcion"))
        e.description = d->value<std::string>().value_or("");
    if (const toml::node* d = t.get("plan"))
        e.plan = d->value<std::string>().value_or("");
    Reader r{t, source + " (" + e.key + ")", {}};
    for (const auto& [k, v] : t)
        if (!known(k.str(), kBeingKeys))
            warnings.push_back(r.where + ": clave desconocida '" + std::string(k.str()) + "'");
    SpeciesInfo& s = e.info;
    r.choice("locomocion", kLocomotion, s.locomotion, true);
    r.choice("alimentacion", kFeeding, s.feeding);
    r.num("velocidad_relativa", s.speedScale);
    r.strings("presas", e.prey);
    r.strings("presas_boca", e.mouthPrey);
    r.strings("huye_de", e.flee);
    r.strings("etiquetas", s.tags);
    r.flag("quimiocinesis", s.chemokinesis);
    r.flag("trampa_acida", s.acidTrap);
    r.flag("fototaxis", s.phototaxis);
    r.num("fotosintesis_relativa", s.photosynthesisScale);
    r.num("divisiones_por_ciclo", s.fissions);
    r.flag("enquista", s.encysts);
    r.flag("conjugacion", s.conjugates);
    r.num("cantidad_inicial", s.initialCount);
    r.flag("sesil", s.sessile);
    r.num("largo_tallo", s.stalkLength);
    r.flag("espasmonema", s.contractileStalk);
    r.flag("camina", s.walker);
    r.num("alcance_tentaculos", s.tentacleReach);
    r.num("alcance_cuello", s.neckReach);
    r.num("manipulacion_horas", s.handlingHours);
    r.flag("cloroplastos", s.chloroplasts);
    r.flag("desliza_por_rafe", s.glider);
    r.flag("auxotrofa_b12", s.vitaminB12);
    r.flag("frustula", s.frustule);
    r.flag("filamento", s.filament);
    r.num("celulas_filamento", s.filamentCells);
    r.num("fragmentacion_por_hora", s.fragmentationPerHour);
    r.num("zoosporas", s.zoosporeChance);
    r.num("colonias_hijas", s.colonyDaughters);
    r.flag("madre_muere_al_liberar", s.motherDiesAtRelease);
    r.num("masa_seca_relativa", s.dryMassScale);
    r.flag("gemacion", s.budding);
    r.flag("anaerobia", s.anaerobe);
    r.flag("boca_posterior", s.mouthAtRear);
    r.num("ciclosis", s.cyclosis);
    r.range("ph", s.phMin, s.phMax);
    r.range("temperatura", s.tMin, s.tMax);
    r.num("salinidad_max", s.salinityMax);
    r.choice("reserva", kStorage, s.chemistry.storage);
    r.choice("pared", kWall, s.chemistry.wall);
    if (const toml::node* g = t.get("genoma")) {
        if (const toml::table* gt = g->as_table()) {
            for (const auto& [k, v] : *gt) {
                int gene = -1;
                for (std::size_t i = 0; i < kGeneCount; ++i)
                    if (k.str() == kGeneKeys[i])
                        gene = static_cast<int>(i);
                if (gene < 0 || !v.value<double>()) {
                    r.errors.push_back(r.where + ": gen desconocido o sin valor numérico en 'genoma': " +
                                       std::string(k.str()));
                    continue;
                }
                s.genome.push_back({static_cast<Gene>(gene), static_cast<float>(*v.value<double>())});
            }
        } else {
            r.errors.push_back(r.where + ": 'genoma' tiene que ser una tabla {gen = valor}");
        }
    }
    if (const toml::table* look = t.get("apariencia") ? t.get("apariencia")->as_table() : nullptr) {
        for (const auto& [k, v] : *look)
            if (!known(k.str(), kLookKeys))
                warnings.push_back(r.where + ": clave desconocida en la apariencia '" + std::string(k.str()) + "'");
        if (const toml::node* st = look->get("estilo")) {
            e.style = st->value<std::string>().value_or("");
            const auto& styles = drawingStyles();
            if (std::find(styles.begin(), styles.end(), e.style) == styles.end()) {
                warnings.push_back(r.where + ": estilo de dibujo desconocido '" + e.style + "' (se usa el de su locomoción)");
                e.style.clear();
            }
        }
        if (const toml::node* c = look->get("color_plastos")) {
            const toml::array* a = c->as_array();
            if (!a || a->size() != 3) {
                r.errors.push_back(r.where + ": 'color_plastos' tiene que ser [r, g, b] (0–255)");
            } else {
                s.plastidColorSet = true;
                for (std::size_t i = 0; i < 3; ++i)
                    s.plastidColor[i] = static_cast<std::uint8_t>(std::clamp((*a)[i].value<std::int64_t>().value_or(0), std::int64_t{0}, std::int64_t{255}));
            }
        }
    }
    const toml::table* shape = t.get("forma") ? t.get("forma")->as_table() : nullptr;
    if (!shape) {
        r.errors.push_back(r.where + ": falta [ser.forma]");
    } else {
        for (const auto& [k, v] : *shape)
            if (!known(k.str(), kShapeKeys))
                warnings.push_back(r.where + ": clave desconocida en la forma '" + std::string(k.str()) + "'");
        Reader f{*shape, r.where + " forma", {}};
        if (!shape->get("semieje_mayor") || !shape->get("semieje_menor"))
            f.errors.push_back(f.where + ": faltan 'semieje_mayor' y 'semieje_menor'");
        f.num("semieje_mayor", s.spec.radiusX);
        f.num("semieje_menor", s.spec.radiusY);
        f.num("memoria_forma", s.spec.shapeRate);
        f.num("nucleo", s.spec.nucleusFraction);
        f.num("cuadratura", s.spec.squareness);
        f.num("afinamiento", s.spec.taper);
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
        if (s.spec.radiusX <= 0.0f || s.spec.radiusY <= 0.0f)
            f.errors.push_back(f.where + ": los semiejes tienen que ser positivos");
        r.errors.insert(r.errors.end(), f.errors.begin(), f.errors.end());
    }
    if (s.phMin > s.phMax || s.tMin > s.tMax)
        r.errors.push_back(r.where + ": rango de pH o de temperatura invertido");
    if (!r.errors.empty()) {
        warnings.insert(warnings.end(), r.errors.begin(), r.errors.end());
        warnings.push_back(r.where + ": el ser no se carga");
        return false;
    }
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
            if (!found)
                gWarnings.push_back(owner.key + ": '" + ref + "' en " + what + " no es un ser ni una etiqueta conocida");
        }
        return set;
    };
    for (Entry& e : gEntries) {
        e.info.key = e.key.c_str();
        e.info.name = e.name.c_str();
        e.info.description = e.description.c_str();
        e.info.plan = e.plan.c_str();
        e.info.style = e.style.c_str();
        e.info.prey = setOf(e, e.prey, "presas");
        e.info.mouthPrey = setOf(e, e.mouthPrey, "presas_boca");
        e.info.flee = setOf(e, e.flee, "huye_de");
    }
}

bool loadParsed(const toml::table& doc, const std::string& source, const std::string& pack) {
    const toml::array* beings = doc.get("ser") ? doc.get("ser")->as_array() : nullptr;
    if (!beings)
        return true; // el archivo no trae seres (minerales, medios…)
    for (const toml::node& n : *beings) {
        const toml::table* t = n.as_table();
        if (!t)
            continue;
        Entry e;
        if (!readBeing(*t, source, pack, e, gWarnings))
            continue;
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
    resolve();
    return true;
}

void ensureBase() {
    std::lock_guard<std::recursive_mutex> lock(gMutex);
    if (gBaseLoaded)
        return;
    gBaseLoaded = true;
    std::string error;
    if (!loadSpeciesFile(assetPath("content/base/seres.toml"), "base", &error))
        gWarnings.push_back("no se pudo cargar el paquete de fábrica: " + error);
}

} // namespace

const std::vector<std::string>& drawingStyles() {
    static const std::vector<std::string> styles = {
        "ciliado",          "flagelado",           "ameboide",          "rostro_de_flagelos",
        "cinturones_y_proboscide", "cinta_helicoidal", "placa_axial",   "plastos_estrellados",
        "plasto_reticulado", "frustula",           "colonia_esferica",  "colonia_plana",
        "corona_peristomal", "corona_de_membranelas", "tentaculos_capitados", "cirros",
        "cuello_extensible", "dos_flagelos",        "levadura"};
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

const std::string& speciesPack(SpeciesId id) {
    ensureBase();
    return gEntries[static_cast<std::size_t>(id)].pack;
}

std::optional<SpeciesId> findSpecies(std::string_view key) {
    ensureBase();
    for (std::size_t i = 0; i < gEntries.size(); ++i)
        if (gEntries[i].key == key)
            return static_cast<SpeciesId>(i);
    return std::nullopt;
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
