#include "game/culture.hpp"

#include "bio/cell_physiology.hpp"
#include "bio/physiology.hpp"
#include "core/profiling.hpp"
#include "core/rng.hpp"
#include "core/units.hpp"
#include "game/reagent.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace {

constexpr double kCarbonPerPmol = 6.0; // la masa es glucosa equivalente: 6 C por pmol

// El piloto automático de la placa navega a ~0,6 de su rapidez máxima (ai/autopilot): la misma
// actividad (gasto de nado) y la misma rapidez de crucero en los encuentros.
constexpr float kCruise = 0.6f;

// Sub-streams del azar del cultivo.
constexpr std::uint64_t kStreamInoculum = rng_stream::kCulture + 0;
constexpr std::uint64_t kStreamMutation = rng_stream::kCulture + 1;
constexpr std::uint64_t kStreamGrazing = rng_stream::kCulture + 2;
constexpr std::uint64_t kStreamHunting = rng_stream::kCulture + 3;
constexpr std::uint64_t kStreamBacteria = rng_stream::kCulture + 4;
constexpr std::uint64_t kStreamPhage = rng_stream::kCulture + 5;
constexpr std::uint64_t kStreamWashout = rng_stream::kCulture + 6;

// Atenuación de la luz por la biomasa fotótrofa [µm² por pmol de materia seca]: absorción específica
// de la clorofila a ~0,02 m²/mg (Kirk 2011, Light and Photosynthesis in Aquatic Ecosystems, 3.ª ed.,
// cap. 9) con ~1,5 % de clorofila en la materia seca → ~0,3 µm²/pg, ×180 pg por pmol de glucosa
// equivalente. ESTIMACIÓN: la clorofila por célula cambia con la luz (fotoaclimatación).
constexpr double kChlorophyllShading = 54.0;

bool elementOf(Solute s, Element& e, double& perPmol) {
    switch (s) {
    case Solute::Phosphate: e = Element::P; perPmol = 1.0; return true;
    case Solute::Nitrogen: e = Element::N; perPmol = 1.0; return true;
    case Solute::Silicate: e = Element::Si; perPmol = 1.0; return true;
    case Solute::Iron: e = Element::Fe; perPmol = 1.0; return true;
    case Solute::Organic: e = Element::C; perPmol = kCarbonPerPmol; return true;
    case Solute::CO2: e = Element::C; perPmol = 1.0; return true;
    default: return false;
    }
}

bool eatsBacteria(FeedingMode m) {
    return m == FeedingMode::OralGroove || m == FeedingMode::Anterior || m == FeedingMode::Phagocytosis;
}

double cellContent(const CultureCell& c) {
    double pmol = c.meta.biomass + c.meta.reserves;
    for (const FoodVacuole& v : c.digestion.vacuoles)
        pmol += v.contentPmol;
    return pmol;
}

} // namespace

const char* soluteKey(Solute s) {
    static const char* const names[] = {"o2", "co2", "phosphate", "nitrogen", "silicate", "iron", "b12", "organic"};
    return names[static_cast<int>(s)];
}

const char* soluteLabel(Solute s) {
    static const char* const names[] = {"o2", "co2", "fosfato", "nitrogeno", "silice", "hierro", "b12", "organica"};
    return names[static_cast<int>(s)];
}

Culture::Culture(const CultureConfig& config) : config_(config), medium_(config.medium) {
    volumeUm3_ = static_cast<double>(config.volumeMl) * 1e12; // 1 mL = 10¹² µm³
    pmolPerMicroMolar_ = volumeUm3_ * 1e-9;                   // 1 µM = 10⁻⁹ pmol/µm³
    refreshMedium();
    auto set = [&](Solute s, double uM) { amount_[static_cast<int>(s)] = uM * pmolPerMicroMolar_; };
    set(Solute::Oxygen, o2Saturation_);
    set(Solute::CO2, co2Saturation_);
    set(Solute::Phosphate, medium_.phosphate);
    set(Solute::Nitrogen, medium_.nitrogen);
    set(Solute::Silicate, medium_.silicate);
    set(Solute::Iron, medium_.iron);
    set(Solute::Vitamin, medium_.vitamin);
    set(Solute::Organic, medium_.organic);
    phages_ = static_cast<double>(medium_.phage) * volumeUm3_;
    ph_ = std::clamp(medium_.carbonatePK + std::log10(medium_.alkalinity / std::max(0.05, static_cast<double>(co2Saturation_))), 4.0, 11.0);
}

double gasExchangeRate(float depthUm, float gasTransferCmH) {
    const double depth = std::max(1.0f, depthUm);
    if (gasTransferCmH > 0.0f)
        return static_cast<double>(gasTransferCmH) * 1e4 / 3600.0 / depth; // [µm/s] / [µm]
    return units::kDiffusionO2 / (depth * depth);
}

void Culture::setGasTransfer(float cmPerHour) {
    config_.gasTransferCmH = std::max(0.0f, cmPerHour);
    airExchange_ = gasExchangeRate(config_.depthUm, config_.gasTransferCmH);
}

void Culture::refreshMedium() {
    q10_ = std::pow(2.0f, (medium_.temperature - 20.0f) / 10.0f);
    viscositySpeed_ = waterViscosity(20.0f) / waterViscosity(medium_.temperature);
    o2Saturation_ = medium_.anoxic ? 0.0f : o2SaturationAt(medium_.temperature) * o2AirFraction(medium_);
    co2Saturation_ = co2SaturationAt(medium_);
    airExchange_ = gasExchangeRate(config_.depthUm, config_.gasTransferCmH);
}

double Culture::exchange(Solute s, double pmol, Flow flow) {
    double& a = amount_[static_cast<int>(s)];
    const double actual = pmol >= 0.0 ? pmol : -std::min(-pmol, std::max(0.0, a));
    a += actual;
    Element e;
    double perPmol;
    if (elementOf(s, e, perPmol))
        ledger_.add(e, flow, actual * perPmol);
    return actual;
}

void Culture::recordExternal(Solute s, double pmol, Flow flow) {
    Element e;
    double perPmol;
    if (elementOf(s, e, perPmol)) {
        ledger_.add(e, flow, pmol * perPmol);
        ledger_.addExternal(e, pmol * perPmol);
    }
}

void Culture::setConcentration(Solute s, double micromolar) {
    double& a = amount_[static_cast<int>(s)];
    const double target = std::max(0.0, micromolar) * pmolPerMicroMolar_;
    recordExternal(s, target - a, Flow::Dose);
    a = target;
}

double Culture::freshConcentration(Solute s) const {
    if (freshSet_[static_cast<int>(s)])
        return fresh_[static_cast<int>(s)];
    const MediumParams& m = config_.medium;
    switch (s) {
    case Solute::Oxygen: return o2Saturation_;
    case Solute::CO2: return co2Saturation_;
    case Solute::Phosphate: return m.phosphate;
    case Solute::Nitrogen: return m.nitrogen;
    case Solute::Silicate: return m.silicate;
    case Solute::Iron: return m.iron;
    case Solute::Vitamin: return m.vitamin;
    case Solute::Organic: return m.organic;
    case Solute::Count: break;
    }
    return 0.0;
}

