#include "game/plate_spec.hpp"

#include "game/content.hpp"
#include "game/progress.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>
#include <sstream>

namespace {

struct FloatField {
    const char* key;
    float MediumParams::*member;
};

constexpr FloatField kMediumFloats[] = {
    {"o2_air_exchange", &MediumParams::o2AirExchange},
    {"nutrient_uptake", &MediumParams::nutrientUptake},
    {"detritus_decay", &MediumParams::detritusDecay},
    {"detritus_spawn_rate", &MediumParams::detritusSpawnRate},
    {"surface_light", &MediumParams::surfaceLight},
    {"phosphate", &MediumParams::phosphate},
    {"nitrogen", &MediumParams::nitrogen},
    {"silicate", &MediumParams::silicate},
    {"iron", &MediumParams::iron},
    {"vitamin", &MediumParams::vitamin},
    {"vitamin_source_per_hour", &MediumParams::vitaminSourcePerHour},
    {"vitamin_decay_per_hour", &MediumParams::vitaminDecayPerHour},
    {"frustule_dissolution_per_hour", &MediumParams::frustuleDissolutionPerHour},
    {"alkalinity", &MediumParams::alkalinity},
    {"carbonate_pk", &MediumParams::carbonatePK},
    {"co2_atmosphere", &MediumParams::co2Atmosphere},
    {"organic", &MediumParams::organic},
    {"glutamine", &MediumParams::glutamine},
    {"serum", &MediumParams::serum},
    {"phenol_red", &MediumParams::phenolRed},
    {"osmolytes", &MediumParams::osmolytes},
    {"phage", &MediumParams::phage},
    {"phage_decay_per_hour", &MediumParams::phageDecayPerHour},
    {"toxin_decay_per_hour", &MediumParams::toxinDecayPerHour},
    {"ahl_half_life_hours", &MediumParams::ahlHalfLifeHours},
    {"temperature", &MediumParams::temperature},
    {"salinity", &MediumParams::salinity},
    {"organic_inflow", &MediumParams::organicInflow},
    {"dust_rate", &MediumParams::dustRate},
    {"perfusion_per_hour", &MediumParams::perfusionPerHour},
};

struct IntField {
    const char* key;
    int WorldConfig::*member;
};

constexpr IntField kConfigInts[] = {
    {"grid_x", &WorldConfig::gridX},
    {"grid_y", &WorldConfig::gridY},
    {"cyano_filaments", &WorldConfig::cyanoFilaments},
    {"oscillatoria_filaments", &WorldConfig::oscillatoriaFilaments},
    {"saprolegnia", &WorldConfig::saprolegnia},
    {"chytrid_clusters", &WorldConfig::chytridClusters},
    {"allomyces", &WorldConfig::allomyces},
    {"lichens", &WorldConfig::lichens},
    {"physarum", &WorldConfig::physarum},
};

// Placas guardadas antes de que los minerales fueran datos: sus claves de sedimento en inglés.
constexpr std::pair<const char*, const char*> kLegacySediment[] = {
    {"clay", "arcilla"},      {"quartz", "cuarzo"}, {"mica", "mica"},
    {"iron_oxide", "oxido_hierro"}, {"sulfur", "azufre"}, {"frustules", "frustula"},
};

// Mineral de una clave "sediment.<id>" (o de una clave vieja); vacío si no es de un mineral.
std::optional<MineralKind> sedimentMineral(const std::string& key) {
    constexpr std::string_view prefix = "sediment.";
    if (key.compare(0, prefix.size(), prefix) != 0)
        return std::nullopt;
    std::string_view id = std::string_view(key).substr(prefix.size());
    for (const auto& [old, now] : kLegacySediment)
        if (id == old)
            id = now;
    return findMineral(id);
}

// Clave del peso de un ser en las placas de versión 1: su nombre con guiones bajos.
std::string legacySpeciesKey(int i) {
    std::string k = "w.";
    for (const char* p = speciesInfo(static_cast<SpeciesId>(i)).name; *p; ++p)
        k += *p == ' ' ? '_' : *p;
    return k;
}

// Ser de una clave "w.<id>" (o "w.<Nombre_del_ser>" de una placa vieja).
std::optional<SpeciesId> weightSpecies(const std::string& key) {
    if (key.compare(0, 2, "w.") != 0)
        return std::nullopt;
    if (const auto id = findSpecies(std::string_view(key).substr(2)))
        return id;
    for (int i = 0; i < speciesCount(); ++i)
        if (key == legacySpeciesKey(i))
            return static_cast<SpeciesId>(i);
    return std::nullopt;
}

} // namespace

