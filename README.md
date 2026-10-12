# bio

Simulación de vida acuática microscópica en C++20 (ver `AGENTS.md` y `docs/`).
Estado actual: **fase 1 — el medio**: agua a bajo Reynolds, química disuelta, detritos,
tinta y 50.000 bacterias con quimiotaxis.

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
- Rueda: zoom · Click derecho + arrastrar: mover cámara · Espacio: pausa

## Estructura

```
src/
  main.cpp              loop con timestep fijo, cámara y herramientas
  core/                 unidades físicas, RNG determinista, profiling, UI de debug
  sim/                  FFT, fluido Stokes–Brinkman, campos químicos, partículas, grilla espacial
  bio/bacteria.*        run & tumble + quimiotaxis temporal (E. coli)
  game/world.*          coordina el medio y los organismos
  render/               campo claro + overlays de campos
tests/                  doctest: incompresibilidad, conservación, determinismo
```
