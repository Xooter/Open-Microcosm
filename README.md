# bio

Herramienta de investigación y de estudio que simula vida microscópica real —gotas de agua,
cultivos celulares y preparados vegetales— en C++20 (ver `AGENTS.md` y `docs/`).
Estado actual: **fases 1–12** (la 12, parcial). Una gota con ~25 especies reales (ciliados,
flagelados, amebas, algas, diatomeas, colonias, filamentos, levaduras, hongos, rotíferos, Hydra,
nauplios), bacterias, cianobacterias, fagos y química (O₂, CO₂/pH, fósforo, nitrógeno, toxinas,
señales de quorum), en seis biomas procedurales, con estética de microscopio de campo claro,
enciclopedia y la partida roguelite con genoma de las fases 1–6.

## Compilar, ejecutar y testear

```sh
cmake --preset release && cmake --build --preset release && ./build/release/bio
ctest --test-dir build/release        # o ./build/release/bio_tests
```

Presets: `debug`, `release`, `profile` (RelWithDebInfo + Tracy).
Las dependencias se descargan solas con FetchContent la primera vez.

## Controles

- **Inicio:** Jugar → elegir bioma, especie fundadora y semilla (Euglena y Amoeba se desbloquean
  jugando) · Enciclopedia · Opciones
- Cilios: **W/S** avanzar/retroceder, **A/D** girar
- Flagelo: **W** o click izquierdo para batir; gira hacia el mouse
- Ameba: **click izquierdo** extiende un pseudópodo hacia el mouse; al tocar una célula más chica, la envuelve sola
- **Al dividirte o morir:** Tab/flechas o click para elegir la célula, **Enter** para confirmar
- **V** conjugar con un ciliado compatible cercano (otro tipo de apareamiento)
- **N** nueva partida · **F** cámara que sigue al jugador · **H** ocultar el panel · rueda: zoom · Espacio: pausa
- Microscopio: **J** tinción (rojo Congo, azul de metileno)
- Laboratorio: **Ctrl+click** revolver · **Shift+click** estirar membrana · **T** tinta · **B** detrito ·
  **C** célula · **K** 50 células · **X** borrar · **L** lisar · **Q** apretar
- Overlays de depuración: **F1** velocidad · **F2** O₂ · **F3** nutrientes · **F4** luz · **F5** fosfato ·
  **F6** nitrógeno · **F7** pH · **F8** microcistina · **F9** autoinductor · **F10** fagos

## Estructura

```
src/
  main.cpp              loop con timestep fijo, menús, cámara y herramientas
  core/                 unidades físicas, RNG determinista, profiling, UI (panel, menús, enciclopedia)
  sim/                  FFT, fluido Stokes–Brinkman, campos químicos, partículas, grilla espacial,
                        cuerpos blandos (XPBD sin inercia), contactos, polígonos
  bio/bacteria.*        run & tumble, quimiotaxis, cepas con quorum sensing, fagos, Bdellovibrio
  bio/cyanobacteria.*   filamentos con heterocistos (fijación de N₂), muerte y microcistina
  bio/fungi.*           quitridios, Saprolegnia y Zoophagus (zoosporas e hifas)
  bio/cell.*            creación de células (membrana, núcleo, vacuola, gránulos)
  bio/species.*         las especies como datos (dieta, reproducción, tolerancias, metazoos)
  bio/locomotion.*      cilios, flagelo y pseudópodos; fuerzas del nadador sobre el agua
  bio/feeding.*         vacuolas digestivas, ingestión, digestión y saciedad
  bio/phagocytosis.*    copa fagocítica (cremallera) y fusión de membranas
  bio/metabolism.*      respiración, fermentación, fotosíntesis, P y N, estrés, inanición
  bio/division.*        mitosis: surco de división y forma de reposo de las hijas
  bio/genome.*          genes, mutación, recombinación, herencia y rasgos fisiológicos
  bio/organelles.*      organelos visibles según el genoma (mitocondrias, cloroplastos, lisosomas)
  ai/autopilot.*        comportamiento por especie (quimiocinesis, trampa de ácido, caza, saltos)
  game/world*.cpp       el mundo: medio, cuerpos, ciclo de vida, hongos, química, metazoos
  game/biome.*          biomas y gotas procedurales
  game/encyclopedia.*   descubrimientos y comportamientos observados
  game/run.*            partida: elección de hija/heredero, censo de biomasa, victoria, extinción
  game/progress.*       meta-progresión, enciclopedia y opciones persistentes
  render/               campo claro, óptica del microscopio, instancing, overlays
assets/shaders/         GLSL 330
tests/                  doctest: medio, células, jugador, ciclo de vida, partida, genoma,
                        ecosistema e interacciones
```

## Reproducibilidad

Una corrida se repite **bit a bit** con el mismo binario, la misma máquina, el mismo motor y las
mismas opciones (escena = `.plate` + intervenciones; *Placa > Guardar escena*). Entre máquinas la
reproducibilidad es **estadística** (réplicas): cambian el compilador, la `libm` y la GPU.

Qué opciones de *Preferencias* cambian la trayectoria de la simulación:

| Opción | ¿Cambia la trayectoria? |
|---|---|
| Física de células en la GPU | **Sí**: el redondeo de la GPU difiere del de la CPU (cada motor es determinista por separado). |
| LOD y sub-pasos lejanos | **Sí**: lo que está fuera de la vista se resuelve con menos sub-pasos (la biología no cambia). |
| Hilos | No: el resultado no depende de la cantidad de hilos. |
| Pasos por cuadro | No: solo cuánto se recupera por cuadro si la simulación se atrasa. |
| Calidad, escala de render, óptica, detalle, VSync, tope de FPS, MSAA | No: solo la imagen. |

La exportación (Ctrl+E) anota el motor, el LOD, la versión y la máquina (`procedencia` en el JSON).

## Licencia

BioGame es software libre bajo la **GNU General Public License, versión 3 o posterior**
(`GPL-3.0-or-later`, texto completo en `LICENSE`). Quien distribuya el programa o una versión
modificada tiene que ofrecer el código fuente con la misma licencia. Las dependencias (raylib y
GLFW: zlib; Dear ImGui, ImPlot, EnTT y toml++: MIT; la fuente Roboto: Apache 2.0) son compatibles;
sus licencias se instalan en `licencias/` junto al ejecutable.
