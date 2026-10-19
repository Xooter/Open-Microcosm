# BioGame — Roadmap nuevo

Estado al 17/10/2026. Junta en un solo lugar lo que estaba repartido entre `roadmap.md`,
`plan_fase20.md`, `plan_p1.md`, `validacion.md`, `ideas.md`, `gameplay.md` y
`propuesta_laboratorio.md`. Lo hecho se resume (el detalle de diseño está en `AGENTS.md` y en los
planes de cada fase); lo pendiente se ordena en fases nuevas.

Leyenda: ✅ hecho · 🟡 en parte (se dice qué falta) · ⬜ por hacer · ❌ descartado.

---

## 1. Lo hecho

### Núcleo de simulación ✅

- **Medio:** placa de Petri circular (o mundo toroidal); agua Stokes–Brinkman exacta con FFT, sin
  inercia; química por difusión espectral + advección (O₂, CO₂/pH con carbonato y Henry, P, N, Si,
  Fe, B₁₂, materia orgánica, toxinas, autoinductor, fagos, reactivo; glutamina, lactato y fMLP solo
  con células animales); luz con Beer–Lambert; temperatura con Q10, salinidad, anoxia, alcalinidad,
  CO₂ del aire variable, osmolitos.
- **Cuerpos blandos:** XPBD sin inercia con tasas [1/s], membrana plástica de ameba, contactos
  intercalados, LOD de tres niveles, cuerpos compuestos (colonias y filamentos con uniones rígidas a
  la flexión), `reshapeRestEven` para cambios grandes de forma.
- **Ciclo de vida:** una sola fisiología para placa y cultivo (`bio/cell_physiology`); respiración,
  fermentación y fotosíntesis; Droop con Liebig (P, N, Si, Fe, B₁₂); masa por esferoide; punto de
  control al tamaño de división; mitosis física, conjugación, quistes, gemación, lisis; genoma con
  organelos físicos.
- **Ecosistema de la gota:** cazadores, envolvedores, sésiles, suctorios, filtradores, raspadores,
  mixótrofos; algas móviles, diatomeas, coloniales y filamentosas; cianobacterias con heterocistos
  y microcistina; quitridios, levaduras, *Saprolegnia*, *Allomyces*, liquen, *Physarum*; fagos con
  mutantes resistentes, *Bdellovibrio*, quorum sensing, biopelícula como refugio. 32 seres en el
  paquete de fábrica.
- **Tiempo:** física en tiempo real, biología ×`kBioTimeScale` (60), migración por arrastre en
  tiempo biológico.

### Contenido como datos (fase 20.0) ✅

Registro con ids de texto; seres, planes corporales, fisiología con fuentes, minerales, biomas,
medios, recipientes, placas preparadas, lecciones, glosario y reactivos en TOML; dietas por
etiquetas; paquetes del usuario y `.biopack` con validación y topes; panel Contenido; compatibilidad
con placas y escenas viejas; huellas de los paquetes en la procedencia.

Un archivo por definición en la carpeta de su tipo (`beings/paramecium.toml`, con su fisiología;
`docs/contenido.md`) y todos los nombres de máquina en inglés (contenido, escenas de cultivo,
programas, experimentos, claves de parámetros, carpetas de datos). Decidido: el texto que se lee
(nombres, descripciones, fuentes, comentarios) sigue en español y lo traduce `assets/lang`; lo
guardado con los nombres viejos se sigue leyendo (`legacy_ids`, `game/legacy_names`) y los paquetes
viejos se migran con `tools/migrate_content.py`.

### Placas preparadas, cultivo celular y preparados (fase 20a–c) ✅

- Cultivo celular en DMEM con incubadora (37 °C, 10 % CO₂), recipiente y campo separados,
  inoculación reproducible y mezclas.
- Células humanas: fibroblasto dérmico, HeLa y neutrófilo; arrastre con paseo persistente, ciclo
  G1/S/G2/M con punto de restricción y G0, inhibición por contacto, Michaelis–Menten de glucosa y
  glutamina, Warburg, apoptosis, quimiotaxis a fMLP y fagocitosis. Validadas (duplicación, MSD de
  Fürth, meseta de confluencia, lactato/glucosa, rapidez del neutrófilo).
- Preparados en portaobjetos: cebolla, *Elodea*, papa con Lugol, punta de raíz, estomas, mucosa
  bucal, frotis con Giemsa y sangre diluida; ósmosis de Boyle–van 't Hoff, plasmólisis y hemólisis
  validadas (Parpart).

### Herramienta de laboratorio (P0) ✅

- **Reproducibilidad:** registro único de parámetros, intervenciones por paso, escenas que se
  repiten bit a bit con 1 y 4 hilos, procedencia completa en la exportación.
- **Medición (fase 13):** gráficos en vivo, CSV/JSON, regla y área, región, curva de crecimiento,
  balance de masa por compartimento y por proceso, mapa de limitación.
