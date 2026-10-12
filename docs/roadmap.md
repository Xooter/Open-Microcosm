# BioGame — Roadmap

Basado en las elecciones de `docs/gameplay.md`. Cada fase tiene un **entregable visible** y un
criterio de "hecho". Las fases 1–4 forman el **vertical slice**: una célula jugable y gelatinosa
que nada, come, crece y se divide en un medio vivo. Antes de sumar contenido se hace un playtest.

## Resumen de decisiones

| Tema | Elegido |
|---|---|
| Gameplay general | Roguelite (C) + linaje continuo (E), combinados |
| Jugabilidad | Locomoción física por tipo de célula, bajo Reynolds (B) + fagocitosis física (D) |
| Objetivo | Dominar el ecosistema por % de biomasa (C) + enciclopedia (E) |
| Carnívoros | Todos: cazadores, envolvedores, emboscadores sésiles, jefes multicelulares, parásitos |
| Herbívoros | Todos: filtradores, pastoreadores, raspadores, mixótrofos, enjambres |
| Algas | Todas: móviles, diatomeas, coloniales, filamentosas, cianobacterias |
| Hongos | Quitridios, levaduras, descomponedores, depredadores (sin simbiontes) |
| Partículas | **Primero B** (fluido en grilla + soft-body), partículas ambientales y bacterias como partículas simples (C), LOD (E) más adelante |
| Efectos visuales | Todos: metaballs/SDF, ciclosis, browniano, plano focal, lisis y tinción |
| Reglas de vida | Metabolismo real, mutualismos, fagos y parasitismo, quorum sensing (sin endosimbiosis) |
| Estilo visual | Campo claro (A) |
| Reproducción | Jugador: mitosis física + conjugación, la más costosa. Cada especie NPC usa la suya propia |
| Organelos | Físicos con espacio limitado (B), locomoción/armas/defensas (C), genoma con mutaciones (D) |
| Rejugabilidad | Gotas procedurales (A), herencia roguelite (B), biomas (C) |
| Realismo | **Estricto** (A): solo especies y mecanismos reales, tiempo comprimido en proporción |

## Decisiones tomadas

1. **Roguelite + linaje:** se usan ambos. **Dentro de la partida** jugás un linaje (al dividirte
   elegís hija y al morir saltás a un descendiente), la partida **termina cuando se extingue el
   linaje**, y **entre partidas** se hereda parte del genoma.
2. **Química:** campos internos mínimos (O₂, luz, nutrientes disueltos, señales) que alimentan el
   metabolismo, el quorum sensing y los blooms. **No** hay modo de visualización para el jugador,
   solo un overlay de debug.
3. **Estilo visual:** únicamente **campo claro** (brightfield).
4. **Escala real:** las bacterias son **partículas simples** (agentes livianos), los protistas son
   soft-bodies y los multicelulares son cuerpos compuestos.

---

## Fase 0 — Base técnica ✅ (hecha)
- CMake + presets, raylib, raygui, EnTT, OpenMP, Tracy y doctest.
- Partículas SoA, grilla espacial, timestep fijo y demo de Particle Life.

## Fase 1 — El medio ✅ (hecha)
- Mundo toroidal con vista cenital (decidido).
- Agua Stokes–Brinkman resuelta con FFT: sin inercia, corrientes locales según el espesor de la
  película.
- Partículas ambientales (detritos, tinta) y **bacterias como partículas simples**, con browniano
  de Stokes–Einstein.
- Bacterias con run & tumble y quimiotaxis temporal de *E. coli*: se acumulan ~11× alrededor de
  una fuente aislada.
- Campos internos: O₂ (intercambio con el aire), nutrientes (de detritos que se descomponen) y
  luz (uniforme, atenuada por sombra).
- Overlay de debug (F1–F4) y tests doctest: incompresibilidad, conservación de masa y
  determinismo con distinta cantidad de hilos.
- Medido: ~0,9 ms por paso de simulación con 50k bacterias.

## Fase 2 — Célula soft-body ✅ (hecha)
- Membrana como anillo de partículas con XPBD **sin inercia** (tasas de relajación [1/s]):
  aristas, flexión por shape matching local, área conservada y memoria de forma por célula
  (película rígida vs. córtex blando).
- Interior: núcleo con nucléolo y vacuola contráctil anclados al citoesqueleto, y gránulos con
  browniano.
- Colisiones punto–arista entre células y auto-colisión, más una pasada anti-penetración. Las
  bacterias y los detritos no entran en las células.
- Render en campo claro: relleno triangulado (ear clipping) + shader con borde óptico y halo.
- Herramientas: crear (C), escena de 50 (K), borrar (X), estirar (Shift + arrastrar), apretar (Q).
- Medido con 50 células comprimidas 10 s: área ≤ 3,6%, aristas ≤ 1,9×, **0 intrusiones**, y
  recuperación de forma tras soltar. ~3,5 ms por paso con 50 células + 50k bacterias.

## Fase 3 — Jugador: locomoción y fagocitosis ✅ (hecha)
- Tres locomociones reales sin inercia: cilios (*Paramecium*, 600 µm/s), flagelo (*Euglena*,
  80 µm/s, bamboleo helicoidal) y pseudópodo (*Amoeba*, ~6 µm/s).
- El nadador empuja el agua (fuerza neta cero) y siente solo el flujo de fondo.
- Alimentación: citostoma con corriente oral y caudal máximo (ciliado), polo anterior
  (flagelado), contacto (ameba). Vacuolas digestivas que se cierran, digieren y expulsan.
- **Fagocitosis física de células** por la ameba: copa por cremallera, inserción de membrana en
  los labios y fusión de membranas al cerrarse (~9 s para una presa de 40 µm).
