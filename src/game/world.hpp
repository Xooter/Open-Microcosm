#pragma once

#include "ai/autopilot.hpp"
#include "bio/bacteria.hpp"
#include "bio/cell.hpp"
#include "bio/cyanobacteria.hpp"
#include "bio/division.hpp"
#include "bio/feeding.hpp"
#include "bio/fungi.hpp"
#include "bio/plasmodium.hpp"
#include "bio/genome.hpp"
#include "bio/organelles.hpp"
#include "bio/locomotion.hpp"
#include "bio/metabolism.hpp"
#include "bio/phagocytosis.hpp"
#include "bio/species.hpp"
#include "game/inspect.hpp"
#include "game/lineage.hpp"
#include "game/mass_budget.hpp"
#include "game/sandbox.hpp"
#include "core/rng.hpp"
#include "core/units.hpp"
#include "core/vec2.hpp"
#include "sim/chem_field.hpp"
#include "sim/dish.hpp"
#include "sim/fft.hpp"
#include "sim/fluid.hpp"
#include "sim/body_particles.hpp"
#include "sim/contacts.hpp"
#include "sim/particles.hpp"
#include "sim/soft_body.hpp"
#include "sim/spatial_grid.hpp"
#include "sim/substep_solver.hpp"

#include <entt/entity/registry.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>


struct MediumParams {
    float o2AirExchange = 0.05f;      // intercambio con el aire por la superficie [1/s]
    float nutrientUptake = 0.0f;      // consumo extra por microbios no simulados [1/s]
    float detritusDecay = 0.0002f;    // descomposición (biológica) de detritos [1/s biológico]
    float detritusSpawnRate = 0.5f;   // detritos que llegan de afuera (hojas, nieve marina) [1/s]
    float surfaceLight = 1.0f;        // irradiancia relativa de la lámpara
    float inkLifetime = 120.0f;       // [s] la tinta es una herramienta de debug
    // Fosfato disuelto inicial [µM]: la gota es un microcosmos enriquecido, con densidades de
    // cultivo (medio Chu 10: ~57 µM; Bold: 1,7 mM). Limita a las algas cuando florecen.
    float phosphate = 100.0f;
    // Nitrógeno inorgánico disuelto [µM]: con N:P = 10 (< 16) el agua termina limitada por N,
    // lo que favorece a las cianobacterias fijadoras (Smith 1983).
    float nitrogen = 1000.0f;
    // Sílice disuelta (ácido silícico) [µM]: las diatomeas la necesitan para cada valva nueva.
    // Aguas dulces 50–300 µM (medio WC/Chu: ~200-300); el mar abierto, 1–10 µM.
    float silicate = 300.0f;
    // Hierro disuelto biodisponible [µM]: en la naturaleza 0,01–1 µM (queda quelado); el medio
    // de cultivo lo da con EDTA, ~1–10 µM. Limita a los fotótrofos en el mar abierto.
    float iron = 1.0f;
    // Vitamina B₁₂ disuelta [µM]: en la naturaleza ~1–10 pM; en cultivo ~100 pM–2 nM. Solo la
    // sintetizan procariotas: las bacterias de la gota la liberan (por hora biológica y por
    // cada 50.000 bacterias) y la luz la degrada.
    float vitamin = 2e-3f;
    float vitaminSourcePerHour = 2e-5f;
    float vitaminDecayPerHour = 0.02f;
    // Disolución de las frústulas muertas (la sílice biogénica es opal, apenas soluble): ~0,2%/h.
    float frustuleDissolutionPerHour = 0.002f;
    float alkalinity = 1000.0f;       // bicarbonato [µM]: fija el pH con el CO₂ (pKa 6,35)
    float phage = 2e-7f;              // fagos libres iniciales [viriones/µm³] (2·10⁵/mL)
    float phageDecayPerHour = 0.05f;  // inactivación (UV, adsorción a partículas)
    float toxinDecayPerHour = 0.02f;  // degradación bacteriana de la microcistina
    float ahlHalfLifeHours = 24.0f;   // a pH 7; a pH alto la lactona se hidroliza más rápido
    // Ambiente de la gota (fase 11).
    float temperature = 20.0f;        // [°C]: Q10 ≈ 2 en las tasas biológicas; cambia la viscosidad
    float salinity = 0.5f;            // [PSU] agua dulce < 1; mar ~35; salmuera del hielo hasta ~100
    bool anoxic = false;              // sin aire (intestino de termita): el O₂ no se repone
    // Aporte de materia orgánica disuelta desde afuera de la gota (hojas y barro que se
    // descomponen, como en una infusión de heno) [µM por hora biológica, en promedio].
    float organicInflow = 0.0f;
    // Polvo que cae del aire a la película de agua (cuarzo, arcilla, mica) [partículas/s, tiempo
    // real: es física, no biología].
    float dustRate = 0.0f;
};

