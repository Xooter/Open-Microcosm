# AGENTS.md — BioGame

Guía para agentes (y humanos) que trabajen en este repo. Leela completa antes de tocar código.

## Visión

BioGame es un **sandbox educativo, científico y de código abierto** que simula vida acuática
microscópica **real**. Las células son cuerpos gelatinosos y
semitransparentes, vistas como en un microscopio, con sus organelos visibles. Nadan, comen por
fagocitosis, crecen, se dividen, se conjugan, mueren y se lisan, todo emergiendo de una
simulación física de partículas y no de animaciones.

Documentos de referencia:
- `docs/gameplay.md`: decisiones de diseño (las opciones marcadas con `[x]` son las elegidas).
- `docs/roadmap.md`: fases, criterios de "hecho" y decisiones tomadas.

## Sandbox

No hay partida, puntaje ni victoria: es un laboratorio. El usuario arma una gota, la observa e
interviene.

- **Gota:** bioma + seed (reproducible), con o sin poblaciones iniciales. Se puede spawnear
  cualquier ser del catálogo (barra inferior) y cambiar el ambiente en vivo desde el panel de
  laboratorio (`core/lab_panel`): secciones plegables de simulación, ambiente, macro y
  micronutrientes, química, microbiología, herramientas, cuerpos blandos, ser controlado y
  estadísticas (con los totales conservados de P, N, Si y Fe).
- **Controlar un ser** (`P` sobre una célula): cada especie conserva su **locomoción física**:
  flagelo (impulso), cilios (deslizamiento), pseudópodo (clic y arrastre) y ondulación. Se respeta
  el **bajo número de Reynolds**: no hay inercia. Al dividirse se sigue con la madre; si el ser
  muere, se puede controlar otro.
- **Modo Explorar** (`Tab`): click sobre cualquier cosa y se abre su ficha (qué es, qué hace y por
  qué). La enciclopedia se llena con lo que la cámara ve.
- **Comer es físico:** se envuelve a la presa con la membrana hasta cerrar una vacuola.
- **Metabolismo real:** respiración con O₂, fermentación sin O₂ y fotosíntesis con luz, limitado
  por P, N y, según la especie, **sílice** (diatomeas), **hierro** (fotótrofas) y **vitamina B₁₂**
  (auxótrofas: *Euglena*, diatomeas), que aportan las bacterias.
- **Genoma:** los organelos y rasgos son genes que mutan en la mitosis y se mezclan en la
  conjugación. Los organelos ocupan espacio físico dentro de la célula.
- **Ecosistema:** carnívoros (cazadores, envolvedores, emboscadores, parásitos y jefes
  multicelulares), herbívoros (algívoros y bacterívoros), algas, hongos, bacterias y fagos.
- **Rejugabilidad:** gotas procedurales por seed y biomas distintos (charco, estanque, musgo,
  intestino de termita, fuente hidrotermal, hielo).
- **Estilo visual:** microscopio de **campo claro** (fondo claro, células semitransparentes).

## Principio rector: realismo estricto

- Solo especies y mecanismos **reales**. Antes de implementar un comportamiento, verificá que
  exista en la biología; si se simplifica, dejá un comentario con lo que hace el organismo real.
- **Tiempo:** la física (nado, agua, difusión, intercambio de gases) corre en tiempo REAL
  (`units::kTimeScale = 1`). La biología (metabolismo, crecimiento, digestión, división) se
  comprime con **una sola constante**, `units::kBioTimeScale` (60: 1 min real = 1 h biológica),
  respetando las proporciones entre procesos y especies. Consecuencias que hay que mantener:
  - lo digerido rinde ×`bioTimeScale` (la alimentación es mecánica, en tiempo real);
  - el intercambio de O₂ de cada célula con el agua se hace a la tasa real (÷`bioTimeScale`).
