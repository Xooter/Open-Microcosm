# AGENTS.md — BioGame

Guía para agentes (y humanos) que trabajen en este repo. Leela completa antes de tocar código.

## Visión

BioGame es una **herramienta de investigación y de estudio, de código abierto**, que simula vida
microscópica **real** como se ve en un microscopio de laboratorio: gotas de agua con sus
microorganismos, **cultivos celulares** (células humanas y animales en placa) y **preparados
vegetales** (tejidos con pared celular). Las células son cuerpos gelatinosos y semitransparentes,
con sus organelos visibles. Nadan o migran, comen, crecen, se dividen, se conjugan, mueren y se
lisan, todo emergiendo de una simulación física de partículas y no de animaciones.

Dos usos con el mismo núcleo de simulación:
- **Investigación:** simulaciones complejas, reproducibles y medibles (series, balances,
  exportación con procedencia, réplicas), también **sin ventana** (`biosim`, `bio_bench`; a futuro,
  bindings de Python). Lo gráfico es opcional: nada de la simulación depende del render.
- **Enseñanza:** mostrar a alumnos lo que pasa, con la imagen del microscopio, fichas que explican
  qué es cada cosa y por qué, y lecciones guiadas.

Documentos de referencia:
- `docs/gameplay.md`: decisiones de diseño (las opciones marcadas con `[x]` son las elegidas).
- `docs/roadmap.md`: fases, criterios de "hecho" y decisiones tomadas.

## Sandbox

No hay partida, puntaje ni victoria: es un laboratorio. El usuario prepara una placa, la observa
e interviene.

- **Placa:** el programa arranca directo en el laboratorio (no hay pantalla de inicio) con una
  **placa de Petri vacía**. Se preconfigura en el panel *Placa* (`PlateSpec`: bioma + seed,
  contenido completo, sin vida o vacía —`DropContent`, `applyDropContent`—, ambiente, nutrientes,
  sedimento, poblaciones y geometría) y se crea con *Crear placa* (Ctrl+N). Ambiente y
  nutrientes se aplican **en vivo** a la placa en marcha; lo demás, al crearla. Las placas se
  guardan en `<datos>/biogame/plates/*.plate`.
- **Placas preparadas** (`game/prepared`; datos en `medios.toml`, `recipientes.toml`,
  `placas.toml`): además de la gota de agua, tipos de placa como en un laboratorio real: el
  **cultivo celular** (DMEM en incubadora a 37 °C y 10 % de CO₂: un campo circular del fondo de un
  recipiente, con la profundidad del medio del recipiente y estéril) y los **preparados** en
  portaobjetos (`PlateKind::Slide`, ver abajo). **Mezclas:** cualquier ser del catálogo se inocula
  en cualquier placa (Catálogo > Inocular..., intervención `Inoculate`, reproducible).
- **Paneles acoplables** (Dear ImGui, ver "Interfaz"): Placa, Catálogo (cualquier ser, con
  íconos), Ficha, Herramientas, Simulación, Preferencias (imagen, fluidez y rendimiento),
  Procesos (tasas de química, microbiología y hongos), Estadísticas (censo, totales conservados
  de P, N, Si y Fe, y el ser controlado), Enciclopedia y Gráficos, Medición, Contenido (paquetes
  cargados, avisos de los datos, importar y exportar `.biopack`), Cultivos, Experimento, Lección y
  Ayuda. Arrancan abiertos Placa,
  Catálogo, Tinciones, Ficha, Estadísticas, Herramientas, Simulación y Medición; los demás se abren
  desde *Ver* (un panel cerrado por defecto pone `open = false` en su constructor).
- **Herramientas** (barra izquierda o teclas 1-0, M y G): Seleccionar, Controlar, Tinta, Detrito,
  Borrar, Lisar, Apretar, Revolver, Estirar, Gotero, Medir, Región y Spawnear (al armar un ser del
  catálogo).
- **Controlar un ser** (herramienta Controlar, o `P` sobre una célula): cada especie conserva su
  **locomoción física**:
  flagelo (impulso), cilios (deslizamiento) y pseudópodo (clic y arrastre). Se respeta
  el **bajo número de Reynolds**: no hay inercia. Al dividirse se sigue con la madre; si el ser
  muere, se puede controlar otro.
- **Seleccionar** (herramienta por defecto, sin modos): un click sobre cualquier cosa elige el ser
  y abre su ficha (qué es, qué hace y por qué). La enciclopedia se llena con lo que la cámara ve.
- **Comer es físico:** se envuelve a la presa con la membrana hasta cerrar una vacuola.
- **Metabolismo real:** respiración con O₂, fermentación sin O₂ y fotosíntesis con luz, limitado
  por P, N y, según la especie, **sílice** (diatomeas), **hierro** (fotótrofas) y **vitamina B₁₂**
  (auxótrofas: *Euglena*, diatomeas), que aportan las bacterias.