// Sedimento inicial de la gota: partículas minerales sueltas y granos de arena.
struct Sediment {
    int clay = 0, quartz = 0, mica = 0, ironOxide = 0, sulfur = 0, frustules = 0;
    int rocks = 0;                              // granos de arena (obstáculos fijos)
    float rockMin = 50.0f, rockMax = 120.0f;    // radio de los granos [µm] (arena fina: 60–250 µm de diámetro)
};

// Componente: grano de arena o piedra. Es un cuerpo con membrana inamovible (LOD "fijo": sin
// XPBD ni arrastre), así los contactos, la exclusión de bacterias y la GPU lo tratan como a un
// obstáculo. No es un organismo: no tiene metabolismo, no se come ni cuenta como biomasa.
struct Rock {
    MineralKind kind = MineralKind::Quartz;
    std::uint32_t seed = 0; // detalles del render (fracturas, inclusiones)
};

const char* mineralName(MineralKind kind);

struct WorldConfig {
    int gridX = 256, gridY = 256; // potencias de 2 (FFT)
    float cellSize = 8.0f;        // [µm] → grilla de 2048 × 2048 µm
    // Placa circular centrada en la grilla: radio = mitad del lado menor − margen (el margen
    // de vidrio aísla la placa de su imagen periódica en la FFT). false = mundo toroidal.
    bool dish = true;
    float dishMargin = 80.0f; // [µm]
    std::size_t bacteria = 50'000;
    std::size_t detritus = 60;
    int cyanoFilaments = 15;      // filamentos de Anabaena al empezar
    int cyanoCells = 12;
    int oscillatoriaFilaments = 0; // tricomas de Oscillatoria al empezar
    int oscillatoriaCells = 20;
    std::uint64_t seed = 1;
    MediumParams medium;          // química y ambiente iniciales (bioma)
    // Hongos al empezar: micelios de Saprolegnia y grupos de zoosporas de quitridios (10 cada uno).
    int saprolegnia = 4, chytridClusters = 6;
    // Más hongos: Allomyces, hongos liquenizantes (junto a algas verdes) y plasmodios de Physarum.
    int allomyces = 0, lichens = 0, physarum = 0;
    // Cianobacterias del bioma: rango térmico (Anabaena 10–35 °C; Mastigocladus 30–57 °C).
    float cyanoTMin = 10.0f, cyanoTMax = 35.0f;
    float patchiness = 0.0f;      // manchas iniciales de materia orgánica y detritos (0–1)
    Sediment sediment;            // minerales y granos de arena iniciales
    // Multiplicador de la población inicial de cada especie (0 = ausente en esta gota).
    float speciesWeight[kSpeciesCount];
    WorldConfig() {
        for (float& w : speciesWeight)
            w = 1.0f;
        speciesWeight[static_cast<int>(SpeciesId::Trichonympha)] = 0.0f; // solo en termitas
    }
};

// Comportamientos e interacciones vistos en la partida (enciclopedia).
struct Observations {
    int chytridInfections = 0;
    int chytridReleases = 0;
    int buds = 0;
    int colonyReleases = 0;
    int filamentDivisions = 0;
    int phageLyses = 0;
    float nitrogenFixed = 0.0f; // [pmol N]
    int bdelloBursts = 0;
    int quorumSwitches = 0;
    int toxinKills = 0;
    int zoospores = 0, zoosporesSettled = 0;
    int restingFormed = 0, restingGerminated = 0; // Allomyces
    int lichenLinks = 0;                          // algas unidas a un hongo liquenizante
    int physarumFusions = 0, sclerotiaFormed = 0, sclerotiaGerminated = 0;
};