- **Masas:** pmol equivalentes de glucosa de materia SECA (`units::kDryMassFraction` = 0,3).
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
- **Placa** (`sim/dish.hpp`, `World::dish()`): el mundo es un pocillo circular de vidrio
  centrado en la grilla (radio = mitad del lado menor − `WorldConfig::dishMargin`); su pared es
  el límite del mundo. Las grillas siguen siendo periódicas por dentro (tamaños potencia de 2,
  la FFT lo necesita), pero nada vive ni cruza fuera del círculo:
  - cuerpos blandos: `soft_body::confineToDish` en cada iteración de XPBD (también en GPU,
    `solve.comp`); los estadísticos se trasladan enteros (`pushIntoDish`);
  - partículas (bacterias, detritos, esporas, *Bdellovibrio*, cianobacterias): `Dish::bounce`;
    las hifas no crecen hacia el vidrio; los spawns usan `World::randomPoint`;
  - agua: máscara de velocidad (`DishMask::flow`), cero en el vidrio y rampa junto a la pared;
  - química: las celdas de vidrio son fantasmas con el valor de su imagen especular (no-flujo
    por imágenes) y la masa se conserva reescalando el agua (`ChemField::setMask`). Los totales
    usan `ChemField::sum()` (solo agua), nunca `grid().sum()`.
  - Con `WorldConfig::dish = false` vuelve el mundo toroidal (bordes envueltos).
- **Agua:** Stokes–Brinkman resuelto exacto con FFT (`sim/fluid`). No hay inercia: la velocidad
  es la respuesta instantánea a las fuerzas. El espesor de la película (`gapHeight`) controla
  qué tan locales son las corrientes.
- **Química:** difusión exacta en Fourier + advección semi-lagrangiana (`sim/chem_field`). El O₂
  entra por intercambio con el aire en toda la superficie. Macronutrientes (P, N, Si) y
  micronutrientes (Fe, B₁₂) son campos disueltos; cada célula toma su cuota con cinética de Monod
  (`bio/metabolism`) y el crecimiento lo limita el más escaso (Liebig). El Si de una diatomea
  muerta queda en su frústula (`AmbientParticles::mass`) y se redisuelve despacio.
- **Luz:** la lámpara ilumina uniforme y los objetos la atenúan (Beer–Lambert).

### Capas de simulación (tiers por escala)

| Tier | Qué | Representación |
|---|---|---|
| Medio | Agua, corrientes y química | Grilla de fluido Stokes–Brinkman (FFT) + campos escalares (O₂, CO₂/pH, luz, materia orgánica, fosfato, nitrógeno, toxinas, autoinductor, fagos) |
| Ambiental | Detritos, bacterias, cianobacterias, hongos | Partículas simples (bacterias, *Bdellovibrio*, zoosporas), cadenas (cianobacterias) y árboles de nodos fijos (hifas) |
| Célula | Protistas, algas y levaduras | Soft-body: anillo de membrana + partículas internas, resuelto con XPBD |
| Compuesto | Colonias, filamentos y metazoos | Soft-bodies unidos por adhesiones (`World::AdhesionLink`; rígidas con diagonales en metazoos) |

LOD (`World::setFocus`, tres niveles): cerca de la vista, detalle completo; más lejos, 1 de cada
`lodStride` sub-pasos de XPBD con un paso más largo (las tasas [1/s] no cambian de significado);
fuera de pantalla, estadístico (se trasladan como sólidos, sin XPBD ni contactos). División,
fagocitosis, conjugación y presas sujetas siempre en detalle. Sin foco (tests), todo detallado.
Contactos: solo los puntos de membrana a menos de su radio de contacto (+ holgura) de otro
cuerpo, medido una vez por sub-paso; dos pasadas por sub-paso (`SoftBodyTuning::contactPasses`).
Las partes de un mismo organismo no chocan entre sí (`ContactMember::group`).

### Cuerpos blandos (`sim/soft_body`, `sim/contacts`)

