# BioGame — Candidatos: seres y experimentos

Dos listas para elegir qué sumar: **seres unicelulares** (y células sueltas de pluricelulares) y
**experimentos clásicos** que el simulador podría repetir como lección o como test de validación.
No es un plan: el orden de trabajo lo pone `roadmap.md`.

**Criterios de inclusión** (los de `AGENTS.md`): seres y mecanismos reales, visibles con un
microscopio óptico y con literatura publicada. Quedan afuera los animales pluricelulares completos
(rotíferos, nematodos, *Hydra*, *Daphnia*) y todo lo especulativo.

**Sobre los números:** los tamaños son órdenes de magnitud para planificar, no datos. Las
referencias son las clásicas de cada experimento. Antes de cargar un valor en
`assets/content/base/` hay que leer la fuente original y anotarla ahí, como el resto de la
fisiología.

**Leyenda**

- **Reusa:** lo que ya tiene el motor (`LocomotionType`, `FeedingMode`, banderas de
  `SpeciesInfo`: `glider`, `frustule`, `filament`, `colonyDaughters`, `sessile`,
  `contractileStalk`, `tentacleReach`, `neckReach`, `walker`, `budding`, `encysts`, `conjugates`,
  `phototaxis`, `acidTrap`; cepas de `Bacteria`, cianobacterias con heterocistos, hongos de
  `bio/fungi`, plasmodio, células animales, tejidos).
- **Nuevo:** el mecanismo que falta. "—" = se carga solo con datos.
- **Prioridad:** **A** (aporta mucho o es barato), **M** y **B**.
- **Estado de los seres:** ✅ ya está en el catálogo (ver «Avance de los seres») · ⏸ investigado
  pero bloqueado (se dice por qué).
- **Estado de los experimentos:** ✅ hecho · 🟢 factible con lo que hay (falta armar la lección o
  el test) · 🟡 falta poco (se nombra qué) · 🔴 falta un mecanismo grande · ⛔ fuera de alcance
  (y por qué).
- **Montaje:** *placa* (gota), *cultivo* (bien mezclado, `game/culture`), *preparado*
  (portaobjetos) o *celular* (cultivo celular en placa).

Biomas que existen: charco, estanque, musgo húmedo, intestino de termita, fuente termal y hielo
marino. Cuando un ser pide otro, se indica (salina, turbera, mar abierto, suelo).

## Avance de los seres

Cada ser cargado tiene su tamaño, su µmax (si hay uno publicado) y las fuentes en
`assets/content/base/beings/<id>.toml` (con su `[physiology.<id>.*]`), y un test que lo hace reproducirse
(`tests/test_candidates.cpp`). µmax se lleva a 20 °C con Q10 = 2.

### Lote 1 (17/10/2026)

| Ser (id) | Tamaño cargado | µmax publicado | Fuente del µmax | Notas |
|---|---|---|---|---|
| *Tetrahymena pyriformis* (`tetrahymena`) | 50 × 28 µm | 0,22/h a 25 °C | Curds & Cockburn 1968, J. Gen. Microbiol. 54: 343 | Amicronucleada: no se conjuga. Presa de *Amoeba* y *Podophrya* |
| *Colpidium campylum* (`colpidium`) | 54 × 24 µm | 1,81 generaciones/día a 20 °C (13,3 h) | Laybourn 1973, tesis (Stirling), tabla 5 | Presa de *Didinium*, *Amoeba* y *Podophrya* |
| *Saccharomyces cerevisiae* (`saccharomyces`) | 6 × 5 µm (diploide) | 90 min a 30 °C (YPD) | Sherman 2002, Methods Enzymol. 350: 3 | No se siembra en las gotas: se inocula |
| *Chlorella vulgaris* (`chlorella`) | 7 µm | 0,75/día a 25 °C (estimación) | Bamba et al. 2015, Appl. Biochem. Biotechnol. 177: 389 | 4 autosporas. Rango térmico de la compilación de 424 cepas (Sci. Total Environ. 2023) |
| *Raphidocelis subcapitata* (`raphidocelis`) | 11 × 5 µm (real 2–3 µm de ancho; espesor ajustado al volumen medido) | 1,5–1,7/día a 21 °C | OECD 201, anexo 2 | Alga del ensayo de toxicidad |
| *Desmodesmus subspicatus* (`desmodesmus`) | 9 × 5 µm (espesor ajustado al volumen medido) | 1,2–1,5/día a 21 °C | OECD 201, anexo 2 | Cepa de laboratorio: células sueltas |
| *Phacus pleuronectes* (`phacus`) | 54 × 38 µm | sin dato | — | Usa el µmax general del metabolismo |
| *Pinnularia viridis* (`pinnularia`) | 120 × 22 µm | sin dato | — | Elipse en vez de valva lineal (ver bloqueos). Usa el µmax general |

Estilo de dibujo nuevo: `cocoide` (célula lisa, sin apéndices ni vacuola contráctil) para las algas
verdes inmóviles con pared.

### Bloqueados y problemas encontrados

- ⏸ ***Acanthamoeba castellanii*:** la citocinesis no se completa en la placa: el surco se estanca
  a ~6,7 µm del eje y la ameba queda dividiéndose para siempre. Le pasa también a *Naegleria*, que
  ya estaba. *Amoeba proteus* (90 µm de radio) sí se divide. Descartado: los organelos en el plano
  de división (solo hay gránulos ahí) y la tensión cortical (anulada, sigue igual). Sospecha: en
  la membrana plástica la forma de reposo es un círculo, así que el eje del surco (la orientación
  del cuerpo, `axisOf`) no está fijo y gira mientras se estrangula. Arreglo propuesto: fijar el
  eje de división al empezar (`Dividing`) para las membranas plásticas. Datos ya investigados:
  trofozoíto de 15–45 µm (Marciano-Cabral & Cabral 2003, Clin. Microbiol. Rev. 16: 273);
  µ ≈ 0,04/h con *E. coli* adherida (Pickup, Pickup & Parry 2007, FEMS Microbiol. Ecol. 59: 556,
  temperatura sin verificar); crece más rápido cerca de 32 °C y se enquista arriba de ~36 °C.
- **Diatomeas grandes y formas de cilindro:** con `cuadratura` 4, o con más de ~80 µm de largo
  (también *Navicula* agrandada), el surco de la división no llega a cerrarse. *Pinnularia* quedó
  como elipse de 120 µm, que sí se divide. Además, la diatomea real se divide en un plano paralelo
  a las valvas (las hijas quedan una sobre otra en vista valvar), no estrangulándose por la
  cintura como hoy.
- **Ciliados que nadan:** *Paramecium* y *P. aurelia* (y los dos ciliados nuevos) a veces tienen
  el anillo de membrana cruzado ~20 s después de dividirse. Es anterior a este lote; falta ver si
  es un cruce transitorio en los choques o un pliegue que queda.

### Lote 2 (17/10/2026)

| Ser (id) | Tamaño cargado | µmax publicado | Fuente del µmax | Notas |
|---|---|---|---|---|
| *Gonium pectorale* (`gonium`) | colonia de 40 µm, 16 células | sin dato de la especie | — | Placa de una célula de espesor; nada a ~40 µm/s (de Maleprade et al. 2020, Phys. Rev. E 101: 022416) |
| *Pandorina morum* (`pandorina`) | colonia de 50 µm, 16 células | sin dato | — | Velocidad: la de *Gonium* |
| *Eudorina elegans* (`eudorina`) | colonia de 100 µm, 32 células | sin dato | — | Velocidad: la de *Gonium* |
| *Stylonychia mytilus* (`stylonychia`) | 300 × 122 µm | ciclo de 12 h (~21 °C) | Shi & Ammermann 2004, Protistology 3: 219 | Ancho por la proporción de *S. harbinensis* |
| *Frontonia leucas* (`frontonia`) | 250 × 138 µm | sin dato | — | Largo medio de Foissner (Serra et al. 2021, BMC Zool. 6: 4); ancho estimado |
| *Tokophrya infusionum* (`tokophrya`) | 44 × 34 µm | sin dato | — | Suctorio que se cría con *Tetrahymena* (ATCC 30297) |
| *Cryptomonas ovata* (`cryptomonas`) | 44 × 18 µm | 0,72 divisiones/día (estimación) | Cloern 1977, J. Phycol. 13: 389 (citado por Ojala 1991) | Temperatura sin verificar. Plan corporal nuevo `criptofita` |

Las volvocales liberan 4 colonias hijas (en la realidad, una por célula: 16 o 32); la biomasa se
reparte igual, así que la tasa de crecimiento no cambia. Estilos de dibujo nuevos:
`colonia_de_placa`, `colonia_compacta` y `colonia_hueca`, con `celulas_colonia` en los datos.

### Lote 3 (17/10/2026)

