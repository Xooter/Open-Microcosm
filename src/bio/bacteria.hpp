#pragma once

#include "bio/feeding.hpp"
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
    // Fase estacionaria (respuesta estricta): sin comida que cubra la mantención, la bacteria baja
    // su gasto a esta fracción y deja de crecer en vez de consumirse al ritmo completo. E. coli
    // reduce su mantención y su tasa de muerte más de 5 veces al pasar hambre; la viabilidad cae
    // exponencialmente porque las vivas reciclan la materia de las muertas (Schink, Biselli, Ammar
    // y Gerland 2019, Cell Systems 9: 64-73). Con el gasto completo, todas adelgazaban al mismo
    // ritmo y morían casi juntas.
    float starvationMaintenance = 0.2f;
    std::size_t maxCount = 150'000; // tope de rendimiento (la comida suele limitar antes)
    // Biopelícula en el vidrio: una bacteria que choca contra la pared puede quedar adherida
    // (adhesión inicial) y las adheridas se sueltan de a poco (dispersión). Adheridas no nadan ni
    // las arrastra el agua; crecen con la comida local y sus hijas quedan en la biopelícula. Los
    // filtradores (citostoma, polo anterior) no las alcanzan; las amebas, que pastan superficies,
    // sí (refugio contra el pastoreo: Matz y Kjelleberg 2005, Trends Microbiol. 13: 302-307).
    // Los dos valores son ESTIMACIONES sin medición directa: fijan una fracción adherida de ~1–2 %.
    float attachProbability = 0.01f; // por choque contra el vidrio
    float detachPerHour = 0.2f;      // [1/h biológica]
    // Cepas: la mayoría son bacilos gramnegativos genéricos; una fracción es Chromobacterium
    // violaceum, que con quorum sensing (autoinductor AHL) produce violaceína, un pigmento
    // violeta tóxico para los protistas que la comen, y forma agregados (biopelícula).
    float violaceumFraction = 0.2f;
    // Bacillus subtilis (cepa 2): bacilo grampositivo (pared gruesa de peptidoglicano), común en
    // suelo y agua dulce. Vive igual que las genéricas; se distingue con la tinción de Gram.
    float bacillusFraction = 0.12f;
    float spirillumFraction = 0.002f; // Spirillum volutans: rara (una cada ~500), pero gigante
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

// Cepas (Bacteria::strain). Spirillum volutans (3) es una bacteria gigante en hélice (~25 µm, unas 40 veces el
// volumen de E. coli), microaerófila: hace aerotaxis hacia un O₂ bajo y forma una banda (Caraway & Krieg
// 1974, J. Bacteriol. 117: 1107; Padgett & Krieg 1986). Con un haz de flagelos en cada polo no da tumbos:
// invierte el sentido. Velocidad, óptimo de O₂ y volumen: órdenes de magnitud, a verificar.
// Pendiente (plan M8): pasar las cepas a datos como los seres.
struct StrainInfo {
    const char* name;
    float pmolScale;     // biomasa de una recién nacida / la de E. coli
    float speed;         // [µm/s]
    float length;        // [µm] (las bocas de los filtradores no tragan las largas)
    bool reverses;       // flagelos bipolares: invierte el sentido en vez de girar al azar
    bool aerotaxis;      // sigue el O₂ hacia `o2Optimum` en vez de la comida
    float o2Optimum;     // [µM]
};
inline constexpr std::uint8_t kSpirillumStrain = 3;
const StrainInfo& strainInfo(std::uint8_t strain);
// Biomasa de una bacteria [pmol] (masa relativa × la de una recién nacida de su cepa).
inline float bacterialPmol(std::uint8_t strain, float mass) { return kBacteriumPmol * strainInfo(strain).pmolScale * mass; }

// Fisiología de una bacteria, común a la placa (partículas) y al cultivo bien mezclado (fase P1).
// Fracción de su crecimiento que conserva: las infectadas (fago o Bdellovibrio) no crecen, su
// maquinaria trabaja para el parásito; las resistentes a los fagos pagan un costo.
float bacterialHealth(const BacteriaParams& p, bool infected, bool resistant);
// Materia orgánica que pide en `hours` [pmol] (Monod con la concentración `organic` [µM]); `mass`
// relativa a una recién nacida.
float bacterialDemand(const BacteriaParams& p, float organic, float mass, float health, float hours,
                      float pmolScale = 1.0f);
// Cambio de su masa relativa al comer `taken` [pmol] con un gasto de mantención `maintenance`
// [fracción de la masa en el paso]. Fase estacionaria: si lo que come no cubre la mantención, baja
// su gasto (`starvationMaintenance`); con comida parcial se sostiene sin crecer, sin comida adelgaza
// despacio. Continuo en lo comido = mantención.
float bacterialMassChange(const BacteriaParams& p, float taken, float mass, float maintenance,
                          float pmolScale = 1.0f);

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
    std::vector<std::uint8_t> attached; // adherida al vidrio (biopelícula)
    // Identidad estable (el índice cambia con swap-and-pop). Solo la leen la inspección y el
    // render (seguir a una bacteria, ubicar sus estructuras); no interviene en la dinámica ni en el
    // azar. Cada hija recibe una nueva.
    std::vector<std::uint32_t> id;
    std::uint32_t nextId = 1;

    BacteriaParams params;
    Dish dish; // pared de la placa (inactiva: mundo toroidal)

    std::size_t size() const { return x.size(); }

    void spawn(std::size_t count, float width, float height, std::uint64_t seed);
    void resetMemory(const Grid2D& nutrients, const Grid2D& oxygen);
    // Swap-and-pop: O(1), mueve la última bacteria al índice i.
    void remove(std::size_t i);
    void add(float px, float py, float heading, float mass, std::uint8_t strain = 0, std::uint8_t resistant = 0,
             std::uint8_t attached = 0);
    // Crecimiento en paralelo: devuelve en `demand` la materia orgánica que cada una quiere
    // absorber [pmol] (el mundo la descuenta del agua según lo que haya).
    void grow(const Grid2D& nutrients, float bioDt, std::vector<float>& demand) const;
    void step(const Fluid& fluid, const Grid2D& nutrients, const Grid2D& oxygen, float dt, std::uint64_t seed,
              std::uint64_t stepIndex, float width, float height);

  private:
    float signal(const Grid2D& nutrients, const Grid2D& oxygen, std::size_t i) const;
};

// Bdellovibrio libres (cazando). Los que están adentro de una presa viven en `Bacteria::bdello`.
struct Bdellovibrios {
    std::vector<float> x, y, heading, hungerHours;
    std::vector<std::uint32_t> id; // identidad estable (ver `Bacteria::id`)
    std::uint32_t nextId = 1;
    std::size_t size() const { return x.size(); }
    void push(float px, float py, float h) {
        id.push_back(nextId++);
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
        id[i] = id.back();
        id.pop_back();
        x.pop_back();
        y.pop_back();
        heading.pop_back();
        hungerHours.pop_back();
    }
};
