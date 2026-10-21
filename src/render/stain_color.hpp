#pragma once

// Colores de las tinciones en el render (game/stain evalúa; esto lo lleva a colores de raylib).

#include "game/stain.hpp"

#include <raylib.h>

#include <algorithm>
#include <initializer_list>

inline Color dyeColor(Rgba c, float alpha) {
    auto b = [](float v) { return static_cast<unsigned char>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
    return {b(c.r), b(c.g), b(c.b), b(alpha)};
}

// El colorante encima del color natural: mezcla los canales y deja al menos la opacidad del colorante.
inline Color overColor(Color base, const StainResult& dye, float opacity = 0.9f) {
    const float t = std::clamp(dye.alpha, 0.0f, 1.0f);
    auto mixc = [&](unsigned char a, float b) { return static_cast<unsigned char>(a + (std::clamp(b, 0.0f, 1.0f) * 255.0f - a) * t); };
    Color o = {mixc(base.r, dye.color.r), mixc(base.g, dye.color.g), mixc(base.b, dye.color.b), base.a};
    o.a = std::max<unsigned char>(base.a, static_cast<unsigned char>(255.0f * opacity * t));
    return o;
}

// Color de una estructura que no es una célula de cuerpo blando (hifas, filamentos, copos) con la
// tinción `st`. En campo claro, de los blancos que tiene, el colorante que más la tiñe (los
// colorantes absorben: domina el más intenso). Con fluorescencia el fondo es oscuro, las emisiones
// de cada blanco se suman (la clorofila roja y el ADN azul con DAPI) y lo que no emite no se ve.
inline Color stainedColor(const StainSpec* st, Color natural, std::initializer_list<StainTarget> targets) {
    if (!st)
        return natural;
    if (st->mode == StainMode::Fluorescence) {
        Rgba sum{0.0f, 0.0f, 0.0f, 0.0f};
        for (const StainTarget t : targets) {
            const StainResult r = evaluateStain(*st, t);
            const float k = std::min(1.0f, 2.5f * r.alpha);
            sum = {sum.r + k * r.color.r, sum.g + k * r.color.g, sum.b + k * r.color.b, std::max(sum.a, k)};
        }
        return sum.a > 0.0f ? dyeColor(sum, sum.a * natural.a / 255.0f) : Color{0, 0, 0, 0};
    }
    StainResult best;
    for (const StainTarget t : targets) {
        const StainResult r = evaluateStain(*st, t);
        if (r.alpha > best.alpha)
            best = r;
    }
    return overColor(natural, best);
}