- **Genoma:** los organelos y rasgos son genes que mutan en la mitosis y se mezclan en la
  conjugación. Los organelos ocupan espacio físico dentro de la célula.
- **Ecosistema de la gota:** protistas carnívoros (cazadores, envolvedores, emboscadores y
  parásitos), herbívoros (algívoros y bacterívoros), algas, hongos, bacterias y fagos.
- **Células humanas en cultivo** (`bio/animal_cell`, `game/world_animal.cpp`): fibroblasto dérmico,
  HeLa y neutrófilo (`[ser.celula]` en los datos, valores medidos con fuente en `fisiologia.toml`).
  Adheridas al fondo (`Adherent`: el agua no las arrastra), migran por arrastre (`LocomotionType::Crawl`)
  en un paseo aleatorio persistente (rapidez y persistencia de los datos), con inhibición por
  contacto de la locomoción (Abercrombie) y de la proliferación; ciclo por fases G1/S/G2/M con punto
  de restricción (suero) y G0; mitosis redondeada; captación de glucosa y glutamina con
  Michaelis–Menten y lactato (Warburg) que se lleva el bicarbonato; apoptosis (sin energía, sin
  suero o por edad) que deja cuerpos apoptóticos sin lisis; el neutrófilo sigue el fMLP que liberan
  las bacterias y las fagocita. Glutamina, lactato y fMLP son campos que solo difunden si hay
  células animales.
- **Preparados** (`bio/osmosis`, `bio/tissue`, `bio/erythrocytes`, `game/world_tissue.cpp`,
  `render/tissue_renderer`): epidermis de cebolla, hoja de *Elodea*, papa con Lugol, punta de raíz
  con mitosis, epidermis de hoja con estomas, mucosa bucal, frotis de sangre con Giemsa y sangre
  diluida. Un tejido (`[placa.tejido]`) son paredes de celulosa generadas por disposición
  (hileras, poligonal, filas, rompecabezas) y un protoplasto vivo (cuerpo blando, componente
  `Protoplast`) en cada célula, encerrado por su pared (`confineProtoplasts`; los protoplastos no
  chocan entre sí) con los gránulos en la capa cortical. **Ósmosis** (Boyle–van 't Hoff, en tiempo
  real): la osmolaridad del medio sale de la sal y de `MediumParams::osmolytes` (sacarosa); el
  protoplasto se plasmoliza en un medio más concentrado que su jugo celular y se recupera en agua;
  los glóbulos rojos (partículas, miles por campo) se arrugan o se hinchan y se hemolizan al pasar
  su volumen crítico (fragilidad osmótica de Parpart). Fijado (`fijado`), el preparado no se mueve
  y el frotis aplasta los leucocitos contra el vidrio (`World::flattenSmear`). Las células que no se
  mueven solas usan `LocomotionType::None` (`locomocion = "ninguna"`).
- **Células de organismos pluricelulares:** células humanas y animales en cultivo (líneas y tipos
  reales: fibroblastos, queratinocitos, neutrófilos…) y tejidos vegetales, con la biología de cada
  una (adhesión al sustrato, migración, inhibición por contacto, apoptosis, pared celular). Se
  simulan células y tejidos simples, no organismos enteros: los animales pluricelulares completos
  (rotíferos, nematodos) solo cuando su simulación sea realista.
- **Variedad:** gotas procedurales por seed y biomas distintos (charco, estanque, musgo,
  intestino de termita, fuente hidrotermal, hielo); placas preparadas de cada tipo.
- **Estilo visual:** microscopio de **campo claro** (fondo claro, células semitransparentes). Los
  cultivos celulares se miran de verdad con **contraste de fase** (lente pendiente): hasta entonces,
  campo claro.

## Principio rector: realismo estricto

- Solo especies y mecanismos **reales**. Antes de implementar un comportamiento, verificá que
  exista en la biología; si se simplifica, dejá un comentario con lo que hace el organismo real.
- Las células humanas y animales son **tipos o líneas celulares reales** con datos publicados
  (tiempo de duplicación, velocidad de migración, tamaño), con su fuente en `assets/species/*.toml`
  como el resto de la fisiología. Nada de células "genéricas" sin referencia.
- **Tiempo:** la física (nado, agua, difusión, intercambio de gases) corre en tiempo REAL
  (`units::kTimeScale = 1`). La biología (metabolismo, crecimiento, digestión, división) se
  comprime con **una sola constante**, `units::kBioTimeScale` (60: 1 min real = 1 h biológica),
  respetando las proporciones entre procesos y especies. Consecuencias que hay que mantener:
  - lo digerido rinde ×`bioTimeScale` (la alimentación es mecánica, en tiempo real);
  - el intercambio de O₂ y CO₂ de cada célula con el agua se hace a la tasa real (÷`bioTimeScale`,
    no ÷`bioScale`: el Q10 sí acelera la respiración);
  - la ingestión de bacterias de una especie con dato medido (`ingestion_maxima`) es esa, en la
    placa y en el cultivo;
  - excepción: la **migración por arrastre** de las células animales (lamelipodio, adhesiones focales)
    es biología —polimerización de actina— y corre en tiempo biológico (un fibroblasto a 40 µm/h se
    ve a ~0,7 µm/s con kBioTimeScale 60). Nadar sigue siendo física, en tiempo real.