- **XPBD sin inercia:** cada paso predice x* = x + v_externa·dt, sin velocidad acumulada, y
  después proyecta las restricciones. Cada restricción relaja su error con una **tasa [1/s]**:
  corrección = 1 − exp(−tasa·h). Así el resultado no depende del sub-paso y el comportamiento
  es viscoelástico.
- **Restricciones por cuerpo:**
  - largo de aristas;
  - flexión por *shape matching* local en ventanas de 5 puntos (la rotación nunca refleja, así
    que deshace pliegues; una restricción de distancia i↔i+2 no lo hace);
  - área (agua incompresible);
  - memoria de forma global (`shapeRate`: película rígida alta, córtex ameboide ~0);
  - anclaje de organelos al citoesqueleto.
- **Contactos:** punto contra arista, con esquema Jacobi. Se **intercalan** con las restricciones
  en cada iteración del sub-paso. Al final del paso, una pasada saca cualquier partícula de
  membrana que haya quedado dentro de otra célula.
- Las bacterias y los detritos se excluyen del interior de las células.
- **Membrana plástica** (ameba, `soft_body::makePlastic`): sin tensión de aristas ni memoria de
  forma; tensión cortical (flujo de curvatura) que alisa arrugas sin frenar lóbulos; área
  conservada en ~1 s (el citoplasma fluye); remallado que agrega o quita membrana. Su membrana
  tiene más masa inversa, así que cede en los contactos.
- **Topología:** `replaceRing` / `resizeRing` cambian el anillo (fusión, inserción de membrana).
  Los eventos de topología se aplican al final del paso.

### Ciclo de vida (`bio/metabolism`, `bio/division`, `World::updateLifecycle`)

- **Metabolismo** (componente `Metabolism`): biomasa, reservas y edad. Respiración (~30 ATP por
  glucosa) o fermentación (2 ATP, 15× más glucosa) según el O₂ local; mantenimiento proporcional a
  la biomasa y a la actividad; crecimiento con rendimiento 0,6 y tasa máxima; fotosíntesis con la
  luz local. Sin reservas hay autofagia; con menos de la mitad de la biomasa de nacimiento, muere.
- **Tamaño:** el área de reposo es la de la biomasa más el contenido de las vacuolas.
- **Mitosis** (componente `Dividing`): con biomasa ×2 y reservas para el costo (el del jugador es
  el más caro), se duplican núcleo y vacuola contráctil, la forma de reposo se transforma de la
  madre en dos hijas (`divisionRestShape`), el surco estrangula y, al cerrarse, el anillo se parte.
  Las hijas se re-muestrean (`resampleRing`) y vuelven a la forma de su especie (`CellShape`).
- **Lisis:** ~40% del contenido se disuelve en nutrientes y el resto queda como detritos.

### Genoma (`bio/genome`, `bio/organelles`)

- Componente `Heredity` = `Genome` (genes cuantitativos, relativos a la especie) + `Traits`
  (multiplicadores fisiológicos derivados con `traitsOf`). `World::applyTraits` los vuelca en
  `Locomotion::power`, `Digestion::rateScale`, `Metabolism` y la forma (`CellShape`).
- `geneAllowed` impone la biología: cloroplastos solo en Euglena, tricocistos solo en ciliados,
  coraza solo en flagelados (lórica) y amebas (testa). Sin endosimbiosis, un gen ausente no
  aparece por mutación.
- Mitosis: cada hija muta su propia copia. Conjugación (`Conjugating`): requiere tipos de
  apareamiento distintos; ambos exconjugantes quedan con el mismo genotipo recombinado.
- Los organelos son partículas interiores con `BodyParticles::tag`; ocupan lugar (`Traits::volume`
  agranda la célula) y cuestan mantenimiento.
- Defensas: coraza ≥ 0,5 impide la fagocitosis; los tricocistos pueden rechazar una copa
  (componente `Discharge`, el depredador retrocede).

### Ecosistema (`bio/species`, `ai/autopilot`, `World::perceive`)

