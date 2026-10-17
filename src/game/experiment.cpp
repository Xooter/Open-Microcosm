#include "game/experiment.hpp"

#include "bio/physiology.hpp"
#include "build_info.hpp"
#include "core/profiling.hpp"
#include "core/rng.hpp"
#include "game/biome.hpp"
#include "game/dose_fit.hpp"
#include "game/export.hpp"
#include "game/parameters.hpp"
#include "game/prepared.hpp"
#include "game/reagent.hpp"
#include "game/run.hpp"
#include "game/scene.hpp"
#include "game/world.hpp"

#include <omp.h>
#include <toml++/toml.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <mutex>
#include <sstream>
#include <thread>

namespace {

constexpr float kPlateDt = 1.0f / 60.0f; // el paso fijo de la placa (como la app)

std::string num(double v) {
    char buf[40];
    const auto r = std::to_chars(buf, buf + sizeof buf, v);
    return std::string(buf, r.ptr);
}

std::string quoted(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\')
            out += '\\';
        if (c == '\n') {
            out += "\\n";
            continue;
        }
        out += c;
    }
    return out + '"';
}

// Columnas comunes a cultivos y placas: las especies (por id), las bacterias y la química.
void columnsFor(ExperimentSpec::Kind kind, std::vector<std::string>& names, std::vector<std::string>& units) {
    const bool culture = kind == ExperimentSpec::Kind::Culture;
    for (int s = 0; s < speciesCount(); ++s) {
        names.push_back(speciesInfo(static_cast<SpeciesId>(s)).key);
        units.push_back(culture ? "individuos/mL" : "individuos");
    }
    names.push_back("bacterias");
    units.push_back(culture ? "bacterias/mL" : "bacterias");
    for (int k = 0; k < kSoluteCount; ++k) {
        names.push_back(soluteKey(static_cast<Solute>(k)));
        units.push_back("µM");
    }
    names.push_back("pH");
    units.push_back("");
}

void cultureRow(const CultureRun& run, std::vector<double>& row) {
    const Culture& c = run.culture();
    const CultureCensus census = c.census();
    const double ml = std::max(1e-12, c.volumeMl());
    row.clear();
    for (int s = 0; s < speciesCount(); ++s)
        row.push_back(census.species[s] / ml);
    row.push_back(static_cast<double>(census.bacteria) / ml);
    for (int k = 0; k < kSoluteCount; ++k)
        row.push_back(c.concentration(static_cast<Solute>(k)));
    row.push_back(c.ph());
}

void plateRow(const World& w, std::vector<double>& row) {
    const Census c = w.census();
    row.clear();
    for (int s = 0; s < speciesCount(); ++s)
        row.push_back(c.species[s]);
    row.push_back(c.bacteria);
    // En el orden de Solute: o2, co2, fosfato, nitrogeno, silice, hierro, b12, organica.
    for (const float v : {c.meanOxygen, c.meanCO2, c.meanPhosphate, c.meanNitrogen, c.meanSilicate, c.meanIron,
                          c.meanVitamin, c.meanOrganic})
        row.push_back(v);
    row.push_back(c.meanPH);
}

float* mediumField(MediumParams& m, const std::string& solute) {
    if (solute == "fosfato") return &m.phosphate;
    if (solute == "nitrogeno") return &m.nitrogen;
    if (solute == "silice") return &m.silicate;
    if (solute == "hierro") return &m.iron;
    if (solute == "b12") return &m.vitamin;
    if (solute == "organica") return &m.organic;
    return nullptr;
}

void store(ReplicaResult& r, const std::vector<double>& row) {
    if (r.values.size() < row.size())
        r.values.resize(row.size());
    for (std::size_t c = 0; c < row.size(); ++c)
        r.values[c].push_back(row[c]);
}

