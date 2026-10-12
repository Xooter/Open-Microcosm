#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

enum class BodyKind : std::uint8_t { Membrane, Interior };

// Partículas de cuerpos blandos (membranas, organelos, gránulos) en layout SoA.
// A diferencia del pool ambiental, los slots NO se compactan: un slot liberado vuelve a una
// lista libre, así que el índice es un ID estable que las restricciones pueden guardar.
// Las posiciones NO se envuelven por partícula: cada cuerpo es contiguo en el plano y se
// traslada entero cuando su centroide cruza un borde del mundo.
struct BodyParticles {
    std::vector<float> x, y;
    std::vector<float> vx, vy;  // velocidad externa del paso actual (agua + herramientas) [µm/s]
    std::vector<float> radius;  // [µm]
    std::vector<float> invMass; // reparte las correcciones: los organelos pesados se mueven menos
    std::vector<std::uint32_t> owner;
    std::vector<BodyKind> kind;
    std::vector<std::uint8_t> tag; // tipo visual libre para capas superiores (organelo, etc.)
    std::vector<std::uint8_t> alive;
    std::vector<std::uint32_t> freeSlots;

    std::size_t capacity() const { return x.size(); }

    std::uint32_t allocate(BodyKind k, float px, float py, float r, float w, std::uint32_t own) {
        std::uint32_t s;
        if (!freeSlots.empty()) {
            s = freeSlots.back();
            freeSlots.pop_back();
        } else {
            s = static_cast<std::uint32_t>(x.size());
            x.push_back(0);
            y.push_back(0);
            vx.push_back(0);
            vy.push_back(0);
            radius.push_back(0);
            invMass.push_back(0);
            owner.push_back(0);
            kind.push_back(k);
            tag.push_back(0);
            alive.push_back(0);
        }
        x[s] = px;
        y[s] = py;
        vx[s] = vy[s] = 0.0f;
        radius[s] = r;
        invMass[s] = w;
        owner[s] = own;
        kind[s] = k;
        tag[s] = 0;
        alive[s] = 1;
        return s;
    }

    void release(std::uint32_t s) {
        alive[s] = 0;
        freeSlots.push_back(s);
    }
};
