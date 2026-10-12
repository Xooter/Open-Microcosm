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
};

// Bacterias como partículas simples (tier ambiental, ver AGENTS.md).
// Nadan con "run & tumble" y hacen quimiotaxis TEMPORAL como E. coli: no miden el gradiente
// en el espacio, comparan la concentración actual con la de hace ~1 s. Si mejora, giran menos.
// En la fase 1 no comen ni se dividen (eso llega con el metabolismo en la fase 4).
class Bacteria {
  public:
    std::vector<float> x, y;
    std::vector<float> heading;      // [rad]
    std::vector<float> tumbleTimer;  // tiempo restante de giro [s]
    std::vector<float> memory;       // señal promediada en memoryTime
    std::vector<std::uint8_t> tumbling;

    BacteriaParams params;

    std::size_t size() const { return x.size(); }

    void spawn(std::size_t count, float width, float height, std::uint64_t seed);
    void resetMemory(const Grid2D& nutrients);
    void step(const Fluid& fluid, const Grid2D& nutrients, float dt, std::uint64_t seed,
              std::uint64_t stepIndex, float width, float height);

  private:
    float signal(const Grid2D& nutrients, float px, float py) const;
};
