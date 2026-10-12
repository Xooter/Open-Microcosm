# bio

Simulación de vida acuática microscópica en C++20 (ver `AGENTS.md` y `docs/`).
Estado actual: **fase 3 — jugador**: una célula controlable con locomoción real (cilios,
flagelo o pseudópodos), que come bacterias y detritos y, como ameba, envuelve células enteras por
fagocitosis. Todo sobre las fases 1–2 (agua a bajo Reynolds, química, bacterias con quimiotaxis,
células gelatinosas que chocan y recuperan su forma).

## Compilar, ejecutar y testear

```sh
cmake --preset release && cmake --build --preset release && ./build/release/bio
ctest --test-dir build/release        # o ./build/release/bio_tests
```

Presets: `debug`, `release`, `profile` (RelWithDebInfo + Tracy).
Las dependencias se descargan solas con FetchContent la primera vez.

## Controles

- **1** cilios (*Paramecium*) · **2** flagelo (*Euglena*) · **3** pseudópodos (*Amoeba*)
- Cilios: **W/S** avanzar/retroceder, **A/D** girar
- Flagelo: **W** o click izquierdo para batir; gira hacia el mouse
- Ameba: **click izquierdo** extiende un pseudópodo hacia el mouse; al tocar una célula más chica, la envuelve sola
- **F** cámara que sigue al jugador · rueda: zoom · click derecho + arrastrar: mover cámara · Espacio: pausa
- Laboratorio: **Ctrl+click** revolver el agua · **Shift+click** estirar una membrana · **T** tinta · **B** detrito
- **C** crear célula · **K** 50 células · **X** borrar célula · **Q** apretar células hacia el cursor
- **F1–F4** overlays (velocidad del agua, O₂, nutrientes, luz)

## Estructura

```
src/
  main.cpp              loop con timestep fijo, cámara y herramientas
  core/                 unidades físicas, RNG determinista, profiling, UI de debug
  sim/                  FFT, fluido Stokes–Brinkman, campos químicos, partículas, grilla espacial,
                        cuerpos blandos (XPBD sin inercia), contactos, polígonos
  bio/bacteria.*        run & tumble + quimiotaxis temporal (E. coli)
  bio/cell.*            creación de células (membrana, núcleo, vacuola, gránulos)
  bio/locomotion.*      cilios, flagelo y pseudópodos; fuerzas del nadador sobre el agua
  bio/feeding.*         vacuolas digestivas, ingestión y digestión
  bio/phagocytosis.*    copa fagocítica (cremallera) y fusión de membranas
  game/world.*          coordina el medio y los organismos
  render/               campo claro (shader de células) + overlays de campos
assets/shaders/         GLSL 330
tests/                  doctest: incompresibilidad, conservación, determinismo, células
```