Tamaños de los diagnósticos de M. Kreutz (realmicrolife.com) y de NIES (The World of Protozoa). µmax
de los ciliados sin medición propia: **estimación alométrica** de Müller & Geller (1993, Arch.
Hydrobiol. 126: 315), ln r = 1,52 ln T − 0,27 ln V − 1,44, a 20 °C con el volumen del esferoide
cargado. Comprobada contra los que tienen dato: *Colpidium* 1,64/día estimado contra 1,25 medido;
*P. caudatum* 1,19 contra 0,95.

| Ser (id) | Especie | Tamaño cargado | µmax | Notas |
|---|---|---|---|---|
| `coleps` | *Coleps hirtus* | 52 × 26 µm | 1,59/día (alométrica) | Armadura de carbonato sin modelar |
| `blepharisma` | *B. americanum* | 230 × 80 µm (ancho estimado) | 0,70/día (alométrica) | Fotofobia sin modelar |
| `colpoda` | *C. cucullus* | 70 × 46 µm (ancho estimado) | 1,19/día (alométrica) | Se divide libre (en la realidad, en quiste reproductivo) |
| `chilodonella` | *C. uncinata* | 45 × 29 µm (ancho estimado) | 1,97/día (alométrica) | Camina; raspa diatomeas |
| `litonotus` | *L. lamella* | 75 × 18 µm | 2,18/día (alométrica) | Caza *Euplotes* |
| `acineta` | *A. tuberosa* | 68 × 44 µm | sin dato | Suctorio con lórica |
| `peranema` | *P. trichophorum* | 50 × 20 µm (ancho estimado) | sin dato | Plan nuevo `euglenido_incoloro`; traga *Euglena* y levaduras |
| `trachelomonas` | *T. hispida* | 31 × 21 µm | sin dato | Lórica como coraza 0,8 |
| `haematococcus` | *H. lacustris* | 34 µm | sin dato | Enquista (sin el color rojo) |
| `ceratium` | *C. furcoides* | 200 × 35 µm | 0,3 divisiones/día (estimación) | Butterwick et al. 2005; plan nuevo `dinoflagelado`; sin cuernos |
| `chaos` | *C. nobile* | 320 µm | sin dato | Multinucleada |
| `euglypha` | *E. strigosa* | 72 × 48 µm | sin dato | Testa como coraza 0,9 |
| `closterium` | *C. moniliferum* | 275 × 45 µm | sin dato | Se desliza muy despacio |
| `cosmarium` | *C. botrytis* | 84 × 66 µm | sin dato | Semicélulas sin dibujar |
| `micrasterias` | *M. rotata* | 300 × 230 µm | sin dato | Lóbulos sin dibujar |
| `gomphonema` | *G. truncatum* | 44 × 12 µm | sin dato | Una célula sobre su tallo (sin la colonia) |
| `nih_3t3` | NIH/3T3 | 70 × 20 µm | 19 h (JCRB0615) | Migración 18 µm/h (0,1–0,5 µm/min), persistencia 2,5 h (Gail & Boone 1970); confluencia y metabolismo estimados |
| `hacat` | HaCaT | 40 × 36 µm | 28 h (Cellosaurus) | Migración 63 µm/h (PLoS ONE 2012, tabla 2, por verificar); metabolismo estimado |

### Lote 4 (17/10/2026)

| Ser (id) | Especie | Tamaño cargado | µmax | Notas |
|---|---|---|---|---|
| `halteria` | *H. grandinella* | 30 × 28 µm | 1,77/día (alométrica) | Saltos sin modelar |
| `epistylis` | *E. plicatilis* | 104 × 38 µm | 1,07/día (alométrica) | Un zooide sobre tallo no contráctil; sin colonia |
| `ulothrix` | *U. tenerrima* | células de 12 × 8 µm, filamento de 10 | sin dato | Cloroplasto como placa axial |
| `phaeodactylum` | *P. tricornutum* | 30 × 5 µm | sin dato | Marina (NCMA CCMP2561, 20 °C); no está en las gotas de agua dulce |
| `thalassiosira` | *T. pseudonana* | 5,2 µm | sin dato | Marina (NCMA CCMP1335, 4–25 °C) |
| `fragilariopsis` | *F. cylindrus* | 7 × 5 µm (real 3–7 × 2–4; espesor ajustado al volumen) | sin dato | Hielo marino (NCMA CCMP1102, −2 a 2 °C) |
| `polarella` | *P. glacialis* | 12 × 7 µm | sin dato | Hielo marino (NCMA CCMP1383, 0–6 °C); enquista |

## Falta validar

Los lotes 2, 3 y 4 se cargaron **sin correr las suites** (para probarlos todos juntos). Antes del
commit:

1. **Suites:** `ctest -L rapidos` (conteo de seres, categorías, paquetes sin avisos), `ctest -L largos`
   (`tests/test_candidates.cpp`: un caso de reproducción por ser) y `ctest -L validacion`.
2. **Reproducción en la placa** (riesgos ya conocidos; ver «Bloqueados»):
   - formas largas, porque el surco puede no cerrarse: `ceratium` (200 × 35), `closterium` (275 × 45),
     `litonotus` (75 × 18), `phaeodactylum` (30 × 5), `blepharisma` (230 × 80);
   - amebas chicas: `euglypha` (radio 36 µm, con coraza);
   - cuerpos muy chicos: `thalassiosira` (radio 2,6 µm), `fragilariopsis` (3,5 × 2,5 µm);
   - sésiles nuevos: `acineta`, `epistylis` (tallo de 200 µm), `gomphonema` (diatomea sésil: combinación
     nueva de `frustula` + `sesil`);
   - filamento nuevo: `ulothrix` (células de 12 × 8 µm, las más chicas de un filamento hasta ahora);
   - estilos de colonia nuevos: `gonium`, `pandorina`, `eudorina` (el lote 2 ya pasó sus casos largos).
3. **Cultivo bien mezclado:** que `Culture::supports` acepte a los que corresponde y que los µmax
   cargados den la tasa esperada (validación nueva por especie con dato medido: `tetrahymena`,
   `colpidium`, `saccharomyces`, `chlorella`, `raphidocelis`, `desmodesmus`, `stylonychia`,
   `cryptomonas`, `ceratium`).
4. **Células animales:** `nih_3t3` y `hacat` con el ciclo, la migración y la confluencia de sus datos
   (como `tests/test_animal_cells.cpp` hace con el fibroblasto y HeLa).
5. **Rendimiento:** charco y estanque suman unas 25 especies nuevas con peso 1 (`peso_base`): medir
   `bio_bench --wildlife 3` contra el commit anterior.
6. **Datos por confirmar:** anchos estimados (*Blepharisma*, *Colpoda*, *Chilodonella*,
   *Peranema*), temperatura de los µmax de *Stylonychia*, *Cryptomonas*, *Ceratium* y *Acanthamoeba*,
   y la tabla de HaCaT.

### Datos investigados que todavía no alcanzan

- ***Gonium*:** Saito (1984, Sci. Pap. Inst. Algol. Res. Hokkaido Univ. 7: 195) midió 5,8
  duplicaciones/día a 25 °C, pero en *G. multicoccum* y con un medio orgánico: no se usa para
  *G. pectorale*.
- ***Chilomonas paramecium*:** sin datos con fuente accesible (la tesis de Michigan State con su
  crecimiento en acetato tiene acceso restringido).
- ***Bodo saltans*:** solo datos de pastoreo (come bacterias de menos de 3 µm); falta tamaño y µmax.
- ***Schizosaccharomyces pombe*:** 108 min de generación (Kovar, Wu & Pollard 2005, Mol. Biol. Cell
  16: 2313), sin la temperatura; diámetro 3,74 µm y largo al dividirse ~14–16 µm (Pombe Pages de
  Forsburg; 15,8 µm a 32 °C en un trabajo de Genetics 2012). Falta confirmar la temperatura.
- **Investigados, sin cargar:** *Spirostomum ambiguum* (884 × 89 µm extendido, 306 × 117 contraído;
  PMC11581044: muy largo para la división actual), *Bursaria truncatella* (300–600 µm, hasta 1,7 mm),
  *Euglena ehrenbergii* (140–296 × 15–26 µm: muy larga), *E. sanguinea* (90–150 µm),
  *Opercularia nutans* (50–110 × 25–40 µm, colonial), *Pelomyxa palustris* (hasta 5 mm: más grande que
  la placa). Marinos con datos de NCMA para un bioma marino futuro.
- **Necesitan código, no solo datos:** bacterias, cianobacterias y arqueas nuevas (las cepas están en
  `bio/bacteria` y `bio/cyanobacteria`), hongos con hifas (`FungusInfo`) y todo lo marcado «Nuevo» en
  las tablas.
- **Velocidades:** *Tetrahymena* nada a ~500 µm/s (estudio de nado helicoidal; 200–350 µm/s en
  otros). No se encontró la de *Colpidium* ni la de *Pinnularia*.

---

