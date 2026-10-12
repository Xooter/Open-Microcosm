# bio

Plantilla de juego/simulación en C++20 con muchas partículas y reglas biológicas
(demo inicial: *Particle Life*, especies que se atraen o repelen de forma asimétrica).

## Compilar

```sh
cmake --preset release && cmake --build --preset release && ./build/release/bio
```

Presets: `debug`, `release`, `profile` (RelWithDebInfo + Tracy).
Las dependencias se descargan solas con FetchContent la primera vez.

## Controles

- Rueda: zoom · Click derecho + arrastrar: mover cámara · Espacio: pausa
- Panel: sliders de reglas, "Nuevas reglas" (matriz aleatoria), "Respawn"

## Librerías

| Lib | Para qué |
|---|---|
| raylib 5.5 | ventana, input, render batcheado, audio |
| raygui 4.0 | UI de debug para tunear parámetros en vivo |
| EnTT 3.14 | ECS para entidades de alto nivel (organismos, colonias) — enlazada, aún sin uso |
| OpenMP | paralelismo multinúcleo en los loops de simulación |
| Tracy 0.11 | profiler de frames (solo con `-DBIO_PROFILING=ON`) |

## Estructura

```
src/
  main.cpp             loop principal con timestep fijo
  core/profiling.hpp   macros PROFILE_SCOPE / PROFILE_FRAME
  core/ui.*            panel raygui
  sim/particles.hpp    datos de partículas en SoA
  sim/rules.*          matriz de interacción entre especies + kernel de fuerza
  sim/world.*          grilla espacial, fuerzas (OpenMP), integración, mundo toroidal
  render/renderer.*    dibujo de partículas
```

## Decisiones de performance

- **SoA** en lugar de array de structs: loops vectorizables y cache-friendly.
- **Grilla espacial** con counting sort cada paso: vecinos en O(n·k), y copias
  ordenadas por celda para leer memoria contigua.
- **Timestep fijo** desacoplado del render.
- `-O3 -ffast-math -march=native` + LTO en Release.

## Próximos pasos sugeridos

- Agregar energía/edad/genoma como nuevos arrays en `Particles` (metabolismo,
  reproducción, muerte) — reemplazar el `resize` por un pool con swap-and-pop.
- Más de ~100k partículas: render con instancing/VBO propio y/o sim en compute shader.
