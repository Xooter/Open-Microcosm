#pragma once

#include "core/vec2.hpp"
#include "sim/dish.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

class Fluid;

// Hongos y pseudohongos acuáticos (tier ambiental). No son cuerpos blandos: las hifas son
// cadenas de nodos fijas al sustrato que crecen por la punta, y las zoosporas son partículas
// nadadoras.
//  - Quitridios (Rhizophydium): la zoospora uniflagelada (~3 µm) nada hacia las algas, se
//    enquista sobre la pared, mete rizoides y crece como esporangio a costa del hospedador;
//    al madurar libera decenas de zoosporas. El alga infectada no se divide y muere.
//  - Saprolegnia (oomiceto, "moho de agua"): micelio cenocítico sobre detritos; secreta enzimas
//    que disuelven la materia orgánica cercana y absorbe lo disuelto. Cuando se le acaba la
//    comida forma zoosporangios en las puntas, que liberan zoosporas biflageladas.
//  - Zoophagus insidians: hifas finas con clavijas cortas y adhesivas; el rotífero que muerde
//    una queda pegado, la hifa lo invade y lo digiere.
enum class FungusKind : std::uint8_t { Chytrid, Saprolegnia, Zoophagus, Count };
inline constexpr int kFungusKinds = static_cast<int>(FungusKind::Count);
const char* fungusName(FungusKind kind);

struct FungiParams {
    // Zoosporas.
    float chytridSporeSpeed = 15.0f;  // [µm/s] (Rhizophydium, Batrachochytrium: 10–20)
    float saproSporeSpeed = 80.0f;    // [µm/s] zoosporas secundarias de Saprolegnia
    float sporeSenseRange = 150.0f;   // [µm] quimiotaxis hacia el hospedador
    float sporeLifeHours = 12.0f;     // nadan con sus reservas; después se enquistan y mueren
    float sporePmol = 0.02f;          // biomasa de una zoospora (~3–4 µm)
    // Quitridio sobre su hospedador.
    float chytridDrainPerHour = 0.15f; // fracción de la biomasa del alga que absorbe cada esporangio
    float chytridYield = 0.5f;
    int chytridMaxSpores = 40;        // por esporangio
    int chytridMaxSporangia = 3;      // por hospedador
    float chytridMaxHours = 24.0f;    // madura aunque el alga siga viva
    // Micelio.
    float hyphaSpacing = 10.0f;       // [µm] entre nodos
    float nodePmol = 0.57f;           // hifa de ~12 µm, muy vacuolada
    float extensionPerHour = 200.0f;  // [µm/h] Saprolegnia: ~3 µm/min por punta
    float branchProbability = 0.12f;  // por nodo nuevo
    float uptakePerHour = 1.0f;       // máxima, relativa a la biomasa
    float halfSaturation = 5.0f;      // [µM] (menos afín que las bacterias)
    float growthYield = 0.6f;
    float maintenancePerHour = 0.02f;
    float phosphorusRatio = 0.01f;
    float starvationHoursToSporulate = 2.0f;
    float sporangiumHours = 1.0f;     // zoosporangio: de la punta hinchada a la liberación
    int sporesPerSporangium = 30;
    float detritusDecayBoost = 30.0f; // exoenzimas: los detritos junto a las hifas se disuelven más rápido
    int pegSpacing = 3;               // Zoophagus: una clavija cada 3 nodos
    std::size_t maxNodesPerMycelium = 400;
    std::size_t maxNodes = 5000;      // tope de rendimiento
    std::size_t maxSpores = 3000;
};

struct Mycelium {
    std::uint32_t id = 0;
    FungusKind kind = FungusKind::Saprolegnia;
    // Nodos: posiciones contiguas (no se envuelven), padre (−1 la raíz) y estado de cada punta.
    std::vector<float> x, y, heading;
    std::vector<std::int32_t> parent;
    std::vector<std::uint8_t> tip;        // punta en crecimiento
    std::vector<std::uint8_t> peg;        // Zoophagus: clavija adhesiva
    std::vector<float> sporangium;        // > 0: zoosporangio madurando [h biológicas]
    float reserves = 0.0f;                // [pmol] absorbido y aún no invertido
    float phosphorus = 0.0f;              // [pmol P]
    float starvingHours = 0.0f;
    float tipCredit = 0.0f;               // avance acumulado de las puntas [µm]
    float biomass(const FungiParams& p) const { return static_cast<float>(x.size()) * p.nodePmol + reserves; }
};

struct Zoospores {
    std::vector<float> x, y, heading, ageHours, phosphorus;
    std::vector<FungusKind> kind;
    std::size_t size() const { return x.size(); }
    void push(FungusKind k, float px, float py, float h, float p);
    void swapRemove(std::size_t i);
};

// Lo que el hongo hace con el agua (lo provee el mundo: conserva la masa).
struct FungiEnv {
    std::function<float(Vec2, float)> takeFood;       // quita hasta `pmol` de materia orgánica disuelta
    std::function<float(Vec2, float)> takePhosphate;  // quita hasta `pmol` de fósforo
    std::function<void(Vec2, float, float)> release;  // devuelve (materia orgánica, fósforo)
    std::function<float(Vec2)> food;                  // materia orgánica disuelta [µM]
    std::function<bool(Vec2)> substrate;              // ¿hay detritos o materia orgánica para germinar?
};

class Fungi {
  public:
    FungiParams params;
    std::vector<Mycelium> mycelia;
    Zoospores spores;
    Dish dish; // pared de la placa

    std::uint32_t spawnMycelium(FungusKind kind, Vec2 at, float heading, float reserves, float phosphorus);
    // `phosphorus`: el fósforo total que se reparte entre las zoosporas.
    void spawnSpores(FungusKind kind, Vec2 at, int count, float phosphorus, std::uint64_t seed, std::uint64_t salt);
    std::size_t nodeCount() const;
    float biomass() const;                // [pmol] micelios + zoosporas
    float phosphorus() const;             // [pmol P]
    const Mycelium* find(std::uint32_t id) const;
    Mycelium* find(std::uint32_t id);

    // Nado de las zoosporas (tiempo real). `steer`: dirección deseada por zoospora (o cero).
    void moveSpores(const Fluid& fluid, const std::vector<Vec2>& steer, float dt, std::uint64_t seed,
                    std::uint64_t step, float width, float height);
    // Absorción, crecimiento apical, ramificación, esporulación, autólisis y germinación
    // (tiempo biológico).
    void grow(const FungiEnv& env, float bioDt, std::uint64_t seed, std::uint64_t step);
    // El hongo come una presa (Zoophagus): se suma a sus reservas.
    void feed(std::uint32_t id, float pmol, float phosphorus);

  private:
    std::uint32_t nextId_ = 1;
};

// Componente: alga infectada por quitridios (esporangios sobre su pared, rizoides adentro).
struct ChytridInfection {
    std::vector<float> siteAngle; // posición de cada esporangio (ángulo en el marco de la célula)
    float mass = 0.0f;            // [pmol] absorbido por los esporangios
    float phosphorus = 0.0f;      // [pmol P]
    float ageHours = 0.0f;
};

// Componente: célula pegada a una clavija adhesiva de Zoophagus.
struct Trapped {
    std::uint32_t mycelium;
    Vec2 point; // la clavija [µm]
};