- **Distribución:** presets `dist` y `windows`, CPack, GPL-3.0-or-later con licencias.
- **Estabilidad:** autoguardado con recuperación, soak, `fallas_numericas`, CI local.
- **Rendimiento (fase 19):** XPBD y contactos en GPU deterministas, `limitApproach` 8,5× más
  rápido, render por lotes propios: 551 células en 16,1 ms por cuadro.

### Experimentos y aula (P1) ✅

Cultivo bien mezclado por individuos con calibración cruzada placa ↔ cultivo; programas (rampas,
ciclos, eventos) en cultivo y placa; panel Cultivos con gota de muestra; Gause (mesetas, exclusión,
*Didinium*); réplicas y lotes (`bio_lote`, panel Experimento, IC 95 %); quimiostato (Monod y Tilman
con exclusión completa) y perfusión; siembra por densidad, pasajes y diluciones seriadas; inglés;
lecciones como protocolos con solución testeada (Jennings, Tilman, Gause, plasmólisis, fragilidad
osmótica); reactivos con farmacodinámica y CIM que emerge; foto y time-lapse por tiempo biológico;
glosario y notas ancladas; accesibilidad (escala de interfaz, HiDPI, Okabe–Ito).

### Óptica ✅ / 🟡

- ✅ Campo claro con post-proceso (viñeta, aberración, grano), vidrio de la placa, zoom semántico
  por tamaño proyectado, plan corporal y vista de modelo.
- ✅ Tinciones por reglas: Gram, Lugol, DAPI, rojo neutro, azul de metileno, rojo Congo, Giemsa,
  orceína; fijar la muestra; vista previa.
- ✅ Óptica fase A: iluminación de Köhler despareja y polvo del ocular y la cámara.

### Validación científica 🟡

Pasan: µmax de *Paramecium* (placa y cultivo), Tilman (lote y quimiostato), Jennings, Monod/Droop,
Monod en quimiostato, calibración de la respuesta funcional, CIM de ampicilina y tetraciclina,
células humanas y ósmosis. Lo que falta está en la fase A de abajo.

---

## 2. Por hacer

### Fase A — Limpieza y deuda abierta

Lo chico o ya decidido, antes de sumar cosas.

- ⬜ **Quitar la enciclopedia** (decidido el 17/10/2026): panel, `game/encyclopedia`, «la
  enciclopedia se llena con lo que la cámara ve», su parte de `Progress` y sus textos y claves de
  traducción. Decidir si `deathTally()` y `observations()` quedan (las usan Estadísticas o la
  exportación) o se van con ella. Actualizar `AGENTS.md`.
- ⬜ Valores con números de la ficha (`game/inspect`) en inglés.
- ⬜ Inoculación en una región dibujada (la intervención no lleva el polígono).
- ⬜ Tinciones: hifas, cianobacterias y copos de avena sin respuesta a Lugol/Gram.
- ⬜ Posición de los organelos (flagelo anterior o posterior) sin modelar.
- ⬜ Archivar los documentos superados (`gameplay.md` describe el juego anterior; `roadmap.md`,
  `propuesta_laboratorio.md` e `ideas.md` quedan absorbidos por este).

### Fase B — Cerrar la validación y el lanzamiento

El criterio de «hecho» del P0 que quedó abierto.

- ⬜ **Gause:** que gane *P. aurelia* (hoy gana la que se adelanta; test `should_fail`). Buscar el
  mecanismo real (sensibilidad de *P. caudatum* a los desechos), no ajustar parámetros.
- ⬜ *Didinium*–*Paramecium* con inmigración (oscilaciones).
- ⬜ µmax de *Amoeba proteus* (cultivo con *Tetrahymena* como presa).
- ⬜ Cargar µmax con fuente para las especies que usan el valor general (0,15/h).
- ⬜ Verificar contra las publicaciones las temperaturas de Gause y las constantes de Tilman y
  Kilham (1976); revisar L = 1,5 de Droop para el P.
- ⬜ Respuesta funcional de *P. caudatum* medida a varias densidades.
- ⬜ **Gota sostenible:** resistencia fenotípica a *Bdellovibrio* (Shemesh y Jurkevitch 2004),
  densidades iniciales realistas para el volumen; reactivar el test de 30 min.
- ⬜ Revisar la materia orgánica de cientos de mM tras una lisis en volumen chico.
- ⬜ **Paquetes:** probar el de Linux en otra distribución y el de Windows en una máquina limpia;
  soak de varias horas sin fallas.
- **Hecho cuando:** pasan todos los tests de `validacion` sin `should_fail` ni `skip` y los dos
  paquetes corren en máquinas limpias. Después: **lanzamiento**.

### Fase C — Lentes

Lo que hace falta para mirar de verdad los cultivos celulares (AGENTS.md: «contraste de fase,
lente pendiente»).

- ⬜ **Contraste de fase:** grises con halo; la lente por defecto del cultivo celular.
- ⬜ **Campo oscuro** (incluido el de Tyndall: partículas diminutas brillantes).
- ⬜ **Fluorescencia** con la de clorofila (roja bajo luz azul, baja con la falta de hierro).
- ⬜ Tinta china para cápsulas (regla nueva del motor de tinciones).
- ⬜ Tinciones sobre la placa real con vista dividida y exposición en tiempo real.
- ⬜ Después: objetivos 10×/40×/100× (cambian el campo y lo que se resuelve; la foto suma el
  aumento), polarización (frústulas, almidón).