void PlateSpec::rebuild() {
    biome = std::clamp(biome, 0, biomeCount() - 1);
    if (!prepared.empty()) {
        if (const PreparedPlate* pp = findPreparedPlate(prepared)) {
            config = makePrepared(*pp, seed);
            return;
        }
        prepared.clear(); // no está cargada (se avisa al leer la placa): queda la gota
    }
    config = makeDrop(static_cast<BiomeId>(biome), seed);
    applyDropContent(config, content);
}

float PlateSpec::wildlife() const {
    return prepared.empty() && content == DropContent::Full ? dropAreaScale(config) : 0.0f;
}

PlateKind PlateSpec::kind() const {
    const PreparedPlate* pp = prepared.empty() ? nullptr : findPreparedPlate(prepared);
    return pp ? pp->kind : PlateKind::Drop;
}

std::string serializePlate(const PlateSpec& s) {
    std::ostringstream out;
    // Precisión de ida y vuelta: con las 6 cifras por defecto, los valores que el bioma varía por
    // seed se redondeaban y la placa leída ya no era la misma (ni su corrida).
    out.precision(std::numeric_limits<float>::max_digits10);
    // Versión 2: bioma, minerales y seres por id de texto (fase 20.0), y los paquetes cargados.
    out << "plate_version=3\n"
        << "biome=" << biome(static_cast<BiomeId>(s.biome)).key << '\n';
    if (!s.prepared.empty())
        out << "prepared=" << s.prepared << '\n';
    out
        << "seed=" << s.seed << '\n'
        << "content=" << static_cast<int>(s.content) << '\n';
    const WorldConfig& c = s.config;
    for (const PackInfo& pack : loadedPacks())
        out << "pack=" << pack.id << ' ' << pack.version << ' ' << fingerprintHex(pack.fingerprint) << '\n';
    out << "depth=" << c.depth << '\n' << "cell_culture=" << c.cellCulture << '\n'
        << "fixed_slide=" << c.fixedSlide << '\n'
        << "erythrocytes_cm2=" << c.erythrocytesPerCm2 << '\n' << "platelets_cm2=" << c.plateletsPerCm2 << '\n';
    for (const auto& [sp, count] : c.seeding)
        out << "seeding=" << speciesInfo(sp).key << ' ' << count << '\n';
    // Versión 3: células con su estado (gota de muestra de un cultivo, fase P1).
    for (const SampledCell& sc : c.sampled) {
        const Metabolism& m = sc.meta;
        out << "sampled=" << speciesInfo(sc.species).key << ' ' << sc.generation << ' ' << m.biomass << ' '
            << m.birthBiomass << ' ' << m.reserves << ' ' << m.phosphorus << ' ' << m.nitrogen << ' ' << m.silicon << ' '
            << m.iron << ' ' << m.vitamin << ' ' << m.ageHours << ' ' << m.pendingFissions;
        for (float g : sc.genome.v)
            out << ' ' << g;
        out << '\n';
    }
    out << "dish=" << c.dish << '\n' << "dish_margin=" << c.dishMargin << '\n' << "cell_size=" << c.cellSize << '\n';
    out << "bacteria=" << c.bacteria << '\n' << "detritus=" << c.detritus << '\n';
    out << "cyano_t_min=" << c.cyanoTMin << '\n' << "cyano_t_max=" << c.cyanoTMax << '\n';
    out << "patchiness=" << c.patchiness << '\n';
    out << "bdellovibrio_fraction=" << c.bdellovibrioFraction << '\n';
    for (const IntField& f : kConfigInts)
        out << f.key << '=' << c.*f.member << '\n';
    for (const FloatField& f : kMediumFloats)
        out << f.key << '=' << c.medium.*f.member << '\n';
    out << "anoxic=" << c.medium.anoxic << '\n';
    for (int k = 0; k < mineralCount(); ++k)
        out << "sediment." << mineralInfo(static_cast<MineralKind>(k)).key << '=' << c.sediment.count[k] << '\n';
    out << "sediment.rocks=" << c.sediment.rocks << '\n';
    out << "rock_min=" << c.sediment.rockMin << '\n' << "rock_max=" << c.sediment.rockMax << '\n';
    for (int i = 0; i < speciesCount(); ++i)
        out << "w." << speciesInfo(static_cast<SpeciesId>(i)).key << '=' << c.speciesWeight[i] << '\n';
    return out.str();
}

