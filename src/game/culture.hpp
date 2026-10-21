#pragma once

#include "bio/bacteria.hpp"
#include "bio/feeding.hpp"
#include "bio/genome.hpp"
#include "bio/locomotion.hpp"
#include "bio/metabolism.hpp"
#include "bio/species.hpp"
#include "game/lineage.hpp"
#include "game/mass_budget.hpp"
#include "game/program.hpp"
#include "game/world.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

// Cultivo bien mezclado (fase P1): un tubo o un frasco de mililitros, como los de Gause o Tilman,
// donde las poblaciones no caben en una placa simulada célula por célula. Sin espacio ni cuerpos
// blandos, pero con la MISMA fisiología que la placa (`bio/cell_physiology`, `updateMetabolism`,
// `digestContents`, `bacterialDemand`): cada célula es un individuo con su metabolismo, su genoma
// y su linaje; las bacterias, superindividuos (cohortes de bacterias iguales).
//
// Lo que en la placa emerge del espacio acá sale de la teoría física de encuentros:
//  - filtradores: respuesta funcional de tipo II con el aclaramiento y la ingestión máxima
//    ([physiology.<id>.*], con su fuente; sin dato, los de la corriente oral y el citostoma de la placa);
//  - cazadores: núcleo de encuentro de Gerritsen y Strickler (1977, J. Fish. Res. Board Can. 34:
//    73-82) con las velocidades de nado y los radios de los datos.
// El medio está bien mezclado: la concentración es la misma en todo el volumen.
//
// Tiempo: no hay física en tiempo real que comprimir, así que no hay `bioTimeScale`. `step(dt)`
// avanza `dt` segundos; la biología, `dt · Q10` (la misma referencia de 20 °C que la placa). El
// intercambio de gases con el aire va a su tasa real.
//
// Luz: la lámpara ilumina la superficie y las fotótrofas la atenúan en la columna (Beer–Lambert);
// cada célula recibe la media de la columna (está bien mezclada). Fagos líticos con adsorción de
// acción de masas, período latente, estallido y mutantes resistentes (los mismos parámetros que la
// placa).
//
// Simplificaciones de esta versión (se dicen en la ficha del cultivo): sin Bdellovibrio ni quorum
// sensing, sin conjugación, sin colonias, filamentos, zoosporas ni células animales (esas especies
// no se pueden sembrar), las bacterias no liberan B₁₂ (un medio para auxótrofas la trae) y los
// suctorios tragan a la presa entera en vez de succionarla.

enum class Solute : std::uint8_t { Oxygen, CO2, Phosphate, Nitrogen, Silicate, Iron, Vitamin, Organic, Count };
inline constexpr int kSoluteCount = static_cast<int>(Solute::Count);
const char* soluteKey(Solute s);   // en los archivos: "o2", "co2", "phosphate"...
const char* soluteLabel(Solute s); // en la interfaz (clave de traducción): "fosfato"...

struct CultureConfig {
    float volumeMl = 0.5f;
    // Profundidad del medio [µm] y velocidad de transferencia de gases por la superficie [cm/h].
    // Con el medio quieto (gasTransferCmH = 0) el O₂ y el CO₂ difunden por toda la columna, a una
    // tasa D / profundidad² (como en un recipiente de cultivo de la placa preparada). Con el medio
    // mezclado la resistencia queda en la película de la superficie: tasa = k / profundidad.
    float depthUm = 5000.0f;
    float gasTransferCmH = 0.0f;
    // Genética (bio/genome): variación de lo que se siembra (0 = un clon, línea pura; 1 = la de una
    // población silvestre) y escala de la varianza mutacional por división (1 = la publicada).
    float geneticDiversity = 0.0f;
    float mutationScale = 1.0f;
    MediumParams medium;
    std::uint64_t seed = 1;
    // Superindividuos de bacterias: hasta este número, una cohorte por bacteria (exacto); más allá,
    // las divisiones duplican el conteo de cada cohorte.
    int maxBacterialCohorts = 4096;
};

struct CultureCell {
    std::uint32_t id = 0;
    SpeciesId species{};
    Metabolism meta;
    Heredity heredity;
    Digestion digestion;
    Lineage lineage;
    float division = -1.0f; // progreso de la citocinesis (< 0: no se divide)
    bool cyst = false;
    float cystAge = 0.0f;   // [s]
};

// Cohorte: `count` bacterias con la misma masa (relativa a una recién nacida) y cepa.
struct BacterialCohort {
    std::uint64_t count = 1;
    float mass = 1.0f;
    std::uint8_t strain = 0;
    std::uint8_t resistant = 0; // a los fagos (receptor alterado)
    float phage = -1.0f;        // horas desde la infección por un fago (< 0: sanas)
};

