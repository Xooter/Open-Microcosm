#include "game/export.hpp"
#include "bio/physiology.hpp"

#include "build_info.hpp"
#include "game/parameters.hpp"

#include "game/biome.hpp"
#include "game/progress.hpp"
#include "game/run.hpp"
#include "game/sandbox.hpp"
#include "game/world.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace {

// Número sin depender de la locale (NaN e infinitos: null en JSON, vacío en CSV).
void appendNumber(std::string& out, double v, const char* nonFinite) {
    if (!finiteNumber(v)) {
        out += nonFinite;
        return;
    }
    char buf[32];
    const auto r = std::to_chars(buf, buf + sizeof buf, v);
    out.append(buf, r.ptr);
}

void appendCsvField(std::string& out, const std::string& s) {
    if (s.find_first_of(",\"\n") == std::string::npos) {
        out += s;
        return;
    }
    out += '"';
    for (char c : s) {
        if (c == '"')
            out += '"';
        out += c;
    }
    out += '"';
}

// Escritor de JSON mínimo: solo lo que hace falta para exportar (objetos, arreglos, cadenas y
// números), con sangría para que sea legible a mano.
class Json {
  public:
    std::string out;

    void beginObject(const char* key = nullptr) { open(key, '{'); }
    void endObject() { close('}'); }
    void beginArray(const char* key = nullptr) { open(key, '['); }
    void endArray() { close(']'); }
    void string(const char* key, const std::string& v) {
        item(key);
        quote(v);
    }
    void number(const char* key, double v) {
        item(key);
        appendNumber(out, v, "null");
    }
    void boolean(const char* key, bool v) {
        item(key);
        out += v ? "true" : "false";
    }
    // Magnitud con unidad: {"valor": v, "unidad": "µm"}.
    void quantity(const char* key, double v, const char* unit) {
        beginObject(key);
        number("valor", v);
        string("unidad", unit);
        endObject();
    }
    void numbers(const char* key, const std::vector<float>& v) {
        item(key);
        out += '[';
        for (std::size_t i = 0; i < v.size(); ++i) {
            if (i)
                out += ',';
            appendNumber(out, v[i], "null");
        }
        out += ']';
    }

  private:
    std::vector<bool> first_;

    void quote(const std::string& s) {
        out += '"';
        for (unsigned char c : s) {
            switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
            }
        }
        out += '"';
    }
    void newline() {
        out += '\n';
        out.append(2 * first_.size(), ' ');
    }
    void item(const char* key) {
        if (!first_.empty()) {
            if (!first_.back())
                out += ',';
            first_.back() = false;
            newline();
        }
        if (key) {
            quote(key);
            out += ": ";
        }
    }
    void open(const char* key, char c) {
        item(key);
        out += c;
        first_.push_back(true);
    }
    void close(char c) {
        const bool empty = first_.back();
        first_.pop_back();
        if (!empty)
            newline();
        out += c;
    }
};

const char* feedingName(FeedingMode f) {
    switch (f) {
    case FeedingMode::OralGroove: return "surco oral (filtrador)";
    case FeedingMode::Anterior: return "polo anterior";
    case FeedingMode::Phagocytosis: return "fagocitosis por contacto";
    case FeedingMode::Raptorial: return "raptorial (presas enteras)";
    case FeedingMode::Tentacles: return "tentaculos (succion)";
    case FeedingMode::NeckStrike: return "cuello extensible";
    case FeedingMode::Osmotroph: return "osmotrofia";
    case FeedingMode::None: return "autotrofa";
    }
    return "";
}

const char* storageName(Storage s) {
    switch (s) {
    case Storage::None: return "ninguna";
    case Storage::Starch: return "almidon";
    case Storage::Glycogen: return "glucogeno";
    case Storage::Paramylon: return "paramilon";
    case Storage::Chrysolaminarin: return "crisolaminarina";
    }
    return "";
}

const char* wallName(CellWall w) {
    switch (w) {
    case CellWall::None: return "ninguna";
    case CellWall::Cellulose: return "celulosa";
    case CellWall::Silica: return "silice";
    case CellWall::YeastGlucan: return "glucano de levadura";
    }
    return "";
}

