#include "game/run.hpp"

#include "core/profiling.hpp"
#include "game/world.hpp"

#include <cmath>
#include <limits>

void Run::start(World& world, const RunConfig& config, std::uint32_t runId) {
    const float interval = sampleInterval_;
    *this = Run{};
    sampleInterval_ = interval;
    config_ = config;
    world.setRun(runId);
    world.populateEcosystem(config.wildlife);
    world.populate(config.competitors);
    // Siembra de una placa preparada (células por cm² del campo, ya convertidas en cantidades).
    for (const auto& [species, count] : world.config().seeding)
        world.inoculate({BeingKind::Species, species}, count, {}, 0.0f);
    world.placeSampled(world.config().sampled); // gota de muestra de un cultivo
    if (world.config().fixedSlide) { // frotis o corte fijado: nada vivo, nada se mueve
        world.flattenSmear();
        world.fix();
    }
    if (config.spawnFounder)
        world.spawnPlayer(config.founder, {0.5f * world.width(), 0.5f * world.height()}, &config.genome);
    census_ = world.census();
    buildColumns();
    startBudget_ = world.massBudget();
    sample(world);
}

void Run::update(World& world, float dt) {
    const std::vector<WorldEvent> events = world.takeEvents();
    if (!events.empty())
        census_ = world.census(); // el panel refleja la división o la muerte en el acto
    for (const WorldEvent& ev : events) {
        switch (ev.type) {
        case WorldEvent::Type::PlayerDivided: ++divisions_; break; // el ser sigue con la madre
        case WorldEvent::Type::PlayerConjugated: ++conjugations_; break;
        case WorldEvent::Type::PlayerDied: deathCause_ = ev.cause; break; // se puede controlar otro ser
        }
    }
    elapsed_ += dt;
    censusTimer_ -= dt;
    if (censusTimer_ <= 0.0f) { // el censo recorre todas las células: una vez por intervalo
        censusTimer_ += sampleInterval_;
        if (censusTimer_ <= 0.0f)
            censusTimer_ = sampleInterval_;
        census_ = world.census();
        sample(world);
    }
}

void Run::setRegion(const World& world, std::vector<Vec2> polygon) {
    region_ = std::move(polygon);
    regionStats_ = quantifyRegion(world, region_);
}

void Run::clearRegion() {
    region_.clear();
    regionStats_ = RegionStats{};
}

namespace {
const char* const kChemNames[] = {"O2", "pH", "Fosfato", "Nitrógeno inorgánico", "Sílice", "Hierro", "Vitamina B12",
                                  "Materia orgánica", "CO2"};
constexpr Element kConserved[] = {Element::P, Element::N, Element::Si, Element::Fe};
} // namespace

void Run::buildColumns() {
    std::vector<SeriesColumn> c;
    c.push_back({"t", "s", "Tiempo"});
    c.push_back({"t bio", "h", "Tiempo"});
    for (int s = 0; s < speciesCount(); ++s)
        // Por el id estable (no por el nombre que se muestra): el CSV no cambia con el idioma.
        c.push_back({speciesInfo(static_cast<SpeciesId>(s)).key, "individuos", "Poblacion"});
    c.push_back({"Bacterias", "individuos", "Poblacion"});
    c.push_back({"Cianobacterias", "células", "Poblacion"});
    c.push_back({"Biomasa viva", "pmol", "Poblacion"});
    for (const char* name : kChemNames)
        c.push_back({name, std::string(name) == "pH" ? "" : "µM", "Quimica"});
    for (int e = 0; e < kElementCount; ++e) {
        const std::string el = elementName(static_cast<Element>(e));
        c.push_back({el + " total", "pmol " + el, "Balance"});
    }
    for (Element e : kConserved) {
        const std::string el = elementName(e);
        c.push_back({el + " disuelto", "pmol " + el, "Balance"});
        c.push_back({el + " en seres", "pmol " + el, "Balance"});
        c.push_back({el + " residuo", "pmol " + el, "Balance"});
    }
    c.push_back({"Región: células", "individuos", "Region"});
    c.push_back({"Región: bacterias", "individuos", "Region"});
    c.push_back({"Región: O2", "µM", "Region"});
    c.push_back({"Región: fosfato", "µM", "Region"});
    c.push_back({"Región: nitrógeno", "µM", "Region"});
    row_.assign(c.size(), 0.0f);
    series_.setColumns(std::move(c));
}

void Run::sample(const World& world) {
    PROFILE_SCOPE("Run::sample");
    budget_ = world.massBudget();
    if (!region_.empty())
        regionStats_ = quantifyRegion(world, region_);
    if (series_.columns() == 0 || !series_.offer())
        return;
    std::size_t k = 0;
    auto put = [&](double v) { row_[k++] = static_cast<float>(v); };
    put(world.simTime());
    put(world.simTime() * world.bioTimeScale() / 3600.0f); // horas biológicas (a 20 °C)
    for (int s = 0; s < speciesCount(); ++s)
        put(census_.species[s]);
    put(census_.bacteria);
    put(census_.cyanoCells);
    put(census_.totalBiomass);
    put(census_.meanOxygen);
    put(census_.meanPH);
    put(census_.meanPhosphate);
    put(census_.meanNitrogen);
    put(census_.meanSilicate);
    put(census_.meanIron);
    put(census_.meanVitamin);
    put(census_.meanOrganic);
    put(census_.meanCO2);
    for (int e = 0; e < kElementCount; ++e)
        put(budget_.total(static_cast<Element>(e)));
    for (Element e : kConserved) {
        const double water = budget_.at(e, Pool::Water);
        put(water);
        put(budget_.total(e) - water);
        put(residual(e));
    }
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const bool r = regionStats_.valid;
    put(r ? regionStats_.cells : nan);
    put(r ? regionStats_.bacteria : nan);
    put(r ? regionStats_.oxygen : nan);
    put(r ? regionStats_.phosphate : nan);
    put(r ? regionStats_.nitrogen : nan);
    series_.push(row_.data());
}