void Culture::setReagent(int index, double ugPerMl) {
    if (index < 0)
        return;
    if (static_cast<int>(reagents_.size()) <= index)
        reagents_.resize(static_cast<std::size_t>(index) + 1, 0.0);
    reagents_[static_cast<std::size_t>(index)] = std::max(0.0, ugPerMl);
}

double Culture::reagentConcentration(int index) const {
    return index >= 0 && index < static_cast<int>(reagents_.size()) ? reagents_[static_cast<std::size_t>(index)] : 0.0;
}

void Culture::renewMedium(double fraction) {
    const double f = std::clamp(fraction, 0.0, 1.0);
    if (f <= 0.0)
        return;
    for (double& r : reagents_) // el medio nuevo no trae reactivo
        r *= 1.0 - f;
    for (int k = 0; k < kSoluteCount; ++k) {
        const auto s = static_cast<Solute>(k);
        double& a = amount_[k];
        const double out = f * std::max(0.0, a), in = f * freshConcentration(s) * pmolPerMicroMolar_;
        a += in - out;
        recordExternal(s, -out, Flow::Washout);
        recordExternal(s, in, Flow::Dose);
    }
    // Bacterias, detritos y fagos se van con el sobrenadante.
    for (std::size_t i = 0; i < bacteria_.size(); ++i) {
        BacterialCohort& b = bacteria_[i];
        Rng rng(config_.seed, kStreamWashout, washouts_++, step_);
        const double expected = f * static_cast<double>(b.count);
        auto gone = static_cast<std::uint64_t>(expected);
        if (rng.uniform() < expected - static_cast<double>(gone))
            ++gone;
        b.count -= std::min(gone, b.count);
        // El carbono de la biomasa bacteriana no se contabiliza como cerrado (frontera abierta).
    }
    bacteria_.erase(std::remove_if(bacteria_.begin(), bacteria_.end(), [](const BacterialCohort& b) { return b.count == 0; }),
                    bacteria_.end());
    const double detritusOut = f * detritus_;
    detritus_ -= detritusOut;
    ledger_.add(Element::C, Flow::Washout, -kCarbonPerPmol * detritusOut);
    ledger_.addExternal(Element::C, -kCarbonPerPmol * detritusOut);
    phages_ *= 1.0 - f;
}

void Culture::washOut(const CultureCell& c) {
    double p = c.meta.phosphorus, n = c.meta.nitrogen, carbon = c.meta.biomass + c.meta.reserves;
    for (const FoodVacuole& v : c.digestion.vacuoles) {
        p += v.phosphorus;
        n += v.nitrogen;
        carbon += v.contentPmol;
    }
    const std::pair<Element, double> out[] = {{Element::P, p}, {Element::N, n}, {Element::Si, c.meta.silicon},
                                              {Element::Fe, cellIron(c.meta)}, {Element::C, kCarbonPerPmol * carbon}};
    for (const auto& [e, pmol] : out) {
        ledger_.add(e, Flow::Washout, -pmol);
        ledger_.addExternal(e, -pmol);
    }
}

void Culture::dilute(double fraction) {
    const double f = std::clamp(fraction, 0.0, 1.0);
    if (f <= 0.0)
        return;
    // Cada protista se va con la probabilidad de la fracción (en el sobrenadante no hay centrifugado).
    std::size_t kept = 0;
    for (std::size_t i = 0; i < cells_.size(); ++i) {
        Rng rng(config_.seed, kStreamWashout + 50, washouts_++, step_);
        if (rng.uniform() < f) {
            washOut(cells_[i]);
            continue;
        }
        if (kept != i)
            cells_[kept] = std::move(cells_[i]);
        ++kept;
    }
    cells_.resize(kept);
    renewMedium(f);
}

void Culture::withdraw(double fraction, double concentrate, std::vector<CultureCell>& out, std::uint64_t& bacteria) {
    const double f = std::clamp(fraction, 0.0, 0.999);
    const double take = std::clamp(f * std::max(1.0, concentrate), 0.0, 1.0); // células y bacterias
    out.clear();
    bacteria = 0;
    std::size_t kept = 0;
    for (std::size_t i = 0; i < cells_.size(); ++i) {
        Rng rng(config_.seed, kStreamWashout + 60, washouts_++, step_);
        if (rng.uniform() < take) {
            washOut(cells_[i]);
            out.push_back(std::move(cells_[i]));
            continue;
        }
        if (kept != i)
            cells_[kept] = std::move(cells_[i]);
        ++kept;
    }
    cells_.resize(kept);
    for (BacterialCohort& b : bacteria_) {
        Rng rng(config_.seed, kStreamWashout + 61, washouts_++, step_);
        const double expected = take * static_cast<double>(b.count);
        auto gone = static_cast<std::uint64_t>(expected);
        if (rng.uniform() < expected - static_cast<double>(gone))
            ++gone;
        gone = std::min(gone, b.count);
        b.count -= gone;
        bacteria += gone;
    }
    bacteria_.erase(std::remove_if(bacteria_.begin(), bacteria_.end(), [](const BacterialCohort& b) { return b.count == 0; }),
                    bacteria_.end());
    // El medio de la gota sale con su concentración: el volumen baja y la concentración no cambia.
    for (int k = 0; k < kSoluteCount; ++k) {
        const double out = f * std::max(0.0, amount_[k]);
        amount_[k] -= out;
        recordExternal(static_cast<Solute>(k), -out, Flow::Washout);
    }
    const double detritusOut = f * detritus_;
    detritus_ -= detritusOut;
    ledger_.add(Element::C, Flow::Washout, -kCarbonPerPmol * detritusOut);
    ledger_.addExternal(Element::C, -kCarbonPerPmol * detritusOut);
    phages_ *= 1.0 - f;
    volumeUm3_ *= 1.0 - f;
    pmolPerMicroMolar_ = volumeUm3_ * 1e-9;
}

