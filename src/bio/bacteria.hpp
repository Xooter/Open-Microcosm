#pragma once

#include "sim/dish.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

class Fluid;
struct Grid2D;

// Parámetros de E. coli (valores de laboratorio, Berg 2004).
struct BacteriaParams {
    float speed = 25.0f;          // velocidad de nado durante una "corrida" [µm/s]
    float baseTumbleRate = 1.0f;  // giros por segundo sin gradiente [1/s]
    float tumbleDuration = 0.1f;  // [s]
    float tumbleAngleMean = 68.0f;  // [grados]
    float tumbleAngleStd = 36.0f;   // [grados]
    float rotDiffusion = 0.06f;   // difusión rotacional [rad²/s]
    float radius = 0.5f;          // para el browniano traslacional [µm]
    float length = 2.0f;          // [µm], solo visual
    float chemotaxisGain = 5.0f;  // sensibilidad al cambio de log(concentración)
    float memoryTime = 1.0f;      // ventana temporal de comparación [s]
    float sensingKd = 1.0f;       // constante de disociación del receptor [µM]
    // Crecimiento (Monod) con la materia orgánica disuelta, en tiempo biológico.
    float growthPerHourMax = 1.4f; // E. coli: duplicación ~30 min con comida abundante
    float halfSaturation = 1.0f;   // Ks [µM]
    float yield = 0.5f;            // biomasa por materia orgánica consumida
    float maintenancePerHour = 0.05f;
    std::size_t maxCount = 150'000; // tope de rendimiento (la comida suele limitar antes)
    // Cepas: la mayoría son bacilos gramnegativos genéricos; una fracción es Chromobacterium
    // violaceum, que con quorum sensing (autoinductor AHL) produce violaceína, un pigmento
    // violeta tóxico para los protistas que la comen, y forma agregados (biopelícula).
    float violaceumFraction = 0.2f;
    // Bacillus subtilis (cepa 2): bacilo grampositivo (pared gruesa de peptidoglicano), común en
    // suelo y agua dulce. Vive igual que las genéricas; se distingue con la tinción de Gram.
    float bacillusFraction = 0.12f;
    float ahlPerHour = 2e-7f;       // autoinductor que libera cada bacteria [pmol/h]
    float quorumThreshold = 0.05f;  // [µM] (decenas de nM en la realidad, con hysteresis)
    float quorumSpeed = 0.3f;       // en agregado nadan menos
    // Fagos líticos (tipo T4): adsorción ∝ concentración de fagos; período latente ~30 min;
    // estallido de ~50 viriones. Mutantes resistentes (receptor alterado) crecen algo menos.
    float phageAdsorption = 1.5e5f; // [µm³/h]
    float phageLatentHours = 0.5f;
    int phageBurst = 50;
    float resistanceMutation = 1e-3f; // por división
    float resistanceCost = 0.1f;
    // Bdellovibrio bacteriovorus: entra al periplasma (bdeloplasto), crece ~3 h y sale con 4–6
    // hijas. Nada a ~100 µm/s (de las bacterias más rápidas).
    float bdelloSpeed = 100.0f;
    float bdelloHours = 3.0f;
    int bdelloProgeny = 5;
    float bdelloStarveHours = 10.0f;
};

// Bacterias como partículas simples (tier ambiental, ver AGENTS.md).
// Nadan con "run & tumble" y hacen quimiotaxis TEMPORAL como E. coli: no miden el gradiente
// en el espacio, comparan la concentración actual con la de hace ~1 s. Si mejora, giran menos.
// Crecen con la materia orgánica disuelta, se dividen al duplicar su masa y mueren de hambre.
class Bacteria {
  public:
    std::vector<float> x, y;
    std::vector<float> heading;      // [rad]
    std::vector<float> tumbleTimer;  // tiempo restante de giro [s]
    std::vector<float> memory;       // señal promediada en memoryTime
    std::vector<std::uint8_t> tumbling;
    std::vector<float> mass;         // relativa a una recién nacida (1); se divide en 2
    std::vector<std::uint8_t> strain;    // 0: bacilo gramnegativo genérico; 1: C. violaceum (gramnegativa, quorum sensing); 2: B. subtilis (grampositiva)
    std::vector<std::uint8_t> resistant; // resistente a los fagos
    std::vector<std::uint8_t> quorum;    // quorum alcanzado (violaceína, agregado)
    std::vector<float> phage;        // horas desde la infección por un fago (< 0: sana)
    std::vector<float> bdello;       // horas desde que entró un Bdellovibrio (< 0: libre)

    BacteriaParams params;
    Dish dish; // pared de la placa (inactiva: mundo toroidal)

    std::size_t size() const { return x.size(); }

    void spawn(std::size_t count, float width, float height, std::uint64_t seed);
    void resetMemory(const Grid2D& nutrients);
    // Swap-and-pop: O(1), mueve la última bacteria al índice i.
    void remove(std::size_t i);
    void add(float px, float py, float heading, float mass, std::uint8_t strain = 0, std::uint8_t resistant = 0);
    // Crecimiento en paralelo: devuelve en `demand` la materia orgánica que cada una quiere
    // absorber [pmol] (el mundo la descuenta del agua según lo que haya).
    void grow(const Grid2D& nutrients, float bioDt, std::vector<float>& demand) const;
    void step(const Fluid& fluid, const Grid2D& nutrients, float dt, std::uint64_t seed,
              std::uint64_t stepIndex, float width, float height);

  private:
    float signal(const Grid2D& nutrients, float px, float py) const;
};

// Bdellovibrio libres (cazando). Los que están adentro de una presa viven en `Bacteria::bdello`.
struct Bdellovibrios {
    std::vector<float> x, y, heading, hungerHours;
    std::size_t size() const { return x.size(); }
    void push(float px, float py, float h) {
        x.push_back(px);
        y.push_back(py);
        heading.push_back(h);
        hungerHours.push_back(0.0f);
    }
    void swapRemove(std::size_t i) {
        x[i] = x.back();
        y[i] = y.back();
        heading[i] = heading.back();
        hungerHours[i] = hungerHours.back();
        x.pop_back();
        y.pop_back();
        heading.pop_back();
        hungerHours.pop_back();
    }
};