ReplicaResult runCultureReplica(const ExperimentSpec& spec, const ExperimentLevel& level, std::uint64_t seed) {
    ReplicaResult out;
    CultureSpec cs = spec.culture;
    cs.seed = seed;
    std::vector<CultureEvent> events = spec.cultureEvents;
    std::vector<CultureEvent> extra;
    for (const auto& [key, value] : level.settings) {
        if (key == "temperatura")
            cs.temperature = static_cast<float>(value);
        else if (key == "luz")
            cs.light = static_cast<float>(value);
        else if (key == "bacterias") {
            events.erase(std::remove_if(events.begin(), events.end(),
                                        [](const CultureEvent& e) { return e.step == 0 && e.op == CultureOp::Bacteria; }),
                         events.end());
            extra.push_back({0, CultureOp::Bacteria, "", value});
        } else if (key.rfind("soluto:", 0) == 0) {
            extra.push_back({0, CultureOp::Concentration, key.substr(7), value});
        } else if (key.rfind("reactivo:", 0) == 0) {
            extra.push_back({0, CultureOp::Reagent, key.substr(9), value});
        } else if (key.rfind("inocular:", 0) == 0) {
            const std::string id = key.substr(9);
            events.erase(std::remove_if(events.begin(), events.end(),
                                        [&](const CultureEvent& e) {
                                            return e.step == 0 && e.op == CultureOp::Inoculate && e.key == id;
                                        }),
                         events.end());
            extra.push_back({0, CultureOp::Inoculate, id, value});
        }
    }
    events.insert(events.begin(), extra.begin(), extra.end());
    CultureRun run;
    run.start(cs);
    run.schedule(events);
    std::vector<double> row;
    const double step = std::max(kCultureStep / 3600.0, spec.sampleHours);
    for (double t = 0.0; t <= spec.hours + 1e-9; t += step) {
        run.advance(t * 3600.0 - run.hours() * 3600.0);
        cultureRow(run, row);
        store(out, row);
    }
    return out;
}

ReplicaResult runPlateReplica(const ExperimentSpec& spec, const ExperimentLevel& level, std::uint64_t seed) {
    ReplicaResult out;
    PlateSpec plate;
    std::vector<Intervention> events;
    parseScene(spec.plateScene, plate, events);
    plate.seed = seed;
    plate.config.seed = seed;
    for (const auto& [key, value] : level.settings) {
        if (key == "temperatura")
            plate.config.medium.temperature = static_cast<float>(value);
        else if (key.rfind("soluto:", 0) == 0) {
            if (float* f = mediumField(plate.config.medium, key.substr(7)))
                *f = static_cast<float>(value);
        } else if (findParameter(key)) {
            Intervention iv;
            iv.kind = InterventionKind::Parameter;
            iv.key = key;
            iv.v[0] = static_cast<float>(value);
            iv.step = 0;
            events.insert(events.begin(), iv);
        }
    }
    World world(plate.config);
    Run run;
    run.start(world, plateRunConfig(plate), 1);
    run.interventions().schedule(events);
    std::vector<double> row;
    auto labHours = [&] { return world.simTime() * world.bioTimeScale() / 3600.0; };
    double next = 0.0;
    while (true) {
        if (labHours() + 1e-9 >= next) {
            plateRow(world, row);
            store(out, row);
            next += std::max(1e-3, spec.sampleHours);
            if (next > spec.hours + 1e-9)
                break;
        }
        run.interventions().beforeStep(world, kPlateDt);
        world.step(kPlateDt);
        run.update(world, kPlateDt);
    }
    return out;
}

// Los registros de contenido se cargan la primera vez que se consultan: antes de abrir los hilos.
void preloadRegistries() {
    speciesCount();
    speciesPhysiology(static_cast<SpeciesId>(0));
    biomeCount();
    vesselCount();
    cultureMediumCount();
    preparedPlateCount();
    reagentCount();
    parameters();
}

} // namespace

int ExperimentResult::column(const std::string& name) const {
    for (std::size_t c = 0; c < columns.size(); ++c)
        if (columns[c] == name)
            return static_cast<int>(c);
    return -1;
}

std::uint64_t replicaSeed(std::uint64_t seed, int level, int replica) {
    // Una por réplica y nivel, sin depender del orden en que corren.
    Rng r(seed, rng_stream::kCulture + 80, static_cast<std::uint64_t>(level), static_cast<std::uint64_t>(replica));
    return r.next() >> 16;
}