- Las especies son datos (`SpeciesInfo`): locomoción, forma, dieta (`preyMask`,
  `mouthPreyMask`), huida (`fleeMask`), modo de alimentación (`FeedingMode`), reproducción,
  quistes y rasgos sésiles (`sessile`, `stalkLength`, `contractileStalk`, `tentacleReach`,
  `neckReach`, `walker`). Nada de código por especie salvo el render.
- Sésiles (`Anchored`): el tallo sostiene el polo posterior; el espasmonema se contrae ante
  perturbaciones. Sus hijas nadan como telotrocos (`Telotroch`) y luego se fijan.
- Suctorios: `Captured` sujeta a la presa y le drena biomasa en tiempo biológico.
- Todos los filtradores (`FeedingMode::OralGroove`) generan su corriente oral en el agua; las
  células móviles sienten solo el flujo de fondo.
- Ciliados: reacción de evitación ante amenazas y choques del polo anterior (mecanorrecepción).
  *Paramecium* además se acumula donde hay CO₂ (`acidTrap`, Jennings): los enjambres emergen.
- Depredación: `handlingHours` (tiempo de manipulación entre presas enteras, `Digestion::handling`)
  y saciedad por volumen ingerido (`satiated`, `World::isSatiated`).
- Algas: filamentos (`filament`: crecen a lo largo con `setRestAreaAxial` y se dividen por tabique),
  colonias (`colonyDaughters`), diatomeas (`glider`, `frustule`), `dryMassScale` para cuerpos
  huecos o vacuolados. Fotótrofas cuentan P y N (`Metabolism::phosphorus/nitrogen`), conservados:
  `World::totalPhosphorus()`; `totalNitrogen()` solo cambia por la fijación de N₂.
- Cianobacterias (`bio/cyanobacteria`): heterocistos que fijan N₂ (inhibidos por O₂) y liberan
  amonio; mueren y liberan microcistina (`toxin_`), dañina para consumidores.
- Hongos (`bio/fungi`, `game/world_fungi.cpp`): quitridios (`ChytridInfection` sobre algas),
  *Saprolegnia* (exoenzimas aceleran la descomposición junto a las hifas), *Zoophagus*
  (`Trapped`); levaduras como especie osmotrófica con gemación (`budding`).
- Química e interacciones microbianas (`game/world_chem.cpp`): pH del sistema carbonato con
  alcalinidad fija; rango de pH por especie (`phMin/phMax`); quorum sensing de *C. violaceum*
  (`Bacteria::strain/quorum`, violaceína en `Digestion::toxin`); fagos líticos como campo de
  concentración (`phage_`) con mutantes resistentes; *Bdellovibrio* libres y bdeloplastos.
- Metazoos (`game/world_metazoa.cpp`): `Compound`/`Segment` (las partes no tienen metabolismo,
  se mueven con el organismo y no chocan entre sí: `ContactMember::group`), `Egg`
  (partenogénesis del rotífero), `Tentacles`/`Stung` (Hydra), llegadas de nauplios.
- `World::deathTally()` y `World::observations()` registran causas de muerte y comportamientos
  vistos (base de la enciclopedia).

### Ambiente, biomas y enciclopedia (`game/biome`, `game/encyclopedia`)

- `MediumParams` (en `WorldConfig::medium`): temperatura (Q10 = 2 en todo lo biológico vía
  `World::bioScale()`, viscosidad y O₂ de saturación), salinidad, anoxia, alcalinidad, P, N.
  Las tolerancias son datos de la especie y se pagan como mantenimiento extra (estrés).
- `makeDrop(bioma, seed)` arma la gota: química, pesos de población por especie
  (`WorldConfig::speciesWeight`), hongos, cianobacterias (rango térmico) y manchas iniciales.
- La enciclopedia se llena con lo que la cámara ve y con `deathTally`/`observations`.

### Render (`render/`)