void speciesList(Json& j, const char* key, std::uint32_t mask) {
    j.beginArray(key);
    for (int s = 0; s < kSpeciesCount; ++s)
        if (mask & speciesBit(static_cast<SpeciesId>(s)))
            j.string(nullptr, speciesInfo(static_cast<SpeciesId>(s)).name);
    j.endArray();
}

// Un valor de fisiologia.toml: el que usa la simulación (a 20 °C) y el publicado, con su fuente.
void writeSourced(Json& j, const char* key, const SourcedValue& v, const char* unit) {
    if (!v.present)
        return;
    j.beginObject(key);
    j.quantity("valor", v.value, unit);
    j.number("publicado", v.published);
    j.string("unidad_publicada", v.unit);
    j.quantity("temperatura_publicada", v.temperature, "°C");
    j.string("fuente", v.source);
    j.boolean("estimacion", v.estimate);
    j.endObject();
}

void writeSpecies(Json& j, SpeciesId id) {
    const SpeciesInfo& s = speciesInfo(id);
    const SpeciesChemistry& chem = speciesChemistry(id);
    j.beginObject();
    j.string("nombre", s.name);
    j.string("categoria", categoryName(categoryOf({BeingKind::Species, id})));
    j.string("locomocion", locomotionName(s.locomotion));
    j.quantity("semieje_mayor", s.spec.radiusX, "µm");
    j.quantity("semieje_menor", s.spec.radiusY, "µm");
    j.number("velocidad_relativa", s.speedScale);
    j.string("alimentacion", feedingName(s.feeding));
    speciesList(j, "presas", s.preyMask | s.mouthPreyMask);
    speciesList(j, "huye_de", s.fleeMask);
    j.quantity("manipulacion_por_presa", s.handlingHours, "h");
    j.boolean("fotosintesis", s.chloroplasts);
    j.number("fotosintesis_relativa", s.photosynthesisScale);
    j.boolean("auxotrofa_b12", s.vitaminB12);
    j.boolean("frustula_de_silice", s.frustule);
    j.number("divisiones_por_ciclo", s.fissions);
    j.boolean("enquista", s.encysts);
    j.boolean("sesil", s.sessile);
    j.quantity("largo_del_tallo", s.stalkLength, "µm");
    j.boolean("filamento", s.filament);
    j.number("colonias_hijas", s.colonyDaughters);
    j.boolean("gemacion", s.budding);
    j.number("masa_seca_relativa", s.dryMassScale);
    j.number("ph_min", s.phMin);
    j.number("ph_max", s.phMax);
    j.quantity("temperatura_min", s.tMin, "°C");
    j.quantity("temperatura_max", s.tMax, "°C");
    j.quantity("salinidad_max", s.salinityMax, "PSU");
    j.boolean("anaerobia", s.anaerobe);
    j.quantity("ciclosis", s.cyclosis, "µm/s");
    j.string("reserva", storageName(chem.storage));
    j.string("pared", wallName(chem.wall));
    const SpeciesPhysiology& phys = speciesPhysiology(id);
    j.beginObject("fisiologia"); // vacío: usa los valores generales del metabolismo
    writeSourced(j, "crecimiento_maximo", phys.growthMax, "1/h");
    writeSourced(j, "p_media_saturacion", phys.phosphorusHalfSaturation, "µM");
    writeSourced(j, "si_media_saturacion", phys.siliconHalfSaturation, "µM");
    j.endObject();
    j.endObject();
}

void writeBudget(Json& j, const char* key, const MassBudget& b) {
    j.beginObject(key);
    j.quantity("tiempo", b.time, "s");
    for (int e = 0; e < kElementCount; ++e) {
        const Element el = static_cast<Element>(e);
        const std::string unit = std::string("pmol ") + elementName(el);
        j.beginObject(elementName(el));
        j.number("total", b.total(el));
        j.string("unidad", unit);
        j.beginObject("compartimentos");
        for (int p = 0; p < kPoolCount; ++p)
            if (b.stock[e][p] != 0.0)
                j.number(poolName(static_cast<Pool>(p)), b.stock[e][p]);
        j.endObject();
        j.beginObject("flujos_acumulados");
        for (int f = 0; f < kFlowCount; ++f) {
            if (b.ledger.toWater[e][f] == 0.0 && b.ledger.fromWater[e][f] == 0.0)
                continue;
            j.beginObject(flowName(static_cast<Flow>(f)));
            j.number("al_agua", b.ledger.toWater[e][f]);
            j.number("desde_el_agua", b.ledger.fromWater[e][f]);
            j.endObject();
        }
        j.endObject();
        j.number("entrada_externa_neta", b.ledger.external[e]);
        j.endObject();
    }
    j.endObject();
}

} // namespace

