# bio

Simulación de vida acuática microscópica en C++20 (ver `AGENTS.md` y `docs/`).
Estado actual: **fase 6 — genoma y organelos**: cada célula tiene un genoma (tamaño, motor,
mitocondrias, cloroplastos, lisosomas, tricocistos, coraza, rigidez, tipo de apareamiento) que
cambia cómo se juega; muta en cada mitosis (al elegir hija ves qué cambió), se recombina por
conjugación y parte de él pasa a la siguiente partida. Sobre las fases 1–5.

## Compilar, ejecutar y testear

```sh
cmake --preset release && cmake --build --preset release && ./build/release/bio
ctest --test-dir build/release        # o ./build/release/bio_tests
```

Presets: `debug`, `release`, `profile` (RelWithDebInfo + Tracy).
Las dependencias se descargan solas con FetchContent la primera vez.

## Controles

- **Inicio:** 1/2/3 o click para elegir la especie fundadora (Euglena y Amoeba se desbloquean jugando)
- Cilios: **W/S** avanzar/retroceder, **A/D** girar
- Flagelo: **W** o click izquierdo para batir; gira hacia el mouse
- Ameba: **click izquierdo** extiende un pseudópodo hacia el mouse; al tocar una célula más chica, la envuelve sola
- **Al dividirte o morir:** Tab/flechas o click para elegir la célula, **Enter** para confirmar
- **V** conjugar con un ciliado compatible cercano (otro tipo de apareamiento)
- **N** nueva partida · **F** cámara que sigue al jugador · **H** ocultar el panel · rueda: zoom · Espacio: pausa
- Laboratorio: **Ctrl+click** revolver · **Shift+click** estirar membrana · **T** tinta · **B** detrito ·
  **C** célula · **K** 50 células · **X** borrar · **L** lisar · **Q** apretar · **F1–F4** overlays

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
  bio/metabolism.*      respiración, fermentación, fotosíntesis, crecimiento, inanición
  bio/division.*        mitosis: surco de división y forma de reposo de las hijas
  bio/genome.*          genes, mutación, recombinación, herencia y rasgos fisiológicos
  bio/organelles.*      organelos visibles según el genoma (mitocondrias, cloroplastos, lisosomas)
  ai/autopilot.*        comportamiento mínimo de las células del linaje que no controlás
  game/run.*            partida: elección de hija/heredero, censo de biomasa, victoria, extinción
  game/progress.*       meta-progresión persistente (desbloqueos y récords)
  game/world.*          coordina el medio y los organismos
  render/               campo claro (shader de células) + overlays de campos
assets/shaders/         GLSL 330
tests/                  doctest: incompresibilidad, conservación, determinismo, células
```