- **Masas:** pmol equivalentes de glucosa de materia SECA (`units::kDryMassFraction` = 0,3).
- Unidades internas: **micrómetros (µm)** y **segundos de simulación**. Las conversiones a
  píxeles van solo en el render.
- No hay inercia a escala de célula: la velocidad depende de la fuerza de propulsión actual, más
  el arrastre del fluido.

## Librerías

Usá **solo** estas. Para agregar otra, primero hay que proponerla y esperar aprobación.
El proyecto es **GPL-3.0-or-later** (`LICENSE`): una librería o un asset nuevo tiene que tener una
licencia compatible con la GPLv3 (MIT, BSD, zlib, Apache 2.0 sí; GPLv2-only o no comercial, no).

| Lib | Versión | Uso |
|---|---|---|
| raylib | 5.5 | Ventana, input, render 2D, shaders GLSL 330 y audio |
| Dear ImGui | 1.91.9b (rama docking) | UI del laboratorio: paneles acoplables, menús, herramientas. Backend de raylib propio (`ui/imgui_raylib`), sin rlImGui |
| ImPlot | 1.0 | Gráficos en vivo del panel Gráficos (se compila dentro de `imgui_lib`) |
| EnTT | 3.14 | ECS para organismos y entidades de alto nivel |
| OpenMP | sistema | Paralelismo de los loops de simulación |
| Tracy | 0.11 | Profiling (preset `profile`) |

| doctest | 2.4.11 | Tests de invariantes físicos (`tests/`, target `bio_tests`) |
| toml++ | 3.4.0 | Datos de las especies con sus fuentes (`assets/species/*.toml`) |

Candidata aprobable cuando su fase lo requiera: **FastNoiseLite** (generación procedural).

Todas las dependencias se traen con `FetchContent` y versión fija en `CMakeLists.txt`. No se usan
paquetes del sistema, salvo OpenMP.

## Build

```sh
cmake --preset release && cmake --build --preset release && ./build/release/bio
```

Hay presets `debug`, `release`, `profile` (Tracy) y `lite` (tiempos por scope sin Tracy). El código
tiene que compilar **sin warnings** con `-Wall -Wextra -Wpedantic`, y `ctest --test-dir build/release`
tiene que pasar (`ctest -L rapidos`: ~1 min; `ctest -L largos -j8`: ~6 min, ~25 en serie; `validacion`:
contra resultados publicados, ~3 min con `-j4`). Largos y validación son un proceso de ctest por caso
(4 hilos cada uno): un caso que repite algo independiente varias veces (presas, especies, tipos de
célula) va en casos separados, así corren en paralelo.

**Presets de distribución:** `dist` (Linux, sin `-march=native`, base x86-64-v2) y `windows`
(cruzado con MinGW, `cmake/mingw-w64.cmake`); `cpack` en su carpeta arma el paquete. Los assets
se buscan junto al ejecutable (`core/paths`) y, en desarrollo, en el árbol de fuentes; los datos
del usuario en la carpeta de la plataforma (`userDataDir`). `tools/ci.sh` es el CI local
(`--todo`, `--paquetes`); `ctest -L rapidos` es la verificación rápida.

**Medir:** `bio_bench` corre una gota fija sin ventana y da ms/paso, el censo y un checksum del
estado (`--wildlife 3` ≈ 700 células, `--gpu` usa el resolvedor de GPU como la app; con el preset
`lite` desglosa por `PROFILE_SCOPE`; `--shot` guarda el cuadro dibujado; por defecto usa un hilo por
núcleo físico, como la app: con SMT, 16 hilos son más lentos que 8). `BIO_GPU_TIMING=1` da el tiempo
de cada kernel de la GPU con marcas de tiempo. Una optimización que no debe cambiar resultados se verifica
comparando checksums de la versión vieja y la nueva compiladas con `-DBIO_STRICT_FP=ON`: con
`-ffast-math` cualquier cambio de código puede reasociar operaciones y alterar los últimos bits, y
la dinámica lo amplifica. Preferí veracidad a velocidad: una optimización que cambia el resultado
físico no se acepta.

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
  entra por intercambio con el aire en toda la superficie; el CO₂ se equilibra con el del aire
  (`MediumParams::co2Atmosphere`: la incubadora pone 5-10 %) según la ley de Henry a la temperatura
  y la salinidad del medio, y el pH sale del bicarbonato con el pK′ del medio (`carbonatePK`). Macronutrientes (P, N, Si) y
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
| Compuesto | Colonias y filamentos | Soft-bodies unidos por adhesiones (`World::AdhesionLink`) |