## Parte 1 — Seres

### 1.1 Ciliados

| Ser | Tamaño | Dónde | Por qué interesa | Reusa | Nuevo | Prio. |
|---|---|---|---|---|---|---|
| ✅ *Tetrahymena thermophila* / *T. pyriformis* | ~50 µm | charco, estanque | Presa ciliada de *Amoeba* (falta para su µmax, roadmap B). Organismo modelo con curvas de crecimiento muy publicadas | Cilia, OralGroove, conjugates | — | **A** |
| *Tetrahymena vorax* | 60–200 µm | charco | Polimorfismo: la forma micróstoma come bacterias y la macróstoma, otros ciliados (inducido por la presa) | Cilia, OralGroove, Raptorial | Cambio de modo de alimentación inducido | B |
| ✅ *Colpidium campylum* / *C. striatum* | 50–80 µm | estanque | El bacterívoro de los microcosmos clásicos (Lawler y Morin, Holyoak y Lawler). Presa de *Didinium* | Cilia, OralGroove | — | **A** |
| ✅ *Colpoda cucullus* / *C. steinii* | 15–80 µm | musgo, suelo | El ciliado del musgo: se enquista y desenquista con la desecación. Se divide dentro de un quiste reproductivo | Cilia, OralGroove, encysts | Quiste de división (2–4 hijas) | M |
| ✅ *Blepharisma japonicum* | 150–300 µm | charco | Pigmento rosa (blefarismina) que lo vuelve fotofóbico y fotosensible. Gigantismo caníbal | Cilia, OralGroove | Fotofobia y daño por luz; canibalismo | M |
| *Spirostomum ambiguum* | 1–3 mm | estanque | Uno de los eucariotas unicelulares más largos. Contrae todo el cuerpo en milisegundos (mionemas, Ca²⁺) | Cilia, OralGroove | Contracción del cuerpo entero | M |
| ✅ *Coleps hirtus* | 40–65 µm | estanque | Placas de carbonato en la película. Histófago: come en grupo células heridas o muertas | Cilia, Raptorial | Atracción a células dañadas | M |
| *Dileptus* | 250–500 µm | estanque | Probóscide con toxicistos que paraliza a la presa | Cilia, Raptorial | Toxicistos (presa inmovilizada) | M |
| *Loxodes* | 100–200 µm | sedimento microóxico | Cuerpos de Müller (gravirreceptores). Evita el O₂ alto y respira nitrato (Fenchel y Finlay) | Cilia, OralGroove | Gravitaxis modulada por O₂ (ver ⛔ eje vertical) | B |
| ✅ *Halteria grandinella* | 20–50 µm | estanque | Nada a saltos con cirros | Cilia, OralGroove | Saltos | B |
| ✅ *Stylonychia mytilus* / *Oxytricha* | 100–300 µm | estanque | Hipotricos caminadores, como *Euplotes* | Cilia, walker, OralGroove | — | M |
| ✅ *Frontonia leucas* | 150–600 µm | estanque | Come diatomeas y filamentos de algas enteros | Cilia, Raptorial | — | M |
| *Bursaria truncatella* | 0,5–1 mm | estanque | Ciliado enorme que caza otros ciliados | Cilia, Raptorial | — | B |
| *Paramecium bursaria* | 80–150 µm | estanque | Endosimbiosis con *Chlorella* (roadmap F). Coexistencia con *P. caudatum* (Gause) | Cilia, OralGroove, conjugates, phototaxis | Endosimbionte heredable que fotosintetiza | **A** |
| *Paramecium tetraurelia* | 100–150 µm | estanque | Modelo genético. Rasgo *killer* (bacterias *Caedibacter*) | Cilia, OralGroove, conjugates | Endosimbionte tóxico para sensibles | B |
| *Carchesium* / *Zoothamnium* | colonias de mm | estanque | Peritricos coloniales: espasmonema común (*Zoothamnium*) o propio de cada zooide (*Carchesium*) | sessile, contractileStalk, colonyDaughters | Colonia ramificada sobre tallo | B |
| ✅ *Epistylis* | colonias de mm | estanque | Peritrico colonial de tallo rígido (contraste con *Vorticella*) | sessile, colonyDaughters | Colonia ramificada | B |
| ✅ *Tokophrya* / *Acineta* | 50–150 µm | estanque | Otros suctorios (comparar con *Podophrya*) | sessile, Tentacles | — | B |
| ✅ *Litonotus* | 100–200 µm | estanque | Depredador aplanado de hipotricos | Cilia, Raptorial | — | B |
| *Lembadion* | 50–200 µm | estanque | Su señal química hace crecer "alas" defensivas en *Euplotes* (Kuhlmann y Heckmann 1985) | Cilia, Raptorial | Kairomona y defensa inducida | M |
| ✅ *Chilodonella* | 50–100 µm | estanque | Raspa la biopelícula con una canasta citofaríngea | Cilia, walker | Pastoreo de la biopelícula | B |
| *Mesodinium rubrum* | 20–50 µm | mar | Kleptoplastia: retiene cloroplastos y núcleo de criptofitas (roadmap F). Saltos muy rápidos | Cilia | Kleptoplastos con vida limitada | M |
| *Tintinnopsis* (tintínidos) | 50–200 µm | mar | Lórica aglutinada, filtradores del plancton marino | Cilia, OralGroove, coraza | — | B |

### 1.2 Flagelados y euglenoides

| Ser | Tamaño | Dónde | Por qué interesa | Reusa | Nuevo | Prio. |
|---|---|---|---|---|---|---|
| ✅ *Peranema trichophorum* | 20–70 µm | charco | Euglenoide fagótrofo: traga *Euglena* y levaduras con una varilla de ingestión. Se desliza con el flagelo rígido | Flagellum, Raptorial | Deslizamiento con flagelo | **A** |
| ✅ *Phacus* | 30–100 µm | charco | Euglenoide rígido y aplanado, con grandes granos de paramilón | Flagellum, phototaxis | — | **A** |
| ✅ *Trachelomonas* | 15–50 µm | charco | Lórica teñida por hierro y manganeso (pardo) | Flagellum, phototaxis, coraza | Lórica que consume Fe | M |
| *Euglena viridis* / *E. spirogyra* | 40–150 µm | charco | Otras euglenas (metabolia, estrías) | Flagellum, phototaxis | — | B |
| *Bodo saltans* | 5–10 µm | estanque | Cinetoplástido bacterívoro, pariente de los tripanosomas | Flagellum, Anterior | — | M |
| *Salpingoeca rosetta* / *Monosiga* | 5–10 µm | estanque, mar | Coanoflagelados: un collar filtra bacterias. Las rosetas las induce una bacteria (Alegado et al. 2012) | Flagellum | Collar filtrador; colonia inducida | M |
| *Dinobryon* | 30–50 µm por célula | estanque oligotrófico | Colonia arborescente en lóricas. Mixótrofo: fotosintetiza y come bacterias | Flagellum, phototaxis, colonyDaughters | Mixotrofia | M |
| *Synura* | colonias de 100–400 µm | estanque frío | Colonia esférica con escamas silíceas | Flagellum, colonyDaughters | Escamas que consumen Si | B |
| *Ochromonas* | 5–15 µm | estanque | Mixótrofo, depredador de *Chlorella* (experimento de Boraas) | Flagellum, Phagocytosis, phototaxis | Mixotrofia | M |
| ✅ *Cryptomonas* | 15–50 µm | estanque | Criptofita con ficoeritrina. Presa de *Mesodinium* | Flagellum, phototaxis | — | M |
| *Chilomonas paramecium* | 20–40 µm | charco con infusión | Criptofita incolora y osmótrofa, clásica de los cultivos de laboratorio | Flagellum, Osmotroph | — | M |
| ✅ *Haematococcus pluvialis* | 10–50 µm | charco temporal | Con estrés acumula astaxantina y forma quistes rojos | Flagellum, phototaxis, encysts | Pigmento por estrés | M |
| *Dunaliella salina* | ~10 µm | salina | Sin pared. Tolera la salinidad extrema con glicerol | Flagellum, phototaxis | Ajuste osmótico por glicerol; bioma salina | M |
| ✅ *Gonium* / *Pandorina* / *Eudorina* | 4–32 células | estanque | Serie volvocal de complejidad creciente hasta *Volvox* | Flagellum, phototaxis, colonyDaughters | — | **A** |

### 1.3 Dinoflagelados

