#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

enum class ParticleKind : std::uint8_t {
    Ink,      // tinta china: partículas de carbono de ~0.2 µm, trazador del flujo
    Detritus, // materia orgánica muerta: se descompone liberando nutrientes disueltos
    Mineral,  // partícula inorgánica (sedimento): no se descompone ni se come
};

// Minerales del sedimento de una gota, como se ven en campo claro.
enum class MineralKind : std::uint8_t {
    Clay,      // arcilla (< 4 µm): queda en suspensión, browniano visible
    Quartz,    // limo de cuarzo (4–60 µm): grano anguloso, transparente, mucho relieve
    Mica,      // escamas de mica: láminas finas y brillantes
    IronOxide, // óxidos de hierro (y materia húmica asociada): opacos, pardo rojizos
    Sulfur,    // glóbulos de azufre elemental (precipitado de fuentes sulfurosas)
    Frustule,  // frústula vacía de diatomea (sílice): queda cuando muere la célula
    Count
};

// Partículas ambientales en layout SoA (un array contiguo por campo).
// Para agregar un campo nuevo, sumalo también en push() y swapRemove().
struct AmbientParticles {
    std::vector<float> x, y;
    std::vector<float> radius; // [µm]
    std::vector<float> mass;   // [pmol equivalentes de glucosa], 0 para la tinta
    std::vector<float> age;    // [s]
    std::vector<ParticleKind> kind;
    std::vector<MineralKind> mineral; // solo para ParticleKind::Mineral
    std::vector<float> angle;         // orientación [rad] (granos angulosos, frústulas)

    std::size_t size() const { return x.size(); }

    void push(ParticleKind k, float px, float py, float r, float m, MineralKind mk = MineralKind::Clay,
              float a = 0.0f) {
        x.push_back(px);
        y.push_back(py);
        radius.push_back(r);
        mass.push_back(m);
        age.push_back(0.0f);
        kind.push_back(k);
        mineral.push_back(mk);
        angle.push_back(a);
    }

    // Swap-and-pop: O(1), pero mueve la última partícula al índice i.
    void swapRemove(std::size_t i) {
        const std::size_t last = size() - 1;
        x[i] = x[last];
        y[i] = y[last];
        radius[i] = radius[last];
        mass[i] = mass[last];
        age[i] = age[last];
        kind[i] = kind[last];
        mineral[i] = mineral[last];
        angle[i] = angle[last];
        x.pop_back();
        y.pop_back();
        radius.pop_back();
        mass.pop_back();
        age.pop_back();
        kind.pop_back();
        mineral.pop_back();
        angle.pop_back();
    }
};
