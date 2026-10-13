#pragma once

#include "core/vec2.hpp"
#include "sim/grid2d.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

// Placa: pocillo circular de vidrio que contiene el agua. Es el límite del mundo.
// Una placa de Petri real mide ~90 mm; a esta escala (µm) lo que existe de verdad es un pocillo
// o un anillo de vidrio sobre el portaobjetos (cámara de conteo): una placa en miniatura.
// Las grillas siguen siendo periódicas por dentro (la FFT lo necesita), pero nada vive ni cruza
// fuera del círculo, así que la periodicidad nunca se nota.
struct Dish {
    Vec2 center;
    float radius = 0.0f; // [µm]; 0 = sin placa (mundo toroidal abierto)

    bool active() const { return radius > 0.0f; }
    // > 0 afuera del agua (en el vidrio), < 0 adentro.
    float signedDistance(Vec2 p) const { return length(p - center) - radius; }
    bool contains(Vec2 p, float margin = 0.0f) const { return !active() || signedDistance(p) <= -margin; }

    // Punto dentro del agua a `margin` de la pared como mínimo.
    Vec2 clampInside(Vec2 p, float margin = 0.0f) const {
        if (!active())
            return p;
        const Vec2 d = p - center;
        const float len = length(d), limit = radius - margin;
        if (len <= limit)
            return p;
        if (len < 1e-6f)
            return center;
        return center + d * (limit / len);
    }

    // Partícula puntual que nada: si salió, vuelve adentro y el rumbo se refleja en la pared.
    // Devuelve true si chocó.
    bool bounce(float& x, float& y, float* heading, float margin) const {
        if (!active())
            return false;
        const Vec2 d = Vec2{x, y} - center;
        const float len = length(d), limit = radius - margin;
        if (len <= limit || len < 1e-6f)
            return false;
        const Vec2 n = d * (1.0f / len);
        x = center.x + n.x * limit;
        y = center.y + n.y * limit;
        if (heading) {
            Vec2 h = {std::cos(*heading), std::sin(*heading)};
            const float into = dot(h, n);
            if (into > 0.0f) {
                h = h - n * (2.0f * into);
                *heading = std::atan2(h.y, h.x);
            }
        }
        return true;
    }

    // Uniforme en el disco (`u`, `v` en [0, 1)), a `margin` de la pared.
    Vec2 sample(float u, float v, float margin = 0.0f) const {
        const float r = (radius - margin) * std::sqrt(u);
        const float a = 6.2831853f * v;
        return center + Vec2{r * std::cos(a), r * std::sin(a)};
    }
};

// Máscaras de una grilla del medio para la placa: qué celdas son vidrio, de qué celda de agua
// toman su valor fantasma (la imagen especular a través de la pared: condición de no-flujo por
// el método de imágenes), a qué celda de agua devuelven lo que les llegue por intercambios, y
// la atenuación del agua junto a la pared (no-slip aproximado).
struct DishMask {
    std::vector<std::uint32_t> outside; // celdas de vidrio
    std::vector<std::uint32_t> mirror;  // celda de agua especular de cada una
    std::vector<std::uint32_t> nearest; // celda de agua más cercana de cada una
    std::vector<std::uint8_t> water;    // 1 = agua
    std::vector<float> flow;            // 0 en el vidrio, 1 lejos de la pared
    std::size_t waterCells = 0;

    bool active() const { return !water.empty() && !outside.empty(); }
    bool isWater(std::size_t k) const { return water.empty() || water[k] != 0; }

    void build(const Dish& dish, int nx, int ny, float dx, float flowRamp) {
        const std::size_t n = static_cast<std::size_t>(nx) * ny;
        outside.clear();
        mirror.clear();
        nearest.clear();
        water.assign(n, 1);
        flow.assign(n, 1.0f);
        waterCells = n;
        if (!dish.active())
            return;
        auto centerOf = [&](int i, int j) { return Vec2{(i + 0.5f) * dx, (j + 0.5f) * dx}; };
        auto cellOf = [&](Vec2 q) {
            return static_cast<std::size_t>(wrapIndex(static_cast<int>(std::floor(q.y / dx)), ny)) * nx +
                   static_cast<std::size_t>(wrapIndex(static_cast<int>(std::floor(q.x / dx)), nx));
        };
        for (int j = 0; j < ny; ++j)
            for (int i = 0; i < nx; ++i) {
                const std::size_t k = static_cast<std::size_t>(j) * nx + i;
                const float sd = dish.signedDistance(centerOf(i, j));
                if (sd >= 0.0f) {
                    water[k] = 0;
                    flow[k] = 0.0f;
                } else {
                    const float t = std::min(1.0f, -sd / flowRamp);
                    flow[k] = t * t * (3.0f - 2.0f * t);
                }
            }
        waterCells = 0;
        for (std::size_t k = 0; k < n; ++k)
            waterCells += water[k];
        // Busca una celda de agua desde `q` acercándose al centro.
        auto waterFrom = [&](Vec2 q) {
            std::size_t target = cellOf(q);
            for (float margin = 0.5f * dx; !water[target] && margin < dish.radius; margin += 0.5f * dx)
                target = cellOf(dish.clampInside(q, margin));
            return target;
        };
        for (int j = 0; j < ny; ++j)
            for (int i = 0; i < nx; ++i) {
                const std::size_t k = static_cast<std::size_t>(j) * nx + i;
                if (water[k])
                    continue;
                const Vec2 p = centerOf(i, j);
                const Vec2 d = p - dish.center;
                const float r = std::max(1e-3f, length(d));
                // Imagen especular a través de la pared: r' = 2R − r.
                const float rm = std::max(0.0f, 2.0f * dish.radius - r);
                outside.push_back(static_cast<std::uint32_t>(k));
                mirror.push_back(static_cast<std::uint32_t>(waterFrom(dish.center + d * (rm / r))));
                nearest.push_back(static_cast<std::uint32_t>(waterFrom(p)));
            }
    }
};
