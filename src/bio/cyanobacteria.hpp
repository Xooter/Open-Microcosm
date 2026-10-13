#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class Fluid;
struct Grid2D;

// Cianobacterias filamentosas (Anabaena): son bacterias, así que van en el tier ambiental como
// cadenas de partículas, no como cuerpos blandos. Cada cuenta es una célula (~5 µm).
//  - Fotosíntesis con la luz y el fósforo locales; al duplicar su masa la célula se divide
//    dentro de la cadena (crecimiento intercalar).
//  - Cada ~10 células una se diferencia en heterocisto (pared gruesa, no se divide; fija N₂).
//  - Los filamentos largos se fragmentan (hormogonios) y se deslizan despacio con inversiones.
//  - Casi nadie se come un filamento largo: por eso forman blooms.
struct CyanoParams {
    float spacing = 5.0f;              // distancia entre células [µm]
    float growthPerHourMax = 0.12f;    // Anabaena: duplicación ~6–8 h
    float phosphorusHalfSaturation = 0.3f; // [µM]
    float phosphorusRatio = 0.01f;     // pmol P por pmol de biomasa
    float cellPmol = 0.004f;           // biomasa de una célula recién dividida [pmol]
    float glideSpeed = 2.0f;           // [µm/s]
    int heterocystSpacing = 10;
    std::size_t maxLength = 40;        // más largo se fragmenta
    std::size_t maxCells = 6000;       // tope de rendimiento
    // Nitrógeno: los heterocistos fijan N₂ (nitrogenasa); el O₂ la inhibe aunque la pared
    // gruesa lo frene, así que fijan más donde las bacterias vecinas consumen O₂ (mutualismo).
    float nitrogenRatio = 0.16f;
    float nitrogenHalfSaturation = 1.0f;     // [µM]
    float fixationPerHeterocyst = 2e-4f;     // [pmol N/h] (alcanza para ~10 células vegetativas)
    float fixationO2Half = 150.0f;           // [µM] inhibición por O₂
    float fixedNLeak = 0.15f;                // fracción de lo fijado que se libera como amonio
    // Muerte (cianófagos, senescencia): la célula muerta corta el filamento (necridio).
    float mortalityPerHour = 0.02f;
    // Microcistina (~0,1–1% del peso seco): se libera al morir la célula.
    float toxinRatio = 0.001f;               // pmol de toxina por pmol de biomasa
    float exudation = 0.1f;                  // fracción de lo fijado que sale como materia orgánica
    // Rango térmico de crecimiento [°C] y temperatura del agua (la fija el bioma).
    float tMin = 10.0f, tMax = 35.0f, temperature = 20.0f;
};

// Lo que una célula muerta (o una exudación) devuelve al agua.
struct CyanoRelease {
    float x, y;
    float organic, phosphorus, nitrogen, toxin; // [pmol]
};

struct CyanoFilament {
    std::vector<float> x, y;     // posiciones contiguas (la cadena no se envuelve por cuenta)
    std::vector<float> mass;     // relativa (1 = recién dividida, se divide en 2)
    std::vector<std::uint8_t> heterocyst;
    float phosphorus = 0.0f;     // [pmol P] de todo el filamento
    float nitrogen = 0.0f;       // [pmol N]
    float direction = 1.0f;      // sentido de deslizamiento
    float reverseTimer = 0.0f;   // [s]
};

class Cyanobacteria {
  public:
    CyanoParams params;
    std::vector<CyanoFilament> filaments;

    void spawn(int count, int cells, float width, float height, std::uint64_t seed);
    std::size_t cellCount() const;
    float biomass() const; // [pmol]
    float phosphorus() const;

    // Movimiento (agua, deslizamiento) y restricciones de la cadena.
    void move(const Fluid& fluid, float dt, std::uint64_t seed, std::uint64_t step, float width, float height);
    // Crecimiento, división, heterocistos, fijación y fragmentación (tiempo biológico).
    // `grow` calcula lo que cada filamento pide del agua [pmol]; el mundo descuenta lo que haya
    // y lo acredita con `credit`, que además devuelve las liberaciones al agua.
    // `ph`: las cianobacterias usan bicarbonato (mecanismo concentrador de CO₂) y crecen mejor a
    // pH alto, que ellas mismas producen al florecer.
    void grow(const Grid2D& light, const Grid2D& phosphate, const Grid2D& nitrogen, const Grid2D& ph,
              float bioDt, std::vector<float>& demandP, std::vector<float>& demandN);
    void credit(const std::vector<float>& takenP, const std::vector<float>& takenN, const Grid2D& oxygen,
                float bioDt, std::uint64_t seed, std::uint64_t step, std::vector<CyanoRelease>& releases);
    float nitrogen() const;
    float fixedTotal = 0.0f;   // N₂ fijado desde el comienzo [pmol N]

  private:
    std::vector<float> potential_; // biomasa que podría formar cada filamento en este paso [pmol]
};