struct CultureCensus {
    int species[kMaxSpecies] = {};
    double biovolume[kMaxSpecies] = {}; // [µm³] (bio/species: cellVolumeUm3)
    int cysts = 0;
    std::uint64_t bacteria = 0;
    double biomass = 0.0; // viva [pmol]
};

// Tasa de intercambio de un gas con el aire [1/s] de una columna de medio de `depthUm`: con
// `gasTransferCmH` > 0 el medio está mezclado y la resistencia es la película de la superficie
// (teoría de las dos películas: flujo = k·(C* − C) por área, tasa = k / profundidad); con 0 está
// quieto y el gas difunde por toda la columna (D / profundidad²).
double gasExchangeRate(float depthUm, float gasTransferCmH);

class Culture {
  public:
    explicit Culture(const CultureConfig& config);

    // Avanza `dt` segundos.
    void step(float dt);

    // ¿Se puede sembrar en un cultivo bien mezclado? (`why`: por qué no.)
    static bool supports(SpeciesId id, std::string* why = nullptr);
    // Inóculo de afuera: `count` individuos en distintas etapas de su ciclo, con su cuota de
    // nutrientes (entra al balance como entrada externa). Devuelve cuántos sembró.
    int inoculate(SpeciesId id, int count);
    void inoculateBacteria(std::uint64_t count, std::uint8_t strain = 0);
    // Recibe lo que sale de otro cultivo (un pasaje): las células con su estado, las bacterias y
    // `volumeMl` de su medio con las concentraciones `micromolar` (una por soluto). Entra de afuera.
    void receive(std::vector<CultureCell> cells, std::uint64_t bacteria, double volumeMl, const double* micromolar);
    // Fija la concentración de un soluto (gotero, medio nuevo); la diferencia es entrada externa.
    void setConcentration(Solute s, double micromolar);
    // Cambio de medio (Gause: se centrifuga, se tira el sobrenadante y se agrega medio nuevo): la
    // fracción `fraction` de lo disuelto, de las bacterias, de los detritos y de los fagos se va y
    // entra medio nuevo (la composición de `config().medium`); los protistas se quedan.
    void renewMedium(double fraction);
    // Dilución: la fracción `fraction` del cultivo, con todo lo que tiene (también los protistas,
    // sorteados), se reemplaza por medio nuevo. Base del quimiostato y de los pasajes.
    void dilute(double fraction);
    // Saca la fracción `fraction` del cultivo sin reponerla (una gota de muestra): las células
    // sorteadas van a `cells`, las bacterias a `bacteria` y el volumen baja. `concentrate` > 1: las
    // células y bacterias se concentran (centrifugado suave) y el medio que sale es el de `fraction`.
    void withdraw(double fraction, double concentrate, std::vector<CultureCell>& cells, std::uint64_t& bacteria);
    // Quimiostato: entra medio nuevo y sale cultivo a la tasa de dilución D [1/h] (el volumen no
    // cambia). Cada célula y cada bacteria sale con probabilidad 1 − e^(−D·dt): el lavado (Monod 1950;
    // Novick y Szilard 1950). 0: cultivo en lote.
    void setDilutionRate(double perHour) { dilutionPerHour_ = std::max(0.0, perHour); }
    // Mezcla del medio: velocidad de transferencia de gases por la superficie [cm/h] (0: quieto).
    void setGasTransfer(float cmPerHour);
    void setMutationScale(float scale) { config_.mutationScale = std::max(0.0f, scale); }
    float mutationScale() const { return config_.mutationScale; }
    float gasTransfer() const { return config_.gasTransferCmH; }
    // Reactivos (game/reagent): concentración [µg/mL] de cada uno en el medio. El medio que entra no
    // trae (se diluyen con el cambio de medio, la dilución y el quimiostato).
    void setReagent(int index, double ugPerMl);
    double reagentConcentration(int index) const;
    // Composición del medio que entra (cambio de medio, dilución, quimiostato): la del medio con que
    // se preparó el cultivo, salvo los solutos que se fijen acá (el reservorio de un quimiostato).
    void setFreshConcentration(Solute s, double micromolar) {
        fresh_[static_cast<int>(s)] = std::max(0.0, micromolar);
        freshSet_[static_cast<int>(s)] = true;
    }
    double dilutionRate() const { return dilutionPerHour_; }
    // Programa temporal (game/program): se aplica antes de cada paso, con la hora de laboratorio.
    Program& program() { return program_; }
    const Program& program() const { return program_; }

