# AGENTS.md — BioGame

Guía para agentes (y humanos) que trabajen en este repo. Leela completa antes de tocar código.

## Visión

BioGame simula vida acuática microscópica **real**. Las células son cuerpos gelatinosos y
semitransparentes, vistas como en un microscopio, con sus organelos visibles. Nadan, comen por
fagocitosis, crecen, se dividen, se conjugan, mueren y se lisan, todo emergiendo de una
simulación física de partículas y no de animaciones.

Documentos de referencia:
- `docs/gameplay.md`: decisiones de diseño (las opciones marcadas con `[x]` son las elegidas).
- `docs/roadmap.md`: fases, criterios de "hecho" y decisiones tomadas.

## Jugabilidad

- El jugador controla **una célula de un linaje**. Al dividirse elige con qué hija seguir y, al
  morir, salta a un descendiente vivo.
- **Locomoción física según el tipo de célula:** flagelo (impulso), cilios (deslizamiento) y
  pseudópodo (clic y arrastre). Se respeta el **bajo número de Reynolds**: no hay inercia.
- **Comer es físico:** se envuelve a la presa con la membrana hasta cerrar una vacuola.
- **Metabolismo real:** respiración con O₂, fermentación sin O₂ y fotosíntesis con luz.
- **Progresión por genoma:** los organelos y rasgos son genes que mutan en la mitosis y se mezclan
  en la conjugación. Los organelos ocupan espacio físico dentro de la célula.
- **Reproducción:** cada especie se reproduce como en la realidad. La del jugador (mitosis o
  conjugación) es la más costosa.
- **Ecosistema:** carnívoros (cazadores, envolvedores, emboscadores, parásitos y jefes
  multicelulares), herbívoros (algívoros y bacterívoros), algas, hongos, bacterias y fagos.
- **Estilo visual:** microscopio de **campo claro** (fondo claro, células semitransparentes).

## Objetivo final

- **Por partida (roguelite):** llevar a tu linaje a dominar el ecosistema de la gota (un % de la
  biomasa total). La partida termina si el linaje se extingue.
- **Entre partidas:** parte del genoma se hereda, se desbloquean especies iniciales y se completa
  la **enciclopedia** de especies y comportamientos observados.
- **Rejugabilidad:** gotas procedurales por seed y biomas distintos (charco, estanque, musgo,
  intestino de termita, fuente hidrotermal, hielo).

## Principio rector: realismo estricto

- Solo especies y mecanismos **reales**. Antes de implementar un comportamiento, verificá que
  exista en la biología; si se simplifica, dejá un comentario con lo que hace el organismo real.
- Las escalas de tiempo se comprimen con **una sola constante global**, respetando las
  proporciones entre especies (una bacteria se divide mucho más rápido que un paramecio).
- Unidades internas: **micrómetros (µm)** y **segundos de simulación**. Las conversiones a
  píxeles van solo en el render.
- No hay inercia a escala de célula: la velocidad depende de la fuerza de propulsión actual, más
  el arrastre del fluido.

## Librerías

Usá **solo** estas. Para agregar otra, primero hay que proponerla y esperar aprobación.

| Lib | Versión | Uso |
|---|---|---|
| raylib | 5.5 | Ventana, input, render 2D, shaders GLSL 330 y audio |
| raygui | 4.0 | UI de debug y menús simples |
| EnTT | 3.14 | ECS para organismos y entidades de alto nivel |
| OpenMP | sistema | Paralelismo de los loops de simulación |
| Tracy | 0.11 | Profiling (preset `profile`) |

| doctest | 2.4.11 | Tests de invariantes físicos (`tests/`, target `bio_tests`) |

Candidatas aprobables cuando su fase lo requiera: **toml++** (datos de especies, fase 7) y
**FastNoiseLite** (generación procedural, fase 11).

Todas las dependencias se traen con `FetchContent` y versión fija en `CMakeLists.txt`. No se usan
paquetes del sistema, salvo OpenMP.

## Build

```sh
cmake --preset release && cmake --build --preset release && ./build/release/bio
```

Hay presets `debug`, `release` y `profile`. El código tiene que compilar **sin warnings** con
`-Wall -Wextra -Wpedantic`, y `ctest --test-dir build/release` tiene que pasar.

## Arquitectura

### Mundo

- **Vista cenital** (se mira desde arriba, como en el microscopio).
- **Mundo toroidal:** los bordes se envuelven. Todas las grillas son periódicas, con tamaños
  potencia de 2.
