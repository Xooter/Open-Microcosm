#include "game/medium_toml.hpp"

namespace {

struct MediumKey {
    const char* key;
    float MediumParams::*member;
    bool nonNegative; // concentraciones y tasas; la temperatura puede ser bajo cero
};

// Claves en español con la unidad de cada una documentada en assets/content/base/biomas.toml.
// Las que también son parámetros en vivo usan el mismo nombre que su clave en game/parameters
// (sin el prefijo "medio.").
constexpr MediumKey kMediumKeys[] = {
    {"temperatura", &MediumParams::temperature, false},
    {"salinidad", &MediumParams::salinity, true},
    {"luz", &MediumParams::surfaceLight, true},
    {"intercambio_o2", &MediumParams::o2AirExchange, true},
    {"alcalinidad", &MediumParams::alkalinity, true},
    {"fosfato", &MediumParams::phosphate, true},
    {"nitrogeno", &MediumParams::nitrogen, true},
    {"silice", &MediumParams::silicate, true},
    {"hierro", &MediumParams::iron, true},
    {"vitamina_b12", &MediumParams::vitamin, true},
    {"b12_fuente", &MediumParams::vitaminSourcePerHour, true},
    {"b12_degradacion", &MediumParams::vitaminDecayPerHour, true},
    {"disolucion_frustulas", &MediumParams::frustuleDissolutionPerHour, true},
    {"fagos", &MediumParams::phage, true},
    {"fagos_inactivacion", &MediumParams::phageDecayPerHour, true},
    {"toxinas_degradacion", &MediumParams::toxinDecayPerHour, true},
    {"ahl_vida_media", &MediumParams::ahlHalfLifeHours, true},
    {"aporte_organico", &MediumParams::organicInflow, true},
    {"descomposicion", &MediumParams::detritusDecay, true},
    {"detritos_entrantes", &MediumParams::detritusSpawnRate, true},
    {"consumo_nutrientes", &MediumParams::nutrientUptake, true},
    {"polvo", &MediumParams::dustRate, true},
    {"glucosa", &MediumParams::organic, true},
    {"glutamina", &MediumParams::glutamine, true},
    {"suero", &MediumParams::serum, true},
    {"rojo_fenol", &MediumParams::phenolRed, true},
    {"pka_carbonato", &MediumParams::carbonatePK, true},
    {"co2_aire", &MediumParams::co2Atmosphere, true},
    {"osmolitos", &MediumParams::osmolytes, true},
};

} // namespace

void readMedium(const toml::table& t, const std::string& where, MediumParams& m, std::vector<std::string>& errors,
                std::vector<std::string>& warnings) {
    toml_io::Reader r{t, where, {}};
    for (const auto& [k, v] : t) {
        bool found = k.str() == "anoxico";
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
    r.flag("anoxico", m.anoxic);
    if (m.surfaceLight > 1.0f)
        r.errors.push_back(where + ": 'luz' es relativa a la lámpara (0–1)");
    errors.insert(errors.end(), r.errors.begin(), r.errors.end());
}

void readSediment(const toml::table& t, const std::string& where, Sediment& s, std::vector<std::string>& errors,
                  std::vector<std::string>& warnings) {
    // Las claves son ids de minerales (de cualquier paquete ya cargado), más los granos de arena.
    toml_io::Reader r{t, where, {}};
    for (const auto& [k, v] : t) {
        const std::string_view key = k.str();
        if (key == "granos" || key == "grano_radio")
            continue;
        const auto kind = findMineral(key);
        if (!kind) {
            warnings.push_back(where + ": '" + std::string(key) + "' no es un mineral conocido");
            continue;
        }
        const std::string name(key);
        r.count(name.c_str(), s[*kind]);
    }
    r.count("granos", s.rocks);
    r.range("grano_radio", s.rockMin, s.rockMax);
    if (s.rockMin <= 0.0f)
        r.errors.push_back(where + ": 'grano_radio' tiene que ser positivo");
    errors.insert(errors.end(), r.errors.begin(), r.errors.end());
}