void Culture::applyProgram(double fromHours, double toHours) {
    for (const ProgramEntry& e : program_.entries) {
        if (e.isAction()) {
            const int times = programEvents(e, fromHours, toHours);
            for (int k = 0; k < times; ++k) {
                if (e.target == "bacteria")
                    inoculateBacteria(static_cast<std::uint64_t>(std::max(0.0, static_cast<double>(e.amount) * volumeMl())));
                else if (e.target == "medium")
                    renewMedium(e.amount);
                else if (e.target == "dilution")
                    dilute(e.amount);
                else if (e.target.rfind("add:", 0) == 0) {
                    for (int s = 0; s < kSoluteCount; ++s)
                        if (e.target.substr(4) == soluteKey(static_cast<Solute>(s)))
                            setConcentration(static_cast<Solute>(s), concentration(static_cast<Solute>(s)) + e.amount);
                } else if (e.target.rfind("inoculate:", 0) == 0) {
                    if (const std::optional<SpeciesId> id = findSpecies(e.target.substr(10)))
                        inoculate(*id, static_cast<int>(e.amount));
                }
            }
            continue;
        }
        float v;
        if (!programValue(e, toHours, v))
            continue;
        if (e.target == "temperature")
            medium_.temperature = v;
        else if (e.target == "light")
            medium_.surfaceLight = std::max(0.0f, v);
        else if (e.target.rfind("solute:", 0) == 0) {
            const std::string key = e.target.substr(7);
            for (int k = 0; k < kSoluteCount; ++k)
                if (key == soluteKey(static_cast<Solute>(k)))
                    setConcentration(static_cast<Solute>(k), v);
        }
    }
}

bool Culture::supports(SpeciesId id, std::string* why) {
    const SpeciesInfo& info = speciesInfo(id);
    const char* reason = nullptr;
    if (info.animalCell.present)
        reason = "las células animales en cultivo viven adheridas al fondo: se miran en su placa";
    else if (info.filament)
        reason = "los filamentos todavía no se simulan bien mezclados";
    else if (info.colonyDaughters > 0)
        reason = "las colonias todavía no se simulan bien mezcladas";
    else if (info.zoosporeChance > 0.0f)
        reason = "la reproducción por zoósporas todavía no se simula bien mezclada";
    if (reason && why)
        *why = reason;
    return reason == nullptr;
}

void Culture::applyTraits(CultureCell& c) {
    const SpeciesInfo& info = speciesInfo(c.species);
    c.digestion.rateScale = c.heredity.traits.digestion;
    c.meta.photosynthetic = c.heredity.traits.photosynthesis > 0.0f;
    c.meta.targetBirthBiomass = targetBirthBiomass(c.species, c.heredity.traits.sizeScale);
    c.meta.fissions = info.fissions;
}

int Culture::inoculate(SpeciesId id, int count) {
    PROFILE_SCOPE("Culture::inoculate");
    if (!supports(id) || count <= 0)
        return 0;
    const SpeciesInfo& info = speciesInfo(id);
    const MetabolismParams& p = metabolismParams_;
    for (int k = 0; k < count; ++k) {
        Rng rng(config_.seed, kStreamInoculum, inoculations_++, step_);
        CultureCell c;
        c.id = nextId_++;
        c.species = id;
        // Como en la placa (World::spawnWild): variación genética y tipos de apareamiento mezclados.
        Genome g = speciesGenome(id);
        mutate(g, info.locomotion, rng, 0.5f);
        g[Gene::MatingType] = rng.uniform() < 0.5f ? 0.0f : 1.0f;
        c.heredity = {g, traitsOf(g, info.locomotion)};
        applyTraits(c);
        c.meta.biomass = c.meta.birthBiomass = c.meta.targetBirthBiomass;
        c.meta.reserves = 0.3f * c.meta.biomass;
        // Población en distintas etapas del ciclo celular (si no, todas se dividirían juntas).
        const float stage = rng.uniform(1.0f, info.fissions > 1 ? 3.5f : 1.8f);
        c.meta.biomass *= stage;
        // El inóculo trae su cuota de nutrientes de su cultivo de origen: entrada externa.
        c.meta.needsSilicon = info.frustule;
        c.meta.needsVitamin = info.vitaminB12;
        c.meta.symbionts = info.photosymbiont.initial;
        if (c.meta.photosynthetic) {
            c.meta.phosphorus = p.phosphorusRatio * c.meta.biomass;
            c.meta.nitrogen = p.nitrogenRatio * c.meta.biomass;
            c.meta.iron = p.ironRatio * c.meta.biomass;
            recordExternal(Solute::Phosphate, c.meta.phosphorus, Flow::Spawn);
            recordExternal(Solute::Nitrogen, c.meta.nitrogen, Flow::Spawn);
            recordExternal(Solute::Iron, cellIron(c.meta), Flow::Spawn);
        }
        if (c.meta.needsSilicon) {
            c.meta.silicon = p.siliconRatio * c.meta.biomass;
            recordExternal(Solute::Silicate, c.meta.silicon, Flow::Spawn);
        }
        if (c.meta.needsVitamin)
            c.meta.vitamin = p.vitaminRatio * c.meta.biomass;
        cells_.push_back(std::move(c));
    }
    return count;
}

void Culture::inoculateBacteria(std::uint64_t count, std::uint8_t strain) {
    if (count == 0)
        return;
    const std::uint64_t room = static_cast<std::uint64_t>(
        std::max<std::ptrdiff_t>(1, config_.maxBacterialCohorts - static_cast<std::ptrdiff_t>(bacteria_.size())));
    const std::uint64_t cohorts = std::min(count, room);
    for (std::uint64_t k = 0; k < cohorts; ++k) {
        Rng rng(config_.seed, kStreamBacteria, inoculations_++, step_);
        const std::uint64_t n = count / cohorts + (k < count % cohorts ? 1 : 0);
        bacteria_.push_back({n, rng.uniform(1.0f, 2.0f), strain}); // como World::inoculate
    }
}

void Culture::receive(std::vector<CultureCell> cells, std::uint64_t bacteria, double volumeMl, const double* micromolar) {
    const double added = std::max(0.0, volumeMl) * 1e12; // [µm³]
    for (int k = 0; k < kSoluteCount; ++k) {
        const double pmol = std::max(0.0, micromolar[k]) * added * 1e-9;
        amount_[k] += pmol;
        recordExternal(static_cast<Solute>(k), pmol, Flow::Spawn);
    }
    volumeUm3_ += added;
    pmolPerMicroMolar_ = volumeUm3_ * 1e-9;
    for (CultureCell& c : cells) {
        c.id = nextId_++;
        // Lo que trae entra al balance de este cultivo.
        double p = c.meta.phosphorus, n = c.meta.nitrogen;
        for (const FoodVacuole& v : c.digestion.vacuoles) {
            p += v.phosphorus;
            n += v.nitrogen;
        }
        recordExternal(Solute::Phosphate, p, Flow::Spawn);
        recordExternal(Solute::Nitrogen, n, Flow::Spawn);
        recordExternal(Solute::Silicate, c.meta.silicon, Flow::Spawn);
        recordExternal(Solute::Iron, cellIron(c.meta), Flow::Spawn);
        cells_.push_back(std::move(c));
    }
    inoculateBacteria(bacteria);
}

std::uint64_t Culture::bacteriaCount() const {
    std::uint64_t n = 0;
    for (const BacterialCohort& b : bacteria_)
        n += b.count;
    return n;
}

int Culture::count(SpeciesId id) const {
    int n = 0;
    for (const CultureCell& c : cells_)
        n += c.species == id ? 1 : 0;
    return n;
}