// Una muerte reciente de un individuo (la ficha de inspección dice de qué murió). `successor`:
// el ser que ocupó su lugar (la zoóspora que salió de la célula, la cría que salió del huevo).
struct DeathRecord {
    entt::entity entity = entt::null;
    entt::entity successor = entt::null;
    SpeciesId species = SpeciesId::Paramecium;
    std::string cause;
    Vec2 at;
    float time = 0.0f; // [s] de simulación
};

// Una célula que acaba de reventar (para el efecto visual de la lisis).
struct LysisMark {
    Vec2 at;
    float radius;
    float time; // [s] de simulación
    std::uint8_t r, g, b;
};

// Componente: conjugación en curso (Paramecium). Dura unas horas biológicas.
struct Conjugating {
    entt::entity partner;
    float progress = 0.0f;
};
inline constexpr float kConjugationHours = 2.0f;

// Componente: tricocistos recién disparados (el depredador retrocede un rato).
struct Discharge {
    float timer; // [s]
};

CellSpec speciesSpec(LocomotionType type);

// Controles del jugador en el frame actual.
struct PlayerInput {
    float forward = 0.0f; // cilios: W/S
    float turn = 0.0f;    // cilios: A/D
    bool propel = false;  // flagelo: batir
    bool pseudopod = false;
    Vec2 aim;             // posición del mouse en el mundo [µm]
};

// Placa circular (sobre grillas periódicas por dentro) vista desde arriba (vista cenital del microscopio). Coordina el medio
// (agua y química), las partículas ambientales, las bacterias y las células (cuerpos blandos).
class World {
  public:
    explicit World(const WorldConfig& config);

    void step(float dt);

    // Herramientas de desarrollo.
    void setStir(Vec2 position, Vec2 velocity, float radius);
    void clearStir() { stirActive_ = false; }
    void dropInk(Vec2 position, float radius, int count);
    void dropDetritus(Vec2 position);

    entt::entity spawnCell(Vec2 center); // célula genérica aleatoria
    entt::entity spawnCell(const CellSpec& spec);
    void spawnTestScene(int count);      // grilla de células para pruebas de estrés
    void removeCellAt(Vec2 position);
    // Compresión: empuja todos los cuerpos hacia un punto [µm/s].
    void setSqueeze(Vec2 center, float speed);
    void clearSqueeze() { squeezeActive_ = false; }
    // Jugador y linaje.
    entt::entity spawnPlayer(LocomotionType type, Vec2 center, const Genome* genome = nullptr);
    // Organismo de una especie con un genoma dado (del linaje actual, sin control).
    entt::entity spawnOrganism(LocomotionType type, Vec2 center, const Genome& genome);
    entt::entity spawnOrganism(SpeciesId species, Vec2 center, const Genome& genome, std::uint32_t run,
                               float angle = 0.0f);
    // Ecosistema silvestre: las especies con sus poblaciones iniciales (× scale).
    void populateEcosystem(float scale = 1.0f);
    // Filamento de `cells` células unidas extremo con extremo (Spirogyra).
    std::vector<entt::entity> spawnFilament(SpeciesId species, Vec2 start, float angle, int cells, std::uint32_t run);

