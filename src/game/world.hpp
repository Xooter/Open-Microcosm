#pragma once

#include "ai/autopilot.hpp"
#include "bio/bacteria.hpp"
#include "bio/cell.hpp"
#include "bio/division.hpp"
#include "bio/feeding.hpp"
#include "bio/genome.hpp"
#include "bio/organelles.hpp"
#include "bio/locomotion.hpp"
#include "bio/metabolism.hpp"
#include "bio/phagocytosis.hpp"
#include "bio/species.hpp"
#include "game/lineage.hpp"
#include "core/rng.hpp"
#include "core/units.hpp"
#include "core/vec2.hpp"
#include "sim/chem_field.hpp"
#include "sim/fft.hpp"
#include "sim/fluid.hpp"
#include "sim/body_particles.hpp"
#include "sim/contacts.hpp"
#include "sim/particles.hpp"
#include "sim/soft_body.hpp"
#include "sim/spatial_grid.hpp"

#include <entt/entity/registry.hpp>

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

struct WorldConfig {
    int gridX = 256, gridY = 128; // potencias de 2 (FFT)
    float cellSize = 8.0f;        // [µm] → mundo de 2048 × 1024 µm
    std::size_t bacteria = 50'000;
    std::size_t detritus = 60;
    std::uint64_t seed = 1;
};

struct MediumParams {
    float o2AirExchange = 0.05f;      // intercambio con el aire por la superficie [1/s]
    float nutrientUptake = 0.0f;      // consumo extra por microbios no simulados [1/s]
    float detritusDecay = 0.0002f;    // descomposición (biológica) de detritos [1/s biológico]
    float detritusSpawnRate = 0.5f;   // detritos que llegan de afuera (hojas, nieve marina) [1/s]
    float surfaceLight = 1.0f;        // irradiancia relativa de la lámpara
    float inkLifetime = 120.0f;       // [s] la tinta es una herramienta de debug
    float phosphate = 5.0f;           // fosfato disuelto inicial [µM] (limita a las algas)
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

// Mundo toroidal visto desde arriba (vista cenital del microscopio). Coordina el medio
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
    entt::entity spawnOrganism(SpeciesId species, Vec2 center, const Genome& genome, std::uint32_t run);
    // Ecosistema silvestre: las especies con sus poblaciones iniciales (× scale).
    void populateEcosystem(float scale = 1.0f);
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
    void setPlayerType(LocomotionType type); // reemplaza al jugador en el mismo lugar
    entt::entity player() const { return player_; }
    void setPlayerInput(const PlayerInput& input) { playerInput_ = input; }
    LocomotionParams& locomotionParams() { return locomotionParams_; }
    MetabolismParams& metabolismParams() { return metabolismParams_; }
    // Compresión del tiempo biológico (por defecto units::kBioTimeScale; ajustable para debug).
    float& bioTimeScale() { return bioTimeScale_; }
    const char* lastDeathCause() const { return lastDeathCause_; }
    // Debug: mata (lisa) a la célula bajo el cursor.
    void lyseAt(Vec2 position);
    FeedingParams& feedingParams() { return feedingParams_; }

    // Agarrar y estirar un punto de membrana con el mouse.
    bool beginGrab(Vec2 position);
    void moveGrab(Vec2 position) { grabTarget_ = position; }
    void endGrab() { grabSlot_ = kNoGrab; }

    float width() const { return width_; }
    float height() const { return height_; }
    float simTime() const { return simTime_; }

