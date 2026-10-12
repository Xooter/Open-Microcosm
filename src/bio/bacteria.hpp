#pragma once

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

    BacteriaParams params;

    std::size_t size() const { return x.size(); }

    void spawn(std::size_t count, float width, float height, std::uint64_t seed);
    void resetMemory(const Grid2D& nutrients);
    // Swap-and-pop: O(1), mueve la última bacteria al índice i.
    void remove(std::size_t i);
    void add(float px, float py, float heading, float mass);
    // Crecimiento en paralelo: devuelve en `demand` la materia orgánica que cada una quiere
    // absorber [pmol] (el mundo la descuenta del agua según lo que haya).
    void grow(const Grid2D& nutrients, float bioDt, std::vector<float>& demand) const;
    void step(const Fluid& fluid, const Grid2D& nutrients, float dt, std::uint64_t seed,
              std::uint64_t stepIndex, float width, float height);

  private:
    float signal(const Grid2D& nutrients, float px, float py) const;
};