    // Uniones de adhesión entre membranas de cuerpos distintos (filamentos, colonias).
    struct AdhesionLink {
        std::uint32_t a, b;           // slots de membrana
        entt::entity ownerA, ownerB;  // células dueñas
        float rest;                   // [µm]
    };
    const std::vector<AdhesionLink>& links() const { return links_; }
    // Conjugación: busca una pareja compatible cerca (ciliado de otro tipo de apareamiento).
    entt::entity conjugationPartner(entt::entity cell) const;
    bool tryConjugate(entt::entity cell);
    // El control pasa a otra célula (del linaje): la anterior queda con piloto automático.
    void setPlayer(entt::entity cell);
    void setRun(std::uint32_t run) { run_ = run; }
    std::uint32_t run() const { return run_; }
    std::vector<entt::entity> lineageMembers() const;
    Census census() const;
    std::vector<WorldEvent> takeEvents() { return std::exchange(events_, {}); }
    // Puebla la gota con células pasivas (competidoras por la biomasa, hasta la fase 7).
    void populate(int cells);
    // Sandbox: el jugador pasa a ser otra especie en el mismo lugar (mismo linaje y generación).
    // Sin jugador, aparece en `fallback`.
    entt::entity setPlayerSpecies(SpeciesId species, Vec2 fallback);
    // Sandbox: spawnea un ser del catálogo en `at` (silvestre, linaje 0).
    void spawnBeing(Being being, Vec2 at);
    // Sandbox: completa biomasa y reservas hasta el umbral de la especie para que se reproduzca
    // por su vía normal (fisión, palintomía, colonias, gemación o huevo). false si no puede.
    bool forceReproduction(entt::entity cell);
    // Fijar la muestra (lo que hacen en la realidad Gram, Lugol o la fijación previa a DAPI: calor,
    // yodo o alcohol desnaturalizan las proteínas). Todo muere sin lisarse y queda como estaba: no
    // nada, no metaboliza, no se divide ni se descompone. Es irreversible; una placa nueva vuelve a
    // empezar. Desde entonces `step` no avanza.
    void fix() { fixed_ = true; }
    bool fixed() const { return fixed_; }
    // Inspección: qué hay en `position` (a `tolerance` µm de su borde). Lo más grande y lo más
    // cercano tienen prioridad: células, granos y después partículas. `skip` (bits de `pickBit`)
    // excluye tipos: lo que la vista muestra solo como densidad no se puede elegir con el mouse.
    Picked pick(Vec2 position, float tolerance, std::uint32_t skip = 0) const;
    // Minerales: una partícula suelta, un grano de arena (false si se superpone con algo) o
    // `count` partículas de sedimento mezclado alrededor de `at`.
    void spawnMineral(MineralKind kind, Vec2 at, Rng& rng);
    bool spawnRock(Vec2 at, float radius, MineralKind kind, Rng& rng);
    void spawnSediment(Vec2 at, int count, Rng& rng);
    std::size_t rockCount() const { return registry_.view<Rock>().size(); }
    // Organismo silvestre (linaje 0) con variación genética. `randomStage`: en una etapa al
    // azar del ciclo celular (población inicial).
    entt::entity spawnWild(SpeciesId species, Vec2 at, Rng& rng, bool randomStage);
    // Micelio (o, para quitridios, un racimo de zoosporas) con el fósforo sacado del agua.
    void spawnFungus(FungusKind kind, Vec2 at, float heading, std::uint64_t salt);
    // Plasmodio de Physarum (el fósforo sale del agua) y copo de avena (cebo).
    void spawnPlasmodium(Vec2 at, float heading);
    void spawnOatFlake(Vec2 at);
    entt::entity player() const { return player_; }
    void setPlayerInput(const PlayerInput& input) { playerInput_ = input; }
    LocomotionParams& locomotionParams() { return locomotionParams_; }
    const LocomotionParams& locomotionParams() const { return locomotionParams_; }
    MetabolismParams& metabolismParams() { return metabolismParams_; }
    const MetabolismParams& metabolismParams() const { return metabolismParams_; }
    // Compresión del tiempo biológico (por defecto units::kBioTimeScale; ajustable para debug).
    float& bioTimeScale() { return bioTimeScale_; }
    float bioTimeScale() const { return bioTimeScale_; }
    // Tiempo biológico por segundo real, con la temperatura (Q10 = 2 respecto de 20 °C).
    float bioScale() const { return bioTimeScale_ * temperatureFactor_; }
    // O₂ en equilibrio con el aire a la temperatura de la gota [µM] (0 si es anóxica).
    float oxygenSaturation() const { return o2Saturation_; }
    const char* lastDeathCause() const { return lastDeathCause_; }
    // Muertes por especie y causa desde el comienzo (enciclopedia, balance).
    const std::map<std::pair<SpeciesId, std::string>, int>& deathTally() const { return deathTally_; }
    // Las últimas muertes de individuos (las más viejas se olvidan). nullptr si no está.
    const DeathRecord* deathOf(entt::entity e) const;
    // ¿Está saciada (manipulando una presa entera o con el citoplasma lleno)?
    bool isSatiated(entt::entity e) const;
    const MediumParams& medium() const { return medium_; }
    // Debug: mata (lisa) a la célula bajo el cursor.
    void lyseAt(Vec2 position);
    FeedingParams& feedingParams() { return feedingParams_; }

    // Agarrar y estirar un punto de membrana con el mouse.
    bool beginGrab(Vec2 position);
    void moveGrab(Vec2 position) { grabTarget_ = position; }
    void endGrab() { grabSlot_ = kNoGrab; }

