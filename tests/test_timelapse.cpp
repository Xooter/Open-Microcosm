// Time-lapse por tiempo biológico (fase P1, paso 13).
#include "game/timelapse.hpp"

#include <doctest/doctest.h>

TEST_CASE("time-lapse: un cuadro por intervalo biológico, sin importar los cuadros por segundo") {
    // La misma media hora biológica mirada a 10 y a 1000 "cuadros": los mismos 7 cuadros (0, 5, …, 30 min).
    for (const int frames : {10, 1000}) {
        TimelapseClock c;
        c.intervalHours = 5.0 / 60.0;
        c.start(0.0);
        for (int i = 0; i <= frames; ++i)
            c.due(0.5 * i / frames);
        CHECK(c.frame == 7);
    }
    // Si en un cuadro pasan varios intervalos se toma una sola foto y el reloj sigue en la grilla.
    TimelapseClock c;
    c.intervalHours = 0.5;
    c.start(1.0);
    CHECK(c.due(1.0));
    CHECK_FALSE(c.due(1.2));
    CHECK(c.due(3.3));
    CHECK(c.next == doctest::Approx(3.5));
    CHECK(c.frameName() == "frame_00002.png");
}