CultureCensus Culture::census() const {
    CultureCensus c;
    for (const CultureCell& cell : cells_) {
        ++c.species[static_cast<int>(cell.species)];
        c.biovolume[static_cast<int>(cell.species)] += cellVolumeUm3(speciesInfo(cell.species), cell.meta.biomass);
        c.cysts += cell.cyst ? 1 : 0;
        c.biomass += cell.meta.biomass;
    }
    for (const BacterialCohort& b : bacteria_) {
        c.bacteria += b.count;
        c.biomass += static_cast<double>(b.count) * b.mass * kBacteriumPmol;
    }
    return c;
}

MassBudget Culture::massBudget() const {
    MassBudget b;
    b.ledger = ledger_;
    b.time = static_cast<float>(time_);
    auto add = [&](Element e, Pool p, double pmol) { b.stock[static_cast<int>(e)][static_cast<int>(p)] += pmol; };
    add(Element::P, Pool::Water, amount(Solute::Phosphate));
    add(Element::N, Pool::Water, amount(Solute::Nitrogen));
    add(Element::Si, Pool::Water, amount(Solute::Silicate));
    add(Element::Fe, Pool::Water, amount(Solute::Iron));
    add(Element::C, Pool::Water, kCarbonPerPmol * amount(Solute::Organic));
    add(Element::C, Pool::Gas, amount(Solute::CO2));
    for (const CultureCell& c : cells_) {
        add(Element::P, Pool::Cells, c.meta.phosphorus);
        add(Element::N, Pool::Cells, c.meta.nitrogen);
        add(Element::Si, Pool::Cells, c.meta.silicon);
        add(Element::Fe, Pool::Cells, cellIron(c.meta));
        add(Element::C, Pool::Cells, kCarbonPerPmol * (c.meta.biomass + c.meta.reserves));
        for (const FoodVacuole& v : c.digestion.vacuoles) {
            add(Element::P, Pool::Vacuoles, v.phosphorus);
            add(Element::N, Pool::Vacuoles, v.nitrogen);
            add(Element::C, Pool::Vacuoles, kCarbonPerPmol * v.contentPmol);
        }
    }
    double bacteria = 0.0;
    for (const BacterialCohort& c : bacteria_)
        bacteria += static_cast<double>(c.count) * c.mass;
    add(Element::C, Pool::Bacteria, kCarbonPerPmol * kBacteriumPmol * bacteria);
    add(Element::C, Pool::Detritus, kCarbonPerPmol * detritus_);
    add(Element::Si, Pool::Sediment, frustules_);
    return b;
}

void Culture::step(float dt) {
    PROFILE_SCOPE("Culture::step");
    if (!program_.empty())
        applyProgram(time_ / 3600.0, (time_ + dt) / 3600.0);
    if (dilutionPerHour_ > 0.0) // el flujo del quimiostato es física: tasa real
        dilute(1.0 - std::exp(-dilutionPerHour_ * dt / 3600.0));
    refreshMedium();
    const float bioDt = dt * q10_;
    stepGases(dt);
    // Lo que llega de afuera y lo que se descompone (como World::stepAmbient).
    if (medium_.organicInflow > 0.0f) {
        const double added = medium_.organicInflow * bioDt / 3600.0 * pmolPerMicroMolar_;
        amount_[static_cast<int>(Solute::Organic)] += added;
        recordExternal(Solute::Organic, added, Flow::Dose);
    }
    if (detritus_ > 0.0) {
        const double decayed = detritus_ * (1.0 - std::exp(-medium_.detritusDecay * bioDt));
        detritus_ -= decayed;
        exchange(Solute::Organic, decayed, Flow::Excretion);
    }
    if (frustules_ > 0.0) {
        const double dissolved = frustules_ * (1.0 - std::exp(-medium_.frustuleDissolutionPerHour * bioDt / 3600.0));
        frustules_ -= dissolved;
        exchange(Solute::Silicate, dissolved, Flow::Dissolution);
    }
    // La luz degrada la B₁₂. Simplificación: las bacterias del cultivo no la liberan (en la placa
    // sí); un medio para auxótrofas la trae.
    amount_[static_cast<int>(Solute::Vitamin)] *=
        std::exp(-medium_.vitaminDecayPerHour * medium_.surfaceLight * bioDt / 3600.0);
    stepBacteria(bioDt);
    stepPhages(bioDt);
    computeLight();
    feed(dt, bioDt);
    stepCells(dt, bioDt);
    time_ += dt;
    ++step_;
}

void Culture::stepGases(float dt) {
    // El O₂ y el CO₂ se equilibran con el aire por la superficie (tasa real, como en la placa).
    const double keep = std::exp(-static_cast<double>(airExchange_) * dt);
    double& o2 = amount_[static_cast<int>(Solute::Oxygen)];
    const double o2Sat = o2Saturation_ * pmolPerMicroMolar_;
    o2 = o2Sat + (o2 - o2Sat) * keep;
    double& co2 = amount_[static_cast<int>(Solute::CO2)];
    const double co2Sat = co2Saturation_ * pmolPerMicroMolar_;
    const double before = co2;
    co2 = co2Sat + (co2 - co2Sat) * keep;
    recordExternal(Solute::CO2, co2 - before, Flow::Air);
    // pH del sistema carbonato con la alcalinidad del medio (como World::updateChemistry).
    ph_ = std::clamp(medium_.carbonatePK + std::log10(medium_.alkalinity / std::max(0.05, concentration(Solute::CO2))),
                     4.0, 11.0);
}