    float width() const { return width_; }
    float height() const { return height_; }
    // Placa: el límite del mundo (pared de vidrio).
    const Dish& dish() const { return dish_; }
    const DishMask& dishMask() const { return dishMask_; }
    // Punto al azar en el agua de la placa, a `margin` de la pared.
    Vec2 randomPoint(Rng& rng, float margin = 0.0f) const;
    float simTime() const { return simTime_; }

    Fluid& fluid() { return fluid_; }
    const Fluid& fluid() const { return fluid_; }
    const ChemField& oxygen() const { return oxygen_; }
    ChemField& oxygen() { return oxygen_; }
    const ChemField& nutrients() const { return nutrients_; }
    ChemField& nutrients() { return nutrients_; }
    const ChemField& phosphate() const { return phosphate_; }
    const ChemField& nitrogen() const { return nitrogen_; }
    const ChemField& silicate() const { return silicate_; }
    const ChemField& iron() const { return iron_; }
    const ChemField& vitamin() const { return vitamin_; }
    // Totales conservados [pmol] (agua + células + frústulas): lo disuelto en la sílice y el hierro.
    double totalSilicon() const;
    double totalIron() const;
    // Agrega a un campo disuelto una dosis [µM sobre toda la placa] (herramienta del laboratorio).
    // Lo agregado cuenta como entrada externa del balance de masa.
    void dose(ChemField& field, Vec2 at, float radius, float micromolar);
    // Lleva la concentración media de un soluto a `target` [µM] (ajuste en vivo del panel Placa),
    // conservando su distribución. La diferencia cuenta como entrada externa.
    void setMeanConcentration(ChemField& field, float target);
    // Media de un campo en el agua de la placa [µM].
    float meanConcentration(const ChemField& field) const;
    // Balance de masa (fase 13): existencias por compartimento y flujos acumulados. Recorre todo
    // el mundo: llamarlo a lo sumo una vez por segundo.
    MassBudget massBudget() const;
    const FluxLedger& ledger() const { return ledger_; }
    ChemField& silicateField() { return silicate_; }
    ChemField& ironField() { return iron_; }
    ChemField& vitaminField() { return vitamin_; }
    ChemField& phosphateField() { return phosphate_; }
    ChemField& nitrogenField() { return nitrogen_; }
    const ChemField& co2() const { return co2_; }
    const ChemField& toxin() const { return toxin_; }
    const ChemField& autoinducer() const { return autoinducer_; }
    const ChemField& phage() const { return phage_; }
    const Grid2D& ph() const { return ph_; }
    const Bdellovibrios& bdellovibrios() const { return bdello_; }
    // Nitrógeno total [pmol N] (se conserva salvo la fijación de N₂, que lo agrega).
    double totalNitrogen() const;
    // Fósforo total [pmol P]: agua + fotótrofas + vacuolas (se conserva).
    double totalPhosphorus() const;
    const Grid2D& light() const { return light_; }
    const AmbientParticles& ambient() const { return ambient_; }
    const Cyanobacteria& cyanobacteria() const { return cyano_; }
    const PlasmodiumSystem& plasmodia() const { return plasmo_; }
    PlasmodiumSystem& plasmodia() { return plasmo_; }
    const Grid2D& slime() const { return slime_; }
    const Fungi& fungi() const { return fungi_; }
    Fungi& fungi() { return fungi_; }
    const Observations& observations() const { return observed_; }
    const std::vector<LysisMark>& lysisMarks() const { return lysisMarks_; }
    // Hongos iniciales (micelios y zoosporas de quitridios).
    void seedFungi();
    void seedPlasmodia();
    Bacteria& bacteria() { return bacteria_; }
    const Bacteria& bacteria() const { return bacteria_; }
    MediumParams& medium() { return medium_; }
    // Recalcula lo que depende de la temperatura, la anoxia y la salinidad (Q10, viscosidad, O₂ de
    // saturación) después de cambiar `medium()` a mano, y lo aplica a las células vivas.
    void refreshMedium();
    SoftBodyTuning& softTuning() { return softTuning_; }
    const entt::registry& registry() const { return registry_; }
    entt::registry& registry() { return registry_; }
    const BodyParticles& bodies() const { return bodies_; }
    std::size_t cellCount() const { return registry_.view<RingBody>().size(); }

