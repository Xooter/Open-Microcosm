#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

// Time-lapse (fase P1, paso 13): una foto cada `intervalHours` horas BIOLÓGICAS de la placa, no por
// cuadro ni por segundo real: la serie de cuadros no depende de los FPS ni de la velocidad de la
// simulación. Si en un cuadro pasan varios intervalos, se toma uno solo (con su hora real en el JSON).
struct TimelapseClock {
    double intervalHours = 0.5;
    double next = 0.0; // próxima hora biológica a fotografiar
    int frame = 0;     // cuadros tomados

    // Empieza a contar desde `hours` (la primera foto, ya).
    void start(double hours) {
        next = hours;
        frame = 0;
    }
    // ¿Toca una foto a la hora `hours`? Si toca, avanza el reloj al próximo múltiplo del intervalo.
    bool due(double hours) {
        if (hours + 1e-9 < next)
            return false;
        const double step = intervalHours > 1e-6 ? intervalHours : 1e-6;
        next += step * std::max(1.0, std::floor((hours - next) / step) + 1.0);
        ++frame;
        return true;
    }
    // Nombre del cuadro: frame_00001.png.
    std::string frameName() const {
        char name[32];
        std::snprintf(name, sizeof name, "frame_%05d.png", frame);
        return name;
    }
};
