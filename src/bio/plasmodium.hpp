#pragma once

#include "core/vec2.hpp"
#include "sim/dish.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

// Physarum polycephalum: moho mucilaginoso (Amoebozoa; NO es un hongo). El plasmodio es UNA sola
// célula gigante multinucleada. Explora con un frente en abanico y, detrás, el citoplasma se
// organiza en una red de tubos: los que transportan más flujo se ensanchan y los demás se
// retraen (Nakagaki 2000; Tero et al., Science 2010 — la red que forma entre fuentes de comida
// se parece en eficiencia y tolerancia a fallos a redes ferroviarias reales).
//  - Red: grafo de nodos y tubos con conductividad D (el radio sale de r ∝ D^¼, Hagen–Poiseuille).
//  - Modelo de Tero: el flujo de citoplasma Q resuelve la ley de Kirchhoff con presiones en los
//    nodos; dD/dt = f(|Q|) − μ·D. Con la comida como fuente y la punta (o la otra comida) como
//    sumidero.
//  - Anastomosis: un tubo que toca otro se fusiona (la célula es una sola): aparecen ciclos.
//  - Rastro de baba extracelular (Reid et al. 2012): las puntas evitan lo ya explorado.
//  - Esclerocio: con hambre prolongada se endurece y duerme hasta que vuelva la comida.
// Simplificaciones (comentadas): el organismo real mide centímetros y sus tubos ~50–200 µm; aquí
// la placa mide ~2 mm, así que los tubos son más finos. No fagocita bacterias (lo real sí).

inline constexpr std::int32_t kAllTips = -2; // sumidero: todas las puntas de exploración

struct PlasmodiumParams {
    float spacing = 10.0f;            // [µm] entre nodos
    float tipSpeedPerHour = 2000.0f;  // [µm/h bio] frente de exploración (~2 mm/h)
    float nodePmol = 0.5f;            // [pmol] de un tramo de tubo de 10 µm a D = 1 (la masa crece con √D)
    float growthYield = 0.6f;
    float uptakePerHour = 1.2f;       // de materia orgánica disuelta, por nodo, relativa a la biomasa
    float halfSaturation = 8.0f;      // [µM]
    float flakeUptakePerHour = 30.0f; // [pmol/h] por copo en contacto (digiere el almidón rápido)
    float flakeLeachPerHour = 0.02f;  // fracción de un copo que se disuelve al agua por hora
    float flakeReach = 22.0f;         // [µm] distancia a la que un nodo "toca" un copo
    float maintenancePerHour = 0.01f;
    float phosphorusRatio = 0.01f;
    float branchProbability = 0.22f;
    float adaptRate = 1.0f;           // [1/h] velocidad de μ y de la respuesta f(Q)
    float gamma = 1.8f;               // exponente de f(Q) = Qᵞ/(1+Qᵞ) (Tero usa 1,0–1,8)
    float youngD = 0.35f;             // conductividad de un tubo recién nacido
    float tipFloorD = 0.15f;          // los tubos de las puntas activas no bajan de aquí
    float pruneD = 0.04f;             // por debajo se retrae
    float solveIntervalHours = 0.03f; // cada cuánto se resuelve la red (tiempo biológico)
    float switchHours = 0.1f;         // cada cuánto cambia el par fuente/sumidero (Tero)
    float fuseDistance = 6.0f;        // [µm] para anastomosar
    float slimeAvoidance = 1.5f;      // peso del rastro de baba en el rumbo
    float lightAvoidance = 1.0f;      // fotofobia
    float starveHoursToSclerotium = 8.0f;
    float sclerotiumFraction = 0.5f;  // lo que se conserva al endurecerse
    float sclerotiumGerminateFood = 10.0f; // [µM]
    float sclerotiumMinHours = 4.0f;
    std::size_t maxNodes = 2500;      // tope de rendimiento
    std::size_t maxEdges = 5000;
    std::size_t maxPlasmodia = 12;
    std::size_t maxFlakes = 300;
};

// Un plasmodio: grafo de nodos y tubos.
struct Plasmodium {
    std::uint32_t id = 0;
    std::vector<float> x, y, heading;
    std::vector<std::uint8_t> tip;   // punta de exploración
    std::vector<std::int32_t> food;  // índice del copo que toca (−1: ninguno)
    std::vector<std::uint8_t> fed;   // su pedazo de red llega a una comida (si no, no explora y se retrae)
    struct Edge {
        std::int32_t a, b;
        float length;
        float D;    // conductividad (0–1)
        float flux; // |Q| del último paso (para el render y la ficha)
    };
    std::vector<Edge> edges;
    float reserves = 0.0f;        // [pmol] absorbido y aún no invertido (azúcares, almidón)
    float phosphorus = 0.0f;      // [pmol P]
    float starvingHours = 0.0f;
    float tipCredit = 0.0f;       // avance acumulado [µm]
    float solveAccum = 0.0f;      // tiempo biológico desde la última resolución [h]
    float switchAccum = 0.0f;
    std::int32_t source = -1, sink = -1; // par vigente (Tero)
    float tubeMass = 0.0f;        // [pmol] contabilizada en los tubos
    float ageHours = 0.0f;