    // LOD (fase 9): las células cerca del foco (lo que muestra la cámara) usan todos los
    // sub-pasos de XPBD; las lejanas, uno de cada `lodStride` con un paso más largo (las tasas
    // [1/s] no cambian de significado), así cuestan una fracción. Sin foco, todo es detallado.
    void setFocus(Vec2 center, Vec2 halfExtent) {
        focusActive_ = true;
        focusCenter_ = center;
        focusHalf_ = halfExtent;
    }
    void clearFocus() { focusActive_ = false; }
    int lodStride = 5;
    // Fase 14: resolvedor alternativo del bucle de XPBD (GPU). nullptr = CPU. No es dueño.
    void setSubstepSolver(SubstepSolver* solver) { substepSolver_ = solver; }
    const SubstepSolver* substepSolver() const { return substepSolver_; }
    std::size_t nearBodies() const { return nearCount_; }
    std::size_t touchingBodies() const {
        std::size_t n = 0;
        for (std::uint8_t t : bodyTouch_)
            n += t;
        return n;
    }

    // Hash del estado completo, para tests de determinismo.
    std::uint64_t stateHash() const;

  private:
    void applyStir();
    void scatterPatches();
    void updateChemistrySources(float dt);
    void stepAmbient(float dt);
    void computeLight();
    void spawnDetritus(float dt);
    void scatterSediment();
    void spawnDust(float dt);
    void rebuildFlowMask();
    entt::entity rockAt(Vec2 position) const;
    void addDetritus(Rng& rng, Vec2 position);
    void collectBodies();
    void classifyLod();
    void buildPerceptionGrid();
    void markTouching(float contactRange, float dt);
    // Contacto a nivel de velocidades (antes de los sub-pasos, igual para CPU y GPU): quita de
    // cada par de células la velocidad de acercamiento que las haría penetrar y la reparte
    // según su movilidad (Stokes: v = F/ζ, con ζ ∝ N/invMass). Un nadador que empuja a otra
    // célula avanza con ella, más despacio; no le clava una cuña ni la atraviesa.
    void limitApproach(float dt, float contactRange);
    void beginBodies(float dt);
    void cpuSubsteps(float dt, int stride, float maxRadius);
    void finishBodies();
    void applyMotion(float dt);
    void updateCup(entt::entity e, RingBody& body, float dt);
    void updateFeeding(float dt);
    Perception perceive(entt::entity e, const RingBody& body, SpeciesId species) const;
    bool repelledByTrichocysts(entt::entity predator, entt::entity prey);
    void hunt(entt::entity predator, const RingBody& body);
    void swallow(entt::entity predator, entt::entity prey);
    void applyAnchor(entt::entity e, RingBody& body, Anchored& anchor, bool disturbed, float dt);
    void anchorHere(entt::entity e);
    void trapWithTentacles(entt::entity e, const RingBody& body);
    void strikeWithNeck(entt::entity e, const RingBody& body, float dt);
    void linkCells(entt::entity a, entt::entity b);
    void solveLinks(float h);
    void relinkAfterDivision(entt::entity mother, entt::entity child);
    void maintainLinks(float bioDt);
    void releaseColonies(entt::entity mother);
    void releaseZoospore(entt::entity mother);
    void settleZoospore(entt::entity e);
    bool smallPrey(const RingBody& prey) const;
    void forBacteriaInBox(Vec2 lo, Vec2 hi, const std::function<void(std::size_t)>& fn) const;
    std::vector<const RingBody*> preyFor(const RingBody& predator) const;
    void engulf(entt::entity predator, const Closure& closure, entt::entity prey);
    Vec2 imageShift(const RingBody& of, Vec2 near) const;
    void feedAtMouth(entt::entity e, RingBody& body, const Locomotion& loco, Digestion& dig, float dt);
    void feedByContact(RingBody& body, Digestion& dig, float dt);
    void removeBacteria(std::vector<std::size_t>& indices);
    void removeAmbient(std::vector<std::size_t>& indices);
    void updateLifecycle(float dt);
    void applyDivisionForces();
    void duplicateOrganelles(entt::entity cell);
    bool septumTips(const RingBody& body, std::size_t& tipA, std::size_t& tipB) const;
    void divide(entt::entity cell, std::size_t tipA, std::size_t tipB);
    void lyse(entt::entity cell, const char* cause);
    void recordDeath(entt::entity cell, const std::string& cause);
    void noteDeath(entt::entity e, SpeciesId species, Vec2 at, const std::string& cause,
                   entt::entity successor = entt::null);
    void applyTraits(entt::entity cell);
    void pullTogether(RingBody& body, const RingBody& partner);
    void holdAt(RingBody& body, Vec2 point);
    void finishConjugation(entt::entity a, entt::entity b);
    void tickDefenses(float dt);
    void exchangeO2(Vec2 at, float pmol);
    // Suma (o quita, hasta lo disponible) `pmol` en las 3×3 celdas alrededor de `at`.
    // Devuelve lo que realmente se intercambió.
    // `flow` rotula el proceso en el balance de masa.
    float exchange(ChemField& field, Vec2 at, float pmol, Flow flow = Flow::Other);
    float exchangeRaw(ChemField& field, Vec2 at, float pmol); // sin registro
    // Elemento que lleva un campo y pmol del elemento por pmol del campo (la materia orgánica
    // es glucosa: 6 C). false si el campo no entra en el balance.
    bool fieldElement(const ChemField& field, Element& element, float& perPmol) const;
    void record(const ChemField& field, Flow flow, double pmol);
    void growBacteria(float dt);
    void stepCyanobacteria(float dt);
    void updateWaterChemistry(float dt);
    void computePH();
    void exchangeCO2(Vec2 at, float pmol);
    float carriedNitrogen(entt::entity e) const;
    const char* applyEnvironmentStress(entt::entity e, const RingBody& body, const SpeciesInfo& info,
                                       MetabolicInput& in, float bioDt);
    void microbialInteractions(float dt);
    void seedBdellovibrio(std::size_t count);
    // Micronutrientes (Si, Fe, B₁₂) de una célula: tomar su cuota inicial del agua, crecer con
    // más biomasa y devolverla al morir o ser comida.
    void seedMicronutrients(Metabolism& m, const SpeciesInfo& info, Vec2 at);
    void growMicronutrients(Metabolism& m, Vec2 at, float factor);
    float releaseMicronutrients(entt::entity e, Vec2 at); // devuelve el Si (para la frústula)
    void stepFungi(float dt);
    void stepLichens(float bioDt);
    void stepPlasmodium(float dt);
    void updateInfections(float bioDt);
    void releaseChytridSpores(entt::entity host);
    Vec2 wrapped(Vec2 at) const;
    float microMolarPerPmol() const;
    void excludeFromCells();
    void rebuildBuckets();
    void resolvePenetrations();
    void pushOutOfCells(float& x, float& y) const;
    entt::entity cellAt(Vec2 position) const;

