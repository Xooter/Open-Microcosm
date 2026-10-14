# BioGame — Roadmap

BioGame es un **sandbox educativo, científico y de código abierto** (ver `AGENTS.md`). Este
roadmap sale de las elecciones de `docs/gameplay.md` (qué vida y qué mecanismos simula) y de las
ideas marcadas con `[x]` en `docs/ideas.md` (qué herramientas de laboratorio tiene). Las ideas sin
marcar quedan en el *Backlog*.

Cada fase tiene un **entregable visible** y un criterio de "hecho". Todo respeta el realismo
estricto: solo organismos y mecanismos reales, con la biología comprimida por `kBioTimeScale`.

## Base hecha ✅

Lo que ya existe y se mantiene; el detalle de diseño está en `AGENTS.md`.

- **Medio:** placa de Petri circular; agua Stokes–Brinkman con FFT (sin inercia); química por
  difusión + advección (O₂, CO₂/pH, P, N, Si, Fe, B₁₂, toxinas, autoinductor, fagos); luz con
  Beer–Lambert; temperatura, salinidad, anoxia y alcalinidad (`MediumParams`).
- **Células:** soft-bodies XPBD sin inercia, membrana plástica de ameba, contactos, LOD de tres
  niveles, cuerpos compuestos (colonias y filamentos) y partículas ambientales
  (bacterias con run & tumble, detritos, tinta).
- **Ciclo de vida:** metabolismo real (respiración, fermentación, fotosíntesis, Liebig con P, N,
  Si, Fe y B₁₂), mitosis física, conjugación, quistes, lisis; genoma con organelos físicos.
- **Ecosistema:** cazadores, envolvedores, sésiles y suctorios, filtradores, raspadores,
  mixótrofos, algas (móviles, diatomeas, coloniales, filamentosas), cianobacterias con fijación
  de N₂ y microcistina, quitridios, levaduras, *Saprolegnia*, *Allomyces*, hongo liquenizante,
  *Physarum*, fagos, *Bdellovibrio* y quorum sensing. Sin animales (se quitaron los metazoos y
  *Zoophagus*, que solo cazaba rotíferos). Registro de muertes y comportamientos.
- **Laboratorio:** ImGui con paneles acoplables, editor de placa (`PlateSpec`, placas guardables,
  placa vacía), catálogo, ficha, enciclopedia, herramientas (Seleccionar, Controlar, Tinta,
  Detrito, Borrar, Lisar, Apretar, Revolver, Estirar, Gotero, Spawnear), control de un ser con
  su locomoción real, `tr()` con idiomas en `assets/lang`.
- **Óptica:** campo claro con post-proceso de microscopio; motor de tinciones por reglas (Gram,
  Lugol, DAPI, rojo neutro, azul de metileno, rojo Congo) con vista previa.
- **Técnica:** tests doctest (invariantes, determinismo con 1 y 4 hilos), Tracy, render
  instanciado de bacterias.

**Pendientes heredados:**
- Rendimiento: con ~540 células el paso cuesta ~29 ms (≈ 35 FPS); 60 FPS con 500+ requiere XPBD y
  contactos en GPU (`src/gpu`).
- Vista previa gráfica de la placa antes de crearla; íconos vectoriales en la barra de
  herramientas; paleta de comandos.
- Tinciones: hifas, cianobacterias y copos de avena sin respuesta a Lugol/Gram; vista dividida
  sobre la placa real; exposición con tiempo real.
- La posición de los organelos (flagelo anterior o posterior) no está modelada.

---

## Fase 13 — Medición: ver los números ✅

Es la base de todo lo científico: sin datos no hay experimento. Va primero.

- **Gráficos en vivo** (idea 11): población por especie, nutrientes, O₂ y pH, con zoom. Panel
  nuevo; la serie temporal se muestrea en `Run`, no en `World`.
- **Exportar datos abiertos** (20): CSV/JSON de series y de la ficha de cada especie, con
  unidades y parámetros usados.
- **Regla y escala** (13): medir distancias y áreas en µm sobre la imagen (herramienta nueva).
- **Cuantificación por región** (15): contar células y medir concentraciones en un área dibujada.
- **Curva de crecimiento automática** (16): tiempo de duplicación, latencia y capacidad de carga.
- **Balance de masa visible** (12): flujos de P, N, Si, Fe y C entre agua, células, detritos y
  sedimento, que cierren al 100 % (extiende los totales conservados de Estadísticas).
- **Mapa de limitación** (17): colorear cada célula por el nutriente que la limita (P, N, Si, Fe,
  B₁₂, luz); lee `Metabolism`, solo render.
- **Hecho cuando:** una gota corrida 1 h biológica exporta CSV con unidades y el balance de P
  cierra dentro de la tolerancia del test de conservación.
