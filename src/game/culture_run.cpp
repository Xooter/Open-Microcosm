#include "game/culture_run.hpp"

#include "core/profiling.hpp"
#include "core/rng.hpp"
#include "bio/cell_physiology.hpp"
#include "core/units.hpp"
#include "game/prepared.hpp"
#include "game/reagent.hpp"
#include "game/progress.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <limits>
#include <optional>
#include <sstream>

namespace {
constexpr const char* kOpKeys[] = {"inocular", "bacterias", "medio",  "dilucion",           "concentracion",
                                   "fagos",    "muestra",   "salida", "entrada", "inocular_densidad",
                                   "quimiostato", "medio_que_entra", "reactivo"};

std::string fnum(double v) {
    char buf[40];
    const auto r = std::to_chars(buf, buf + sizeof buf, v);
    return std::string(buf, r.ptr);
}

// Lo que pasa de un cultivo a otro, sin espacios (va como una sola palabra del evento):
// "b=<bacterias>;s=<c0>/<c1>/...;c=<célula>|<célula>", célula = id,generación,biomasa,nacer,reservas,
// P,N,Si,Fe,B12,edad,fisiones pendientes,genes...
std::string encodeTransfer(const std::vector<CultureCell>& cells, std::uint64_t bacteria, const double* uM) {
    std::string out = "b=" + std::to_string(bacteria) + ";s=";
    for (int k = 0; k < kSoluteCount; ++k)
        out += (k ? "/" : "") + fnum(uM[k]);
    out += ";c=";
    for (std::size_t i = 0; i < cells.size(); ++i) {
        const CultureCell& c = cells[i];
        const Metabolism& m = c.meta;
        out += (i ? "|" : "") + std::string(speciesInfo(c.species).key) + ',' + std::to_string(c.lineage.generation);
        for (const double v : {m.biomass, m.birthBiomass, m.reserves, m.phosphorus, m.nitrogen, m.silicon, m.iron,
                               m.vitamin, m.ageHours})
            out += ',' + fnum(v);
        out += ',' + std::to_string(m.pendingFissions);
        for (const float g : c.heredity.genome.v)
            out += ',' + fnum(g);
    }
    return out;
}

bool decodeTransfer(const std::string& text, std::vector<CultureCell>& cells, std::uint64_t& bacteria, double* uM) {
    cells.clear();
    bacteria = 0;
    std::fill(uM, uM + kSoluteCount, 0.0);
    std::stringstream parts(text);
    std::string part;
    while (std::getline(parts, part, ';')) {
        if (part.rfind("b=", 0) == 0)
            bacteria = std::stoull(part.substr(2));
        else if (part.rfind("s=", 0) == 0) {
            std::stringstream s(part.substr(2));
            std::string tok;
            for (int k = 0; k < kSoluteCount && std::getline(s, tok, '/'); ++k)
                uM[k] = std::stod(tok);
        } else if (part.rfind("c=", 0) == 0 && part.size() > 2) {
            std::stringstream cs(part.substr(2));
            std::string cell;
            while (std::getline(cs, cell, '|')) {
                std::stringstream f(cell);
                std::vector<std::string> t;
                std::string tok;
                while (std::getline(f, tok, ','))
                    t.push_back(tok);
                if (t.size() < 12 + kGeneCount)
                    return false;
                const std::optional<SpeciesId> sp = findSpecies(t[0]);
                if (!sp)
                    continue;
                CultureCell c;
                c.species = *sp;
                c.lineage.generation = static_cast<std::uint32_t>(std::stoul(t[1]));
                Metabolism& m = c.meta;
                float* fields[] = {&m.biomass, &m.birthBiomass, &m.reserves, &m.phosphorus, &m.nitrogen,
                                   &m.silicon, &m.iron,         &m.vitamin,  &m.ageHours};
                for (int k = 0; k < 9; ++k)
                    *fields[k] = std::stof(t[2 + k]);
                m.pendingFissions = std::stoi(t[11]);
                for (std::size_t g = 0; g < kGeneCount; ++g)
                    c.heredity.genome.v[g] = std::stof(t[12 + g]);
                const SpeciesInfo& info = speciesInfo(c.species);
                c.heredity.traits = traitsOf(c.heredity.genome, info.locomotion);
                c.meta.photosynthetic = c.heredity.traits.photosynthesis > 0.0f;
                c.meta.targetBirthBiomass = targetBirthBiomass(c.species, c.heredity.traits.sizeScale);
                c.meta.fissions = info.fissions;
                c.meta.needsSilicon = info.frustule;
                c.meta.needsVitamin = info.vitaminB12;
                c.digestion.rateScale = c.heredity.traits.digestion;
                cells.push_back(std::move(c));
            }
        }
    }
    return true;
}
static_assert(std::size(kOpKeys) == static_cast<std::size_t>(CultureOp::Count));
} // namespace