std::string seriesCsv(const TimeSeries& series) {
    std::string out;
    for (std::size_t c = 0; c < series.columns(); ++c) {
        if (c)
            out += ',';
        const SeriesColumn& col = series.info(c);
        appendCsvField(out, col.unit.empty() ? col.name : col.name + " [" + col.unit + "]");
    }
    out += '\n';
    for (std::size_t r = 0; r < series.rows(); ++r) {
        for (std::size_t c = 0; c < series.columns(); ++c) {
            if (c)
                out += ',';
            appendNumber(out, series.column(c)[r], "");
        }
        out += '\n';
    }
    return out;
}

std::string machineDescription() {
#if defined(_WIN32)
    if (const char* id = std::getenv("PROCESSOR_IDENTIFIER"))
        return id;
#else
    std::ifstream in("/proc/cpuinfo");
    std::string line;
    while (std::getline(in, line))
        if (line.rfind("model name", 0) == 0) {
            const std::size_t colon = line.find(':');
            return colon == std::string::npos ? line : line.substr(line.find_first_not_of(' ', colon + 1));
        }
#endif
    return "";
}

std::string runJson(const World& world, const Run& run, const PlateSpec& plate, const RunProvenance& provenance) {
    Json j;
    j.beginObject();
    j.string("formato", "biogame-run");
    j.number("version", 2);
    j.string("programa", "BioGame: sandbox de vida microscopica");

    // Procedencia: con el mismo binario, máquina, motor y opciones, la corrida se repite bit a
    // bit; entre máquinas (otra CPU, otra GPU o driver) solo estadísticamente, con réplicas.
    j.beginObject("procedencia");
    j.string("version_programa", BIO_VERSION);
    j.string("commit", BIO_GIT);
    j.string("compilacion", BIO_BUILD_FLAGS);
    j.string("motor_xpbd", provenance.engine);
    if (!provenance.gpu.empty())
        j.string("gpu", provenance.gpu);
    j.boolean("lod", provenance.lod);
    j.string("maquina", machineDescription());
    // Cuerpos que la red de seguridad tuvo que rearmar (posiciones no finitas): debería ser 0.
    j.number("fallas_numericas", static_cast<double>(world.numericalFaults()));
    // Intervenciones del usuario (van en la escena .plate que acompaña la exportación).
    j.number("intervenciones", static_cast<double>(run.interventions().entries().size()));
    char hash[20];
    std::snprintf(hash, sizeof hash, "%016llx", static_cast<unsigned long long>(run.interventions().hash()));
    j.string("huella_intervenciones", hash);
    j.string("reproducibilidad", "bit a bit con el mismo binario, maquina, motor y opciones; entre maquinas, "
                                 "estadistica (replicas)");
    j.endObject();

    // Todos los parámetros ajustables con su valor vigente (game/parameters).
    j.beginObject("registro_de_parametros");
    World& mutableWorld = const_cast<World&>(world); // los accesos del registro no modifican nada
    for (const ParamInfo& p : parameters())
        j.quantity(p.key, parameterValue(mutableWorld, p), p.unit);
    j.endObject();
    j.string("bioma", biome(static_cast<BiomeId>(plate.biome)).name);
    j.string("seed", std::to_string(plate.seed));
    j.quantity("tiempo_simulado", world.simTime(), "s");
    j.quantity("tiempo_biologico", world.simTime() * world.bioTimeScale() / 3600.0f, "h");

    j.beginObject("parametros");
    j.number("escala_de_tiempo_biologico", world.bioTimeScale());
    j.string("escala_de_tiempo_biologico_nota", "segundos biologicos por segundo real (60: 1 min real = 1 h biologica)");
    j.number("fraccion_de_masa_seca", units::kDryMassFraction);
    j.quantity("intervalo_de_muestreo", run.sampleInterval() * static_cast<float>(run.series().stride()), "s");
    // La placa con el ambiente vigente (los cambios en vivo del panel Placa incluidos): el mismo
    // texto clave=valor que un .plate, así la corrida se puede volver a armar.
    PlateSpec live = plate;
    live.config.medium = world.medium();
    j.beginObject("placa");
    std::istringstream lines(serializePlate(live));
    std::string line;
    while (std::getline(lines, line)) {
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos || line.empty() || line[0] == '#')
            continue;
        j.string(line.substr(0, eq).c_str(), line.substr(eq + 1));
    }
    j.endObject();
    const MetabolismParams& mp = world.metabolismParams();
    j.beginObject("metabolismo");
    j.quantity("mantenimiento", mp.maintenancePerHour, "1/h");
    j.quantity("crecimiento_maximo", mp.growthPerHourMax, "1/h");
    j.number("rendimiento", mp.growthYield);
    j.quantity("o2_media_saturacion", mp.o2HalfSaturation, "µM");
    j.number("penalidad_fermentacion", mp.fermentationPenalty);
    j.quantity("fotosintesis_maxima", mp.photosynthesisPerHour, "1/h");
    j.number("exudacion", mp.exudation);
    j.quantity("cuota_p", mp.phosphorusRatio, "pmol P/pmol");
    j.quantity("cuota_n", mp.nitrogenRatio, "pmol N/pmol");
    j.quantity("cuota_si", mp.siliconRatio, "pmol Si/pmol");
    j.quantity("cuota_fe", mp.ironRatio, "pmol Fe/pmol");
    j.quantity("cuota_b12", mp.vitaminRatio, "pmol/pmol");
    j.quantity("p_media_saturacion", mp.phosphorusHalfSaturation, "µM");
    j.quantity("n_media_saturacion", mp.nitrogenHalfSaturation, "µM");
    j.quantity("si_media_saturacion", mp.siliconHalfSaturation, "µM");
    j.quantity("fe_media_saturacion", mp.ironHalfSaturation, "µM");
    j.quantity("b12_media_saturacion", mp.vitaminHalfSaturation, "µM");
    j.endObject();
    j.endObject();

    const TimeSeries& s = run.series();
    j.beginObject("serie");
    j.beginArray("columnas");
    for (std::size_t c = 0; c < s.columns(); ++c) {
        j.beginObject();
        j.string("nombre", s.info(c).name);
        j.string("unidad", s.info(c).unit);
        j.string("grupo", s.info(c).group);
        j.endObject();
    }
    j.endArray();
    j.beginObject("datos");
    for (std::size_t c = 0; c < s.columns(); ++c)
        j.numbers(s.info(c).name.c_str(), s.column(c));
    j.endObject();
    j.endObject();

    j.beginObject("balance_de_masa");
    j.string("nota", "P, N, Si y Fe se conservan salvo entradas externas (gotero, ajustes, fijacion de N2). "
                     "El C tiene frontera abierta: el CO2 se intercambia con el aire en tiempo real.");
    writeBudget(j, "inicio", run.startBudget());
    writeBudget(j, "ahora", run.budget());
    j.beginObject("residuo");
    for (int e = 0; e < kElementCount; ++e) {
        const Element el = static_cast<Element>(e);
        if (conservedElement(el))
            j.number(elementName(el), run.residual(el));
    }
    j.endObject();
    j.endObject();
    j.endObject();
    j.out += '\n';
    return j.out;
}

std::string speciesJson() {
    Json j;
    j.beginObject();
    j.string("formato", "biogame-species");
    j.number("version", 2); // 2: fisiología con fuentes
    j.beginArray("especies");
    for (int s = 0; s < kSpeciesCount; ++s)
        writeSpecies(j, static_cast<SpeciesId>(s));
    j.endArray();
    j.endObject();
    j.out += '\n';
    return j.out;
}

std::string defaultExportsDir() {
    namespace fs = std::filesystem;
    return (fs::path(defaultProgressPath()).parent_path() / "exports").string();
}

bool writeTextFile(const std::string& path, const std::string& text) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::ofstream out(path, std::ios::binary);
    if (!out)
        return false;
    out << text;
    return static_cast<bool>(out);
}

std::string timestampedName(const std::string& prefix) {
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y%m%d_%H%M%S", &tm);
    return prefix + "_" + buf;
}