| Ser | Tamaño | Dónde | Por qué interesa | Reusa | Nuevo | Prio. |
|---|---|---|---|---|---|---|
| *Peridinium* | 30–70 µm | estanque | Teca de celulosa y dos flagelos, uno en el cíngulo: nado en espiral | Flagellum, phototaxis, coraza | Nado helicoidal | M |
| ✅ *Ceratium hirundinella* | 100–400 µm | lago | Cuernos largos. Migra en la columna de agua | Flagellum, phototaxis | — | B |
| *Gymnodinium* | 10–80 µm | estanque, mar | Atecado; algunas especies forman mareas rojas | Flagellum, phototaxis | — | B |
| *Noctiluca scintillans* | 0,2–2 mm | mar | Bioluminiscencia cuando el agua se corta (estímulo mecánico) | Phagocytosis | Bioluminiscencia, flotación | B |
| *Lingulodinium polyedra* (*Gonyaulax*) | 30–50 µm | mar | Ritmo circadiano de bioluminiscencia (Hastings y Sweeney) | Flagellum, phototaxis | Reloj circadiano | B |
| *Oxyrrhis marina* | 10–40 µm | mar | Heterótrofo modelo, depredador de algas | Flagellum, Phagocytosis | — | B |
| ✅ *Polarella glacialis* | ~10 µm | hielo marino | Habitante real del hielo marino (bioma que ya existe) | Flagellum, phototaxis | — | M |

### 1.4 Amebas, heliozoos y afines

| Ser | Tamaño | Dónde | Por qué interesa | Reusa | Nuevo | Prio. |
|---|---|---|---|---|---|---|
| ⏸ *Acanthamoeba castellanii* | 15–35 µm | suelo, agua | Modelo de fagocitosis. Come bacterias y forma quistes muy resistentes | Pseudopod, Phagocytosis, encysts | — | **A** |
| *Dictyostelium discoideum* | ~10 µm | suelo, musgo | Agregación por AMPc en ondas espirales. Se simula hasta la corriente de agregación (la babosa y el cuerpo fructífero son multicelulares) | Pseudopod, Phagocytosis | Quimiotaxis a AMPc con relevo; campo de AMPc | **A** |
| *Vampyrella* | 30–60 µm | estanque | Perfora la pared de *Spirogyra* y sorbe el protoplasto | Pseudopod | Perforación de la pared | M |
| *Difflugia* | 100–300 µm | estanque, musgo | Testa de granos de arena: los toma del sedimento | Pseudopod, coraza | Testa con minerales de `minerals/` | M |
| ✅ *Euglypha* / *Nebela* / *Assulina* / *Hyalosphenia* | 30–150 µm | musgo, turbera | Tecamebas indicadoras de humedad (paleoecología) | Pseudopod, coraza | Testa de placas silíceas (consume Si) | M |
| *Actinosphaerium* / *Actinophrys sol* | 40 µm–1 mm | estanque | Heliozoos: los axopodios atrapan ciliados al contacto. Flotan | Phagocytosis | Axopodios; fusión al comer presas grandes | M |
| ✅ *Chaos carolinense* | 1–5 mm | estanque | Ameba gigante multinucleada | Pseudopod, Phagocytosis | — | B |
| *Pelomyxa palustris* | 0,5–3 mm | sedimento anóxico | Sin mitocondrias; bacterias endosimbiontes | Pseudopod, Phagocytosis | Endosimbiontes metabólicos | B |
| *Mayorella* / *Thecamoeba* | 50–200 µm | estanque | Otras morfologías amebianas (subpseudópodos, superficie rugosa) | Pseudopod, Phagocytosis | — | B |
| *Entamoeba histolytica* | 15–60 µm | intestino humano | Parásito que fagocita eritrocitos (eritrofagocitosis), visible en un preparado | Pseudopod, Phagocytosis | Presa: eritrocitos (`bio/erythrocytes`) | M |
| *Allogromia* (foraminífero) | 0,2–1 mm | mar | Reticulopodios con flujo bidireccional | — | Reticulopodios | B |

### 1.5 Algas verdes

| Ser | Tamaño | Dónde | Por qué interesa | Reusa | Nuevo | Prio. |
|---|---|---|---|---|---|---|
| ✅ *Chlorella vulgaris* | 2–10 µm | estanque | Alga modelo de crecimiento. Presa de *Ochromonas*, simbionte de *P. bursaria* | None | Autosporas (4–16 hijas por división) | **A** |
| ✅ *Desmodesmus* / *Scenedesmus* | cenobios de 4–8 | estanque | Forma cenobios. Más colonial cuando lo pastorean | colonyDaughters | — | **A** |
| ✅ *Raphidocelis subcapitata* | 8–14 µm | estanque | Alga del bioensayo estándar de toxicidad (OECD 201) | None | — | **A** |
| ✅ *Closterium* | 100–500 µm | estanque | Desmidia en media luna con cristales de yeso en vacuolas terminales. Se desliza secretando mucílago | glider | — | M |
| ✅ *Cosmarium* / *Staurastrum* | 30–100 µm | estanque, turbera | Desmidias de dos semicélulas | None | — | M |
| ✅ *Micrasterias* | 100–300 µm | turbera ácida | Morfogénesis de la semicélula después de dividirse | None | Morfogénesis (B) | B |
| *Cladophora* | filamentos ramificados | estanque | El alga del experimento de Engelmann | filament | Ramificación | M |
| ✅ *Ulothrix* | filamentos | arroyo | Filamento simple con cloroplasto en banda | filament | — | B |
| *Botryococcus braunii* | colonias | estanque | Colonia aceitosa (hidrocarburos) | colonyDaughters | — | B |
| *Chlamydomonas nivalis* / *Sanguina* | 20–40 µm | nieve, hielo | Nieve roja: quistes con carotenoides | Flagellum, encysts | Pigmento por estrés | M |
| *Acetabularia* | 2–5 cm | mar | Célula gigante uninucleada; injertos de Hämmerling | — | Morfogénesis regulada por el núcleo | B |

### 1.6 Diatomeas

| Ser | Tamaño | Dónde | Por qué interesa | Reusa | Nuevo | Prio. |
|---|---|---|---|---|---|---|
| ✅ *Pinnularia* | 50–200 µm | estanque | Diatomea grande con el rafe bien visible: la del deslizamiento en clase | glider, frustule | — | **A** |
| *Nitzschia* | 20–150 µm | estanque, mar | Deslizamiento rápido; la palea es indicadora de contaminación orgánica | glider, frustule | — | M |
| *Bacillaria paxillifer* | colonias | estuario | Colonia que se desliza como una regla extensible | glider, frustule, colonyDaughters | Coordinación del deslizamiento entre células | M |
| *Fragilaria crotonensis* | cintas | lago | Colonias en cinta. Compite por Si | frustule, colonyDaughters | — | M |
| *Melosira* / *Aulacoseira* | cadenas | lago | Céntricas en cadena, pesadas (se hunden) | filament, frustule | — | M |
| *Ulnaria* (*Synedra*) | 100–400 µm | estanque | Diatomea en aguja | glider, frustule | — | B |
| *Tabellaria* | colonias en zigzag | lago | Otra forma colonial | frustule, colonyDaughters | — | B |
| ✅ *Gomphonema* | 20–80 µm | arroyo | Se fija con un tallo mucilaginoso | sessile, frustule | — | B |
| ✅ *Phaeodactylum tricornutum* | 10–30 µm | mar | Modelo genómico. Polimórfica (oval, fusiforme, trirradiada) | frustule | — | M |
| ✅ *Thalassiosira pseudonana* | 4–6 µm | mar | Céntrica modelo para la captación de Si | frustule | — | M |
| ✅ *Fragilariopsis cylindrus* | 3–80 µm | hielo marino | Diatomea dominante del hielo marino (bioma que ya existe) | glider, frustule | — | M |

### 1.7 Otros eucariotas fotótrofos

| Ser | Tamaño | Dónde | Por qué interesa | Reusa | Nuevo | Prio. |
|---|---|---|---|---|---|---|
| *Emiliania huxleyi* | ~5 µm | mar | Cocolitofórido: placas de calcita. Lo infecta un virus propio | None | Calcificación (C inorgánico) | B |
| *Galdieria sulphuraria* / *Cyanidioschyzon* | 2–15 µm | fuente termal ácida | Rodófitas de pH ~1–3 y 40–56 °C | None | Bioma termal ácido | M |

### 1.8 Cianobacterias

Ya hay cianobacterias genéricas en cadena con heterocistos (`bio/cyanobacteria`). Estas serían
especies con nombre y datos.