    WorldConfig config_;
    MediumParams medium_;
    float width_, height_;
    Dish dish_;
    DishMask dishMask_;

    Fft2D fft_;
    Fluid fluid_;
    ChemField oxygen_, nutrients_, phosphate_, nitrogen_, silicate_, iron_, vitamin_, co2_, toxin_, autoinducer_, phage_;
    Grid2D light_, shading_;
    AmbientParticles ambient_;
    Bacteria bacteria_;
    Cyanobacteria cyano_;
    std::vector<float> cyanoDemand_;
    Fungi fungi_;
    PlasmodiumSystem plasmo_;
    Grid2D slime_; // rastro de baba del plasmodio (Reid et al. 2012): las puntas lo evitan
    std::vector<entt::entity> photoCells_;
    std::vector<Vec2> photoPoints_;
    std::vector<Vec2> sporeSteer_;
    std::vector<entt::entity> hostCells_;
    Grid2D hyphaCover_, ph_, inflowPattern_;
    std::vector<float> flowMask_; // placa × granos de arena: el agua no fluye dentro de la roca
    float dustAccumulator_ = 0.0f;
    Bdellovibrios bdello_;
    SpatialGrid bdelloGrid_;
    std::vector<float> cyanoDemandN_;
    std::vector<CyanoRelease> cyanoReleases_;
    Observations observed_;
    FluxLedger ledger_;
    std::vector<LysisMark> lysisMarks_;