- **Agua:** Stokes–Brinkman resuelto exacto con FFT (`sim/fluid`). No hay inercia: la velocidad
  es la respuesta instantánea a las fuerzas. El espesor de la película (`gapHeight`) controla
  qué tan locales son las corrientes.
- **Química:** difusión exacta en Fourier + advección semi-lagrangiana (`sim/chem_field`). El O₂
  entra por intercambio con el aire en toda la superficie.
- **Luz:** la lámpara ilumina uniforme y los objetos la atenúan (Beer–Lambert).

### Capas de simulación (tiers por escala)

| Tier | Qué | Representación |
|---|---|---|
| Medio | Agua, corrientes y química | Grilla de fluido Stokes–Brinkman (FFT) + campos escalares (O₂, luz, nutrientes, señales) |
| Ambiental | Nutrientes, detritos y bacterias | Partículas simples con browniano y arrastre (las bacterias son agentes livianos) |
| Célula | Protistas, algas y levaduras | Soft-body: anillo de membrana + partículas internas, resuelto con XPBD |
| Compuesto | Colonias, filamentos y multicelulares | Varios soft-bodies unidos por restricciones de adhesión |

El LOD (fase 9) degrada las células lejanas a menos partículas o a simulación estadística.

### Datos

- **Partículas en SoA** (`src/sim/particles.hpp`): un array contiguo por campo. Toda partícula
  tiene tipo (membrana, citoplasma, organelo, ambiental) y dueño (entidad de célula).
- **Organismos en EnTT:** una célula es una entidad con componentes (membrana: rango de partículas
  y área objetivo; metabolismo; genoma; especie; IA). **Nunca** pongas partículas individuales
  como entidades del ECS.
- **Handles estables:** las partículas se mueven en memoria (swap-and-pop, reordenamiento por
  celda), así que las referencias entre sistemas usan IDs estables con tabla de indirección,
  nunca índices crudos guardados entre frames.
- **Especies data-driven:** los parámetros de cada especie (tamaño, velocidad, dieta,
  reproducción, comportamiento) viven en datos, no en código.

### Loop

- Timestep **fijo** (`kFixedDt`), desacoplado del render. Se permiten sub-pasos de XPBD dentro
  de cada paso.
- Orden de cada paso: fluido → química → fuerzas y propulsión → restricciones XPBD → colisiones
  → metabolismo y ciclo de vida → IA → eventos de topología (fagocitosis, división, lisis,
  fusión) aplicados al final, nunca a mitad de un paso.
- **Determinismo por seed:** toda aleatoriedad sale de `Rng` (`core/rng.hpp`), que se calcula
  a partir de (seed, stream, índice, paso), así que no depende del hilo que lo ejecuta. Nada de
  `rand()` ni de `std::random_device` dentro de la simulación. Los loops paralelos solo escriben
  en su propio índice. El test de determinismo compara corridas con 1 y 4 hilos.

### Estructura de carpetas (objetivo)

```
src/
  main.cpp          loop principal
  core/             profiling, UI de debug, utilidades
  sim/              fft, fluid, chem_field, grid2d, particles, spatial_grid, constraints (XPBD)
  bio/              cell, membrane, metabolism, lifecycle, genome, organelles, species
  ai/               quimiotaxis, caza, huida, fototaxis
  game/             linaje, run roguelite, objetivo de biomasa, enciclopedia
  render/           renderer, shaders de membrana, post-proceso de microscopio
assets/
  shaders/          GLSL 330
  species/          datos de especies
docs/               gameplay.md, roadmap.md
tests/              doctest
```

`biosim` (todo menos `main`, `render/` y `core/ui`) es una librería sin raylib: la usan el juego
y los tests.

`sim/` no conoce a `bio/`, y `bio/` no conoce a `render/`. El render solo lee estado de
simulación.

## Reglas de performance

- En los loops calientes no se hace ninguna asignación de memoria por frame: se reusan buffers.
- Paralelizá con OpenMP solo cuando cada iteración escribe en su propio índice. Si hay escrituras
  cruzadas, usá buffers por hilo y una reducción.
- Toda función de simulación nueva lleva `PROFILE_SCOPE`. Medí con Tracy antes de optimizar.
- Objetivo: **60 FPS** con el contenido de la fase en curso en hardware de escritorio medio.

## Convenciones de código

- C++20. Formato con `.clang-format` (LLVM, 4 espacios, 100 columnas).
- Tipos en `PascalCase`, funciones y variables en `camelCase`, miembros privados con sufijo `_`,
  constantes `kPascalCase`.
- Comentarios en español, breves, explicando el **por qué** o la biología real detrás.
- Las features medianas y grandes se planifican antes con la skill `plan` y se implementan en el
  orden del roadmap.