std::vector<ExperimentLevel> experimentLevels(const ExperimentSpec& spec) {
    std::vector<ExperimentLevel> levels(1);
    for (const ExperimentFactor& f : spec.factors) {
        if (f.values.empty())
            continue;
        std::vector<ExperimentLevel> next;
        for (const ExperimentLevel& l : levels)
            for (double v : f.values) {
                ExperimentLevel n = l;
                n.settings.emplace_back(f.key, v);
                n.label += (n.label.empty() ? "" : ", ") + f.key + "=" + num(v);
                next.push_back(n);
            }
        levels = std::move(next);
    }
    if (levels.size() == 1 && levels[0].label.empty())
        levels[0].label = "base";
    return levels;
}

double studentT975(int dof) {
    // Cuantil 0,975 de la t de Student (tabla; ~1,96 para muchos grados de libertad).
    static const double table[] = {0.0,   12.706, 4.303, 3.182, 2.776, 2.571, 2.447, 2.365, 2.306, 2.262, 2.228,
                                   2.201, 2.179,  2.160, 2.145, 2.131, 2.120, 2.110, 2.101, 2.093, 2.086, 2.080,
                                   2.074, 2.069,  2.064, 2.060, 2.056, 2.052, 2.048, 2.045, 2.042};
    if (dof <= 0)
        return 0.0;
    if (dof <= 30)
        return table[dof];
    return 1.96 + 2.4 / dof; // aproximación (error < 0,2 % desde 30)
}

SummaryPoint summarize(const ExperimentResult& r, int level, int column, std::size_t sample) {
    SummaryPoint p;
    double sum = 0.0, sq = 0.0;
    for (const ReplicaResult& rep : r.replicas) {
        if (rep.level != level || column < 0 || static_cast<std::size_t>(column) >= rep.values.size() ||
            sample >= rep.values[static_cast<std::size_t>(column)].size())
            continue;
        const double v = rep.values[static_cast<std::size_t>(column)][sample];
        sum += v;
        sq += v * v;
        ++p.n;
    }
    if (p.n == 0)
        return p;
    p.mean = sum / p.n;
    if (p.n > 1) {
        p.sd = std::sqrt(std::max(0.0, (sq - p.n * p.mean * p.mean) / (p.n - 1)));
        p.ci95 = studentT975(p.n - 1) * p.sd / std::sqrt(static_cast<double>(p.n));
    }
    return p;
}

ExperimentResult runExperiment(const ExperimentSpec& spec, const std::function<void(int, int)>& progress,
                               const std::atomic<bool>* cancel) {
    PROFILE_SCOPE("runExperiment");
    preloadRegistries();
    const auto t0 = std::chrono::steady_clock::now();
    ExperimentResult result;
    result.spec = spec;
    result.levels = experimentLevels(spec);
    columnsFor(spec.kind, result.columns, result.units);
    const double step = spec.kind == ExperimentSpec::Kind::Culture ? std::max(kCultureStep / 3600.0, spec.sampleHours)
                                                                   : std::max(1e-3, spec.sampleHours);
    for (double t = 0.0; t <= spec.hours + 1e-9; t += step)
        result.times.push_back(t);
    const int replicas = std::max(1, spec.replicas);
    const int total = static_cast<int>(result.levels.size()) * replicas;
    result.replicas.resize(static_cast<std::size_t>(total));
    std::atomic<int> next{0}, done{0};
    auto worker = [&] {
        omp_set_num_threads(1); // una corrida por hilo: sin anidar hilos de OpenMP
        for (int job = next++; job < total; job = next++) {
            if (cancel && cancel->load())
                break;
            const int level = job / replicas, replica = job % replicas;
            const std::uint64_t seed = replicaSeed(spec.seed, level, replica);
            const ExperimentLevel& lv = result.levels[static_cast<std::size_t>(level)];
            ReplicaResult r = spec.kind == ExperimentSpec::Kind::Culture ? runCultureReplica(spec, lv, seed)
                                                                        : runPlateReplica(spec, lv, seed);
            r.level = level;
            r.replica = replica;
            r.seed = seed;
            result.replicas[static_cast<std::size_t>(job)] = std::move(r);
            const int d = ++done;
            if (progress)
                progress(d, total);
        }
    };
    int threads = spec.threads > 0 ? spec.threads : std::max(1, static_cast<int>(std::thread::hardware_concurrency()) / 2);
    threads = std::max(1, std::min(threads, total));
    std::vector<std::thread> pool;
    for (int k = 1; k < threads; ++k)
        pool.emplace_back(worker);
    worker();
    for (std::thread& t : pool)
        t.join();
    // Las corridas canceladas quedan sin datos: se sacan.
    result.replicas.erase(std::remove_if(result.replicas.begin(), result.replicas.end(),
                                         [](const ReplicaResult& r) { return r.values.empty(); }),
                          result.replicas.end());
    result.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return result;
}