void Culture::stepBacteria(float bioDt) {
    PROFILE_SCOPE("Culture::stepBacteria");
    const BacteriaParams& bp = bacteriaParams_;
    const float hours = bioDt / 3600.0f;
    const float maintenance = bp.maintenancePerHour * bioDt / 3600.0f;
    // Paso implícito para el sustrato: la concentración con la que comen es la del final del paso,
    // S = S₀ − U(S), donde U es lo que toman todas a esa concentración (Monod). Con la del principio,
    // en un paso de un minuto se comían todo lo que había entrado y el quimiostato no llegaba a su
    // estado estacionario (S* = K (D + m) / (µmax − D − m)) sino a ~0. U crece con S: bisección.
    const double start = std::max(0.0, concentration(Solute::Organic));
    auto uptake = [&](double conc) {
        double u = 0.0;
        for (const BacterialCohort& b : bacteria_)
            u += static_cast<double>(b.count) * bacterialDemand(bp, static_cast<float>(conc), b.mass,
                                                                bacterialHealth(bp, b.phage >= 0.0f, b.resistant != 0), hours);
        return u / pmolPerMicroMolar_; // [µM]
    };
    double lo = 0.0, hi = start;
    if (start > 0.0 && uptake(start) > 0.0)
        for (int it = 0; it < 40; ++it) {
            const double mid = 0.5 * (lo + hi);
            (start - mid - uptake(mid) > 0.0 ? lo : hi) = mid;
        }
    const float c = static_cast<float>(0.5 * (lo + hi));
    double demand = 0.0;
    for (const BacterialCohort& b : bacteria_)
        demand += static_cast<double>(b.count) *
                  bacterialDemand(bp, c, b.mass, bacterialHealth(bp, b.phage >= 0.0f, b.resistant != 0), hours);
    const double available = std::max(0.0, amount(Solute::Organic));
    const double share = demand > available ? available / demand : 1.0;
    double taken = 0.0, released = 0.0;
    const std::size_t n = bacteria_.size();
    for (std::size_t i = 0; i < n; ++i) {
        BacterialCohort& b = bacteria_[i];
        const float each = static_cast<float>(
            share * bacterialDemand(bp, c, b.mass, bacterialHealth(bp, b.phage >= 0.0f, b.resistant != 0), hours));
        taken += static_cast<double>(b.count) * each;
        const float change = bacterialMassChange(bp, each, b.mass, maintenance);
        // Reactivos: la muerte que agregan con lo que crece ahora la cohorte (Regoes et al. 2004).
        double kill = 0.0;
        for (std::size_t r = 0; r < reagents_.size(); ++r)
            if (reagents_[r] > 0.0 && reagent(static_cast<int>(r)).affects("bacteria"))
                kill += reagentKillRate(reagent(static_cast<int>(r)), reagents_[r],
                                        change / std::max(1e-6f, b.mass) / std::max(1e-9f, hours));
        b.mass += change;
        if (kill > 0.0 && b.count > 0) {
            Rng rng(config_.seed, kStreamBacteria + 70, i, step_);
            const double expected = static_cast<double>(b.count) * (1.0 - std::exp(-kill * hours));
            auto dead = static_cast<std::uint64_t>(expected);
            if (rng.uniform() < expected - static_cast<double>(dead))
                ++dead;
            dead = std::min(dead, b.count);
            released += static_cast<double>(dead) * b.mass * kBacteriumPmol; // la pared revienta: lisis
            b.count -= dead;
            if (b.count == 0)
                continue;
        }
        if (b.mass < 0.5f) { // mueren de hambre: su materia vuelve al agua
            released += static_cast<double>(b.count) * b.mass * kBacteriumPmol;
            b.count = 0;
        } else if (b.mass >= 2.0f) { // fisión binaria
            b.mass *= 0.5f;
            // Mutantes resistentes a los fagos: una fracción de las hijas (como en la placa).
            std::uint64_t mutants = 0;
            if (!b.resistant) {
                Rng rng(config_.seed, kStreamBacteria + 50, i, step_);
                mutants = std::min<std::uint64_t>(b.count, rng.poisson(static_cast<double>(b.count) * bp.resistanceMutation));
            }
            BacterialCohort daughters = b;
            daughters.count -= mutants;
            if (static_cast<int>(bacteria_.size()) < config_.maxBacterialCohorts) {
                bacteria_.push_back(daughters); // la referencia `b` ya no vale después de esto
            } else {
                b.count += daughters.count;
            }
            if (mutants > 0) {
                BacterialCohort r = daughters;
                r.count = mutants;
                r.resistant = 1;
                bacteria_.push_back(r);
            }
        }
    }
    // La masa que pierden al mantenerse se respira (C abierto); la que ganan sale del agua.
    exchange(Solute::Organic, -taken, Flow::Uptake);
    if (released > 0.0)
        exchange(Solute::Organic, released, Flow::Death);
    bacteria_.erase(std::remove_if(bacteria_.begin(), bacteria_.end(), [](const BacterialCohort& b) { return b.count == 0; }),
                    bacteria_.end());
}

void Culture::setPhages(double virions) {
    phages_ = std::max(0.0, virions);
}

void Culture::stepPhages(float bioDt) {
    PROFILE_SCOPE("Culture::stepPhages");
    const BacteriaParams& bp = bacteriaParams_;
    const float hours = bioDt / 3600.0f;
    phages_ *= std::exp(-medium_.phageDecayPerHour * hours); // inactivación (UV, adsorción a partículas)
    // Las infectadas cumplen su período latente y estallan: viriones nuevos y su contenido al agua.
    double released = 0.0;
    for (BacterialCohort& b : bacteria_) {
        if (b.phage < 0.0f)
            continue;
        b.phage += hours;
        if (b.phage >= bp.phageLatentHours) {
            phages_ += static_cast<double>(b.count) * bp.phageBurst;
            released += static_cast<double>(b.count) * b.mass * kBacteriumPmol;
            b.count = 0;
        }
    }
    if (released > 0.0)
        exchange(Solute::Organic, released, Flow::Death);
    // Adsorción a las sensibles (acción de masas): cada una se infecta con probabilidad
    // k·[fago]·dt. Las de este paso forman una cohorte nueva por cepa.
    const double concentration = phages_ / volumeUm3_;
    const double p = std::min(1.0, static_cast<double>(bp.phageAdsorption) * concentration * hours);
    BacterialCohort newly[3];
    std::uint64_t infected[3] = {};
    double massSum[3] = {};
    const std::size_t n = bacteria_.size();
    for (std::size_t i = 0; i < n && p > 0.0; ++i) {
        BacterialCohort& b = bacteria_[i];
        if (b.phage >= 0.0f || b.resistant || b.count == 0)
            continue;
        Rng rng(config_.seed, kStreamPhage, phageEvents_++, step_);
        const std::uint64_t k = std::min<std::uint64_t>(b.count, rng.poisson(static_cast<double>(b.count) * p));
        if (k == 0)
            continue;
        b.count -= k;
        const int s = std::min<int>(b.strain, 2);
        infected[s] += k;
        massSum[s] += static_cast<double>(k) * b.mass;
        newly[s].strain = b.strain;
    }
    for (int s = 0; s < 3; ++s) {
        if (infected[s] == 0)
            continue;
        const double used = std::min(phages_, static_cast<double>(infected[s]));
        phages_ -= used; // cada infección consume el virión que se adsorbió
        BacterialCohort c = newly[s];
        c.count = infected[s];
        c.mass = static_cast<float>(massSum[s] / static_cast<double>(infected[s]));
        c.phage = 0.0f;
        bacteria_.push_back(c);
    }
    bacteria_.erase(std::remove_if(bacteria_.begin(), bacteria_.end(), [](const BacterialCohort& b) { return b.count == 0; }),
                    bacteria_.end());
}

void Culture::computeLight() {
    // Beer–Lambert en la columna: k = atenuación específica × biomasa fotótrofa por volumen; la célula
    // bien mezclada recibe la media de la columna, I₀ (1 − e^(−kD)) / (kD).
    double phototrophs = 0.0;
    for (const CultureCell& c : cells_)
        phototrophs += c.meta.photosynthetic ? c.meta.biomass : 0.0;
    const double kd = kChlorophyllShading * phototrophs / volumeUm3_ * config_.depthUm;
    light_ = medium_.surfaceLight * (kd > 1e-6 ? (1.0 - std::exp(-kd)) / kd : 1.0);
}