- **Hecho cuando:** la misma placa se ve en las cuatro lentes y cada una muestra algo que las otras
  no.

### Fase D — Química ampliada (fase 15 vieja)

- ⬜ Ciclo del silicio completo (disolución con pH y temperatura, sedimentación de frústulas).
- ⬜ Quelación del hierro (sideróforos, óxido a pH alto).
- ⬜ Más micronutrientes (B₁, B₇, Zn, Co, Mo, Mn) con auxotrofías reales.
- ⬜ Carbono inorgánico limitante y mecanismos concentradores.
- ⬜ Biofilms con matriz (cambian el flujo, retienen nutrientes, gradientes internos, corte).
- ⬜ Muestras reales: cargar T, pH, conductividad y nutrientes medidos.
- ⬜ Temperatura, pH base y salinidad como campos (hoy globales; costo: `bioScale()` deja de ser un
  escalar).
- **Hecho cuando:** cada nutriente nuevo tiene un test de conservación y una especie limitada por él
  de forma observable.

### Fase E — Cultivo bien mezclado completo

Lo que `Culture::supports` todavía rechaza.

- ⬜ Células animales, filamentos, colonias y zoosporas en el cultivo.
- ⬜ Conjugación, *Bdellovibrio*, quorum y liberación de B₁₂ por las bacterias.
- ⬜ Reactivos con otros mecanismos: metales, sal y blancos eucariotas.
- **Hecho cuando:** toda placa preparada tiene un equivalente bien mezclado con la misma fisiología.

### Fase F — Más biología celular

- ⬜ Más tipos celulares reales con datos publicados (queratinocito, otras líneas).
- ⬜ Apertura y cierre de estomas.
- ⬜ Endosimbiosis y kleptoplastia como experimento (*P. bursaria* con *Chlorella*).
- ⬜ Resistencia antimicrobiana con costo y transferencia horizontal (plásmidos, transducción).
- ⬜ Lisogenia de los fagos.

### Fase G — Análisis

- ⬜ Trayectorias, velocidad media y desplazamiento cuadrático medio de cualquier ser.
- ⬜ Panel de cuotas celulares (cuántas veces Q₀ tiene la célula elegida).
- ⬜ Árbol de linaje con mutaciones por rama.
- ⬜ Dos placas lado a lado con la misma seed y una variable distinta.
- ⬜ Línea de tiempo: rebobinar re-simulando desde un punto de control.
- ⬜ Comparar con el modelo analítico (Monod, Lotka–Volterra) en el mismo gráfico.
- ⬜ Unidades de laboratorio en la interfaz (mg/L, mmol/L).

### Fase H — Núcleo sin ventana y bindings (resto de la fase 18)

- ✅ `biosim` sin raylib, `bio_bench`, `bio_lote`.
- ⬜ Interfaz mínima de C o Python (proponer la librería de bindings antes de agregarla).
- **Hecho cuando:** un cuaderno barre un parámetro en N corridas y grafica el resultado.

### Fase I — Rendimiento (resto de la fase 19)

- ⬜ `limitApproach` (~8 ms, serial), espera de la GPU (`collect`, kernel `pairs`), química + FFT,
  `applyMotion`, `resolvePenetrations`, `perceive`.
- **Hecho cuando:** 60 FPS con 540+ células y GPU en hardware de escritorio medio, con checksums
  iguales.

### Fase J — Interfaz y enseñanza

- ⬜ Vista previa gráfica de la placa antes de crearla.
- ⬜ Íconos vectoriales en la barra de herramientas; paleta de comandos.
- ⬜ Modo estudiante / modo científico (misma simulación, otra interfaz).
- ⬜ Hipótesis y predicción fuera de las lecciones.
- ⬜ Escenarios históricos (eutrofización, floración de cianobacterias).
- ⬜ Fidelidad declarada por proceso (conceptual, cuantitativo, experimental) en la ficha.

---

## 3. Descartado ❌

- Partida, puntaje, victoria, roguelite y herencia entre partidas (visión del 15/10/2026: es un
  laboratorio).
- Enciclopedia y descubrimiento (17/10/2026).
- Animales pluricelulares (rotíferos, *Hydra*) y *Zoophagus*, hasta que su simulación sea realista.
- Plano focal: todo ocurre en un plano.
- Vulkan: raylib 5.5 no lo tiene y el cuadro está limitado por la CPU.
- Modo arcade, biología especulativa, realismo exagerado.

## 4. Orden propuesto

1. **A** (limpieza: enciclopedia y deuda chica).
2. **B** (validación y paquetes) → **lanzamiento**.
3. **C** (lentes: contraste de fase primero, para los cultivos).
4. **H** (bindings) y **G** (análisis), que aprovechan lo que ya hay.
5. **D**, **E** y **F** (biología y química nuevas), en el orden que pidan las lecciones.
6. **I** y **J** cuando haga falta.
