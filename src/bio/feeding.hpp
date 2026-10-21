#pragma once

#include "core/vec2.hpp"
#include "sim/body_particles.hpp"

#include <cstdint>
#include <vector>

struct RingBody;

// Vacuola digestiva (fagosoma): un organelo que contiene presas ingeridas.
struct FoodVacuole {
    static constexpr std::uint32_t kNoSlot = ~0u; // célula sin cuerpo (cultivo bien mezclado)
    std::uint32_t slot = kNoSlot;
    float contentArea = 0.0f;   // área 2D del contenido [µm²] (define el tamaño)
    float contentPmol = 0.0f;   // alimento [pmol equivalentes de glucosa]
    float initialPmol = 0.0f;
    float phosphorus = 0.0f;    // [pmol P] de presas fotótrofas: se excreta al digerirse
    float nitrogen = 0.0f;      // [pmol N] ídem (se excreta como amonio)
    // Rinde por pmol digerido. Presas chicas (bacterias, detritos) se comen a ritmo MECÁNICO
    // (tiempo real) y rinden ×bioTimeScale; las células enteras las limita la digestión, que
    // ya corre en tiempo biológico, y rinden ×1.
    float nutritionScale = 1.0f;
    bool wholeCell = false;
    std::uint8_t r = 150, g = 130, b = 120;
    bool open = true;           // la vacuola en formación sigue recibiendo presas
    float age = 0.0f;
    int items = 0;
};

// Componente: vacuolas y estadísticas de alimentación.
struct Digestion {
    std::vector<FoodVacuole> vacuoles;
    int bacteriaEaten = 0, detritusEaten = 0, cellsEaten = 0;
    float pmolIngested = 0.0f, pmolDigested = 0.0f;
    float mouthBudget = 0.0f; // presas que todavía puede tragar en este paso (boca o contacto)
    float rateScale = 1.0f;   // lisosomas: multiplicador genético de la digestión
    float handling = 0.0f;    // tiempo de manipulación que queda tras una presa entera [s biológicos]
    float toxin = 0.0f;       // violaceína ingerida (se detoxifica de a poco)
};

struct FeedingParams {
    float digestionRate = 1.0f / 600.0f; // Paramecium: vacuolas de ~5–30 min [1/s biológico]
    float wholeCellDigestionHours = 3.0f; // Didinium digiere un paramecio en ~2–4 h
    float mouthRadius = 6.0f;            // alcance del citostoma para células y detritos [µm]
    // Radio de captura de bacterias del citostoma [µm], horizontal y vertical: la corriente oral arrastra
    // el agua de alrededor del cuerpo hacia el surco oral. CALIBRADO para que la placa coma lo que dice
    // la respuesta funcional de P. caudatum (aclaramiento ~24 µL/h con pocas presas, [physiology.<id>.*];
    // `microcosm_bench --respuesta-funcional paramecium`, tests/test_culture.cpp).
    float bacteriaCaptureRadius = 75.0f;
    float ciliateMaxParticle = 5.0f;     // radio máximo que entra por la boca del ciliado [µm]
    float flagellateMaxParticle = 3.0f;
    float contactRate = 1.0f;            // ameba: ingestión por contacto [1/s]
    float contactMaxRate = 10.0f;        // ameba: fagocitosis simultánea máxima [presas/s]
    float contactGap = 4.0f;             // alcance de la adhesión fuera de la membrana [µm]
    float wrapReach = 15.0f;             // la membrana a esta distancia de la presa la envuelve [µm]
    int vacuoleCapacity = 150;           // bacterias por vacuola (una de 5–10 µm contiene cientos)
    float mouthMaxRate = 30.0f;          // caudal máximo del citostoma del ciliado [bacterias/s]
    // Euglena es fotótrofa/osmótrofa; los euglénidos fagótrofos (Peranema) comen despacio.
    float flagellateMaxRate = 2.0f;
    float vacuoleGrowRate = 1.0f;        // el fagosoma se infla hasta su tamaño [1/s]
    float vacuoleOpenTime = 6.0f;        // tiempo hasta que la vacuola se cierra [s]
    float lipReach = 1.5f;               // la membrana de la copa dentro de esta distancia del contacto se guía [µm]
    float zipperSpeed = 8.0f;            // la membrana trepa sobre la presa [µm/s]
    float adhesionSpeed = 10.0f;         // adhesión mutua membrana–presa (tope) [µm/s]
    float maxPreyAreaRatio = 0.6f;       // una ameba envuelve presas de hasta 60% de su área
    int minPocket = 10;                  // puntos mínimos de membrana en la copa fagocítica
};