const char* cultureOpKey(CultureOp op) { return kOpKeys[static_cast<int>(op)]; }

CultureConfig CultureSpec::config() const {
    CultureConfig c;
    if (const CultureMedium* m = findCultureMedium(medium))
        c.medium = m->composition;
    if (const Vessel* v = findVessel(vessel)) {
        c.volumeMl = v->volumeMl;
        c.depthUm = v->depthUm();
    }
    c.medium.temperature = temperature;
    c.medium.surfaceLight = light;
    c.seed = seed;
    return c;
}

void CultureRun::start(const CultureSpec& spec) {
    const float interval = sampleHours_;
    *this = CultureRun{};
    sampleHours_ = interval;
    spec_ = spec;
    culture_ = std::make_unique<Culture>(spec.config());
    culture_->program() = spec.program;
    buildColumns();
    sample();
}

void CultureRun::buildColumns() {
    std::vector<SeriesColumn> c;
    c.push_back({"t", "h", "Tiempo"});
    for (int s = 0; s < speciesCount(); ++s)
        c.push_back({speciesInfo(static_cast<SpeciesId>(s)).key, "individuos/mL", "Poblacion"});
    c.push_back({"bacterias", "bacterias/mL", "Poblacion"});
    c.push_back({"fagos", "viriones/mL", "Poblacion"});
    for (int k = 0; k < kSoluteCount; ++k)
        c.push_back({soluteKey(static_cast<Solute>(k)), "µM", "Quimica"});
    c.push_back({"pH", "", "Quimica"});
    c.push_back({"luz", "", "Quimica"});
    c.push_back({"volumen", "mL", "Quimica"});
    row_.assign(c.size(), 0.0f);
    series_.setColumns(std::move(c));
}

void CultureRun::sample() {
    if (!culture_ || !series_.offer())
        return;
    const Culture& c = *culture_;
    const CultureCensus census = c.census();
    const double ml = std::max(1e-9, c.volumeMl());
    std::size_t k = 0;
    auto put = [&](double v) { row_[k++] = static_cast<float>(v); };
    put(hours());
    for (int s = 0; s < speciesCount(); ++s)
        put(census.species[s] / ml);
    put(static_cast<double>(census.bacteria) / ml);
    put(c.phages() / ml);
    for (int s = 0; s < kSoluteCount; ++s)
        put(c.concentration(static_cast<Solute>(s)));
    put(c.ph());
    put(c.light());
    put(c.volumeMl());
    series_.push(row_.data());
}

void CultureRun::advance(double seconds) {
    PROFILE_SCOPE("CultureRun::advance");
    if (!culture_)
        return;
    carry_ += seconds;
    while (carry_ >= kCultureStep) {
        carry_ -= kCultureStep;
        // Lo programado para este paso (escena de cultivo).
        while (scriptNext_ < script_.size() && script_[scriptNext_].step <= culture_->steps())
            apply(script_[scriptNext_++]);
        culture_->step(kCultureStep);
        if (hours() + 1e-9 >= nextSample_) {
            nextSample_ = hours() + sampleHours_;
            sample();
        }
    }
}

