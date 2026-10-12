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
- CMake + presets, raylib, raygui, EnTT, OpenMP y Tracy.
- Partículas SoA, grilla espacial, timestep fijo y demo de Particle Life.

## Fase 1 — El medio
- Fluido 2D en grilla (*stable fluids*) con corrientes y arrastre viscoso (bajo Reynolds).
- Partículas ambientales: nutrientes, detritos y **bacterias como partículas simples**, con movimiento browniano.
- Campos internos mínimos: O₂, luz (gradiente desde la superficie) y nutrientes disueltos.
- Overlay de debug (solo para desarrollo) que muestra cada campo.
- **Hecho cuando:** soltar tinta en la gota muestra corrientes creíbles a 60 FPS.

## Fase 2 — Célula soft-body
- Membrana como anillo de partículas con restricciones XPBD: distancia, flexión y área (presión).
- Citoplasma y organelos como partículas internas confinadas por la membrana.
- Render gelatinoso semitransparente con SDF o metaballs, en **campo claro**.
- Colisión célula–célula: se deforman y se empujan sin atravesarse.
- **Hecho cuando:** 50 células chocan, se aplastan y recuperan la forma sin explotar.

## Fase 3 — Jugador: locomoción y fagocitosis
- Locomoción física por tipo: flagelo (impulso), cilios (deslizamiento continuo) y pseudópodo
  (clic y arrastre). Sin inercia: al dejar de propulsarse, la célula frena.
- Fagocitosis física: envolver a la presa con la membrana y cerrar una vacuola (cambio de
  topología). La presa se escapa si el cierre falla.
- Cámara de microscopio con zoom y paneo suave.
- **Hecho cuando:** se puede perseguir, envolver y comer una presa usando solo física.

## Fase 4 — Metabolismo y ciclo de vida
- Metabolismo real simplificado: respiración con O₂ (mucho ATP), fermentación sin O₂ (poco ATP,
  acidifica el medio) y fotosíntesis con luz (libera O₂).
- Crecimiento de área, **mitosis física** (duplicar organelos, anillo contráctil, partir la
  membrana en dos) y muerte por **lisis** (el contenido se dispersa como comida).
- La reproducción del jugador es la más costosa en energía y tiempo.
- HUD mínimo de energía, tamaño y edad.
- **Hecho cuando:** la célula come, crece y se divide; si no come, muere y se lisa.
- 🎯 **Vertical slice completo**: hacer playtest y decidir el rumbo.

## Fase 5 — Linaje y estructura roguelite
- Al dividirse, el jugador elige con qué hija seguir; las demás quedan como NPC de su linaje.
- Al morir salta a un descendiente vivo. Si el linaje se extingue, termina la partida.
- Objetivo de la partida: alcanzar X % de la biomasa de la gota.
- Herencia entre partidas (parte del genoma) y especies iniciales desbloqueables.
- **Hecho cuando:** se puede jugar una partida completa de principio a fin y empezar otra.

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