LOD (`World::setFocus`, tres niveles): cerca de la vista, detalle completo; más lejos, 1 de cada
`lodStride` sub-pasos de XPBD con un paso más largo (las tasas [1/s] no cambian de significado);
fuera de pantalla, estadístico (se trasladan como sólidos, sin XPBD ni contactos). División,
fagocitosis, conjugación y presas sujetas siempre en detalle. Sin foco (tests), todo detallado.
Contactos: solo los puntos de membrana a menos de su radio de contacto (+ holgura) de otro
cuerpo, medido una vez por sub-paso; dos pasadas por sub-paso (`SoftBodyTuning::contactPasses`).

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
- **Cambios grandes de forma** (una célula animal que pasa de redonda a huso, o se redondea para la
  mitosis): `reshapeRestEven` reparte la forma de reposo por longitud de arco. La proyección radial
  de `reshapeRest` amontona los puntos y deja la forma cruzada; con flexión de ventanas eso hace que
  la célula se empuje sola.
- **Topología:** `replaceRing` / `resizeRing` cambian el anillo (fusión, inserción de membrana).
  Los eventos de topología se aplican al final del paso.

### Ciclo de vida (`bio/metabolism`, `bio/division`, `World::updateLifecycle`)

- **Metabolismo** (componente `Metabolism`): biomasa, reservas y edad. Respiración (~30 ATP por
  glucosa) o fermentación (2 ATP, 15× más glucosa) según el O₂ local; mantenimiento proporcional a
  la biomasa y a la actividad; crecimiento con rendimiento 0,6 y tasa máxima; fotosíntesis con la
  luz local. Sin reservas hay autofagia; con menos de la mitad de la biomasa de nacimiento, muere.
- **Una sola fisiología** (`bio/cell_physiology`, `bacterialDemand`/`bacterialMassChange`,
  `digestionFractions`/`digestVacuole`): la entrada de cada célula (µmax de los datos, rasgos,
  estrés de pH, temperatura, sal y toxinas, quiste), el crecimiento de las bacterias y la cinética de
  las vacuolas son funciones sin cuerpo que usan la placa y el cultivo bien mezclado (fase P1). La
  placa solo pone de dónde sale el entorno y adónde va lo que se intercambia: no dupliques esa
  biología en otro sistema.
- **Tamaño:** el área de reposo es la de la biomasa más el contenido de las vacuolas. La masa es la
  del **esferoide** de semiejes mayor, menor y vertical (`espesor` en `[ser.forma]`; por defecto, el
  menor) por la materia seca relativa (`cellMassScale`), no la de la esfera de la silueta 2D.
- **Punto de control del ciclo:** con el tamaño de división la célula deja de invertir en biomasa y
  junta las reservas de la citocinesis (si no, una célula con poca comida no se divide nunca).
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
  *Saprolegnia* (exoenzimas aceleran la descomposición junto a las hifas); levaduras como especie
  osmotrófica con gemación (`budding`).
- Química e interacciones microbianas (`game/world_chem.cpp`): pH del sistema carbonato con
  alcalinidad fija; rango de pH por especie (`phMin/phMax`); quorum sensing de *C. violaceum*
  (`Bacteria::strain/quorum`, violaceína en `Digestion::toxin`); fagos líticos como campo de
  concentración (`phage_`) con mutantes resistentes; *Bdellovibrio* libres y bdeloplastos.
- Biopelícula (`Bacteria::attached`): las bacterias se adhieren al vidrio al chocar y se sueltan
  de a poco; adheridas no nadan y los filtradores no las alcanzan (las amebas sí).
- Más hongos: `FungusInfo` (tabla de datos de `bio/fungi`) define cada tipo; *Allomyces* (ramificación
  dicotómica, esporangios de resistencia en `Fungi::resting`) y el hongo liquenizante (`Mycobiont`,
  componente `Lichenized` en el alga: ella cede azúcar, él devuelve fósforo y la protege de los
  quitridios; `World::stepLichens`).
- *Physarum polycephalum* (`bio/plasmodium`, `game/world_plasmodium.cpp`): moho mucilaginoso, no un
  hongo. `PlasmodiumSystem` = grafo de nodos y tubos con conductividad D; flujo de Kirchhoff
  (gradiente conjugado serial: determinista) y adaptación de Tero; puntas que siguen el gradiente de
  materia orgánica y evitan la baba (`World::slime_`) y la luz; anastomosis; copos de avena
  (`OatFlakes`) y esclerocios. El fósforo del plasmodio, copos y esclerocios entra en
  `totalPhosphorus()`.
- `World::deathTally()` y `World::observations()` registran causas de muerte y comportamientos
  vistos (base de la enciclopedia).

### Cultivo bien mezclado (`game/culture`, fase P1)