float Culture::cruiseSpeed(const CultureCell& c) const {
    const SpeciesInfo& info = speciesInfo(c.species);
    if (info.sessile || info.locomotion == LocomotionType::None)
        return 0.0f;
    return kCruise * maxSpeedOf(locomotionParams_, info.locomotion) * info.speedScale * c.heredity.traits.speed *
           viscositySpeed_;
}

float Culture::radius(const CultureCell& c) const {
    const CellSpec& s = speciesInfo(c.species).spec;
    // Radio de la esfera de igual sección que la elipse, con la masa actual.
    const float base = std::sqrt(s.radiusX * s.radiusY) * c.heredity.traits.sizeScale;
    return base * std::cbrt(std::max(0.5f, c.meta.biomass / std::max(1e-6f, c.meta.targetBirthBiomass)));
}

bool Culture::fits(const CultureCell& predator, const CultureCell& prey) const {
    // Apertura bucal (M5), con la escala lineal de cada célula (genoma y masa actual).
    auto scale = [](const CultureCell& c) {
        return c.heredity.traits.sizeScale * std::cbrt(std::max(0.5f, c.meta.biomass / std::max(1e-6f, c.meta.targetBirthBiomass)));
    };
    return fitsMouth(speciesInfo(predator.species), scale(predator), speciesInfo(prey.species), scale(prey));
}

double Culture::encounterKernel(const CultureCell& predator, const CultureCell& prey) const {
    const SpeciesInfo& info = speciesInfo(predator.species);
    const double reach = std::max(info.tentacleReach, info.neckReach);
    const double r = radius(predator) + radius(prey) + reach;
    const double v = cruiseSpeed(predator), u = cruiseSpeed(prey);
    const double fast = std::max(u, v), slow = std::min(u, v);
    if (fast <= 0.0)
        return 0.0;
    // Gerritsen y Strickler (1977): Z = π R² (slow² + 3 fast²) / (3 fast).
    return units::kPi * r * r * (slow * slow + 3.0 * fast * fast) / (3.0 * fast);
}

double Culture::preyEncounterRate(const CultureCell& predator) const {
    const SpeciesInfo& info = speciesInfo(predator.species);
    if (!info.prey.any())
        return 0.0;
    double rate = 0.0;
    for (const CultureCell& q : cells_)
        if (&q != &predator && !q.cyst && info.prey.test(static_cast<std::size_t>(q.species)) && fits(predator, q))
            rate += encounterKernel(predator, q) * (1.0 - q.heredity.traits.trichocystDefense);
    return rate / volumeUm3_;
}

double Culture::bacterivory(const CultureCell& c, double density) const {
    const SpeciesInfo& info = speciesInfo(c.species);
    if (!eatsBacteria(info.feeding) || c.cyst)
        return 0.0;
    const SpeciesPhysiology& physio = speciesPhysiology(c.species);
    const FeedingParams& fp = feedingParams_;
    // Ingestión máxima [bacterias/s biológico]: el dato publicado o el caudal del citostoma de la
    // placa (que traga a ritmo mecánico y rinde ×bioTimeScale: por segundo biológico, lo mismo).
    double maxRate = info.feeding == FeedingMode::OralGroove ? fp.mouthMaxRate
                     : info.feeding == FeedingMode::Anterior ? fp.flagellateMaxRate
                                                             : fp.contactMaxRate;
    if (physio.maxIngestion.present)
        maxRate = physio.maxIngestion.value / 3600.0;
    // Aclaramiento [µm³/s]: el dato o el de la placa, que es física: la corriente oral que atraviesa
    // el disco de captura del citostoma; en el flagelado, el polo anterior que barre el agua al
    // nadar; en la ameba, el encuentro con las bacterias que nadan (Gerritsen y Strickler).
    double clearance = 0.0;
    const double mouth = units::kPi * fp.mouthRadius * fp.mouthRadius;
    if (physio.clearance.present)
        clearance = physio.clearance.value / 3600.0;
    else if (info.feeding == FeedingMode::OralGroove)
        clearance = locomotionParams_.oralCurrent * mouth;
    else if (info.feeding == FeedingMode::Anterior)
        clearance = mouth * cruiseSpeed(c);
    else {
        const double r = radius(c) + bacteriaParams_.radius, v = cruiseSpeed(c), u = bacteriaParams_.speed;
        const double fast = std::max(u, v), slow = std::min(u, v);
        clearance = units::kPi * r * r * (slow * slow + 3.0 * fast * fast) / (3.0 * fast);
    }
    // Tipo II (Holling): con pocas presas filtra a su aclaramiento; con muchas, a su caudal máximo.
    const double encounter = clearance * density;
    return maxRate > 0.0 ? encounter / (1.0 + encounter / maxRate) : 0.0;
}

std::uint64_t Culture::removeBacteria(std::uint64_t count, double& pmol) {
    // Se sacan `count` bacterias al azar, cada cohorte en proporción a su tamaño (muestreo
    // secuencial: la última cohorte completa lo que falta, así salen exactamente `count`).
    pmol = 0.0;
    std::uint64_t left = bacteriaCount(), toTake = std::min(count, left), removed = 0;
    for (std::size_t i = 0; i < bacteria_.size() && toTake > 0; ++i) {
        BacterialCohort& b = bacteria_[i];
        Rng rng(config_.seed, kStreamGrazing + 50, i, step_);
        const double expected = static_cast<double>(toTake) * static_cast<double>(b.count) / static_cast<double>(left);
        std::uint64_t r = static_cast<std::uint64_t>(expected);
        if (rng.uniform() < expected - static_cast<double>(r))
            ++r;
        r = std::min({r, b.count, toTake});
        left -= b.count;
        b.count -= r;
        toTake -= r;
        removed += r;
        pmol += static_cast<double>(r) * b.mass * kBacteriumPmol;
    }
    bacteria_.erase(std::remove_if(bacteria_.begin(), bacteria_.end(), [](const BacterialCohort& b) { return b.count == 0; }),
                    bacteria_.end());
    return removed;
}

