#pragma once

#include "core/vec2.hpp"
#include "sim/body_particles.hpp"

#include <cstdint>
#include <vector>

struct RingBody;

// Vacuola digestiva (fagosoma): un organelo que contiene presas ingeridas.
struct FoodVacuole {
    std::uint32_t slot;
    float contentArea = 0.0f;   // área 2D del contenido [µm²] (define el tamaño)
    float contentPmol = 0.0f;   // alimento [pmol equivalentes de glucosa]
    float initialPmol = 0.0f;
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
};

struct FeedingParams {
    float digestionRate = 1.0f / 600.0f; // Paramecium: vacuolas de ~5–30 min [1/s biológico]
    float mouthRadius = 6.0f;            // alcance del citostoma [µm]
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
    float vacuoleOpenTime = 6.0f;        // tiempo hasta que la vacuola se cierra [s]
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

// Agrega presa a la vacuola abierta, o crea una nueva en `where`. El tamaño de la célula lo
// fija el metabolismo (biomasa + contenido de las vacuolas).
void ingest(Digestion& dig, RingBody& body, BodyParticles& bp, Vec2 where, float area, float pmol,
            std::uint8_t r, std::uint8_t g, std::uint8_t b, bool forceNew);

// Cierra vacuolas llenas o viejas (tiempo real, `dt`), digiere (tiempo biológico, `bioDt`) y
// expulsa las vacías por el citoprocto. Devuelve el alimento digerido [pmol].
float digest(Digestion& dig, RingBody& body, BodyParticles& bp, const FeedingParams& params,
             float dt, float bioDt);

float vacuoleContentArea(const Digestion& dig);