- Un tubo o un frasco de mililitros (Gause, Tilman) donde las poblaciones no caben en una placa.
  Sin espacio ni cuerpos blandos, pero con **la misma fisiología** que la placa: cada célula es un
  individuo (`CultureCell`: `Metabolism`, `Heredity`, `Digestion`, `Lineage`); las bacterias,
  cohortes (`BacterialCohort`, superindividuos: exacto hasta `maxBacterialCohorts`).
- Los encuentros salen de la teoría física: filtradores con respuesta funcional de tipo II
  (`ingestion_maxima` y `aclaramiento` en `fisiologia.toml`; sin dato, la corriente oral y el
  citostoma de la placa) y cazadores con el núcleo de Gerritsen y Strickler (1977).
- No hay `bioTimeScale`: `step(dt)` avanza `dt` segundos y la biología `dt · Q10`; los gases se
  intercambian con el aire a su tasa real (D / profundidad²). El balance (`massBudget`, `FluxLedger`)
  es el mismo que el de la placa.
- Luz: Beer–Lambert en la columna por la biomasa fotótrofa (cada célula recibe la media). Fagos
  líticos de acción de masas con los parámetros de la placa (las infectadas son cohortes propias).
- Operaciones: cambio de medio centrifugado (`renewMedium`: los protistas quedan), dilución,
  quimiostato (`setDilutionRate`, lavado binomial), gota de muestra y pasajes (`withdraw`,
  `receive`); lo que sale va al balance como `Flow::Washout`. Las bacterias toman el sustrato con un
  paso implícito (la concentración del final del paso), si no el quimiostato no llega a S*.
- **Programas** (`game/program`): rampas, ciclos, senos y eventos con la hora de laboratorio, guardados
  como definición (`programa=` en las escenas); se aplican una vez por paso.
- **Sesión de cultivo** (`game/culture_run`): como `Run` para la placa: toda operación pasa por
  `CultureRun::apply`, queda anotada con su paso y la escena `.cultivo` la repite. La gota de muestra
  (`sampleDrop`) arma un `PlateSpec` v3 con las células con su estado (`WorldConfig::sampled`).
- **Experimentos por lote** (`game/experiment`, `bio_lote`, panel Experimento): TOML con réplicas y
  factores; una corrida por hilo (OpenMP en 1, registros precargados); formato largo, resumen con
  media, DE e IC 95 % y JSON con la procedencia.
- Lo que todavía no se simula bien mezclado no se siembra (`Culture::supports`): células animales,
  filamentos, colonias y zoosporas. Tampoco hay Bdellovibrio, quorum ni conjugación.

### Lecciones y protocolos (`game/lesson`, panel Lección, fase P1)

- Una lección es un protocolo con pasos, como datos (`lecciones.toml`, `[[leccion]]` en los
  paquetes): qué se prepara (placa preparada, gota de un bioma o escena de cultivo), los controles
  que ve el alumno (claves del registro), los parámetros que fija (con su motivo), acciones con nombre
  (una intervención o una operación del cultivo) y pasos con objetivo, pregunta o predicción.
- Los objetivos son **condiciones con nombre** (`tiempo_h`, `poblacion`, `bacterias`,
  `concentracion`, `ph`, `plasmolisis`, `hemolisis`, `cerca_centro`, `fraccion`): el código aporta
  el mecanismo y los datos eligen cuál. Una condición nueva se suma en `lessonQuantity`.
- Sin puntaje: las preguntas muestran su explicación al responder y el informe (Markdown) lo corrige
  el docente. Un parámetro fijado se ve trabado (`App::protocolLocks`, `parameterControl`); se puede
  destrabar, queda anotado y `runJson` marca `fuera_de_protocolo`.
- Cada lección trae su **solución** (intervenciones u operaciones con su paso) y
  `tests/test_lessons.cpp` la recorre: si un cambio del modelo rompe una lección, se entera el test.

### Ambiente, biomas y enciclopedia (`game/biome`, `game/encyclopedia`)

- `MediumParams` (en `WorldConfig::medium`): temperatura (Q10 = 2 en todo lo biológico vía
  `World::bioScale()`, viscosidad y O₂ de saturación), salinidad, anoxia, alcalinidad, P, N.
  Las tolerancias son datos de la especie y se pagan como mantenimiento extra (estrés).
- `makeDrop(bioma, seed)` arma la gota: química, pesos de población por especie
  (`WorldConfig::speciesWeight`), hongos, cianobacterias (rango térmico) y manchas iniciales.
- Biomas y minerales son datos del paquete (`assets/content/base/biomas.toml`, `minerales.toml`;
  registros `game/biome` y `game/mineral`, como los seres): el medio se lee con claves en español
  (`game/medium_toml`, el mismo lector servirá para los medios de cultivo), el sedimento es un
  conteo por mineral y los pesos van por id de ser. Un simbionte obligado (etiqueta
  `simbionte_obligado`) solo aparece donde el bioma lo nombra. Los lectores de los paquetes usan
  `core/toml_reader` (errores con lugar, claves desconocidas avisadas).