    std::size_t nodes() const { return x.size(); }
    float biomass() const { return tubeMass + reserves; }
};

// Copo de avena: cebo sólido de almidón. Se disuelve despacio y el plasmodio se alimenta tocándolo.
struct OatFlakes {
    std::vector<float> x, y, pmol, phosphorus;
    std::size_t size() const { return x.size(); }
    void push(float px, float py, float mass, float p) {
        x.push_back(px);
        y.push_back(py);
        pmol.push_back(mass);
        phosphorus.push_back(p);
    }
    void swapRemove(std::size_t i);
};

// Esclerocio: plasmodio endurecido, dormido (años en la naturaleza).
struct Sclerotia {
    std::vector<float> x, y, ageHours, pmol, phosphorus;
    std::size_t size() const { return x.size(); }
    void swapRemove(std::size_t i);
};

// Lo que el plasmodio hace con el agua (lo provee el mundo: conserva la masa).
struct PlasmodiumEnv {
    std::function<float(Vec2)> food;                  // materia orgánica disuelta [µM]
    std::function<float(Vec2, float)> takeFood;
    std::function<float(Vec2, float)> takePhosphate;
    std::function<void(Vec2, float, float)> release;  // (materia orgánica, fósforo)
    // Rumbo deseado en `at` (atracción por comida, rechazo del rastro y de la luz). Cero: sin pista.
    std::function<Vec2(Vec2)> cue;
    std::function<void(Vec2)> markSlime;              // deja baba
};

class PlasmodiumSystem {
  public:
    PlasmodiumParams params;
    std::vector<Plasmodium> plasmodia;
    OatFlakes flakes;
    Sclerotia sclerotia;
    Dish dish;
    struct Stats {
        int fusions = 0, sclerotiaFormed = 0, sclerotiaGerminated = 0;
    } stats;

    // Un plasmodio nuevo con `reserves` y el fósforo dado. Devuelve su id (0 si no cabe).
    std::uint32_t spawn(Vec2 at, float heading, float reserves, float phosphorus);
    void addFlake(Vec2 at, float pmol, float phosphorus) { flakes.push(at.x, at.y, pmol, phosphorus); }

    float biomass() const;     // [pmol] plasmodios + esclerocios + copos
    float phosphorus() const;  // [pmol P]
    std::size_t nodeCount() const;
    std::size_t edgeCount() const;
    const Plasmodium* find(std::uint32_t id) const;
    Plasmodium* find(std::uint32_t id);

    // Un paso de tiempo biológico: absorción, flujo y adaptación de la red, exploración,
    // fusión, retracción, esclerocio. Determinista (serial, orden fijo).
    void step(const PlasmodiumEnv& env, float bioDt, std::uint64_t seed, std::uint64_t step);

    // Resuelve los flujos Q de un plasmodio entre `source` y `sink` con presiones de Kirchhoff
    // (gradiente conjugado). Devuelve false si no están conectados. Público para los tests.
    bool solveFlow(Plasmodium& pl, std::int32_t source, std::int32_t sink, float inflow);
    // Un paso del modelo de Tero sobre los flujos ya resueltos. Público para los tests.
    void adapt(Plasmodium& pl, float hours) const;
    // Métricas de la red (Tero 2010): longitud total de tubos [µm] y nodos conectados.
    static float totalLength(const Plasmodium& pl);
    static std::size_t cycles(const Plasmodium& pl); // aristas − nodos + componentes

  private:
    std::uint32_t nextId_ = 1;
    // Buffers reusados (sin asignaciones por paso).
    std::vector<std::int32_t> hashHead_, hashNext_, sinks_, remap_, adjStart_, adjList_, queue_;
    std::vector<float> oldD_, cg_p_, cg_d_, cg_r_, cg_z_, cg_q_, cg_diag_, cg_b_, conduct_;
    std::vector<std::uint8_t> inComp_;
    void hashBuild(const Plasmodium& pl);
    void hashInsert(const Plasmodium& pl, std::int32_t node);
    template <typename F> void hashNear(const Plasmodium& pl, Vec2 at, float radius, F&& fn) const;
    float tubeMassOf(const Plasmodium& pl) const;
    void grow(Plasmodium& pl, const PlasmodiumEnv& env, float hours, std::uint64_t seed, std::uint64_t step);
    void prune(Plasmodium& pl, const PlasmodiumEnv& env);
    void compact(Plasmodium& pl);
    void labelFed(Plasmodium& pl);
};