    double concentration(Solute s) const { return amount_[static_cast<int>(s)] / pmolPerMicroMolar_; } // [µM]
    double amount(Solute s) const { return amount_[static_cast<int>(s)]; }                             // [pmol]
    double ph() const { return ph_; }
    double phages() const { return phages_; }          // viriones libres
    double light() const { return light_; }            // irradiancia relativa media de la columna
    void setPhages(double virions);                    // la diferencia entra de afuera
    double volumeMl() const { return volumeUm3_ * 1e-12; }
    double volumeUm3() const { return volumeUm3_; }
    double detritus() const { return detritus_; } // materia orgánica particulada [pmol]
    double time() const { return time_; }          // [s]
    std::uint64_t steps() const { return step_; }
    float bioScale() const { return q10_; }       // biológico por segundo (Q10 respecto de 20 °C)

    const std::vector<CultureCell>& cells() const { return cells_; }
    const std::vector<BacterialCohort>& bacteria() const { return bacteria_; }
    std::uint64_t bacteriaCount() const;
    int count(SpeciesId id) const;
    CultureCensus census() const;
    MassBudget massBudget() const;
    const std::map<std::pair<SpeciesId, std::string>, int>& deathTally() const { return deathTally_; }

    const CultureConfig& config() const { return config_; }
    MediumParams& medium() { return medium_; }
    const MediumParams& medium() const { return medium_; }
    MetabolismParams& metabolismParams() { return metabolismParams_; }
    FeedingParams& feedingParams() { return feedingParams_; }
    BacteriaParams& bacteriaParams() { return bacteriaParams_; }
    LocomotionParams& locomotionParams() { return locomotionParams_; }

    // Respuesta funcional del filtrador `cell` con `bacteriaPerUm3` presas [bacterias/s biológico]
    // (la usa la calibración contra la placa).
    double bacterivory(const CultureCell& cell, double bacteriaPerUm3) const;

  private:
    double exchange(Solute s, double pmol, Flow flow); // > 0 al agua; < 0 toma lo que haya
    void applyProgram(double fromHours, double toHours);
    double freshConcentration(Solute s) const;
    void washOut(const CultureCell& c); // lo que se lleva el medio de una célula que sale
    void recordExternal(Solute s, double pmol, Flow flow);
    void refreshMedium();
    void stepGases(float dt);
    void stepBacteria(float bioDt);
    void stepPhages(float bioDt);
    void computeLight();
    void feed(float dt, float bioDt);
    void stepCells(float dt, float bioDt);
    void divide(std::size_t index);
    void lyse(std::size_t index, const char* cause);
    void releaseEaten(const CultureCell& prey);
    void applyTraits(CultureCell& cell);
    // Rapidez de nado de crucero [µm/s] y radio equivalente [µm].
    float cruiseSpeed(const CultureCell& cell) const;
    float radius(const CultureCell& cell) const;
    // Núcleo de encuentro [µm³/s] entre un cazador y una presa.
    bool fits(const CultureCell& predator, const CultureCell& prey) const;
    double encounterKernel(const CultureCell& predator, const CultureCell& prey) const;
    // Tasa de encuentros con presas capturables [1/s] (sin manipulación ni saciedad).
    double preyEncounterRate(const CultureCell& predator) const;
    std::uint64_t removeBacteria(std::uint64_t count, double& pmol);

    CultureConfig config_;
    MediumParams medium_;
    MetabolismParams metabolismParams_;
    FeedingParams feedingParams_;
    BacteriaParams bacteriaParams_;
    LocomotionParams locomotionParams_;
    double volumeUm3_ = 0.0, pmolPerMicroMolar_ = 1.0;
    double amount_[kSoluteCount] = {};
    double detritus_ = 0.0, frustules_ = 0.0; // [pmol] y [pmol Si] (sílice biogénica sin disolver)
    double ph_ = 7.0, light_ = 1.0, phages_ = 0.0;
    float q10_ = 1.0f, viscositySpeed_ = 1.0f, o2Saturation_ = 0.0f, co2Saturation_ = 0.0f, airExchange_ = 0.0f;
    double time_ = 0.0;
    std::uint64_t step_ = 0;
    std::uint32_t nextId_ = 1;
    std::uint64_t inoculations_ = 0, phageEvents_ = 0;
    Program program_;
    double dilutionPerHour_ = 0.0;
    double fresh_[kSoluteCount] = {};
    std::vector<double> reagents_; // [µg/mL] por índice del registro de reactivos
    bool freshSet_[kSoluteCount] = {};
    std::uint64_t washouts_ = 0;
    std::vector<CultureCell> cells_;
    std::vector<BacterialCohort> bacteria_;
    FluxLedger ledger_;
    std::map<std::pair<SpeciesId, std::string>, int> deathTally_;
    // Del paso (se reusan).
    std::vector<std::uint64_t> eatenBacteria_;
    std::vector<std::pair<std::size_t, const char*>> dying_;
    std::vector<std::size_t> splits_;
    std::vector<std::uint8_t> eatenCell_;
};