- **Hecho:** panel Gráficos (ImPlot) con poblaciones, química, balance, región y curva de
  crecimiento; exportación CSV/JSON (Ctrl+E); herramientas Medir (distancia, área y barra de
  escala) y Región; balance de masa por compartimento y por proceso en Estadísticas; mapa de
  limitación (L). Test de "hecho" en `tests/test_budget.cpp`.

## Fase 14 — Reactivos y escenas

- **Reactivos con dosis-respuesta** (6): antibióticos, sal, ácidos y toxinas con el gotero;
  tolerancias por especie como datos (`SpeciesInfo`), no código por especie.
- **Escenas guardables** (1): exportar e importar una gota con seres puestos a mano (extiende
  `PlateSpec`/`.plate` con la lista de seres; texto clave=valor versionado).
- **Línea de tiempo con replays** (2): grabar seed y eventos del usuario; rebobinar re-simulando
  desde el último punto de control (aprovecha el determinismo).
- **Modo aula** (32): gota con seed fija y parámetros bloqueados que el docente comparte.
- **Hecho cuando:** una escena guardada se reproduce idéntica con 1 y 4 hilos y un replay llega
  al mismo estado final.

## Fase 15 — Química ampliada

- **Ciclo del silicio completo** (41): disolución según pH y temperatura; sedimentación de
  frústulas fuera del campo de visión.
- **Quelación del hierro** (42): sideróforos que compiten por Fe; precipitación a óxido a pH alto.
- **Más micronutrientes** (43): B₁, B₇, Zn, Co, Mo y Mn con auxotrofías reales por especie.
- **Carbono inorgánico** (44): CO₂ limitante, mecanismos concentradores de carbono y efecto en pH.
- **Biofilms y matriz extracelular** (46): tapetes que cambian el flujo y retienen nutrientes.
- **Suelo y agua de verdad** (48): cargar una muestra con T, pH, conductividad y nutrientes
  medidos (`PlateSpec`) y ver qué comunidad sostiene.
- **Hecho cuando:** cada nutriente nuevo tiene un test de conservación y una especie que se
  limita por él (Liebig) de forma observable.

## Fase 16 — Lentes y fluorescencia

- **Lentes intercambiables** (21): campo claro, contraste de fase, campo oscuro y fluorescencia,
  cada uno con la información que muestra el real (shaders en `render/`).
- **Fluorescencia de clorofila** (23): las algas brillan en rojo bajo luz azul y el brillo baja
  con la falta de hierro.
- **Tinciones restantes** (22): tinta china para cápsulas, sumada al motor de reglas de `stain`.
- **Modo accesible** (40): paleta para daltonismo, texto grande y subtítulos de eventos.
- **Hecho cuando:** la misma placa se ve en las cuatro lentes y cada una muestra algo que las
  otras no.

## Fase 17 — Enseñanza

- **Lecciones guiadas** (31): recorridos cortos con un objetivo observable y cuestionario (datos
  en `assets/`, sin código por lección).
- **Casos reales** (38): Gause (*Paramecium*), Tilman (diatomeas y sílice) y Jennings (CO₂),
  como escenas de la fase 14 con su gráfico de la fase 13.
- **Hecho cuando:** un alumno reproduce el resultado de Gause siguiendo la lección, sin tocar
  nada fuera del panel.

## Fase 18 — Núcleo sin ventana y bindings

- **`biosim` como librería** (50) con una interfaz mínima (C o Python) para correr gotas sin
  ventana y leer series, usando la exportación de la fase 13.
- Habilita los experimentos por lote del backlog (idea 3). Requiere proponer y aprobar la
  librería de bindings antes de agregarla (ver *Librerías* en `AGENTS.md`).
- **Hecho cuando:** un cuaderno barre un parámetro en N gotas y grafica el resultado.

## Fase 19 — Escala

- XPBD y contactos en GPU para llegar a 60 FPS con 500+ células (pendiente de la fase de GPU
  anterior); medir con Tracy antes de tocar.

---

## Backlog (ideas sin marcar de `docs/ideas.md`)

- **Experimentos:** lotes (3), rampas y programas (4), réplicas lado a lado (5), dilución y
  siembra (7), quimiostato (8), pinzas ópticas (9), micropipeta (10).
- **Análisis:** trayectorias y MSD (14), cuotas celulares (18), árbol de linaje (19).
- **Óptica:** objetivos 10×/40×/100× (24), foto con barra de escala (25), time-lapse (26), campo
  oscuro de Tyndall (27), polarización (28), lupa (29), halo de nutrientes (30).
- **Educación:** notas en la ficha (33), referencias por ficha (34), glosario (35), hipótesis y
  predicción (36), escenarios históricos (37).
- **Biología:** endosimbiosis y kleptoplastia como experimento (45), epidemias y resistencia (47).
- **Comunidad:** especies como datos TOML (49; requiere aprobar toml++).
- **De gameplay, descartado o sin elegir:** contraste de fase, fluorescencia como modo único,
  plano focal (descartado: todo en un plano), modo arcade, biología especulativa.