// Presas pequeñas (1 µm³ de E. coli; densidad ≈ agua).
inline constexpr float kBacteriumArea = 2.0f;           // 2 × 1 µm
inline constexpr float kBacteriumPmol = 0.3f / 180.0f;  // pmol eq. glucosa (materia seca)

// Biomasa seca de una célula a partir de su área 2D (esfera de igual sección), y la inversa.
float cellMassPmol(float area);
float cellAreaForMass(float pmol);

// Agrega presa a la vacuola abierta, o crea una nueva en `where` (`phosphorus`: P de la presa). El tamaño de la célula lo
// fija el metabolismo (biomasa + contenido de las vacuolas).
void ingest(Digestion& dig, RingBody& body, BodyParticles& bp, Vec2 where, float area, float pmol,
            std::uint8_t r, std::uint8_t g, std::uint8_t b, bool forceNew, float phosphorus = 0.0f,
            float nutritionScale = 1.0f, float nitrogen = 0.0f);

// Lo que la digestión devuelve al agua [pmol].
struct Excreted {
    float phosphorus = 0.0f;
    float nitrogen = 0.0f;
};

// Saciedad: digiriendo una presa entera, el depredador no ataca otra (tiempo de manipulación).
bool satiated(const Digestion& dig, float biomass);

// Cierra vacuolas llenas o viejas (tiempo real, `dt`), digiere (tiempo biológico, `bioDt`) y
// expulsa las vacías por el citoprocto. Devuelve el alimento digerido [pmol].
// `excreted` suma el fósforo liberado al agua [pmol P].
float digest(Digestion& dig, RingBody& body, BodyParticles& bp, const FeedingParams& params,
             float dt, float bioDt, Excreted& excreted);

float vacuoleContentArea(const Digestion& dig);

// La cinética de la digestión, sin el cuerpo: la usan `digest` (placa) y el cultivo bien mezclado
// (fase P1), que no tiene partículas. Fracción del contenido que se digiere en `bioDt`: presas
// chicas (bacterias, detritos) y células enteras.
struct DigestionFractions {
    float small = 0.0f, whole = 0.0f;
};
DigestionFractions digestionFractions(const Digestion& dig, const FeedingParams& params, float bioDt);
// La vacuola abierta se cierra cuando está llena o tiene `vacuoleOpenTime` (tiempo real).
bool vacuoleCloses(const FoodVacuole& v, const FeedingParams& params);
// Digiere una vacuola cerrada: suma a `dig` y `excreted` y devuelve lo que rinde [pmol].
float digestVacuole(FoodVacuole& v, const DigestionFractions& f, Digestion& dig, Excreted& excreted);
// Casi vacía: se expulsa por el citoprocto (lo que quedaba de P y N sale al agua).
bool vacuoleSpent(const FoodVacuole& v);
void expelVacuole(const FoodVacuole& v, Excreted& excreted);
// Agrega una presa al contenido de la vacuola.
// `items`: cuántas presas son (la vacuola se cierra al llenarse).
void addToVacuole(FoodVacuole& v, Digestion& dig, float area, float pmol, float phosphorus, float nitrogen,
                  int items = 1);

// Sin cuerpo (cultivo): la presa va a la vacuola abierta o a una nueva; `digestContents` cierra,
// digiere y expulsa igual que `digest`.
void ingestContents(Digestion& dig, float area, float pmol, bool forceNew, float phosphorus = 0.0f,
                    float nutritionScale = 1.0f, float nitrogen = 0.0f, int items = 1);
float digestContents(Digestion& dig, const FeedingParams& params, float dt, float bioDt, Excreted& excreted);
