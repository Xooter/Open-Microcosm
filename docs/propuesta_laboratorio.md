# BioGame — Propuesta: laboratorio virtual de sistemas biológicos

Resumen de una propuesta externa (pegada en la sesión de 2026-10-14), reordenada por utilidad y
cruzada con lo que ya existe (`AGENTS.md`), `docs/roadmap.md` e `docs/ideas.md`.

**Tesis:** pasar de "microscopio virtual con bichos" a un *laboratorio donde se construyen,
modifican, observan y miden comunidades microscópicas*. Diferenciales: (1) organismos
configurables como datos, (2) ambiente espacial y dinámico, (3) ver lo que un microscopio real no
puede (tiempo acelerado, gradientes, procesos internos), (4) convertir una observación en un
experimento cuantitativo y reproducible.

**Leyenda de estado:** ✅ ya existe · 🗺 ya está en el roadmap (fase) · 🆕 nuevo.
**Categorías:** `FEAT` función nueva · `QOL` comodidad de uso · `ARQ` arquitectura · `CONT`
contenido biológico · `EDU` enseñanza · `PLAT` plataforma/comunidad.

## Principios (se mantienen en todas las fases)

| Principio | Estado |
|---|---|
| Especies como datos, nunca código por especie ("cada microorganismo es contenido, no una feature") | ✅ `SpeciesInfo`, `FungusInfo`; falta que sean archivos externos (fase C) |
| Modelo separado del render (`biosim` sin raylib) | ✅ |
| Ambiente como campos espaciales (T, pH, O₂, nutrientes, luz…) | ✅ casi todo; **temperatura, pH base y salinidad son globales** 🆕 pasarlos a campo |
| Unidades reales en todo (µm, µm/s, µM, °C…) | ✅ internas; 🆕 mostrar mg/L y mmol/L en la UI |
| Reproducibilidad (seed, versión, parámetros) | ✅ seed y determinismo; 🆕 versión del motor y del organismo en el archivo |
| Honestidad del modelo: cada proceso declara su fidelidad y supuestos | 🆕 ver "Fidelidad" abajo |
| No buscar exactitud molecular total: eso mata el proyecto | decisión |

**Fidelidad por modelo (🆕 ARQ):** cada organismo/proceso declara `conceptual` (escuela),
`cuantitativo` (universidad: ej. Monod) o `experimental` (investigación: incertidumbre, réplicas).
La ficha muestra qué modelo usa y qué simplifica (conecta con idea 34, referencias por ficha).

---

## Fase A — Observabilidad y tiempo (máxima utilidad, poco riesgo)

Convierte lo visual en herramienta científica. Casi todo ya está planificado.

| # | Qué | Cat. | Estado |
|---|---|---|---|
| A1 | Gráficos en vivo: población por especie, biomasa, O₂, CO₂, pH, nutrientes | FEAT | 🗺 fase 13 |
| A2 | Exportar CSV/JSON con unidades y parámetros | FEAT | 🗺 fase 13 |
| A3 | Curva de crecimiento: μ, tiempo de duplicación, latencia, capacidad de carga | FEAT | 🗺 fase 13 |
| A4 | Velocidad de simulación 1×…10.000× con pausa y paso a paso | QOL | 🆕 verificar qué hay hoy; con `kBioTimeScale` 60 ya hay base |
| A5 | Retroceder / volver a un punto guardado | QOL | 🗺 fase 14 (replays por seed) |
| A6 | Regla, escala y cuantificación por región | QOL | 🗺 fase 13 |
| A7 | Unidades de laboratorio en la UI (mg/L, mmol/L) | QOL | 🆕 |
| A8 | Selección de organismo con ficha individual y contador de población | QOL | ✅ |

## Fase B — Experimentos reproducibles

| # | Qué | Cat. | Estado |
|---|---|---|---|
| B1 | Experimento = archivo con ID, seed, versión del motor, ambiente, organismos, duración | FEAT | 🆕 extiende `.plate` (🗺 fase 14, escenas) |
| B2 | Comparar dos escenarios lado a lado, cambiando una variable | FEAT | idea 5 (backlog) → **subir de prioridad** |
| B3 | Reactivos con dosis-respuesta (antibiótico, sal, ácido, toxina) | FEAT | 🗺 fase 14 |
| B4 | Rampas y programas (día/noche, pulso de fosfato, estación) | FEAT | idea 4 (backlog) → subir |
| B5 | Réplicas: N corridas con distinta semilla, con media e intervalo de confianza | FEAT | 🆕 |
| B6 | Barrido paramétrico (ej. pH 3→11, paso 0,5, 100 réplicas) → "tasa de crecimiento vs pH" | FEAT | idea 3 + 🗺 fase 18 (necesita núcleo sin ventana) |
| B7 | Quimiostato, dilución y siembra seriada | FEAT | ideas 7–8 (backlog) |
| B8 | Hipótesis y predicción antes de correr | EDU | idea 36 (backlog) |