- La enciclopedia se llena con lo que la cámara ve y con `deathTally`/`observations`.

### Render (`render/`)

- Campo claro: cobertura de las células → shader con borde óptico y halo. La pared de la placa
  se dibuja con `dish_glass.fs` (vidrio, canto oscuro, línea de Becke y menisco) antes del
  post-proceso. Con placa hay una sola copia del mundo (sin toro no se repite al alejar). Todo ocurre en un
  mismo plano (no hay plano focal). El campo se dibuja en una textura y pasa por
  `microscope_post.fs` (viñeta, aberración cromática, grano).
- **Zoom semántico** (`render/lod.hpp`, `RenderLodSettings`: toda la configuración en un lugar):
  cada ser elige su nivel (`DetailLevel`: densidad, simplificado, normal, detalle, subcelular) por
  su tamaño real proyectado en píxeles, no por el zoom. Bacterias y *Bdellovibrio*: una pasada
  lineal recorta por la vista y reparte entre la densidad (`DensityGrid` en `game/`, celdas
  potencia de 2 ancladas al mundo → `PopulationLayer`, turbidez Beer–Lambert) e instancias
  (`InstancedParticles`, solo las visibles), con fundido cruzado. De cerca, los más cercanos al
  centro (tope `maxDetailed`) y el elegido se dibujan con estructuras (`render/structure_renderer`).
- **Plan corporal** (`bio/body_plan`): qué estructuras tiene cada ser y su tamaño real
  (`Simulated` o `Declared`). Cada estructura aparece sola cuando su tamaño proyectado alcanza unos
  píxeles; lo que no resuelve el microscopio óptico solo se ve en la **vista de modelo**
  (`ViewMode`). Lo declarado se ubica con un hash del id estable (`Bacteria::id`), nunca con azar.
  `game/scale_path` da la escala biológica y la miga de pan (Placa > organismo > célula > estructura).

### Interfaz (`app/`, `ui/`)

- **Un solo espacio de trabajo:** `App` (`app/app.cpp`) abre la ventana, crea la placa y corre el
  bucle: UI → entrada sobre la placa → paso de simulación → dibujo del mundo → dibujo de la UI
  encima. No hay máquina de pantallas ni modos excluyentes.
- **Dockspace con centro transparente:** el centro libre (`App::viewport`) es donde se ve la placa;
  la cámara, el zoom mínimo y el LOD usan ese rectángulo. Los clicks son del mundo solo si
  `!ImGui::GetIO().WantCaptureMouse` (y las teclas, si no `WantTextInput`).
- **Para sumar una función de UI:** un panel es una clase que hereda de `ui::Panel`
  (`ui/panels/*.cpp`) y una línea en `ui/panels/panels.cpp`; una acción de menú o atajo es un
  `ui::Command` en `App::registerCommands`; una herramienta es un valor de `Tool` (`app/tools.*`)
  y su caso en `App::toolInput`. Los menús y los atajos salen de la misma tabla de comandos.
- **Widgets del laboratorio** (`ui/widgets`): `slider`, `sliderLog`, `checkbox`, `combo`, `readout`,
  `bar`, `section`. Rótulo a la izquierda, ayuda al pasar el mouse y valor tipeable (Ctrl+click).
  Todo texto visible pasa por `ui::tr(clave)` (la clave es el texto en español); el ID de cada
  ventana es estable (`título###clave`) para que la disposición guardada no dependa del idioma.
- **Idiomas** (fase P1): `assets/lang/en.txt` (inglés) y, en cada paquete de contenido, su
  `idiomas/<código>.txt`. Los formatos de `readout` también se traducen: la traducción conserva los
  especificadores (lo verifica `tools/claves_tr.py --check`, que corre en `tools/ci.sh`). Un texto
  nuevo se suma con `tools/claves_tr.py --check assets/lang/en.txt --faltantes`; lo que se arma en
  tiempo de ejecución sale con `BIO_TR_FALTANTES=archivo ./bio`. Si el español de una clave cambia, su
  traducción deja de coincidir y se ve en español (nunca una traducción vieja). Las columnas de las
  series y del CSV van por id (no cambian con el idioma). La ficha traduce títulos, rótulos, nombres y
  descripciones; los valores que arma `game/inspect` con números quedan en español (pendiente).
- **Estilo:** todos los colores y espaciados salen de `ui/theme`. La fuente trae Latin-1 (acentos,
  ñ, µ, °): se puede escribir con acentos en la UI nueva.
