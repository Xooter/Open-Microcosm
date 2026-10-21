#include "game/medium_toml.hpp"

namespace {

struct MediumKey {
    const char* key;
    float MediumParams::*member;
    bool nonNegative; // concentraciones y tasas; la temperatura puede ser bajo cero
};

// Claves con la unidad de cada una documentada en docs/content/.
// Las que también son parámetros en vivo usan el mismo nombre que su clave en game/parameters
// (sin el prefijo "medio.").
constexpr MediumKey kMediumKeys[] = {
    {"temperature", &MediumParams::temperature, false},
    {"salinity", &MediumParams::salinity, true},
    {"light", &MediumParams::surfaceLight, true},
    {"o2_exchange", &MediumParams::o2AirExchange, true},
    {"alkalinity", &MediumParams::alkalinity, true},
    {"phosphate", &MediumParams::phosphate, true},
    {"nitrogen", &MediumParams::nitrogen, true},
    {"silicate", &MediumParams::silicate, true},
    {"iron", &MediumParams::iron, true},
    {"vitamin_b12", &MediumParams::vitamin, true},
    {"b12_source", &MediumParams::vitaminSourcePerHour, true},
    {"b12_decay", &MediumParams::vitaminDecayPerHour, true},
    {"frustule_dissolution", &MediumParams::frustuleDissolutionPerHour, true},
    {"phages", &MediumParams::phage, true},
    {"phage_inactivation", &MediumParams::phageDecayPerHour, true},
    {"toxin_decay", &MediumParams::toxinDecayPerHour, true},
    {"ahl_half_life", &MediumParams::ahlHalfLifeHours, true},
    {"organic_inflow", &MediumParams::organicInflow, true},
    {"decomposition", &MediumParams::detritusDecay, true},
    {"detritus_inflow", &MediumParams::detritusSpawnRate, true},
    {"nutrient_uptake", &MediumParams::nutrientUptake, true},
    {"dust", &MediumParams::dustRate, true},
    {"glucose", &MediumParams::organic, true},
    {"glutamine", &MediumParams::glutamine, true},
    {"serum", &MediumParams::serum, true},
    {"phenol_red", &MediumParams::phenolRed, true},
    {"carbonate_pka", &MediumParams::carbonatePK, true},
    {"air_co2", &MediumParams::co2Atmosphere, true},
    {"osmolytes", &MediumParams::osmolytes, true},
};

} // namespace

void readMedium(const toml::table& t, const std::string& where, MediumParams& m, std::vector<std::string>& errors,
                std::vector<std::string>& warnings) {
    toml_io::Reader r{t, where, {}};
    for (const auto& [k, v] : t) {
        bool found = k.str() == "anoxic";
        for (const MediumKey& mk : kMediumKeys)
            found = found || k.str() == mk.key;
        if (!found)
            warnings.push_back(where + ": clave desconocida '" + std::string(k.str()) + "'");
    }
    for (const MediumKey& mk : kMediumKeys) {
        if (mk.nonNegative)
            r.count(mk.key, m.*mk.member);
        else
            r.num(mk.key, m.*mk.member);
    }
    r.flag("anoxic", m.anoxic);
    if (m.surfaceLight > 1.0f)
        r.errors.push_back(where + ": 'light' es relativa a la lámpara (0–1)");
    errors.insert(errors.end(), r.errors.begin(), r.errors.end());
}

void readSediment(const toml::table& t, const std::string& where, Sediment& s, std::vector<std::string>& errors,
                  std::vector<std::string>& warnings) {
    // Las claves son ids de minerales (de cualquier paquete ya cargado), más los granos de arena.
    toml_io::Reader r{t, where, {}};
    for (const auto& [k, v] : t) {
        const std::string_view key = k.str();
        if (key == "grains" || key == "grain_radius")
            continue;
        const auto kind = findMineral(key);
        if (!kind) {
            warnings.push_back(where + ": '" + std::string(key) + "' no es un mineral conocido");
            continue;
        }
        const std::string name(key);
        r.count(name.c_str(), s[*kind]);
    }
    r.count("grains", s.rocks);
    r.range("grain_radius", s.rockMin, s.rockMax);
    if (s.rockMin <= 0.0f)
        r.errors.push_back(where + ": 'grain_radius' tiene que ser positivo");
    errors.insert(errors.end(), r.errors.begin(), r.errors.end());
}