void Culture::feed(float dt, float bioDt) {
    PROFILE_SCOPE("Culture::feed");
    // Bacterívoros: cada uno, su respuesta funcional con la densidad de bacterias del medio. Se
    // sortea cuántas come (Poisson) y se sacan todas juntas.
    const double density = static_cast<double>(bacteriaCount()) / volumeUm3_;
    eatenBacteria_.assign(cells_.size(), 0);
    std::uint64_t total = 0;
    for (std::size_t i = 0; i < cells_.size(); ++i) {
        const double rate = bacterivory(cells_[i], density);
        if (rate <= 0.0)
            continue;
        Rng rng(config_.seed, kStreamGrazing, cells_[i].id, step_);
        eatenBacteria_[i] = rng.poisson(rate * bioDt);
        total += eatenBacteria_[i];
    }
    if (total > 0) {
        double pmol = 0.0;
        const std::uint64_t removed = removeBacteria(total, pmol);
        // Si se pidieron más de las que había, cada uno se queda con su parte; la masa se reparte
        // en proporción a lo que comió (se conserva exacta).
        for (std::size_t i = 0; i < cells_.size() && removed > 0; ++i) {
            if (eatenBacteria_[i] == 0)
                continue;
            const double share = static_cast<double>(eatenBacteria_[i]) / static_cast<double>(total);
            const auto items = static_cast<int>(std::llround(share * static_cast<double>(removed)));
            CultureCell& c = cells_[i];
            ingestContents(c.digestion, kBacteriumArea * static_cast<float>(items), static_cast<float>(pmol * share), false, 0.0f,
                           1.0f, 0.0f, std::max(1, items));
            c.digestion.bacteriaEaten += items;
        }
    }

    // Cazadores de células: encuentros de Gerritsen y Strickler en tiempo real (nadar es física);
    // la presa se traga entera y la digestión (biológica) fija el tiempo de manipulación.
    eatenCell_.assign(cells_.size(), 0);
    for (std::size_t i = 0; i < cells_.size(); ++i) {
        CultureCell& p = cells_[i];
        const SpeciesInfo& info = speciesInfo(p.species);
        if (!info.prey.any() || p.cyst || eatenCell_[i] || p.digestion.handling > 0.0f ||
            satiated(p.digestion, p.meta.biomass))
            continue;
        if (info.feeding == FeedingMode::Tentacles && p.meta.reserves > 0.6f * p.meta.biomass)
            continue; // con el citoplasma lleno, el suctorio no atrapa más (World::isSatiated)
        const double rate = preyEncounterRate(p);
        if (rate <= 0.0)
            continue;
        Rng rng(config_.seed, kStreamHunting, p.id, step_);
        if (rng.uniform() >= 1.0 - std::exp(-rate * dt))
            continue;
        // Qué presa: en proporción a su núcleo de encuentro y a lo que se defiende.
        double pick = rng.uniform() * rate * volumeUm3_;
        std::size_t chosen = cells_.size();
        for (std::size_t k = 0; k < cells_.size(); ++k) {
            const CultureCell& q = cells_[k];
            if (k == i || q.cyst || !info.prey.test(static_cast<std::size_t>(q.species)) || !fits(p, q))
                continue;
            pick -= encounterKernel(p, q) * (1.0 - q.heredity.traits.trichocystDefense);
            chosen = k;
            if (pick <= 0.0)
                break;
        }
        if (chosen == cells_.size() || eatenCell_[chosen])
            continue;
        const CultureCell& q = cells_[chosen];
        // La coraza (lórica, testa) impide tragarla.
        if (q.heredity.traits.armor >= 0.5f)
            continue;
        float phosphorus = q.meta.phosphorus, nitrogen = q.meta.nitrogen;
        for (const FoodVacuole& v : q.digestion.vacuoles) {
            phosphorus += v.phosphorus;
            nitrogen += v.nitrogen;
        }
        const float area = units::kPi * radius(q) * radius(q);
        ingestContents(p.digestion, area, static_cast<float>(cellContent(q)), true, phosphorus, 1.0f, nitrogen);
        acquireSymbionts(info, q.species, p.meta);
        ++p.digestion.cellsEaten;
        p.digestion.handling = info.handlingHours * 3600.0f;
        releaseEaten(q);
        eatenCell_[chosen] = 1;
        ++deathTally_[{q.species, std::string("comida por ") + info.name}];
    }
    // Las presas tragadas salen del cultivo (en orden inverso: los índices siguen valiendo).
    for (std::size_t k = cells_.size(); k-- > 0;)
        if (eatenCell_[k])
            cells_.erase(cells_.begin() + static_cast<std::ptrdiff_t>(k));
}

void Culture::releaseEaten(const CultureCell& prey) {
    // Simplificación (como en la placa, World::releaseMicronutrients): el hierro y la B₁₂ vuelven
    // disueltos de inmediato; la sílice de una diatomea comida se disuelve en el agua.
    if (cellIron(prey.meta) > 0.0f)
        exchange(Solute::Iron, cellIron(prey.meta), Flow::Death);
    if (prey.meta.vitamin > 0.0f)
        exchange(Solute::Vitamin, prey.meta.vitamin, Flow::Death);
    if (prey.meta.silicon > 0.0f)
        exchange(Solute::Silicate, prey.meta.silicon, Flow::Dissolution);
}