std::string experimentLongCsv(const ExperimentResult& r) {
    std::string out = "nivel,replica,seed,t [h],columna,unidad,valor\n";
    for (const ReplicaResult& rep : r.replicas)
        for (std::size_t c = 0; c < rep.values.size() && c < r.columns.size(); ++c) {
            const std::vector<double>& v = rep.values[c];
            if (std::all_of(v.begin(), v.end(), [](double x) { return x == 0.0; }))
                continue; // especies ausentes
            for (std::size_t s = 0; s < v.size() && s < r.times.size(); ++s)
                out += quoted(r.levels[static_cast<std::size_t>(rep.level)].label) + ',' + std::to_string(rep.replica) + ',' +
                       std::to_string(rep.seed) + ',' + num(r.times[s]) + ',' + r.columns[c] + ',' + quoted(r.units[c]) + ',' +
                       num(v[s]) + '\n';
        }
    return out;
}

namespace {
bool columnHasData(const ExperimentResult& r, std::size_t c) {
    for (const ReplicaResult& rep : r.replicas)
        if (c < rep.values.size())
            for (double x : rep.values[c])
                if (x != 0.0)
                    return true;
    return false;
}
} // namespace

std::string experimentSummaryCsv(const ExperimentResult& r) {
    std::string out = "nivel,t [h],columna,unidad,n,media,de,ic95\n";
    for (std::size_t l = 0; l < r.levels.size(); ++l)
        for (std::size_t c = 0; c < r.columns.size(); ++c) {
            if (!columnHasData(r, c))
                continue;
            for (std::size_t s = 0; s < r.times.size(); ++s) {
                const SummaryPoint p = summarize(r, static_cast<int>(l), static_cast<int>(c), s);
                if (p.n == 0)
                    continue;
                out += quoted(r.levels[l].label) + ',' + num(r.times[s]) + ',' + r.columns[c] + ',' + quoted(r.units[c]) + ',' +
                       std::to_string(p.n) + ',' + num(p.mean) + ',' + num(p.sd) + ',' + num(p.ci95) + '\n';
            }
        }
    return out;
}