- **Opciones** (`Options` en `game/progress`, `<datos>/biogame/options.txt`): las aplica
  `App::applyOptions` (post-proceso, escala de render, detalle, VSync, hilos, LOD, GPU) y el tope
  de FPS `App::applyFrameCap`; el MSAA solo al crear la ventana. Los presets de calidad
  (`applyGraphicsPreset`) tocan solo la imagen, nunca la simulación. La escala de render vive en
  `Renderer`: la escena se dibuja más chica con la cámara escalada y se reescala al final.
- La disposición de paneles se guarda en `<datos>/biogame/layout.ini` (*Ver > Restablecer
  disposición* la rehace). Los íconos del catálogo se dibujan una vez con raylib en un atlas
  (`ui/being_icons`).
- `PlateSpec` (`game/plate_spec`, sin raylib) es la fuente de verdad de lo que se preconfigura;
  `parsePlate`/`serializePlate` son texto clave=valor versionado (`plate_version`) y lo que falta
  queda como en el bioma.

### Medición (`game/mass_budget`, `game/timeseries`, `game/growth_fit`, `game/region`, `game/export`)

- **Balance de masa:** `World::massBudget()` da las existencias de P, N, Si, Fe y C por
  compartimento (`Pool`); `totalPhosphorus()` y los demás totales son su suma (una sola
  contabilidad). Los flujos los acumula `World::ledger_` (`FluxLedger`): **todo intercambio con el
  agua pasa por `World::exchange(campo, punto, pmol, Flow)`** con el proceso que corresponde
  (`Uptake`, `Excretion`, `Death`, `Spawn`…); `exchangeRaw` no registra. Lo que entra desde afuera
  (gotero `dose`, `setMeanConcentration`, fijación de N₂, CO₂ del aire) va a `external`. El cierre
  es `budgetResidual` = cambio del total − entradas externas. El C tiene frontera abierta (el aire
  y la materia orgánica que llega no se contabilizan como cerrados): no se le exige cierre.
- **Serie temporal:** la muestrea `Run` (no `World`) cada `sampleInterval` [s de simulación] junto
  con el censo: tiempo, poblaciones, medias químicas (`Census::mean*`), balance y región. Columnas
  con nombre y unidad, guardadas por columna; con tope de filas se decima (1 de cada 2). NaN marca
  lo que falta (región sin dibujar): con `-ffast-math` usar `finiteNumber`, nunca `std::isfinite`.
- **Curva de crecimiento:** `fitGrowth` (µmax por ventana deslizante sobre ln N, latencia de Pirt,
  K logística por sección áurea). Determinista y sin raylib.
- **Región:** `quantifyRegion` cuenta individuos por centroide y promedia las celdas de agua con el
  centro adentro. La región vive en `Run` (`setRegion`) y entra en la serie.
- **Exportar** (Ctrl+E): `seriesCsv`, `runJson` (parámetros: la placa con el ambiente vigente,
  metabolismo, escala de tiempo; serie; balance) y `speciesJson`, en `<datos>/biogame/exports`.
  Números con `std::to_chars` (punto decimal sin importar la locale).
- **Mapa de limitación:** `Metabolism::limiting` (Liebig: el factor que dejó más baja la inversión
  en biomasa) solo lo lee el render (`CellRenderer::setLimitationMap`, tecla L).

### Tinciones (`game/stain`, `render/cell_renderer`, `ui/panels/stain_panel`)

- **Datos, no código por colorante:** `StainSpec` = lista de reglas {blanco, respuesta, colores,
  intensidad}. Los blancos son compuestos o estructuras reales (`StainTarget`: núcleo, nucleoide,
  almidón, glucógeno, paramilón, compartimentos ácidos con su pH, pared grampositiva y
  gramnegativa, clorofila…). Las respuestas: `Fixed`, `PhIndicator` (vira con el pH) y
  `GramRetention` (pierde el colorante con el tiempo de decolorante: la pared fina en ~3 s, la
  gruesa en ~60 s; sobre-decolorar vuelve rosa a la grampositiva). De fábrica: Gram, Lugol,
  DAPI (fluorescencia, fondo oscuro), rojo neutro, azul de metileno, rojo Congo, Giemsa (sangre:
  núcleos violeta, hemoglobina rosa) y orceína acética (cromosomas).
- **Bioquímica real en la simulación:** `speciesChemistry(id)` da la reserva (`Storage`: almidón,
  glucógeno, paramilón, crisolaminarina) y la pared (`CellWall`) de cada especie; solo el almidón
  verdadero (clorofitas) vira a azul-negro con Lugol y *Euglena* (paramilón) no reacciona.
  `Bacteria::strain` 2 es *Bacillus subtilis* (grampositiva); las demás son gramnegativas.
- **Solo visual:** el render pregunta `evaluateStain` por cada blanco que dibuja; no toca `World`
  ni `Rng`. `vital` = false marca las que en la realidad fijan o matan (Gram, Lugol, DAPI): para
  ésas el panel ofrece *Fijar la muestra* (`World::fix`), que mata todo sin lisis y detiene `step`.