bool CultureRun::apply(CultureEvent ev) {
    if (!culture_)
        return false;
    Culture& c = *culture_;
    ev.step = c.steps();
    bool ok = true;
    switch (ev.op) {
    case CultureOp::Inoculate:
        if (const std::optional<SpeciesId> id = findSpecies(ev.key))
            ok = c.inoculate(*id, static_cast<int>(ev.value)) > 0;
        else
            ok = false;
        break;
    case CultureOp::Bacteria:
        c.inoculateBacteria(static_cast<std::uint64_t>(std::max(0.0, ev.value) * c.volumeMl()));
        break;
    case CultureOp::Renew: c.renewMedium(ev.value); break;
    case CultureOp::Dilute: c.dilute(ev.value); break;
    case CultureOp::Concentration:
        ok = false;
        for (int k = 0; k < kSoluteCount; ++k)
            if (ev.key == soluteKey(static_cast<Solute>(k))) {
                c.setConcentration(static_cast<Solute>(k), ev.value);
                ok = true;
            }
        break;
    case CultureOp::Phages: c.setPhages(c.phages() + std::max(0.0, ev.value) * c.volumeMl()); break;
    case CultureOp::Sample: {
        // La gota se rearma igual al repetir la escena (mismo estado, mismo azar); acá solo se saca.
        std::vector<CultureCell> cells;
        std::uint64_t bacteria = 0;
        double concentrate = 1.0;
        std::from_chars(ev.key.data(), ev.key.data() + ev.key.size(), concentrate);
        c.withdraw(ev.value * 1e-3 / c.volumeMl(), concentrate, cells, bacteria);
        break;
    }
    case CultureOp::Withdraw: {
        std::vector<CultureCell> cells;
        std::uint64_t bacteria = 0;
        c.withdraw(ev.value * 1e-3 / c.volumeMl(), 1.0, cells, bacteria);
        break;
    }
    case CultureOp::Receive: {
        std::vector<CultureCell> cells;
        std::uint64_t bacteria = 0;
        double uM[kSoluteCount];
        ok = decodeTransfer(ev.key, cells, bacteria, uM);
        if (ok)
            c.receive(std::move(cells), bacteria, ev.value * 1e-3, uM);
        break;
    }
    case CultureOp::InoculateDensity: {
        const std::optional<SpeciesId> id = findSpecies(ev.key);
        if (!id) {
            ok = false;
            break;
        }
        Rng rng(spec_.seed, rng_stream::kCulture + 91, events_.size(), c.steps());
        const auto n = static_cast<int>(rng.poisson(std::max(0.0, ev.value) * c.volumeMl()));
        ok = n == 0 || c.inoculate(*id, n) > 0;
        break;
    }
    case CultureOp::Chemostat: c.setDilutionRate(ev.value); break;
    case CultureOp::Inflow:
        ok = false;
        for (int k = 0; k < kSoluteCount; ++k)
            if (ev.key == soluteKey(static_cast<Solute>(k))) {
                c.setFreshConcentration(static_cast<Solute>(k), ev.value);
                ok = true;
            }
        break;
    case CultureOp::Reagent:
        if (const int r = findReagent(ev.key); r >= 0)
            c.setReagent(r, ev.value);
        else
            ok = false;
        break;
    case CultureOp::Count: ok = false; break;
    }
    if (ok)
        events_.push_back(std::move(ev));
    return ok;
}

void CultureRun::schedule(std::vector<CultureEvent> script) {
    script_ = std::move(script);
    std::stable_sort(script_.begin(), script_.end(), [](const auto& a, const auto& b) { return a.step < b.step; });
    scriptNext_ = 0;
}

double CultureRun::dropVolumeUl(const WorldConfig& plate) {
    const float radius = 0.5f * static_cast<float>(std::min(plate.gridX, plate.gridY)) * plate.cellSize - plate.dishMargin;
    return units::kPi * radius * radius * plate.depth * 1e-9; // 1 µL = 10⁹ µm³
}