std::string experimentJson(const ExperimentResult& r) {
    std::ostringstream j;
    j << "{\n  \"formato\": \"biogame-experimento\",\n  \"version\": 1,\n";
    j << "  \"nombre\": " << quoted(r.spec.name) << ",\n";
    j << "  \"tipo\": \"" << (r.spec.kind == ExperimentSpec::Kind::Culture ? "cultivo" : "placa") << "\",\n";
    j << "  \"procedencia\": {\"version_programa\": " << quoted(BIO_VERSION) << ", \"commit\": " << quoted(BIO_GIT)
      << ", \"compilacion\": " << quoted(BIO_BUILD_FLAGS) << ", \"motor_xpbd\": \"CPU\", \"maquina\": "
      << quoted(machineDescription())
      << ", \"reproducibilidad\": \"cada replica bit a bit con el mismo binario y maquina; entre maquinas, "
         "estadistica\", \"segundos\": "
      << num(r.seconds) << "},\n";
    j << "  \"replicas_por_nivel\": " << r.spec.replicas << ",\n  \"seed\": " << r.spec.seed << ",\n";
    j << "  \"horas\": " << num(r.spec.hours) << ",\n  \"muestreo_h\": " << num(r.spec.sampleHours) << ",\n";
    j << "  \"definicion\": " << quoted(serializeExperiment(r.spec)) << ",\n";
    j << "  \"niveles\": [";
    for (std::size_t l = 0; l < r.levels.size(); ++l) {
        j << (l ? ", " : "") << "{\"nombre\": " << quoted(r.levels[l].label) << ", \"seeds\": [";
        bool first = true;
        for (const ReplicaResult& rep : r.replicas)
            if (rep.level == static_cast<int>(l)) {
                j << (first ? "" : ", ") << rep.seed;
                first = false;
            }
        j << "]}";
    }
    j << "],\n  \"tiempos_h\": [";
    for (std::size_t s = 0; s < r.times.size(); ++s)
        j << (s ? ", " : "") << num(r.times[s]);
    j << "],\n  \"resumen\": {";
    bool firstCol = true;
    for (std::size_t c = 0; c < r.columns.size(); ++c) {
        if (!columnHasData(r, c))
            continue;
        j << (firstCol ? "" : ",") << "\n    " << quoted(r.columns[c]) << ": {\"unidad\": " << quoted(r.units[c])
          << ", \"niveles\": [";
        firstCol = false;
        for (std::size_t l = 0; l < r.levels.size(); ++l) {
            j << (l ? ", " : "") << "{\"media\": [";
            std::ostringstream sd, ci;
            for (std::size_t s = 0; s < r.times.size(); ++s) {
                const SummaryPoint p = summarize(r, static_cast<int>(l), static_cast<int>(c), s);
                j << (s ? ", " : "") << num(p.mean);
                sd << (s ? ", " : "") << num(p.sd);
                ci << (s ? ", " : "") << num(p.ci95);
            }
            j << "], \"de\": [" << sd.str() << "], \"ic95\": [" << ci.str() << "]}";
        }
        j << "]}";
    }
    j << "\n  }";
    // Dosis-respuesta: con un solo factor que es un reactivo, la CI50 (Hill) y la CIM de las bacterias
    // al final, como en una dilución en caldo (crecimiento visible: ~10⁷ por mL, ESTIMACIÓN).
    const int bact = r.column("bacterias");
    if (r.spec.factors.size() == 1 && r.spec.factors[0].key.rfind("reactivo:", 0) == 0 && bact >= 0 && !r.times.empty()) {
        std::vector<double> doses, finals;
        for (std::size_t l = 0; l < r.levels.size(); ++l) {
            doses.push_back(r.levels[l].settings.empty() ? 0.0 : r.levels[l].settings[0].second);
            finals.push_back(summarize(r, static_cast<int>(l), bact, r.times.size() - 1).mean);
        }
        const DoseFit f = fitDoseResponse(doses, finals);
        const bool culture = r.spec.kind == ExperimentSpec::Kind::Culture;
        j << ",\n  \"dosis_respuesta\": {\"reactivo\": " << quoted(r.spec.factors[0].key.substr(9))
          << ", \"unidad\": \"ug/mL\", \"ci50\": " << num(f.valid ? f.ic50 : 0.0) << ", \"hill\": " << num(f.hill)
          << ", \"r2\": " << num(f.r2);
        if (culture)
            j << ", \"cim\": " << num(minimumInhibitory(doses, finals, 1e7));
        j << "}";
    }
    j << "\n}\n";
    return j.str();
}

