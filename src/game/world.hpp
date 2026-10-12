#pragma once

#include "bio/bacteria.hpp"
#include "bio/cell.hpp"
#include "bio/division.hpp"
#include "bio/feeding.hpp"
#include "bio/locomotion.hpp"
#include "bio/metabolism.hpp"
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

#include <entt/entity/registry.hpp>

#include <cstdint>
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
    float nutrientUptake = 0.02f;     // consumo por microbios no simulados [1/s]
    float detritusDecay = 0.002f;     // descomposición de detritos [1/s]
    float detritusSpawnRate = 0.1f;   // detritos que llegan al campo de visión [1/s]
    float surfaceLight = 1.0f;        // irradiancia relativa de la lámpara
    float inkLifetime = 120.0f;       // [s] la tinta es una herramienta de debug
};

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
    // Jugador.
    entt::entity spawnPlayer(LocomotionType type, Vec2 center);
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
    void applyPlayerMotion(float dt);
    void updatePlayerFeeding(float dt);
    std::vector<const RingBody*> preyFor(const RingBody& predator) const;
    void engulf(const struct Closure& closure, entt::entity prey);
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
    void exchangeO2(Vec2 at, float pmol);
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
    ChemField oxygen_, nutrients_;
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
    std::vector<std::size_t> eatenBacteria_, eatenAmbient_;

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