- Campo claro: cobertura de las células → shader con borde óptico y halo. La pared de la placa
  se dibuja con `dish_glass.fs` (vidrio, canto oscuro, línea de Becke y menisco) antes del
  post-proceso. Con placa hay una sola copia del mundo (sin toro no se repite al alejar). Todo ocurre en un
  mismo plano (no hay plano focal). El campo se dibuja en una textura y pasa por
  `microscope_post.fs` (viñeta, aberración cromática, grano).
- Bacterias con instancing (`InstancedParticles`): un buffer por frame, una llamada por copia
  del mundo toroidal.

### Sesión y control (`game/run`, `game/progress`, `ai/autopilot`)

- `World` no sabe de reglas de sesión: emite `WorldEvent` (el ser controlado se dividió, conjugó o
  murió) y ofrece `census()`. `Run` los cuenta y mantiene el censo; no hay victoria ni derrota.
- Componente `Lineage` (sesión y generación). Las hijas que no se controlan llevan `Autopilot`
  (deambular y comer). Movimiento, copas fagocíticas y alimentación son genéricos para toda célula
  con `Locomotion`; el ser controlado solo se distingue por de dónde salen sus controles.
- La alimentación de muchas bocas usa una grilla de bacterias; lo comido se marca y se quita todo
  junto al final del paso. Las fagocitosis (que destruyen entidades) se aplican después de
  recorrer la vista de EnTT.
- `Progress` guarda solo la enciclopedia (especies vistas, comportamientos, biomas) como texto
  clave=valor en `defaultProgressPath()`. Todo el catálogo está disponible desde el inicio.

### Jugador (`bio/locomotion`, `bio/feeding`, `bio/phagocytosis`)

- **Locomoción:** suma velocidad a las partículas del cuerpo (sin inercia), con velocidades de
  laboratorio: cilios ~600 µm/s, flagelo ~80 µm/s con bamboleo, pseudópodo ~6 µm/s.
- **Agua:** el nadador ejerce un par de fuerzas opuestas (fuerza neta cero). El agua se resuelve
  dos veces: sin las fuerzas del nadador (`Fluid::backgroundAt`, que es lo que él siente) y con
  ellas (lo que siente todo lo demás). Si no, el nadador se arrastraría con su propio flujo.
- **Comer:** el ciliado por el citostoma (con corriente oral y caudal máximo), el flagelado por el
  polo anterior (despacio) y la ameba por contacto. Lo ingerido forma **vacuolas digestivas**
  (organelos) que se cierran, se digieren y se expulsan.
- **Fagocitosis de células** (ameba, componente `PhagocyticCup`): el contacto con una presa
  elegible inicia la copa; la presa queda sujeta (unión ligando–receptor); el frente de cada labio
  avanza y la membrana se agrega en el labio; cuando los labios se tocan, `findClosure` valida
  que el anillo exterior conserve orientación y área, y la presa pasa a un fagosoma.

### Datos

- **Partículas en SoA**, en dos pools:
  - `sim/particles.hpp` (ambientales: tinta, detritos): compacto, con swap-and-pop.
  - `sim/body_particles.hpp` (membranas, organelos, gránulos): **slots estables** con lista
    libre, así el índice sirve como ID en las restricciones. Cada partícula tiene tipo y dueño
    (entidad de célula). Las posiciones de un cuerpo son contiguas (no se envuelven por
    partícula); el cuerpo entero se traslada cuando su centroide cruza un borde.
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
  core/             profiling, UI y panel de laboratorio, utilidades
  sim/              fft, fluid, chem_field, grid2d, particles, body_particles, soft_body (XPBD),
                    contacts, polygon, spatial_grid
  bio/              cell, membrane, metabolism, lifecycle, genome, organelles, species
  ai/               autopilot (deambular); luego quimiotaxis, caza, huida, fototaxis
  game/             sesión de sandbox, linaje, enciclopedia, biomas
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