bool parsePlate(const std::string& text, PlateSpec& s, std::vector<std::string>* missing) {
    // Primero lo que decide el bioma; después, el resto pisa los valores.
    std::vector<std::pair<std::string, std::string>> kv;
    std::istringstream in(text);
    std::string line;
    bool versioned = false;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        kv.emplace_back(line.substr(0, eq), line.substr(eq + 1));
        versioned |= kv.back().first == "plate_version";
    }
    if (!versioned)
        return false;
    PlateSpec p;
    auto num = [](const std::string& v, double fallback) {
        try {
            return std::stod(v);
        } catch (...) {
            return fallback;
        }
    };
    auto lack = [&](const std::string& what) {
        if (missing && std::find(missing->begin(), missing->end(), what) == missing->end())
            missing->push_back(what);
    };
    for (const auto& [k, v] : kv) {
        if (k == "biome") {
            // Versión 2: id de texto; versión 1: el índice (los de fábrica no cambiaron de orden).
            if (const auto id = findBiome(v))
                p.biome = static_cast<int>(*id);
            else if (!v.empty() && std::isdigit(static_cast<unsigned char>(v[0])))
                p.biome = static_cast<int>(num(v, 0));
            else
                lack("bioma '" + v + "'");
        } else if (k == "pack") {
            const std::string id = v.substr(0, v.find(' '));
            if (!findPack(id))
                lack("paquete '" + id + "'");
        }
        else if (k == "seed") { // entero: un double pierde precisión desde 2^53
            try {
                p.seed = std::stoull(v);
            } catch (...) {
                p.seed = 1;
            }
        }
        else if (k == "prepared") {
            if (findPreparedPlate(v))
                p.prepared = v;
            else
                lack("placa preparada '" + v + "'");
        }
        else if (k == "content") p.content = static_cast<DropContent>(std::clamp(static_cast<int>(num(v, 2)), 0, 2));
    }
    p.rebuild();
    WorldConfig& c = p.config;
    bool seedingRead = false, sampledRead = false;
    for (const auto& [k, v] : kv) {
        const double x = num(v, 0);
        if (k == "dish") c.dish = true; // siempre con la pared de vidrio (las placas viejas sin vidrio ya no van)
        else if (k == "dish_margin") c.dishMargin = static_cast<float>(x);
        else if (k == "cell_size") c.cellSize = static_cast<float>(x);
        else if (k == "bacteria") c.bacteria = static_cast<std::size_t>(std::max(0.0, x));
        else if (k == "detritus") c.detritus = static_cast<std::size_t>(std::max(0.0, x));
        else if (k == "cyano_t_min") c.cyanoTMin = static_cast<float>(x);
        else if (k == "cyano_t_max") c.cyanoTMax = static_cast<float>(x);
        else if (k == "patchiness") c.patchiness = static_cast<float>(x);
        else if (k == "bdellovibrio_fraction") c.bdellovibrioFraction = static_cast<float>(x);
        else if (k == "anoxic") c.medium.anoxic = x != 0;
        else if (k == "rock_min") c.sediment.rockMin = static_cast<float>(x);
        else if (k == "cell_culture") c.cellCulture = x != 0;
        else if (k == "fixed_slide") c.fixedSlide = x != 0;
        else if (k == "erythrocytes_cm2") c.erythrocytesPerCm2 = static_cast<float>(x);
        else if (k == "platelets_cm2") c.plateletsPerCm2 = static_cast<float>(x);
        else if (k == "depth") c.depth = static_cast<float>(std::max(1.0, x));
        else if (k == "seeding") {
            // La siembra del archivo reemplaza a la de la placa preparada.
            if (!seedingRead)
                c.seeding.clear();
            seedingRead = true;
            const std::size_t space = v.find(' ');
            const std::string id = v.substr(0, space);
            if (const auto sp = findSpecies(id))
                c.seeding.emplace_back(*sp, space == std::string::npos ? 0 : static_cast<int>(num(v.substr(space + 1), 0)));
            else
                lack("ser '" + id + "'");
        }
        else if (k == "rock_max") c.sediment.rockMax = static_cast<float>(x);
        else if (k == "sampled") {
            if (!sampledRead)
                c.sampled.clear();
            sampledRead = true;
            std::istringstream fields(v);
            std::string id;
            SampledCell sc;
            Metabolism& m = sc.meta;
            fields >> id >> sc.generation >> m.biomass >> m.birthBiomass >> m.reserves >> m.phosphorus >> m.nitrogen >>
                m.silicon >> m.iron >> m.vitamin >> m.ageHours >> m.pendingFissions;
            for (float& g : sc.genome.v)
                fields >> g;
            if (const auto sp = findSpecies(id); sp && fields) {
                sc.species = *sp;
                c.sampled.push_back(sc);
            } else if (!sp) {
                lack("ser '" + id + "'");
            }
        }
        for (const IntField& f : kConfigInts)
            if (k == f.key) c.*f.member = static_cast<int>(x);
        for (const FloatField& f : kMediumFloats)
            if (k == f.key) c.medium.*f.member = static_cast<float>(x);
        if (k == "sediment.rocks") c.sediment.rocks = static_cast<int>(x);
        else if (const auto mineral = sedimentMineral(k)) c.sediment[*mineral] = static_cast<int>(x);
        if (k.compare(0, 2, "w.") == 0) {
            if (const auto sp = weightSpecies(k))
                c.speciesWeight[static_cast<int>(*sp)] = static_cast<float>(x);
            else
                lack("ser '" + k.substr(2) + "'");
        } else if (k.compare(0, 9, "sediment.") == 0 && k != "sediment.rocks" && !sedimentMineral(k)) {
            lack("mineral '" + k.substr(9) + "'");
        }
    }
    // La grilla tiene que ser potencia de 2 (la FFT lo necesita).
    for (int* g : {&c.gridX, &c.gridY}) {
        int n = 64;
        while (n < *g && n < 1024)
            n *= 2;
        *g = n;
    }
    s = p;
    return true;
}

std::string defaultPlatesDir() {
    namespace fs = std::filesystem;
    return (fs::path(defaultProgressPath()).parent_path() / "plates").string();
}

bool savePlate(const PlateSpec& spec, const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::ofstream out(path);
    if (!out)
        return false;
    out << serializePlate(spec);
    return static_cast<bool>(out);
}

bool loadPlate(const std::string& path, PlateSpec& spec) {
    std::ifstream in(path);
    if (!in)
        return false;
    std::stringstream ss;
    ss << in.rdbuf();
    return parsePlate(ss.str(), spec);
}

std::vector<std::string> listPlates(const std::string& dir) {
    namespace fs = std::filesystem;
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec))
        if (e.is_regular_file() && e.path().extension() == ".plate")
            names.push_back(e.path().stem().string());
    std::sort(names.begin(), names.end());
    return names;
}