| Ser | Tamaño | Dónde | Por qué interesa | Reusa | Nuevo | Prio. |
|---|---|---|---|---|---|---|
| *Microcystis aeruginosa* | colonias de células de 3–6 µm | lago eutrófico | Floraciones tóxicas (microcistina ya existe). Flota con vesículas de gas | cyanobacteria, toxina | Colonias mucilaginosas; flotación | **A** |
| *Dolichospermum* (*Anabaena*) | filamentos | estanque | Heterocistos y acinetos (esporas de resistencia) | cyanobacteria (heterocistos) | Acinetos | **A** |
| *Oscillatoria* / *Phormidium* | filamentos | estanque, fuente termal | Filamentos que se deslizan y oscilan | cyanobacteria | Deslizamiento del filamento | M |
| *Nostoc* | colonias gelatinosas | musgo, suelo | Colonias resistentes a la desecación, heterocistos | cyanobacteria | Matriz gelatinosa | M |
| *Synechococcus elongatus* | 1–3 µm | agua dulce | Reloj circadiano de tres proteínas (KaiABC; Kondo et al. 1993; Nakajima et al. 2005) | partículas | Reloj circadiano | M |
| *Synechocystis* PCC 6803 | 1–3 µm | agua dulce | Fototaxis por pili (*twitching*) | partículas | Twitching | B |
| *Synechococcus* termófilo / *Mastigocladus* | — | fuente termal | Tapetes de las fuentes termales | cyanobacteria | — | M |
| *Arthrospira* (*Spirulina*) | espirales | lago alcalino | Filamento helicoidal | cyanobacteria | Bioma alcalino | B |
| *Merismopedia* / *Gloeocapsa* | colonias | estanque | Colonia plana en placa; colonia con vainas | cyanobacteria | — | B |
| *Prochlorococcus* | ~0,6 µm | mar | El fotótrofo más abundante del océano | partículas | — | B |

### 1.9 Bacterias

Hoy son partículas con cepa (`Bacteria::strain`: un bacilo genérico, *C. violaceum* y
*B. subtilis*).

| Ser | Tamaño | Dónde | Por qué interesa | Reusa | Nuevo | Prio. |
|---|---|---|---|---|---|---|
| *Escherichia coli* | ~2 µm | intestino, laboratorio | La bacteria modelo: corrida y voltereta (Berg), diauxia, quimiostato, Luria–Delbrück, Lenski | Bacteria, quimiotaxis, fagos | Cepa con nombre y datos | **A** |
| *Staphylococcus aureus* | ~1 µm, en racimos | piel | Grampositiva en racimos. Antibiograma clásico | Bacteria, Gram | Agrupamiento en racimos | M |
| *Streptococcus pneumoniae* | ~1 µm, diplococos | vías aéreas | Cápsula (tinta china, roadmap C). Griffith | Bacteria, Gram | Cápsula | M |
| *Vibrio fischeri* | ~2 µm | mar | Quorum sensing con bioluminiscencia (Nealson et al. 1970) | Bacteria, quorum | Bioluminiscencia | M |
| *Pseudomonas fluorescens* | ~2 µm | suelo, agua | Radiación adaptativa en microcosmos estático (Rainey y Travisano 1998) | Bacteria, biopelícula | Morfotipos (*wrinkly spreader*) | M |
| *Bacillus subtilis* (ampliar) | ~3 µm | suelo | Esporulación y resistencia al calor | Bacteria (cepa 2) | Esporas | M |
| *Spirillum volutans* | 15–60 µm | estanque | Bacteria gigante, visible en detalle. Forma bandas aerotácticas | Bacteria | Aerotaxis | M |
| *Lactobacillus* | 2–5 µm | leche, intestino | Fermentación láctica; baja el pH | Bacteria, Gram | — | M |
| *Serratia marcescens* | ~1,5 µm | ambiente | Pigmento rojo (prodigiosina) | Bacteria | — | B |
| *Caulobacter crescentus* | 1–3 µm | agua dulce | División asimétrica: célula con tallo e hija nadadora | Bacteria | Ciclo con tallo | B |
| *Myxococcus xanthus* | 5–7 µm | suelo | Deslizamiento social y depredación en grupo | Bacteria | Deslizamiento social | B |
| *Magnetospirillum* | 1–3 µm | sedimento | Magnetotaxis (Blakemore 1975) | Bacteria | Campo magnético | B |
| *Beggiatoa* | filamentos | sedimento sulfuroso | Gradientes de O₂ y H₂S, gránulos de azufre | cyanobacteria (filamento) | Ciclo del S | B |
| *Rhodospirillum rubrum* | 1–2 µm | estanque | Bacteria púrpura fotótrofa; fobofototaxis (Engelmann) | Bacteria | Fototaxis bacteriana | B |
| *Proteus mirabilis* | 1–3 µm | intestino | Enjambre (*swarming*) en superficies | Bacteria | Enjambre | B |
| *Thermus aquaticus* | 3–10 µm | fuente termal | La bacteria de la Taq (Brock 1969) | Bacteria | — | M |
| Espiroquetas (*Treponema*) | 5–20 µm | intestino de termita | Simbiontes que mueven a *Mixotricha* | Bacteria | Adhesión a otro ser | B |

### 1.10 Arqueas

| Ser | Tamaño | Dónde | Por qué interesa | Reusa | Nuevo | Prio. |
|---|---|---|---|---|---|---|
| *Sulfolobus acidocaldarius* | ~1 µm | fuente termal ácida | Arquea termoacidófila (fuente termal) | Bacteria | Rango térmico y de pH extremo | M |
| *Halobacterium salinarum* | 2–5 µm | salina | Membrana púrpura (bacteriorrodopsina, Oesterhelt y Stoeckenius 1971), fototaxis | Bacteria | Bioma salina | B |
| *Haloquadratum walsbyi* | cuadrada, ~2 µm | salina | Células cuadradas y planas | Bacteria | — | B |
| *Methanobrevibacter* | ~1 µm | intestino de termita | Metanógena del intestino de termita | Bacteria | Metanogénesis (H₂ → CH₄) | B |

### 1.11 Levaduras y hongos

| Ser | Tamaño | Dónde | Por qué interesa | Reusa | Nuevo | Prio. |
|---|---|---|---|---|---|---|
| ✅ *Saccharomyces cerevisiae* | 5–10 µm | fruta, laboratorio | La levadura modelo: gemación, fermentación y efecto Crabtree. Gause 1932 | Osmotroph, budding | Etanol como campo | **A** |
| *Schizosaccharomyces pombe* | 3 × 8–14 µm | fermentados | Fisión binaria medial; control de tamaño (Nurse 1975) | Osmotroph | División por fisión medial | M |
| *Rhodotorula* | 3–8 µm | ambiente | Levadura rosa (carotenoides) | Osmotroph, budding | — | B |
| *Rhizophydium* / *Zygorhizidium* en *Asterionella* | — | lago | Quitridios con nombre sobre su hospedador (Canter y Lund) | `ChytridInfection` | — | M |
| *Penicillium* | hifas | ambiente | El experimento de Fleming (1929): halo de inhibición | hifas de `bio/fungi` | Antibiótico secretado | M |

### 1.12 Parásitos y simbiontes

| Ser | Tamaño | Dónde | Por qué interesa | Reusa | Nuevo | Prio. |
|---|---|---|---|---|---|---|
| *Plasmodium falciparum* | anillos de 1–2 µm en eritrocitos | frotis de sangre | El diagnóstico de malaria con Giemsa | frotis, Giemsa | Parasitismo intracelular del eritrocito | M |
| *Trypanosoma brucei* | 15–30 µm | frotis de sangre | Tripomastigote entre los eritrocitos | Flagellum | — | M |
| *Giardia lamblia* | 10–20 µm | intestino | Trofozoíto con disco adhesivo y dos núcleos | Flagellum | Adhesión por disco | B |
| *Trichomonas vaginalis* | 10–20 µm | mucosa | Flagelado con membrana ondulante | Flagellum | — | B |
| *Toxoplasma gondii* | 4–8 µm | cultivo celular | Taquizoítos dentro de fibroblastos | células animales | Parasitismo intracelular | B |
| *Mixotricha paradoxa* | ~500 µm | intestino de termita (*Mastotermes*) | La mueven espiroquetas adheridas a su superficie | intestino de termita | Locomoción por simbiontes | B |
| *Pyrsonympha* / *Streblomastix* / *Joenia* | 50–300 µm | intestino de termita | Otros flagelados del intestino de termita | Flagellum | — | B |
| *Holospora* | ~1–15 µm | dentro de *Paramecium* | Bacteria que infecta el núcleo de *Paramecium* | — | Infección nuclear | B |

### 1.13 Células de pluricelulares (cultivo celular y preparados)