- **Preview:** `ui::StainPreview` dibuja un espécimen de prueba fijo (un `World` chico) con el
  MISMO `Renderer` que la placa (`Renderer::drawWorldTo` en una textura), sin teñir y teñido,
  en vistas de células, detalle y bacterias. Sumar un blanco: un valor de `StainTarget`, su
  evaluación en el render y, si hace falta, un dato en `SpeciesChemistry`.
- Las tinciones del usuario se guardan en `<datos>/biogame/stains/*.stain` (texto clave=valor
  versionado, `stain_version`).

### Reproducibilidad (`game/parameters`, `game/intervention`, `game/scene`)

- **Registro de parámetros** (`parameters()`): todo parámetro ajustable de la placa en marcha
  está en una sola tabla (clave estable, unidad, rango, acceso). Los paneles dibujan sus
  controles desde ella (`ui/param_controls`) y `runJson` la exporta entera. Un parámetro nuevo se
  suma ahí, no en un panel.
- **Intervenciones:** **toda acción que cambia el mundo** (herramientas, parámetros, ambiente,
  solutos, ser controlado, fijar, foco del LOD, motor) pasa por `App::intervene` /
  `interveneState` → `Run::interventions()`: se aplica entre pasos y queda anotada con su paso.
  Nunca llames a `World` directamente desde la interfaz. Lo que se mantiene apretado (tinta,
  gotero) es un pincel que `InterventionLog::beforeStep` aplica **una vez por paso** (no por
  cuadro: el resultado no puede depender de los FPS).
- **Escena** = `.plate` + líneas `event=paso tipo …`. Cargarla y crear la placa repite la corrida
  (`InterventionLog::schedule`); la exportación la incluye. Nivel prometido: bit a bit con el
  mismo binario, máquina, motor y opciones; entre máquinas, estadístico (réplicas). La
  procedencia (versión, commit, flags, motor, GPU, CPU, huella de las intervenciones) va en
  `runJson`.
- Los floats de los archivos de texto se escriben con precisión de ida y vuelta (`max_digits10`
  o `std::to_chars`): con 6 cifras, una placa leída ya no es la misma.

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
  polo anterior (despacio) y la ameba por contacto. Las bacterias tienen una **altura en la
  película** (`World::filmHeight`): la boca solo traga las que pasan a su altura (la vista es 2D,
  pero la película no). El radio de captura de bacterias está calibrado con el aclaramiento medido
  (`bio_bench --respuesta-funcional`: la placa y el cultivo tienen que comer lo mismo). Lo ingerido forma **vacuolas digestivas**
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
- **Paquetes de contenido** (`game/content`): el de fábrica (`assets/content/base/`) y los del
  usuario (`<datos>/biogame/paquetes/`, carpeta o un solo `.biopack`), cargados al arrancar; se
  importan y exportan desde el panel Contenido y `bio_bench --paquete` los carga sin ventana. Son
  solo datos, acotados (8 MB, ids `[a-z0-9_]`); cada registro guarda el TOML de origen de cada
  definición para exportarla tal cual. Las placas y escenas nombran todo por id de texto y listan
  los paquetes (`pack=`), y `runJson` anota sus huellas.
- **Contenido como datos** (`assets/content/base/`: seres, planes corporales, fisiología,
  minerales y biomas): los parámetros de cada especie (tamaño, velocidad, dieta, reproducción,
  comportamiento, apariencia) viven en datos, no en código. Los enums (`SpeciesId`, `MineralKind`,
  `BiomeId`) son índices en el registro con alias para los de fábrica; no agregues casos por
  especie o por mineral en el código: sumá un mecanismo o un estilo con nombre que los datos elijan.

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
  main.cpp          crea la `App` y la corre
  app/              `App` (bucle, cámara, entrada sobre la placa), herramientas y selección
  ui/               marco de UI: backend ImGui, tema, widgets, comandos, paneles, `tr()`, íconos
  ui/panels/        un archivo por panel
  core/             profiling, unidades, rng, vec2
  sim/              fft, fluid, chem_field, grid2d, particles, body_particles, soft_body (XPBD),
                    contacts, polygon, spatial_grid
  bio/              cell, membrane, metabolism, lifecycle, genome, organelles, species
  ai/               autopilot (deambular); luego quimiotaxis, caza, huida, fototaxis
  game/             sesión de sandbox, linaje, enciclopedia, biomas
  render/           renderer, shaders de membrana, post-proceso de microscopio
assets/
  fonts/            fuente de la UI (Roboto, Apache 2.0)
  lang/             idiomas de la UI (`<código>.txt`, líneas clave<TAB>traducción)
  shaders/          GLSL 330
  species/          datos de especies
docs/               gameplay.md, roadmap.md
tests/              doctest
```

`biosim` (todo menos `main`, `app/`, `ui/` y `render/`) es una librería sin raylib: la usan el juego
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