## Fase C — Organismos como datos (extensibilidad)

La propiedad arquitectónica a proteger. Hoy las especies son tablas en C++.

| # | Qué | Cat. | Estado |
|---|---|---|---|
| C1 | Especies en archivos externos (TOML/YAML) con validación de unidades y coherencia biológica | PLAT | idea 49; requiere aprobar toml++ |
| C2 | Composición por módulos: morfología, locomoción, metabolismo, reproducción, sentidos, genética, interacciones, preferencias ambientales | ARQ | ✅ parcial (`SpeciesInfo`); 🆕 separar en bloques declarativos |
| C3 | Comportamientos reutilizables y nombrados (fototaxis, quimiotaxis, fagocitosis, fisión, gemación, fragmentación) | ARQ | ✅ en código genérico; 🆕 referenciables desde datos |
| C4 | Ambientes y experimentos también como archivos (`environments/`, `experiments/`) | PLAT | 🆕 (`PlateSpec` ya es texto) |
| C5 | Editor de organismos / ambientes / experimentos en la UI | PLAT | 🆕 después de C1 |
| C6 | Criterio de éxito: crear "anaerobia que consume lactato, se divide cada 40 min, forma biofilm y hace quimiotaxis" **sin recompilar** | meta | |

## Fase D — Ambiente espacial y metabolismo (gradientes)

| # | Qué | Cat. | Estado |
|---|---|---|---|
| D1 | Temperatura, pH y salinidad como campos (T[x,y], etc.), con fuentes puntuales | ARQ/FEAT | 🆕 (hoy `MediumParams` global; el pH ya sale del sistema carbonato) |
| D2 | Mostrar gradientes (O₂, nutrientes) como mapa y perfil, y a los organismos respondiendo | FEAT | ✅ overlay de campos; 🆕 perfil/corte |
| D3 | Anaerobiosis estratificada: aerobias arriba, anaerobias donde O₂ ≈ 0 | CONT | ✅ base (fermentación, anoxia); 🆕 escena de demostración |
| D4 | Biofilms con matriz, adhesión, gradientes internos y resistencia diferencial; **corte del biofilm** | FEAT | 🗺 fase 15 (sin el corte) |
| D5 | Metabolismo por vías: sustrato → vía → ATP → biomasa; aerobia/anaerobia, fotosíntesis, quimiosíntesis, ciclo del N | CONT | ✅ parcial (resp., ferm., foto, N₂); 🆕 quimiosíntesis, desnitrificación, lactato y otros sustratos |
| D6 | Más micronutrientes, Fe quelado, ciclo del Si, carbono inorgánico, muestras reales | CONT | 🗺 fase 15 |
| D7 | Quimiotaxis por receptores → respuesta motora (no "ir al objetivo") | FEAT | ✅ en bacterias (*E. coli*, run & tumble); 🆕 extender a protistas |

## Fase E — Microscopía simulada y "Reveal"

| # | Qué | Cat. | Estado |
|---|---|---|---|
| E1 | Lentes: campo claro, fase, campo oscuro, fluorescencia, tinciones | FEAT | 🗺 fase 16 (tinciones ✅) |
| E2 | Imperfecciones: resolución, profundidad de campo, ruido, iluminación | FEAT | ✅ parcial (grano, aberración); 🆕 resolución según aumento (idea 24). Sin plano focal por decisión |
| E3 | Foto con barra de escala, aumento, fecha y seed; time-lapse | QOL | ideas 25–26 (backlog) → subir |
| E4 | **Modo Reveal:** capa "observable" vs capa "real" (ATP, expresión génica, gradientes, organelos internos) | FEAT | 🆕 depende de G1 |
| E5 | Microscopía electrónica | FEAT | 🆕 baja prioridad |
| E6 | Modo accesible | QOL | 🗺 fase 16 |

## Fase F — Genética y evolución