| Célula | Tamaño | Montaje | Por qué interesa | Reusa | Nuevo | Prio. |
|---|---|---|---|---|---|---|
| ✅ Queratinocito humano (HaCaT o primario) | 10–20 µm | celular | Islotes epiteliales (roadmap F) | Crawl, ciclo celular | Adhesión célula–célula | **A** |
| ✅ Fibroblasto murino NIH 3T3 | 20–50 µm | celular | La línea de la inhibición por contacto (Todaro y Green 1963) | Crawl, ciclo celular | — | **A** |
| MDCK | 10–20 µm | celular | Epitelio en monocapa | Crawl, ciclo celular | Adhesión célula–célula | M |
| Macrófago (RAW 264.7, o monocito diferenciado) | 10–20 µm | celular | Fagocitosis de bacterias y cuerpos apoptóticos | Crawl, Phagocytosis | Diferenciación del monocito | M |
| Queratocito de pez | 20–40 µm | celular | Migración rápida en abanico; modelo de motilidad | Crawl | — | M |
| Espermatozoide humano | 50 µm con cola | preparado | Nado flagelar y espermograma (movilidad) | Flagellum | Nado con flagelo posterior | M |
| Eosinófilo, basófilo, plaquetas | 2–15 µm | frotis | Leucograma completo con Giemsa | frotis | — | M |
| Eritrocito de ave o anfibio | 10–20 µm | frotis | Eritrocito nucleado, para comparar | `bio/erythrocytes` | Núcleo | B |
| Pelo estaminal de *Tradescantia* | células de ~100 µm | preparado | Ciclosis y mitosis en vivo | tejido | Ciclosis | M |
| Grano de polen | 20–100 µm | preparado | Germinación del tubo polínico en sacarosa | tejido | Crecimiento apical | B |
| Célula internodal de *Chara* / *Nitella* | cm | preparado | La ciclosis más rápida (Kamiya) | tejido | Ciclosis | B |
| Pelo radical | — | preparado | Crecimiento apical | tejido | Crecimiento apical | B |
| HepG2 | 10–20 µm | celular | Línea hepática | Crawl, ciclo celular | — | B |

---

## Parte 2 — Experimentos

### 2.1 Poblaciones y comunidades

| # | Experimento | Referencia | Seres | Montaje | Qué se mide | Resultado esperado | Estado | Uso |
|---|---|---|---|---|---|---|---|---|
| 1 | Crecimiento logístico | Gause 1934 | *P. caudatum*, *P. aurelia* | cultivo | N(t), r, K | Curva sigmoide con meseta | ✅ | lección, validación |
| 2 | Exclusión competitiva | Gause 1934 | *P. aurelia* contra *P. caudatum* | cultivo | Abundancias | Gana *P. aurelia* | 🟡 hoy gana la que se adelanta (roadmap B) | validación |
| 3 | Coexistencia por uso del espacio | Gause 1934 | *P. caudatum* y *P. bursaria* | cultivo, placa | Abundancias | Coexisten (*P. bursaria* come en el fondo) | 🔴 *P. bursaria* y un nicho espacial | lección |
| 4 | Depredador sin refugio | Gause 1934 | *Didinium*, *Paramecium* | cultivo | N(t) de ambos | El depredador extingue a la presa y después se extingue | ✅ | lección |
| 5 | Con refugio (sedimento) | Gause 1934 | *Didinium*, *Paramecium* | placa | N(t) | La presa persiste en el refugio | 🟡 refugio que *Didinium* no alcanza | lección |
| 6 | Con inmigración | Gause 1934 | *Didinium*, *Paramecium* | cultivo | N(t) | Oscilaciones | 🟡 inmigración programada (roadmap B) | validación |
| 7 | Metilcelulosa | Luckinbill 1973 | *Didinium*, *Paramecium* | cultivo | Ciclos | Menos encuentros: ciclos más largos y persistentes | 🟡 viscosidad del medio como parámetro | lección |
| 8 | Oscilaciones largas | Veilleux 1979 | *Didinium*, *Paramecium* | cultivo | Período y amplitud | Varios ciclos acoplados | 🟡 | validación |
| 9 | Metapoblaciones | Holyoak y Lawler 1996 | *Didinium*, *Colpidium* | varios cultivos | Persistencia | Subdividir prolonga la persistencia | 🔴 cultivos conectados y *Colpidium* | investigación |
| 10 | Paradoja del enriquecimiento | Rosenzweig 1971; Bohannan y Lenski 1997 | *E. coli*, fago T4 | cultivo | Amplitud | Más nutriente da oscilaciones más grandes | 🟡 cepa *E. coli* con nombre | lección, validación |
| 11 | Respuesta funcional tipo II | Holling 1959; Fenchel 1980 | *Paramecium*, bacterias | cultivo, placa | Ingestión según la densidad de presa | Saturación hiperbólica | 🟢 (`bio_bench --respuesta-funcional`; roadmap B) | validación |
| 12 | Productividad y largo de la cadena trófica | Kaunzinger y Morin 1998 | bacterias, *Colpidium*, *Didinium* | cultivo | Niveles que persisten | Más nutriente sostiene cadenas más largas | 🟡 *Colpidium* | investigación |
| 13 | Arquitectura de la red trófica | Lawler y Morin 1993 | ciliados y bacterias | cultivo | Variabilidad poblacional | Cambia con la omnivoría y la cantidad de presas | 🟡 más ciliados | investigación |
| 14 | Calentamiento en microcosmos | Petchey et al. 1999 | comunidad de protistas | cultivo, placa | Riqueza por nivel trófico | Al calentar se pierden más depredadores y herbívoros | 🟢 con programas de temperatura | investigación |
| 15 | Alometría del crecimiento | Fenchel 1974 | protistas de varios tamaños | cultivo | µmax según la masa | Baja con la masa en ley de potencia | 🟡 µmax con fuente para cada especie (roadmap B) | validación |
| 16 | Competencia de levaduras | Gause 1932 | *S. cerevisiae*, *Schizosaccharomyces kephir* | cultivo | Biomasa y alcohol | El etanol que producen limita a ambas | 🔴 etanol como campo | lección |
| 17 | Piedra, papel o tijera | Kerr et al. 2002 | *E. coli*: productora de colicina, sensible y resistente | placa | Coexistencia | En espacio local coexisten; mezcladas, no | 🔴 colicinas | investigación |
| 18 | Cooperadores y tramposos | Gore et al. 2009 | *S. cerevisiae* SUC2⁺ y SUC2⁻ | cultivo | Frecuencias | Coexistencia (juego de la gallina) | 🔴 invertasa extracelular | investigación |
| 19 | Colonias inducidas por depredación | Boraas et al. 1998 | *Chlorella*, *Ochromonas* | cultivo | Tamaño de colonia | Aparecen colonias que el depredador no traga | 🔴 *Chlorella* y *Ochromonas* | lección |
| 20 | Defensa inducida | Kuhlmann y Heckmann 1985 | *Euplotes*, *Lembadion* | placa | Forma y supervivencia | *Euplotes* con "alas" y menos predado | 🔴 kairomonas | lección |
| 21 | Floración y eutrofización | escenario (roadmap J) | *Microcystis*, algas | placa, cultivo | Biomasa, O₂, toxina | Un pulso de P causa floración y anoxia nocturna | 🟡 *Microcystis* | lección |
| 22 | Ciclo día–noche del O₂ y el pH | práctica clásica | algas | placa | O₂ y pH | Suben de día y bajan de noche | 🟢 programas de luz | lección |

### 2.2 Recursos y crecimiento

| # | Experimento | Referencia | Seres | Montaje | Qué se mide | Resultado esperado | Estado | Uso |
|---|---|---|---|---|---|---|---|---|
| 23 | Curva de Monod | Monod 1949 | *Asterionella* | cultivo | µ según S | Hipérbola | ✅ | validación |
| 24 | Quimiostato | Novick y Szilard 1950; Monod 1950 | bacterias | cultivo | S* y N* según D | S* no depende del aporte; lavado si D > µmax | ✅ | lección, validación |
| 25 | Cuota celular | Droop 1968 | *Asterionella* | cultivo | µ según la cuota | µ = µ′(1 − Q₀/Q) | ✅ (P); falta el panel de cuotas (roadmap G) | validación |
| 26 | Competencia por P y Si | Tilman 1977 | *Asterionella*, *Cyclotella* | cultivo | Ganador según Si:P | Cambia con la proporción | ✅ | lección, validación |
| 27 | Diauxia | Monod 1942 | *E. coli* | cultivo | Densidad óptica | Dos fases exponenciales con una latencia | 🔴 dos azúcares y represión catabólica | lección |
| 28 | Fases de la curva de crecimiento | Buchanan 1918; Monod 1949 | bacterias | cultivo | N(t) | Latencia, exponencial, estacionaria y muerte | 🟢 (`fitGrowth`) | lección |
| 29 | Temperatura y crecimiento | Ratkowsky et al. 1982 | bacterias, protistas | cultivo | µ según T | √µ lineal debajo del óptimo; cae arriba | 🟡 hoy Q10 = 2 fijo, sin óptimo ni máximo | validación |
| 30 | Curva fotosíntesis–irradiancia | Jassby y Platt 1976 | algas | cultivo | Fotosíntesis según la luz | Saturación y fotoinhibición | 🟡 fotoinhibición | validación |
| 31 | Efecto Crabtree | De Deken 1966 | *S. cerevisiae* | cultivo | Etanol con O₂ | Fermenta aunque haya O₂ si sobra glucosa | 🔴 regulación del metabolismo | lección |
| 32 | Bioensayo de B₁₂ | Hutner et al. 1949 | *Euglena gracilis* | cultivo | Crecimiento según la B₁₂ | Crecimiento proporcional a la dosis | 🟢 (B₁₂ ya limita a *Euglena*) | lección, validación |
| 33 | Toxicidad algal (CE50) | OECD 201 | *Raphidocelis* | cultivo | Inhibición del crecimiento | Curva dosis–respuesta | 🟡 reactivos sobre eucariotas (roadmap E) | lección |
| 34 | Frecuencia de heterocistos | práctica clásica | cianobacterias | cultivo, placa | Heterocistos cada 100 células | Más heterocistos sin N combinado | 🟢 | lección |
| 35 | Limitación por Si de diatomeas | Tilman y Kilham 1976 | diatomeas | cultivo | µ según Si | Monod con su K | 🟢 (falta verificar las constantes, roadmap B) | validación |