    entt::registry registry_;
    BodyParticles bodies_;
    SoftBodyTuning softTuning_;
    ContactSolver contacts_;
    std::vector<RingBody*> bodyList_;
    std::vector<float> bodyMobility_;         // 1/ζ de cada cuerpo (limitApproach)
    std::vector<std::uint8_t> bodySkip_;
    std::vector<CupObstacle> cupObstacles_; // células junto a la presa de la copa (se reusa el buffer)
    std::vector<std::uint8_t> bodyNear_, memberActive_;
    std::vector<std::uint32_t> memberBody_;
    std::vector<std::uint8_t> bodyTouch_;
    SubstepSolver* substepSolver_ = nullptr;
    bool gpuPending_ = false;
    float pendingDt_ = 0.0f, pendingMaxRadius_ = 0.0f;
    int pendingStride_ = 1;
    std::vector<LinkConstraint> linkConstraints_;
    std::vector<std::vector<entt::entity>> percGrid_;
    int percGridX_ = 1, percGridY_ = 1;
    std::vector<float> bodyReach_;
    bool focusActive_ = false;
    Vec2 focusCenter_, focusHalf_;
    std::size_t nearCount_ = 0;
    std::vector<ContactMember> members_;
    std::vector<Vec2> penetration_;
    // Grilla gruesa de cajas de células para excluir bacterias y detritos del interior.
    float bucketSize_ = 32.0f;
    int bucketsX_ = 0, bucketsY_ = 0;
    std::vector<std::vector<std::uint32_t>> buckets_;

    entt::entity player_ = entt::null;
    PlayerInput playerInput_;
    LocomotionParams locomotionParams_;
    FeedingParams feedingParams_;
    MetabolismParams metabolismParams_;
    float bioTimeScale_ = units::kBioTimeScale;
    float temperatureFactor_ = 1.0f, o2Saturation_ = units::kO2Saturation, viscositySpeed_ = 1.0f;
    const char* lastDeathCause_ = "";
    std::map<std::pair<SpeciesId, std::string>, int> deathTally_;
    std::vector<DeathRecord> recentDeaths_; // las últimas kRecentDeaths
    std::vector<std::pair<entt::entity, const char*>> dying_;
    std::vector<entt::entity> settling_, released_, colonyReleases_, zoosporeReleases_, zoosporeSettling_;
    std::vector<AdhesionLink> links_;
    struct PendingSplit {
        entt::entity cell;
        std::size_t tipA, tipB;
    };
    std::vector<PendingSplit> splits_;
    std::vector<std::pair<entt::entity, entt::entity>> conjugationsDone_;
    std::vector<entt::entity> abandonedConjugations_;
    std::vector<entt::entity> encysting_, excysting_;
    std::vector<std::size_t> eatenBacteria_, eatenAmbient_;
    std::vector<std::uint8_t> takenBacteria_, takenAmbient_;
    std::vector<float> bacterialDemand_;
    SpatialGrid bacteriaGrid_;
    struct EngulfEvent {
        entt::entity predator, prey;
        Closure closure;
    };
    std::vector<EngulfEvent> engulfs_;
    std::vector<std::pair<entt::entity, entt::entity>> swallows_;
    std::unordered_map<entt::entity, Vec2> smallPreyCenter_;
    std::vector<WorldEvent> events_;
    std::uint32_t run_ = 1;

    bool squeezeActive_ = false;
    Vec2 squeezeCenter_;
    float squeezeSpeed_ = 0.0f;
    static constexpr std::uint32_t kNoGrab = ~0u;
    std::uint32_t grabSlot_ = kNoGrab;
    Vec2 grabTarget_;
    // Meta efectiva del agarre: el cursor, pero a lo sumo `kGrabReach` del punto agarrado. Un
    // resorte con fuerza máxima (no un teletransporte): si otra membrana frena a la célula, el
    // punto no puede forzar el paso a través de ella.
    Vec2 grabGoal() const;

    bool stirActive_ = false;
    Vec2 stirPosition_, stirVelocity_;
    float stirRadius_ = 30.0f;

    float slowDiffusion_ = 0.0f;
    std::uint64_t step_ = 0, spawnCounter_ = 0, toolCounter_ = 0, cellCounter_ = 0;
    float spawnAccumulator_ = 0.0f;
    float simTime_ = 0.0f;
    bool fixed_ = false;
};
