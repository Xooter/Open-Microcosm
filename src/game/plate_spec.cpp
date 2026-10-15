#include "game/plate_spec.hpp"

#include "game/progress.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
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
    {"phage", &MediumParams::phage},
    {"phage_decay_per_hour", &MediumParams::phageDecayPerHour},
    {"toxin_decay_per_hour", &MediumParams::toxinDecayPerHour},
    {"ahl_half_life_hours", &MediumParams::ahlHalfLifeHours},
    {"temperature", &MediumParams::temperature},
    {"salinity", &MediumParams::salinity},
    {"organic_inflow", &MediumParams::organicInflow},
    {"dust_rate", &MediumParams::dustRate},
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

struct SedimentField {
    const char* key;
    int Sediment::*member;
};

constexpr SedimentField kSedimentInts[] = {
    {"clay", &Sediment::clay},   {"quartz", &Sediment::quartz},       {"mica", &Sediment::mica},
    {"iron_oxide", &Sediment::ironOxide}, {"sulfur", &Sediment::sulfur}, {"frustules", &Sediment::frustules},
    {"rocks", &Sediment::rocks},
};

std::string speciesKey(int i) {
    std::string k = "w.";
    for (const char* p = speciesInfo(static_cast<SpeciesId>(i)).name; *p; ++p)
        k += *p == ' ' ? '_' : *p;
    return k;
}

} // namespace

void PlateSpec::rebuild() {
    biome = std::clamp(biome, 0, kBiomeCount - 1);
    config = makeDrop(static_cast<BiomeId>(biome), seed);
    applyDropContent(config, content);
}

float PlateSpec::wildlife() const { return content == DropContent::Full ? dropAreaScale(config) : 0.0f; }

std::string serializePlate(const PlateSpec& s) {
    std::ostringstream out;
    // Precisión de ida y vuelta: con las 6 cifras por defecto, los valores que el bioma varía por
    // seed se redondeaban y la placa leída ya no era la misma (ni su corrida).
    out.precision(std::numeric_limits<float>::max_digits10);
    out << "plate_version=1\n"
        << "biome=" << s.biome << '\n'
        << "seed=" << s.seed << '\n'
        << "content=" << static_cast<int>(s.content) << '\n';
    const WorldConfig& c = s.config;
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
    for (const SedimentField& f : kSedimentInts)
        out << "sediment." << f.key << '=' << c.sediment.*f.member << '\n';
    out << "rock_min=" << c.sediment.rockMin << '\n' << "rock_max=" << c.sediment.rockMax << '\n';
    for (int i = 0; i < kSpeciesCount; ++i)
        out << speciesKey(i) << '=' << c.speciesWeight[i] << '\n';
    return out.str();
}

bool parsePlate(const std::string& text, PlateSpec& s) {
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
    for (const auto& [k, v] : kv) {
        if (k == "biome") p.biome = static_cast<int>(num(v, 0));
        else if (k == "seed") { // entero: un double pierde precisión desde 2^53
            try {
                p.seed = std::stoull(v);
            } catch (...) {
                p.seed = 1;
            }
        }
        else if (k == "content") p.content = static_cast<DropContent>(std::clamp(static_cast<int>(num(v, 2)), 0, 2));
    }
    p.rebuild();
    WorldConfig& c = p.config;
    for (const auto& [k, v] : kv) {
        const double x = num(v, 0);
        if (k == "dish") c.dish = x != 0;
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
        else if (k == "rock_max") c.sediment.rockMax = static_cast<float>(x);
        for (const IntField& f : kConfigInts)
            if (k == f.key) c.*f.member = static_cast<int>(x);
        for (const FloatField& f : kMediumFloats)
            if (k == f.key) c.medium.*f.member = static_cast<float>(x);
        for (const SedimentField& f : kSedimentInts)
            if (k == std::string("sediment.") + f.key) c.sediment.*f.member = static_cast<int>(x);
        for (int i = 0; i < kSpeciesCount; ++i)
            if (k == speciesKey(i)) c.speciesWeight[i] = static_cast<float>(x);
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