### 2.3 Comportamiento y taxis

| # | Experimento | Referencia | Seres | Montaje | Qué se mide | Resultado esperado | Estado | Uso |
|---|---|---|---|---|---|---|---|---|
| 36 | Acumulación por CO₂ | Jennings 1906 | *Paramecium* | placa | Fracción cerca de la fuente | Se juntan donde hay CO₂ | ✅ | lección, validación |
| 37 | Reacción de evitación | Jennings 1906 | ciliados | placa | Retroceso y giro | Retroceden ante choques y estímulos | 🟢 (mecanismo hecho; falta la lección) | lección |
| 38 | Galvanotaxis | Verworn 1889 | *Paramecium* | placa | Dirección del nado | Nadan hacia el cátodo | 🔴 campo eléctrico | lección |
| 39 | Geotaxis negativa | Jensen 1893 | *Paramecium* | — | — | Se acumulan arriba | ⛔ vista cenital, sin eje vertical | — |
| 40 | Capilar quimiotáctico | Pfeffer 1884; Adler 1973 | bacterias | placa | Bacterias junto a la fuente | Acumulación según el atrayente | 🟢 gotero como fuente puntual | lección |
| 41 | Corrida y voltereta | Berg y Brown 1972 | *E. coli* | placa | Largo de las corridas, MSD | Corridas más largas a favor del gradiente | 🟡 trayectorias (roadmap G) | lección, validación |
| 42 | Bandas y anillos quimiotácticos | Adler 1966; Budrene y Berg 1991 | *E. coli* | placa | Patrón | Bandas que avanzan; anillos y puntos | 🔴 atrayente secretado | investigación |
| 43 | Espectro de acción (aerotaxis) | Engelmann 1882 | *Cladophora* o *Spirogyra*, bacterias | placa | Bacterias a lo largo del filamento | Se acumulan bajo luz roja y azul | 🔴 aerotaxis y luz con espectro | lección |
| 44 | Fototaxis y fotofobia | Diehn 1973; Foster y Smyth 1980 | *Euglena*, *Chlamydomonas* | placa | Distribución según la luz | Van a la luz moderada; huyen de la intensa | 🟢 (fototaxis hecha); 🟡 fotofobia | lección |
| 45 | Laberinto de *Physarum* | Nakagaki et al. 2000 | *Physarum* | placa | Red final | Queda el camino más corto entre dos copos | 🟡 paredes internas | lección |
| 46 | Red ferroviaria de Tokio | Tero et al. 2010 | *Physarum* | placa | Red entre copos | Red eficiente y tolerante a fallas | 🟢 copos y evitación de la luz ya existen | lección, investigación |
| 47 | Agregación por AMPc | Bonner 1947; Konijn et al. 1967 | *Dictyostelium* | placa | Patrón de agregación | Ondas espirales y corrientes hacia los centros | 🔴 *Dictyostelium* y relevo de AMPc | lección |
| 48 | Neutrófilo que persigue una bacteria | película de David Rogers (década de 1950) | neutrófilo, bacterias | celular | Trayectoria | Sigue el fMLP y fagocita | 🟢 (mecanismo hecho; falta la lección) | lección |
| 49 | Índice quimiotáctico | Zigmond 1977; Nelson et al. 1975 | neutrófilos | celular | Fracción que avanza hacia la fuente | Sesgo según el gradiente | 🟡 fMLP en el gotero | validación |
| 50 | Habituación de *Stentor* | Jennings 1902; Wood 1970; Dexter et al. 2019 | *Stentor* | placa | Respuesta a estímulos repetidos | Jerarquía de respuestas; habituación | 🔴 respuesta con memoria | investigación |
| 51 | Descarga de tricocistos | práctica clásica | *Paramecium* | placa | Tricocistos expulsados | Descarga con ácido o tinta | 🟡 descarga por un reactivo | lección |
| 52 | Contracción del espasmonema | Amos 1971 | *Vorticella* | placa | Contracción | Depende del Ca²⁺, no del ATP | ✅ mecanismo; 🟡 el Ca²⁺ | lección |

### 2.4 Fisiología celular

| # | Experimento | Referencia | Seres | Montaje | Qué se mide | Resultado esperado | Estado | Uso |
|---|---|---|---|---|---|---|---|---|
| 53 | Vacuola contráctil y salinidad | Kitching 1934, 1938 | *Paramecium*, *Amoeba* | placa | Frecuencia de vaciado | Baja al subir la osmolaridad del medio | 🟡 ósmosis en protistas | lección, validación |
| 54 | Vacuolas con levadura y rojo Congo | práctica clásica | *Paramecium*, levadura | placa | Color de las vacuolas | Viran de rojo a azul al acidificarse | 🟢 (rojo Congo y pH de los compartimentos ya existen) | lección |
| 55 | Tasa de formación de vacuolas | práctica clásica | *Paramecium* | placa | Vacuolas por minuto | Sube con la densidad de partículas hasta saturar | 🟢 | lección |
| 56 | Ciclosis y temperatura | práctica clásica | *Elodea* | preparado | Velocidad de los cloroplastos | Más rápida con la temperatura | 🔴 ciclosis | lección |
| 57 | Plasmólisis | práctica clásica | epidermis de cebolla | preparado | Fracción plasmolizada | Plasmólisis incipiente en el isotónico | ✅ | lección |
| 58 | Fragilidad osmótica | Parpart 1947 | eritrocitos | preparado | Hemólisis según NaCl | Curva sigmoide | ✅ | lección |
| 59 | Viabilidad con azul de tripano | práctica clásica | células animales | celular | Fracción teñida | Solo las muertas se tiñen | 🟡 regla de exclusión por membrana | lección |
| 60 | Captación de rojo neutro | Borenfreund y Puerner 1985 | células animales, protistas | celular, placa | Tinción de los lisosomas | Proporcional a las células vivas | 🟢 (rojo neutro ya existe) | lección |
| 61 | Respiración o fermentación | práctica clásica | levadura | cultivo | CO₂ y O₂ | Sin O₂ fermenta: más glucosa por ATP | 🟢 | lección |
| 62 | Fotosíntesis en luz y oscuridad | práctica clásica | algas | placa | O₂ y pH | Suben con luz y bajan en oscuridad | 🟢 | lección |
| 63 | Blanqueo de *Euglena* | Provasoli et al. 1948 | *Euglena* | cultivo | Células sin cloroplastos | Pérdida heredable con estreptomicina o calor | 🟡 reactivo con blanco en el cloroplasto | lección |
| 64 | Simbiosis *P. bursaria*–*Chlorella* | Karakashian 1963 | *P. bursaria*, *Chlorella* | cultivo, placa | Crecimiento con y sin luz | Las verdes crecen sin presa con luz | 🔴 endosimbionte (roadmap F) | lección |
| 65 | Amebas dependientes de sus bacterias | Jeon 1972 | *Amoeba proteus* | cultivo | Supervivencia | La infección pasa a ser imprescindible | 🔴 | investigación |

### 2.5 Microbiología y genética