PlateSpec CultureRun::sampleDrop(float concentrate) {
    PlateSpec drop;
    drop.content = DropContent::EmptyPetri;
    drop.seed = spec_.seed * 1000003ull + samples_ + 1;
    drop.rebuild();
    if (!culture_)
        return drop;
    Culture& c = *culture_;
    WorldConfig& w = drop.config;
    // El agua de la gota es la del cultivo.
    MediumParams& m = w.medium;
    const MediumParams& cm = c.medium();
    m.temperature = cm.temperature;
    m.salinity = cm.salinity;
    m.alkalinity = cm.alkalinity;
    m.carbonatePK = cm.carbonatePK;
    m.surfaceLight = std::max(0.3f, cm.surfaceLight); // bajo el microscopio, la lámpara prendida
    m.phosphate = static_cast<float>(c.concentration(Solute::Phosphate));
    m.nitrogen = static_cast<float>(c.concentration(Solute::Nitrogen));
    m.silicate = static_cast<float>(c.concentration(Solute::Silicate));
    m.iron = static_cast<float>(c.concentration(Solute::Iron));
    m.vitamin = static_cast<float>(c.concentration(Solute::Vitamin));
    m.organic = static_cast<float>(c.concentration(Solute::Organic));
    m.phage = static_cast<float>(c.phages() / c.volumeUm3());
    m.organicInflow = 0.0f;
    // La gota se lleva su volumen del cultivo (y, concentrada, más células por volumen).
    const double ul = dropVolumeUl(w);
    const double fraction = ul * 1e-3 / c.volumeMl();
    std::vector<CultureCell> cells;
    std::uint64_t bacteria = 0;
    c.withdraw(fraction, concentrate, cells, bacteria);
    w.bacteria = static_cast<std::size_t>(std::min<std::uint64_t>(bacteria, 150'000)); // tope de la placa
    w.bdellovibrioFraction = 0.0f;
    w.sampled.clear();
    for (const CultureCell& cell : cells)
        w.sampled.push_back({cell.species, cell.meta, cell.heredity.genome, cell.lineage.generation});
    char buf[32];
    const auto r = std::to_chars(buf, buf + sizeof buf, static_cast<double>(concentrate));
    events_.push_back({c.steps(), CultureOp::Sample, std::string(buf, r.ptr), ul});
    ++samples_;
    return drop;
}

int CultureRun::countAliquot(SpeciesId id, float volumeMl, std::uint64_t draw) const {
    if (!culture_)
        return 0;
    const double density = culture_->count(id) / std::max(1e-9, culture_->volumeMl());
    Rng rng(spec_.seed, rng_stream::kCulture + 90, draw, culture_->steps());
    return static_cast<int>(rng.poisson(density * volumeMl));
}

void CultureRun::transferTo(CultureRun& dest, double volumeMl) {
    if (!culture_ || !dest.culture_ || volumeMl <= 0.0)
        return;
    Culture& c = *culture_;
    const double v = std::min(volumeMl, 0.999 * c.volumeMl());
    double uM[kSoluteCount];
    for (int k = 0; k < kSoluteCount; ++k)
        uM[k] = c.concentration(static_cast<Solute>(k));
    std::vector<CultureCell> cells;
    std::uint64_t bacteria = 0;
    c.withdraw(v / c.volumeMl(), 1.0, cells, bacteria);
    events_.push_back({c.steps(), CultureOp::Withdraw, "", v * 1e3});
    dest.apply({0, CultureOp::Receive, encodeTransfer(cells, bacteria, uM), v * 1e3});
}

std::string serializeCultureScene(const CultureSpec& s, const std::vector<CultureEvent>& events) {
    std::ostringstream out;
    out.precision(std::numeric_limits<double>::max_digits10);
    out << "cultivo_version=1\n"
        << "nombre=" << s.name << '\n'
        << "recipiente=" << s.vessel << '\n'
        << "medio=" << s.medium << '\n'
        << "temperatura=" << s.temperature << '\n'
        << "luz=" << s.light << '\n'
        << "seed=" << s.seed << '\n';
    out << serializeProgram(s.program);
    for (const CultureEvent& e : events)
        out << "evento=" << e.step << ' ' << cultureOpKey(e.op) << ' ' << e.value << (e.key.empty() ? "" : " ") << e.key
            << '\n';
    return out.str();
}

bool parseCultureScene(const std::string& text, CultureSpec& s, std::vector<CultureEvent>& events) {
    std::istringstream in(text);
    std::string line;
    bool versioned = false;
    s.program.entries.clear();
    events.clear();
    while (std::getline(in, line)) {
        const std::size_t eq = line.find('=');
        if (line.empty() || line[0] == '#' || eq == std::string::npos)
            continue;
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "cultivo_version")
            versioned = true;
        else if (k == "nombre")
            s.name = v;
        else if (k == "recipiente")
            s.vessel = v;
        else if (k == "medio")
            s.medium = v;
        else if (k == "temperatura")
            s.temperature = std::stof(v);
        else if (k == "luz")
            s.light = std::stof(v);
        else if (k == "seed")
            std::from_chars(v.data(), v.data() + v.size(), s.seed);
        else if (k == "programa") {
            ProgramEntry e;
            if (parseProgramEntry(v, e))
                s.program.entries.push_back(e);
        } else if (k == "evento") {
            std::istringstream f(v);
            CultureEvent e;
            std::string op;
            if (!(f >> e.step >> op >> e.value))
                continue;
            f >> e.key;
            for (int i = 0; i < static_cast<int>(CultureOp::Count); ++i)
                if (op == kOpKeys[i]) {
                    e.op = static_cast<CultureOp>(i);
                    events.push_back(e);
                }
        }
    }
    return versioned;
}

std::string defaultCulturesDir() {
    return (std::filesystem::path(defaultProgressPath()).parent_path() / "cultivos").string();
}
