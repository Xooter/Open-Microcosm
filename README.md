# bio

Simulación de vida acuática microscópica en C++20 (ver `AGENTS.md` y `docs/`).
Estado actual: **fase 2 — célula soft-body**: células gelatinosas con membrana, núcleo, vacuola
contráctil y gránulos, que chocan, se aplastan y recuperan su forma, sobre el medio de la
fase 1 (agua a bajo Reynolds, química disuelta, detritos y 50.000 bacterias con quimiotaxis).

## Compilar, ejecutar y testear

```sh
cmake --preset release && cmake --build --preset release && ./build/release/bio
ctest --test-dir build/release        # o ./build/release/bio_tests
```

Presets: `debug`, `release`, `profile` (RelWithDebInfo + Tracy).
Las dependencias se descargan solas con FetchContent la primera vez.

## Controles

- Click izquierdo + arrastrar: revolver el agua
- T (mantener): soltar tinta · D: soltar un detrito
- F1 velocidad · F2 O₂ · F3 nutrientes · F4 luz (overlays de debug)
- C: crear una célula · K: escena de 50 células · X: borrar la célula bajo el cursor
- Shift + click izquierdo + arrastrar: estirar una membrana · Q (mantener): apretar las células hacia el cursor
- Rueda: zoom · Click derecho + arrastrar: mover cámara · Espacio: pausa

## Estructura

```
src/
  main.cpp              loop con timestep fijo, cámara y herramientas
  core/                 unidades físicas, RNG determinista, profiling, UI de debug
  sim/                  FFT, fluido Stokes–Brinkman, campos químicos, partículas, grilla espacial,
                        cuerpos blandos (XPBD sin inercia), contactos, polígonos
  bio/bacteria.*        run & tumble + quimiotaxis temporal (E. coli)
  bio/cell.*            creación de células (membrana, núcleo, vacuola, gránulos)
  game/world.*          coordina el medio y los organismos
  render/               campo claro (shader de células) + overlays de campos
assets/shaders/         GLSL 330
tests/                  doctest: incompresibilidad, conservación, determinismo, células
```