bool parseExperiment(const std::string& text, ExperimentSpec& spec, std::string* error) {
    toml::table doc;
    try {
        doc = toml::parse(text);
    } catch (const toml::parse_error& e) {
        if (error)
            *error = std::string(e.description());
        return false;
    }
    const toml::table* ex = doc["experimento"].as_table();
    if (!ex) {
        if (error)
            *error = "falta [experimento]";
        return false;
    }
    spec = ExperimentSpec{};
    spec.name = (*ex)["nombre"].value_or(spec.name);
    const std::string kind = (*ex)["tipo"].value_or(std::string("cultivo"));
    spec.kind = kind == "placa" ? ExperimentSpec::Kind::Plate : ExperimentSpec::Kind::Culture;
    spec.replicas = static_cast<int>((*ex)["replicas"].value_or(static_cast<std::int64_t>(spec.replicas)));
    spec.seed = static_cast<std::uint64_t>((*ex)["seed"].value_or(static_cast<std::int64_t>(spec.seed)));
    spec.hours = (*ex)["horas"].value_or(spec.hours);
    spec.sampleHours = (*ex)["muestreo_h"].value_or(spec.sampleHours);
    spec.threads = static_cast<int>((*ex)["hilos"].value_or(static_cast<std::int64_t>(0)));
    if (const toml::table* c = doc["cultivo"].as_table()) {
        CultureSpec& cs = spec.culture;
        cs.name = spec.name;
        cs.vessel = (*c)["recipiente"].value_or(cs.vessel);
        cs.medium = (*c)["medio"].value_or(cs.medium);
        cs.temperature = static_cast<float>((*c)["temperatura"].value_or(static_cast<double>(cs.temperature)));
        cs.light = static_cast<float>((*c)["luz"].value_or(static_cast<double>(cs.light)));
        std::string scene = "cultivo_version=1\n";
        if (const toml::array* p = (*c)["programa"].as_array())
            for (const toml::node& n : *p)
                if (auto s = n.value<std::string>())
                    scene += "programa=" + *s + '\n';
        if (const toml::array* e = (*c)["eventos"].as_array())
            for (const toml::node& n : *e)
                if (auto s = n.value<std::string>())
                    scene += "evento=" + *s + '\n';
        CultureSpec parsed;
        parseCultureScene(scene, parsed, spec.cultureEvents);
        cs.program = parsed.program;
    }
    if (const toml::table* p = doc["placa"].as_table())
        spec.plateScene = (*p)["escena"].value_or(std::string());
    if (const toml::array* fs = doc["factor"].as_array())
        for (const toml::node& n : *fs) {
            const toml::table* t = n.as_table();
            if (!t)
                continue;
            ExperimentFactor f;
            f.key = (*t)["clave"].value_or(std::string());
            if (const toml::array* v = (*t)["valores"].as_array())
                for (const toml::node& x : *v)
                    if (auto d = x.value<double>())
                        f.values.push_back(*d);
            if (!f.key.empty() && !f.values.empty())
                spec.factors.push_back(f);
        }
    if (spec.kind == ExperimentSpec::Kind::Plate && spec.plateScene.empty()) {
        if (error)
            *error = "un experimento de placa necesita [placa] escena";
        return false;
    }
    return true;
}

std::string serializeExperiment(const ExperimentSpec& s) {
    std::ostringstream out;
    out << "[experimento]\nnombre = " << quoted(s.name) << "\ntipo = \""
        << (s.kind == ExperimentSpec::Kind::Culture ? "cultivo" : "placa") << "\"\nreplicas = " << s.replicas
        << "\nseed = " << s.seed << "\nhoras = " << num(s.hours) << "\nmuestreo_h = " << num(s.sampleHours) << "\n";
    if (s.kind == ExperimentSpec::Kind::Culture) {
        const CultureSpec& c = s.culture;
        out << "\n[cultivo]\nrecipiente = " << quoted(c.vessel) << "\nmedio = " << quoted(c.medium)
            << "\ntemperatura = " << num(c.temperature) << "\nluz = " << num(c.light) << "\nprograma = [";
        const std::string program = serializeProgram(c.program);
        std::istringstream lines(program);
        std::string line;
        bool first = true;
        while (std::getline(lines, line)) {
            out << (first ? "" : ", ") << quoted(line.substr(9));
            first = false;
        }
        out << "]\neventos = [";
        first = true;
        const std::string scene = serializeCultureScene(c, s.cultureEvents);
        std::istringstream sl(scene);
        while (std::getline(sl, line))
            if (line.rfind("evento=", 0) == 0) {
                out << (first ? "" : ", ") << quoted(line.substr(7));
                first = false;
            }
        out << "]\n";
    } else {
        out << "\n[placa]\nescena = " << quoted(s.plateScene) << "\n";
    }
    for (const ExperimentFactor& f : s.factors) {
        out << "\n[[factor]]\nclave = " << quoted(f.key) << "\nvalores = [";
        for (std::size_t k = 0; k < f.values.size(); ++k)
            out << (k ? ", " : "") << num(f.values[k]);
        out << "]\n";
    }
    return out.str();
}