- Cámara de microscopio que sigue al jugador en el mundo toroidal. Render de cilios con onda
  metacrónica, flagelo con desenfoque de movimiento, estigma, citostoma y vacuolas.
- Medido: ~4,6 ms por paso con 30 células + ameba + 50k bacterias.

## Fase 4 — Metabolismo y ciclo de vida ✅ (hecha) — 🎯 vertical slice
- Escala de tiempo: física en tiempo real, biología comprimida ×60 (decisión tomada).
- Metabolismo: respiración/fermentación según el O₂ local, fotosíntesis (*Euglena*) con la luz,
  mantenimiento + costo de nadar, crecimiento con rendimiento y tasa máxima, autofagia.
- Mitosis física para las tres especies: duplicación de núcleo y vacuola, reorganización del
  córtex de madre a dos hijas, surco y partición de la membrana (~25–60 s reales).
- Muerte por inanición y lisis (nutrientes disueltos + detritos).
- HUD: reservas, biomasa, progreso de división, O₂, luz y edad; R para reaparecer.
- Pendiente: la fermentación todavía no acidifica el medio (falta un campo de pH; entra cuando
  el pH tenga efectos sobre otras especies).
- Medido: ~5 ms por paso con 30 células + ameba + 50k bacterias.
- **Siguiente:** playtest del vertical slice antes de seguir con la fase 5.

## Fase 5 — Linaje y estructura roguelite ✅ (hecha)
- Pantalla de inicio con especie fundadora; Euglena se desbloquea en la generación 3 y Amoeba en
  la 5 (o al ganar). Récords y desbloqueos persistentes en disco.
- Al dividirse, el jugador elige hija (simulación en pausa); la otra queda con piloto automático
  (deambula y come). Al morir elige un descendiente; sin descendientes, el linaje se extingue.
- Objetivo: el linaje alcanza el 50% de la biomasa viva de la gota (se puede seguir jugando).
- Movimiento, fagocitosis y alimentación generalizados a cualquier célula móvil.
- Medido: ~3,2 ms por paso en una partida real (24 competidoras + 50k bacterias).
- **Hallazgos de balance (para la fase 7):**
  - Las bacterias no se reproducen: el linaje agota la comida de la gota y crece cada vez más
    lento (un paramecio pasa de 12% a 19% de dominio en 10 min sin llegar a dividirse).
  - El dominio por biomasa favorece a los organismos grandes: con competidoras pasivas de
    ~200 pmol, un linaje de Euglena (~9 pmol por célula) necesitaría cientos de células.
  - Las competidoras son pasivas (no comen ni mueren) hasta que exista el ecosistema real.
- Pendiente para la fase 6: la herencia de genoma entre partidas.

## Fase 6 — Genoma y organelos físicos
- Genoma: cada rasgo es un gen. La mitosis copia el genoma con mutaciones y la conjugación lo
  mezcla con otra célula.
- Los organelos ocupan espacio real: más organelos hacen una célula más grande, lenta y visible.
  La posición importa (por ejemplo, el flagelo atrás).
- Locomoción, armas y defensas: flagelo, cilios, pseudópodo, tricocistos, toxicistos, pared,
  teca o caparazón, y frústula.
- **Hecho cuando:** dos linajes con genomas distintos se juegan claramente distinto.

## Fase 7 — Ecosistema NPC (núcleo)
- IA por quimiotaxis (*run & tumble*), caza, huida y fototaxis. Comportamiento data-driven por
  especie.
- Reproducción propia de cada especie: fisión, gemación, colonial o quistes, según la biología
  real.
- Especies base: *Chlamydomonas*, bacterias, *Paramecium*, *Euglena*, *Didinium* y *Amoeba*.
- **Hecho cuando:** 30 min sin jugador producen ciclos depredador–presa estables.

## Fase 8 — Ecosistema completo e interacciones
- **Algas:** diatomeas, *Volvox* / *Pediastrum*, *Spirogyra* (terreno vivo) y cianobacterias.
- **Herbívoros:** *Vorticella* / *Stentor* (filtradores sésiles), *Euplotes* y amebas testadas, y
  enjambres con tricocistos.
- **Carnívoros:** *Lacrymaria*, suctorios (trampas) y *Bdellovibrio*.
- **Hongos:** quitridios, levaduras (fermentación), *Saprolegnia* (descomposición) y *Zoophagus*.
- **Interacciones:** mutualismos (fijación de nitrógeno), fagos y epidemias, quorum sensing y
  blooms tóxicos.
- **Hecho cuando:** la cadena trófica completa se sostiene y cada interacción se puede observar.

## Fase 9 — Jefes multicelulares y LOD
- Rotíferos, *Hydra* y larvas de copépodo como cuerpos compuestos, con corona ciliada que genera
  succión en el fluido.
- LOD: las células cercanas son detalladas, las lejanas simplificadas y las fuera de pantalla se
  simulan de forma estadística.
- **Hecho cuando:** un rotífero entra en escena con 500+ células activas a 60 FPS.

## Fase 10 — Estilo visual
- Post-proceso de campo claro: fondo claro, células semitransparentes, halo y refracción sutil.
- Ciclosis, browniano visible, plano focal (enfocar capas y esconderse fuera de foco), lisis y
  tinción de vacuolas digestivas.

## Fase 11 — Rejugabilidad y contenido
- Gotas procedurales por seed: temperatura, luz, pH, salinidad y mezcla de especies.
- Biomas: charco, estanque, musgo húmedo, intestino de termita, fuente hidrotermal y hielo marino.
- Enciclopedia: catalogar especies y comportamientos observados.
- Guardado, menú y opciones.

## Fase 12 — Escala y GPU
- Simulación en compute shaders si se superan ~100k partículas.
- Render instanciado y profiling continuo con Tracy.