void Culture::stepCells(float dt, float bioDt) {
    PROFILE_SCOPE("Culture::stepCells");
    const MetabolismParams& mp = metabolismParams_;
    dying_.clear();
    splits_.clear();
    for (std::size_t i = 0; i < cells_.size(); ++i) {
        CultureCell& c = cells_[i];
        const SpeciesInfo& info = speciesInfo(c.species);
        MetabolicInput in;
        in.o2 = static_cast<float>(concentration(Solute::Oxygen));
        in.light = static_cast<float>(light_);
        Excreted excreted;
        in.digested = digestContents(c.digestion, feedingParams_, dt, bioDt, excreted);
        c.digestion.handling = std::max(0.0f, c.digestion.handling - bioDt);
        if (info.feeding == FeedingMode::Osmotroph) {
            const float org = static_cast<float>(std::max(0.0, concentration(Solute::Organic)));
            const float want = osmotrophDemand(c.meta.biomass, org, bioDt);
            if (want > 0.0f)
                in.digested -= static_cast<float>(exchange(Solute::Organic, -want, Flow::Uptake));
        }
        if (excreted.phosphorus > 0.0f)
            exchange(Solute::Phosphate, excreted.phosphorus, Flow::Excretion);
        if (excreted.nitrogen > 0.0f)
            exchange(Solute::Nitrogen, excreted.nitrogen, Flow::Excretion);
        in.phosphate = static_cast<float>(concentration(Solute::Phosphate));
        in.nitrogen = static_cast<float>(concentration(Solute::Nitrogen));
        in.silicate = static_cast<float>(concentration(Solute::Silicate));
        in.iron = static_cast<float>(concentration(Solute::Iron));
        in.vitamin = static_cast<float>(concentration(Solute::Vitamin));
        in.activity = cruiseSpeed(c) > 0.0f ? kCruise : 0.0f;
        in.bioDt = bioDt;
        applySpeciesPhysiology(in, c.species, info, &c.heredity, mp);
        if (c.cyst)
            applyCyst(in);
        CellSurroundings around;
        around.o2 = in.o2;
        around.ph = static_cast<float>(ph_);
        around.temperature = medium_.temperature;
        around.salinity = medium_.salinity;
        around.light = in.light;
        around.pigment = c.meta.pigment;
        const char* stress = applyEnvironmentStress(info, around, &c.digestion.toxin, bioDt, in);
        const float pBefore = c.meta.phosphorus, nBefore = c.meta.nitrogen;
        const float siBefore = c.meta.silicon, feBefore = c.meta.iron, b12Before = c.meta.vitamin;
        const MetabolicOutput out = updateMetabolism(c.meta, mp, in);
        updateStressPigment(info, in.light, c.meta.limiting, c.cyst, bioDt, c.meta);
        if (const float sugar = stepPhotosymbionts(info, mp, c.meta, in.light, bioDt); sugar > 0.0f) {
            c.meta.reserves += sugar;
            exchange(Solute::Oxygen, 6.0 * sugar, Flow::Other); // como la fotosíntesis de las algas
            exchange(Solute::CO2, -6.0 * sugar, Flow::Other);
        }
        if (const float want = loricaIronDemand(info, c.meta, bioDt); want > 0.0f)
            c.meta.loricaIron -= static_cast<float>(exchange(Solute::Iron, -want, Flow::Uptake));
        if (out.phosphateTaken > 0.0f)
            c.meta.phosphorus = pBefore - static_cast<float>(exchange(Solute::Phosphate, -out.phosphateTaken, Flow::Uptake));
        if (out.nitrogenTaken > 0.0f)
            c.meta.nitrogen = nBefore - static_cast<float>(exchange(Solute::Nitrogen, -out.nitrogenTaken, Flow::Uptake));
        if (out.siliconTaken > 0.0f)
            c.meta.silicon = siBefore - static_cast<float>(exchange(Solute::Silicate, -out.siliconTaken, Flow::Uptake));
        if (out.ironTaken > 0.0f)
            c.meta.iron = feBefore - static_cast<float>(exchange(Solute::Iron, -out.ironTaken, Flow::Uptake));
        if (out.vitaminTaken > 0.0f)
            c.meta.vitamin = b12Before - static_cast<float>(exchange(Solute::Vitamin, -out.vitaminTaken, Flow::Uptake));
        // Gases: sin compresión del tiempo, lo que respira en `bioDt` es lo que respira en `dt`.
        exchange(Solute::CO2, out.o2Consumed + out.co2Fermented - out.o2Produced, Flow::Other);
        exchange(Solute::Oxygen, out.o2Produced - out.o2Consumed, Flow::Other);
        if (out.exudate > 0.0f)
            exchange(Solute::Organic, out.exudate, Flow::Excretion);
        if (isDead(c.meta, mp)) {
            dying_.push_back({i, stress ? stress : "inanicion"});
            continue;
        }
        // Quistes (Didinium): se enquista con hambre si no hay presas a mano y sale cuando vuelve a
        // haberlas. Simplificación de la percepción de la placa (presas a menos de 300 µm): esperar
        // menos de un encuentro por hora.
        if (info.encysts) {
            const double perHour = preyEncounterRate(c) * 3600.0;
            if (c.cyst) {
                c.cystAge += dt;
                if (c.cystAge > 30.0f && perHour >= 1.0) {
                    c.cyst = false;
                    c.cystAge = 0.0f;
                }
                continue;
            }
            if (c.meta.starving && c.division < 0.0f && perHour < 1.0) {
                c.cyst = true;
                c.cystAge = 0.0f;
                continue;
            }
        } else if (c.cyst) {
            continue;
        }
        const float cost = mp.npcDivisionCost;
        if (c.division < 0.0f && readyToDivide(c.meta, cost)) {
            c.meta.reserves -= cost * c.meta.biomass;
            c.division = 0.0f;
        } else if (c.division >= 0.0f) {
            c.division += bioDt / (mp.divisionHours * 3600.0f);
            if (c.division >= 1.0f)
                splits_.push_back(i);
        }
    }
    // Eventos al final (los índices de `splits_` y `dying_` son anteriores a cualquier cambio).
    for (const std::size_t i : splits_)
        divide(i);
    for (std::size_t k = dying_.size(); k-- > 0;)
        lyse(dying_[k].first, dying_[k].second);
}

void Culture::divide(std::size_t index) {
    CultureCell& mother = cells_[index];
    const SpeciesInfo& info = speciesInfo(mother.species);
    mother.division = -1.0f;
    mother.meta.pendingFissions = mother.meta.pendingFissions > 0 ? mother.meta.pendingFissions - 1 : mother.meta.fissions - 1;
    CultureCell child = mother;
    child.id = nextId_++;
    child.meta = splitMetabolism(mother.meta, 0.5f);
    // Las vacuolas se reparten alternadas.
    child.digestion = Digestion{};
    child.digestion.rateScale = mother.digestion.rateScale;
    std::vector<FoodVacuole> keep;
    for (std::size_t k = 0; k < mother.digestion.vacuoles.size(); ++k)
        (k % 2 ? child.digestion.vacuoles : keep).push_back(mother.digestion.vacuoles[k]);
    mother.digestion.vacuoles = std::move(keep);
    ++mother.lineage.generation;
    child.lineage = mother.lineage;
    Rng ra(config_.seed, kStreamMutation, mother.id * 2ull, step_);
    Rng rb(config_.seed, kStreamMutation, mother.id * 2ull + 1, step_);
    Heredity ha, hb;
    daughterGenomes(mother.heredity, info.locomotion, ra, rb, 0.25f, ha, hb);
    mother.heredity = ha;
    child.heredity = hb;
    if (info.frustule)
        shrinkFrustule(child.heredity, info.locomotion);
    applyTraits(mother);
    applyTraits(child);
    cells_.push_back(std::move(child));
}

void Culture::lyse(std::size_t index, const char* cause) {
    const CultureCell& c = cells_[index];
    ++deathTally_[{c.species, cause}];
    const double total = cellContent(c);
    // La membrana revienta: una parte se disuelve y el resto queda como detritos.
    exchange(Solute::Organic, kLysisDissolved * total, Flow::Death);
    detritus_ += kLysisParticulate * total;
    double phosphorus = c.meta.phosphorus, nitrogen = c.meta.nitrogen;
    for (const FoodVacuole& v : c.digestion.vacuoles) {
        phosphorus += v.phosphorus;
        nitrogen += v.nitrogen;
    }
    if (phosphorus > 0.0)
        exchange(Solute::Phosphate, phosphorus, Flow::Death);
    if (nitrogen > 0.0)
        exchange(Solute::Nitrogen, nitrogen, Flow::Death);
    if (cellIron(c.meta) > 0.0f)
        exchange(Solute::Iron, cellIron(c.meta), Flow::Death);
    if (c.meta.vitamin > 0.0f)
        exchange(Solute::Vitamin, c.meta.vitamin, Flow::Death);
    frustules_ += c.meta.silicon; // el Si queda en la frústula vacía
    cells_.erase(cells_.begin() + static_cast<std::ptrdiff_t>(index));
}