| # | Experimento | Referencia | Seres | Montaje | Qué se mide | Resultado esperado | Estado | Uso |
|---|---|---|---|---|---|---|---|---|
| 66 | CIM | Regoes et al. 2004 (modelo) | bacterias | cultivo | CIM, CI50 | La CIM publicada | ✅ | validación |
| 67 | Antibiograma por difusión | Bauer et al. 1966 | bacterias | placa | Halo de inhibición | Diámetro según el log de la dosis | 🟢 el reactivo ya difunde desde el gotero | lección |
| 68 | Antibiótico de un hongo | Fleming 1929 | *Penicillium*, *S. aureus* | placa | Halo junto a las hifas | Inhibición alrededor del hongo | 🔴 secreción por las hifas | lección |
| 69 | Persistencia | Bigger 1944; Balaban et al. 2004 | *E. coli* | cultivo | Supervivencia en el tiempo | Muerte bifásica | 🔴 estado latente fenotípico | investigación |
| 70 | Test de fluctuación | Luria y Delbrück 1943 | bacterias, fagos | lote de cultivos | Resistentes por réplica | Varianza mucho mayor que Poisson (mutación previa) | 🟢 mutantes resistentes a fagos, `bio_lote` | lección, validación |
| 71 | Curva de un paso del fago | Ellis y Delbrück 1939 | bacterias, fagos | cultivo | Fagos libres en el tiempo | Latencia y escalón (tamaño de la explosión) | 🟡 latencia explícita | lección |
| 72 | Fago y bacteria en quimiostato | Chao et al. 1977 | *E. coli*, fago | cultivo | Densidades | Coexistencia con resistentes | 🟢 | validación |
| 73 | Quorum sensing | Nealson et al. 1970 | *C. violaceum* (*V. fischeri*) | placa, cultivo | Violaceína (luz) según la densidad | Encendido solo por encima del umbral | 🟢 con *C. violaceum*; 🟡 en el cultivo (roadmap E) | lección |
| 74 | Tipos de apareamiento | Sonneborn 1937 | *P. aurelia* | placa | Conjugación entre tipos | Solo se aparean tipos distintos | 🟢 (mecanismo hecho; falta la lección) | lección |
| 75 | Rasgo *killer* | Sonneborn 1938 | *P. tetraurelia* | placa | Supervivencia de sensibles | Los sensibles mueren junto a los *killer* | 🔴 | lección |
| 76 | Evolución a largo plazo | Lenski et al. 1991 | *E. coli* | cultivo seriado | Fitness relativa | Sube, cada vez más despacio | 🔴 genoma en las bacterias | investigación |
| 77 | Radiación adaptativa | Rainey y Travisano 1998 | *P. fluorescens* | placa | Morfotipos | Aparecen nichos (*wrinkly spreader* en la superficie) | 🔴 | investigación |
| 78 | Multicelularidad de copo de nieve | Ratcliff et al. 2012 | *S. cerevisiae* | cultivo | Tamaño de los grupos | Agrupamientos seleccionados por sedimentación | 🔴 | investigación |
| 79 | Resistencia con costo | roadmap F | bacterias | cultivo | Frecuencia de resistentes | Con antibiótico ganan; sin él pierden | 🟡 costo de la resistencia | lección |
| 80 | Coloración de Gram | Gram 1884 | bacterias | placa | Color | Grampositivas violeta, gramnegativas rosa | ✅ | lección |
| 81 | Columna de Winogradsky | Winogradsky (década de 1880) | comunidad | — | Bandas | Zonas de O₂ y S | ⛔ eje vertical y ciclo del S | — |
| 82 | Transformación de Griffith | Griffith 1928 | *S. pneumoniae* | — | — | — | ⛔ requiere ratones | — |
| 83 | Hershey–Chase | Hershey y Chase 1952 | fago T2 | — | — | — | ⛔ molecular | — |
| 84 | Réplica en placa | Lederberg y Lederberg 1952 | bacterias | — | — | — | ⛔ colonias en agar | — |

### 2.6 Ciclo celular y cultivo celular

| # | Experimento | Referencia | Seres | Montaje | Qué se mide | Resultado esperado | Estado | Uso |
|---|---|---|---|---|---|---|---|---|
| 85 | Tiempo de duplicación | Gey et al. 1952 | HeLa, fibroblasto | celular | N(t) | El tiempo publicado | 🟢 | validación |
| 86 | Inhibición por contacto de la proliferación | Todaro y Green 1963 | fibroblasto (3T3) | celular | Densidad de saturación | Meseta en la confluencia; HeLa no frena | ✅ mecanismo; 🟢 lección | lección, validación |
| 87 | Inhibición por contacto de la locomoción | Abercrombie y Heaysman 1954 | fibroblasto | celular | Velocidad y giros al chocar | Se frenan y se alejan | ✅ mecanismo; 🟢 lección | lección |
| 88 | Ensayo de herida | Liang et al. 2007 | fibroblasto, queratinocito | celular | Ancho de la herida | Se cierra en horas | 🟢 (Borrar o Región para hacer la raya) | lección, validación |
| 89 | Punto de restricción | Pardee 1974 | fibroblasto | celular | Fracción en G0 | Sin suero se detienen en G1 | ✅ mecanismo; 🟢 lección | lección |
| 90 | Efecto Warburg | Warburg 1956 | HeLa | celular | Lactato y pH | Acidifica el medio aun con O₂ | ✅ mecanismo; 🟢 lección | lección |
| 91 | Apoptosis por falta de suero | Kerr et al. 1972 | células animales | celular | Cuerpos apoptóticos | Sin lisis ni inflamación | ✅ | lección |
| 92 | Fagocitosis por neutrófilos | práctica clásica | neutrófilos, bacterias | celular | Bacterias por célula | Fagocitosis saturable | 🟢 | lección |
| 93 | Ensayo clonogénico | Puck y Marcus 1956 | HeLa | celular | Fracción que forma colonia | Supervivencia según la dosis | 🟡 colonias desde células aisladas | investigación |
| 94 | Límite de Hayflick | Hayflick y Moorhead 1961 | fibroblasto | pasajes | Duplicaciones acumuladas | ~40–60 y senescencia; HeLa no la tiene | 🔴 contador replicativo y pasajes | lección |
| 95 | Índice mitótico | práctica clásica | raíz de cebolla | preparado | Fracción en cada fase | Las fases según su duración | ✅ preparado; 🟢 lección de conteo | lección |
| 96 | Test de *Allium* | Levan 1938; Fiskesjö 1985 | raíz de cebolla | preparado | Índice mitótico, anomalías | Baja con tóxicos; c-mitosis con colchicina | 🔴 reactivos sobre la mitosis | lección |
| 97 | Control de tamaño | Nurse 1975 | *S. pombe* | cultivo | Tamaño al dividirse | Se dividen a un tamaño fijo | 🟡 *S. pombe* | lección |
| 98 | *Adder* bacteriano | Taheri-Araghi et al. 2015 | *E. coli* | cultivo | Tamaño agregado por ciclo | Constante, sin importar el tamaño inicial | 🔴 tamaño individual de las bacterias | investigación |
| 99 | Estomas y luz | roadmap F | epidermis de hoja | preparado | Apertura | Abren con luz o KCl; cierran con ABA | 🔴 | lección |
| 100 | Germinación de polen | práctica clásica | polen | preparado | Largo del tubo | Según la sacarosa y el boro | 🔴 crecimiento apical | lección |

---

## Fuera de alcance (por ahora)

- **Animales pluricelulares completos** (rotíferos, nematodos, *Hydra*, *Daphnia*): descartados
  en el roadmap hasta que su simulación sea realista. Excluye los experimentos que los necesitan
  (*Daphnia* y la defensa de *Scenedesmus*, rotíferos en quimiostato de Fussmann et al. 2000).
- **Eje vertical:** geotaxis, migración vertical, sedimentación y columnas estratificadas. La
  vista es cenital y todo ocurre en un plano.
- **Biología molecular sin imagen:** Hershey–Chase, Meselson–Stahl, telómeros y ribozimas de
  *Tetrahymena* (Blackburn, Cech). No son experimentos de microscopio.
- **Colonias en agar** (réplica en placa, conteo de UFC): el mundo es una película de agua, no un
  medio sólido.

---

## Prioridades

**Seres** (aportan mucho o cuestan poco):

1. *Tetrahymena*: destraba el µmax de *Amoeba* y es presa de casi todos los depredadores.
2. *Colpidium*: el bacterívoro de los microcosmos clásicos (experimentos 9, 12 y 13).
3. *E. coli* con nombre y datos: la base de los experimentos 10, 27, 41, 69, 70, 72 y 76.
4. *Chlorella*: presa, simbionte (64) y colonias inducidas (19).
5. *Paramecium bursaria*: endosimbiosis (roadmap F) y coexistencia de Gause (3).
6. *Saccharomyces cerevisiae*: levadura modelo para los experimentos 16, 18, 31 y 61.
7. *Gonium*, *Pandorina* y *Eudorina*: solo datos; completan la serie volvocal.
8. *Phacus* y *Pinnularia*: solo datos; clásicos del práctico.
9. *Peranema*: un euglenoide depredador, contraste con *Euglena*.
10. *Microcystis* y *Dolichospermum*: floraciones (21) y heterocistos con nombre (34).

**Experimentos** (factibles hoy o casi):

1. Antibiograma por difusión (67).
2. Test de fluctuación de Luria–Delbrück (70).
3. Vacuolas con levadura y rojo Congo (54).
4. Ensayo de herida (88).
5. Red de Tokio con *Physarum* (46).
6. Bioensayo de B₁₂ con *Euglena* (32).
7. Fototaxis de *Euglena* y *Chlamydomonas* (44).
8. Inhibición por contacto y efecto Warburg (86, 90): ya tienen el mecanismo; falta la lección.
9. Fago y bacteria en quimiostato (72).
10. Respuesta funcional tipo II (11), pendiente en el roadmap B.