| # | Qué | Cat. | Estado |
|---|---|---|---|
| F1 | Mutación, selección y evolución en cientos de generaciones a tiempo acelerado | FEAT | ✅ genoma y mutación en mitosis; 🆕 poblaciones grandes de bacterias con genoma + presión ambiental |
| F2 | Árbol de linaje con mutaciones por rama | FEAT | idea 19 (backlog) → subir |
| F3 | Transferencia horizontal: conjugación, transformación, transducción | CONT | ✅ conjugación de ciliados; 🆕 plásmidos bacterianos, transducción por fagos |
| F4 | Resistencia antimicrobiana con costo, observable en el árbol | CONT | idea 47 (backlog) |
| F5 | Genotipo → expresión → regulación → fenotipo (genes que responden al ambiente) | ARQ | 🆕 grande; hacerlo solo si C1–C3 ya andan |

## Fase G — Sistemas complejos

| # | Qué | Cat. | Estado |
|---|---|---|---|
| G1 | Procesos internos modelados (ATP, ribosomas, ADN) para alimentar Reveal | ARQ | 🆕 acotar: solo ATP, reservas y expresión de pocos genes |
| G2 | Virus: adsorción → entrada → replicación → ensamblaje → liberación; ciclo lítico y lisogénico | CONT | ✅ fagos líticos como campo; 🆕 lisogenia y ciclo por hospedador con partículas |
| G3 | Simbiosis y endosimbiosis como experimento | CONT | ✅ liquen, mutualismos; idea 45 (backlog) |
| G4 | Comunidades y ciclos biogeoquímicos (C, N, P, S) | CONT | ✅ P, N, Si, Fe; 🆕 C y S |
| G5 | Modelos estocásticos y matemáticos avanzados (Gillespie, ODE comparables) | FEAT | 🆕 comparar la simulación con el modelo analítico (Monod, Lotka–Volterra) |
| G6 | Escenarios históricos (Gran Oxidación, bola de nieve, eutrofización) | EDU | idea 37 (backlog) |

## Fase H — Enseñanza por niveles

| # | Qué | Cat. | Estado |
|---|---|---|---|
| H1 | **Modo estudiante / modo científico:** misma simulación, distinta interfaz (play, T, pH, nutrientes vs. condiciones iniciales, ecuaciones, semilla, réplicas, IC, exportar) | QOL | 🆕 apoyado en paneles ImGui |
| H2 | Lecciones guiadas con cuestionario; modo aula con seed fija | EDU | 🗺 fases 14 y 17 |
| H3 | Casos clásicos: Gause, Tilman, Jennings | EDU | 🗺 fase 17 |
| H4 | Tres niveles de uso (escuela / universidad / investigación) con tareas tipo ("determiná el efecto de la glucosa sobre μ") | EDU | 🆕 paquetes de contenido |
| H5 | Glosario interactivo, notas y referencias por ficha | EDU | ideas 33–35 (backlog) |
| H6 | Paquetes educativos, colaboración y versionado de experimentos | PLAT | 🆕 al final |

## Fase I — Plataforma

| # | Qué | Cat. | Estado |
|---|---|---|---|
| I1 | `biosim` sin ventana + API (C/Python) para cuadernos y lotes | PLAT | 🗺 fase 18 |
| I2 | Scripting y plugins/mods | PLAT | 🆕 después de C1 |
| I3 | Rendimiento: 500+ células a 60 FPS (XPBD y contactos en GPU) | ARQ | 🗺 fase 19 |
| I4 | Eventual paso a 3D | ARQ | solo se protege con la separación modelo/render |

---

## Orden recomendado (por utilidad / costo)

1. **A** (observabilidad y tiempo) — ya planificado, habilita todo lo demás.
2. **B** (experimentos reproducibles) — B1, B2, B4 y B5 antes que B6.
3. **C** (organismos como datos) — antes de sumar más especies a mano: cada especie nueva en C++
   es deuda.
4. **D** (gradientes, biofilms, metabolismo) y **E** (lentes y Reveal) en paralelo; D es contenido,
   E es render.
5. **F** y **G** (evolución, virus): grandes; F1/F2 primero porque ya hay genoma.
6. **H** (niveles de enseñanza) — la interfaz de modo estudiante/científico puede adelantarse
   cuando A y B existan.
7. **I** (plataforma) al final, salvo I1, que destraba B6.

## Qué falta decidir

- **toml++ vs YAML** para especies (el repo solo prevé toml++).
- **Alcance de Reveal (G1):** qué procesos internos se modelan de verdad y cuáles son ilustración.
  Para mantener el realismo estricto, mostrar solo lo que el modelo calcula.
- **Fidelidad:** si se declara por organismo o por proceso.
- **Campo de temperatura:** el costo de pasar T a campo es la cadena Q10 en `World::bioScale()`,
  que hoy es un escalar.