    Fluid& fluid() { return fluid_; }
    const Fluid& fluid() const { return fluid_; }
    const ChemField& oxygen() const { return oxygen_; }
    ChemField& oxygen() { return oxygen_; }
    const ChemField& nutrients() const { return nutrients_; }
    ChemField& nutrients() { return nutrients_; }
    const ChemField& phosphate() const { return phosphate_; }
    // Fósforo total [pmol P]: agua + fotótrofas + vacuolas (se conserva).
    double totalPhosphorus() const;
    const Grid2D& light() const { return light_; }
    const AmbientParticles& ambient() const { return ambient_; }
    Bacteria& bacteria() { return bacteria_; }
    const Bacteria& bacteria() const { return bacteria_; }
    MediumParams& medium() { return medium_; }
    SoftBodyTuning& softTuning() { return softTuning_; }
    const entt::registry& registry() const { return registry_; }
    entt::registry& registry() { return registry_; }
    const BodyParticles& bodies() const { return bodies_; }
    std::size_t cellCount() const { return registry_.view<RingBody>().size(); }

    // Hash del estado completo, para tests de determinismo.
    std::uint64_t stateHash() const;

  private:
    void applyStir();
    void updateChemistrySources(float dt);
    void stepAmbient(float dt);
    void computeLight();
    void spawnDetritus(float dt);
    void addDetritus(Rng& rng, Vec2 position);
    void collectBodies();
    void stepBodies(float dt);
    void applyMotion(float dt);
    void updateCup(entt::entity e, RingBody& body, float dt);
    void updateFeeding(float dt);
    Perception perceive(entt::entity e, const RingBody& body, SpeciesId species) const;
    bool repelledByTrichocysts(entt::entity predator, entt::entity prey);
    void hunt(entt::entity predator, const RingBody& body);
    void swallow(entt::entity predator, entt::entity prey);
    bool smallPrey(const RingBody& prey) const;
    void forBacteriaInBox(Vec2 lo, Vec2 hi, const std::function<void(std::size_t)>& fn) const;
    std::vector<const RingBody*> preyFor(const RingBody& predator) const;
    void engulf(entt::entity predator, const Closure& closure, entt::entity prey);
    Vec2 imageShift(const RingBody& of, Vec2 near) const;
    void feedAtMouth(RingBody& body, const Locomotion& loco, Digestion& dig, float dt);
    void feedByContact(RingBody& body, Digestion& dig, float dt);
    void removeBacteria(std::vector<std::size_t>& indices);
    void removeAmbient(std::vector<std::size_t>& indices);
    void updateLifecycle(float dt);
    void applyDivisionForces();
    void duplicateOrganelles(entt::entity cell);
    void divide(entt::entity cell, std::size_t tipA, std::size_t tipB);
    void lyse(entt::entity cell, const char* cause);
    void applyTraits(entt::entity cell);
    void pullTogether(RingBody& body, const RingBody& partner);
    void finishConjugation(entt::entity a, entt::entity b);
    void tickDefenses(float dt);
    void exchangeO2(Vec2 at, float pmol);
    // Suma (o quita, hasta lo disponible) `pmol` en las 3×3 celdas alrededor de `at`.
    // Devuelve lo que realmente se intercambió.
    float exchange(ChemField& field, Vec2 at, float pmol);
    void growBacteria(float dt);
    float microMolarPerPmol() const;
    void excludeFromCells();
    void rebuildBuckets();
    void resolvePenetrations();
    void pushOutOfCells(float& x, float& y) const;
    entt::entity cellAt(Vec2 position) const;

    WorldConfig config_;
    MediumParams medium_;
    float width_, height_;

    Fft2D fft_;
    Fluid fluid_;
    ChemField oxygen_, nutrients_, phosphate_;
    Grid2D light_, shading_;
    AmbientParticles ambient_;
    Bacteria bacteria_;

    entt::registry registry_;
    BodyParticles bodies_;
    SoftBodyTuning softTuning_;
    ContactSolver contacts_;
    std::vector<RingBody*> bodyList_;
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
    const char* lastDeathCause_ = "";
    std::vector<entt::entity> dying_;
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

    bool stirActive_ = false;
    Vec2 stirPosition_, stirVelocity_;
    float stirRadius_ = 30.0f;

    std::uint64_t step_ = 0, spawnCounter_ = 0, toolCounter_ = 0, cellCounter_ = 0;
    float spawnAccumulator_ = 0.0f;
    float simTime_ = 0.0f;
};
