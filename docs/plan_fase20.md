# Plan — Fase 20: contenido como datos, placas preparadas y cultivo celular humano

Primera fase de la visión nueva (AGENTS.md, «Visión»): BioGame como herramienta de investigación y
de estudio. Plan del 15/10/2026, aprobado en sus decisiones; se implementa en el orden de abajo,
con commits chicos y tests básicos en cada paso. El lanzamiento espera a que esta fase esté hecha.

Avance: ✅ hecho · 🟡 en parte (se dice qué falta) · sin marca: pendiente.

## Decisiones tomadas

1. **Migración celular en tiempo biológico.** Las células que se arrastran sobre el sustrato
   (adherentes y leucocitos) migran con `bioTimeScale`, como la división: así se respeta la
   proporción real entre lo que migra una célula y su ciclo celular. El nado de los protistas
   sigue en tiempo real (es hidrodinámica). Excepción explícita en AGENTS.md.
2. **Tipos celulares básicos primero:** fibroblasto dérmico humano, HeLa y neutrófilo.
3. **Recipiente y campo simulado separados.** El recipiente (placa de 35 mm, pocillo de 6, 24 o
   96) fija el volumen y la profundidad del medio, es decir la química; el **campo simulado**
   (0,5–4 mm de lado, por rendimiento) es lo que se ve y se simula célula por célula. La
   interfaz nunca promete la placa entera.
4. **Grado universitario / de investigación.** Mecanismos reales y cuantitativos con datos
   publicados (ciclo celular por fases con punto de restricción, factores de crecimiento,
   metabolismo de glucosa y glutamina, lactato y pH, consumo de O₂, migración como paseo aleatorio
   persistente, inhibición por contacto, apoptosis). Cada número con su fuente; lo que no tenga
   medición directa queda marcado como estimación. Validación contra datos publicados.
5. **Expandible y compartible sin recompilar.** Seres, medios, sedimentos, placas preparadas,
   preparados y configuraciones son **datos** en paquetes que se cargan al arrancar y se pueden
   compartir como un archivo. El código aporta los **mecanismos** (locomociones, modos de
   alimentación, ciclos de vida, estilos de dibujo); los datos los combinan y parametrizan. Un ser
   nuevo hecho con mecanismos existentes no necesita compilar; un mecanismo nuevo sí.

## Estado de partida (lo que hay que cambiar)

- Especies: `enum class SpeciesId` (24) y tabla C++ `kSpecies`; 168 referencias a especies
  concretas en 11 archivos (render, enciclopedia, ficha, plan corporal, hongos, sandbox).
- Dietas y huidas: máscaras de 32 bits (`speciesBit`); con las especies nuevas se agotan.
- Minerales: `enum class MineralKind`; biomas: tabla C++ (`game/biome`).
- Fisiología: ya es datos (`assets/species/fisiologia.toml`, toml++), con fuentes.
- Placa: `PlateSpec` + `makeDrop(bioma, seed)` + `DropContent`; `.plate` versionado; escenas =
  `.plate` + intervenciones (`Spawn` guarda `BeingKind` y `SpeciesId` numéricos).
- Medio: `MediumParams` (P, N, Si, Fe, B₁₂, alcalinidad, temperatura); el CO₂ del aire es fijo
  (`units::kCO2Saturation`).

## 20.0 — Contenido como datos y paquetes (base de todo lo demás)

**Objetivo:** que todo el contenido viva en paquetes de datos, los de fábrica incluidos, sin
sistemas paralelos (los seres actuales se migran, no conviven con un sistema nuevo).

1. ✅ **Registro de contenido** (`game/content`, sin raylib): tipos de ser, minerales y sedimentos,
   medios, biomas y placas preparadas, cada uno con **id estable de texto**
   (`"protista.paramecium_caudatum"`, `"celula.fibroblasto_dermico_humano"`) y un índice de
   ejecución. `SpeciesId` deja de ser un enum: pasa a ser el índice en el registro.
   *Hecho para los seres* (`bio/species`: registro con id de texto, `speciesCount()`, tope
   `kMaxSpecies`, carga del paquete de fábrica en el primer uso). Los nombres del enum quedan solo
   como alias de los seres de fábrica para el código por especie que falta migrar (paso 3).
   *Hecho también para minerales* (`game/mineral`: `MineralKind` es el índice, `mineralCount()`,
   tope `kMaxMinerals`; el sedimento es un conteo por mineral) *y biomas* (`game/biome`:
   `biomeCount()`, `findBiome`). Los registros se leen sin candado una vez cargados (atómico).
   Medios y placas preparadas se registran igual en la fase 20a (el lector del medio,
   `game/medium_toml`, ya es común).
2. ✅ **Paquetes** (`<assets>/content/base/` de fábrica y `<datos>/biogame/paquetes/` del usuario):
   archivos TOML con `[paquete]` (id, versión, autor, licencia, versión mínima del programa) y
   tablas de definiciones. Un paquete se comparte como **un solo archivo `.biopack`** (TOML con
   todas sus definiciones; sin bibliotecas nuevas). Se valida al cargar (esquema, unidades,
   rangos, fuentes obligatorias para la fisiología, referencias a mecanismos existentes) y los
   errores se muestran en la interfaz sin cerrar el programa.
   *Hecho:* `assets/content/base/` (`paquete.toml`, `seres.toml`, `planes.toml`, `fisiologia.toml`,
   `minerales.toml`, `biomas.toml`), lector TOML común (`core/toml_reader`), carga desde archivo o texto,
   validación con avisos (claves desconocidas, valores fuera de lista, referencias sin resolver,
   ids repetidos) sin cerrar el programa; tests en la suite `paquetes`. Paquetes del usuario en
   `<datos>/biogame/paquetes/` (carpeta o `.biopack`), cargados al arrancar; el `.biopack` es un
   TOML con `[paquete]` y `[[ser]]`, `[[plan]]`, `[[mineral]]`, `[[bioma]]`,
   `[fisiologia.<id>]` y `[[escena]]`; los avisos se ven en el panel *Contenido* (paso 7).
3. ✅ **Ser como datos:** morfología (semiejes, forma, rigidez), mecanismos por nombre (locomoción,
   modo de alimentación, reproducción, quistes, sésil…), fisiología con fuentes (lo de
   `fisiologia.toml` se integra), química (reserva, pared), plan corporal (estructuras y tamaños),
   textos de la ficha y la enciclopedia, y **apariencia** (colores, organelos visibles, estilo de
   dibujo por nombre). Lo que hoy es código por especie en el render pasa a estilos con nombre
   (`"cinta_helicoidal"`, `"frustula"`, `"colonia_volvox"`…) que los datos eligen.
   *Hecho:* morfología, mecanismos, química (reserva y pared), genoma, etiquetas, descripción
   (ficha y enciclopedia), plan corporal (`planes.toml`, un paquete puede traer planes nuevos),
   apariencia (`[ser.apariencia]`: estilo de dibujo con nombre y color de los plastos; el render ya
   no distingue especies, verificado con capturas) y conjugación (dato; ahora solo dentro de la
   especie: antes cualquier ciliado podía ser pareja) y fisiología con fuentes
   (`content/base/fisiologia.toml`, por id del ser; un paquete suma la suya).
4. ✅ **Dietas por etiquetas:** cada ser tiene etiquetas (`bacteria`, `ciliado`, `alga_verde`,
   `celula_humana`…) y las dietas y huidas son conjuntos de etiquetas, sin límite de cantidad.
5. ✅ **Migración de lo existente:** las 24 especies, los minerales y los biomas pasan al paquete de
   fábrica. **Hecho cuando:** `bio_bench` da el mismo checksum (IEEE estricto) antes y después en
   tres escenas, y los tests rápidos y largos pasan.
   *Hecho para las 24 especies:* checksums estrictos iguales en tres escenas con CPU y GPU (el
   TOML se generó volcando la tabla C++ con precisión de ida y vuelta).
   *Hecho para minerales y biomas:* `minerales.toml` (tamaño, densidad, opacidad, floculación,
   estilo de dibujo con nombre, textos de la ficha) y `biomas.toml` (medio con claves en español,
   sedimento por id de mineral, pesos por id de ser: un bioma puede nombrar seres y minerales de
   otros paquetes). Trichonympha deja de ser un caso en el código: etiqueta `simbionte_obligado`
   (solo aparece donde el bioma la nombra). Las placas guardan el sedimento por id de mineral y
   leen las claves viejas. Verificado: las placas de los 6 biomas × 5 seeds salen idénticas y los
   checksums estrictos de las tres escenas, con CPU y GPU, no cambian (bioma 1 seed 7 fauna 2,3:
   56a8a4b097813cc0 / b6e3ea17b6b61a8c; bioma 0 seed 3 fauna 1,5: f61d88a58b1b0d67 /
   e599a11607eff58e; bioma 2 seed 11 fauna 1: ba6edfcfdabc3afe / 1891c4109b5501f1). Desde 20a la
   solubilidad del CO₂ depende de la temperatura (cambio físico, no optimización): el bioma 2
   (musgo, 12 °C) pasa a 3b4e92277afb47e6 / 1f0ac66569028f83; los otros dos no cambian en 200 pasos. Quedan en
   código, por ahora: las mezclas de barro y de polvo (proporciones de los minerales de fábrica).
6. ✅ **Compatibilidad:** las escenas y `.plate` guardan ids de texto; los archivos viejos (ids
   numéricos) se leen con una tabla de traducción. La exportación (`runJson`) anota los paquetes
   usados con su versión y huella (reproducibilidad).
   *Hecho:* `.plate` versión 2 (`biome=musgo`, `w.<id del ser>`, `sediment.<id del mineral>`,
   `pack=id versión huella`); las placas de versión 1 (bioma por índice, pesos por nombre,
   sedimento en inglés) se leen igual. Los eventos de las escenas que nombran un ser (`Spawn`,
   `PlayerSpecies`) llevan su id y al leer manda el id. Lo que la placa o la escena nombra y no está
   cargado (paquete, bioma, ser, mineral) se avisa al cargarla (interfaz y `bio_bench`) y se carga
   lo demás. `game/content`: paquetes cargados con su manifiesto y huella (FNV-1a de sus archivos);
   `runJson` los anota en la procedencia (y el id del bioma).
7. ✅ **Compartir:** exportar e importar desde la interfaz (panel nuevo *Contenido*): un ser, un
   medio, una placa preparada, una escena o un paquete entero, a un `.biopack` o desde uno. Al
   cargar una escena que usa un paquete que falta, se avisa cuál.
   *Hecho:* panel *Contenido* (paquetes cargados con versión, licencia, cantidades y huella;
   avisos de todos los registros; importar un `.biopack` por ruta —se copia a la carpeta del
   usuario y se carga en el momento, el catálogo rehace sus íconos—; exportar seres, minerales,
   biomas y la placa en pantalla con sus intervenciones). Cada definición se exporta tal como se
   leyó, con su plan corporal si no es de fábrica y su fisiología; las escenas de un paquete
   importado aparecen entre las placas guardadas. `bio_bench --paquete` carga paquetes sin
   ventana. Probado en la interfaz (importar *Colpoda cucullus* desde un `.biopack`, verlo en el
   catálogo, sembrarlo, ver su ficha, exportarlo con la placa) y en limpio (otra carpeta de
   datos: el paquete exportado se carga y su escena se reproduce). Los medios y las placas
   preparadas se exportarán igual cuando existan (20a).
8. ✅ **Seguridad:** los paquetes son solo datos (nada ejecutable); tamaños y cantidades acotados.
   *Hecho:* solo TOML; un paquete de más de 8 MB se rechaza; id del paquete de 1 a 64 caracteres
   `[a-z0-9_]` (no arma rutas); los nombres de las escenas instaladas se sanean; topes de
   seres (`kMaxSpecies`), minerales (`kMaxMinerals`) y biomas; `programa_minimo` rechaza paquetes
   para una versión más nueva; ids repetidos se saltean; claves desconocidas se avisan.

## 20a — Placas preparadas

1. ✅ **Tipo de placa** (`PlateKind`: gota, cultivo celular; preparado vegetal en una fase
   siguiente) en `PlateSpec` y `.plate`. Las placas preparadas son datos (20.0): tipo, medio,
   recipiente, campo, siembra inicial.
   *Hecho:* `game/prepared` (registro de medios, recipientes y placas preparadas, como los demás);
   `assets/content/base/placas.toml` («Cultivo celular (DMEM, incubadora)»); `PlateSpec::prepared`
   y `prepared=`, `depth=` y `seeding=` en el `.plate`; los `.biopack` llevan `[[medio]]`,
   `[[recipiente]]` y `[[placa]]`.
2. ✅ **Medios de cultivo como datos** con su composición y fuente: DMEM alta glucosa (25 mM de
   glucosa, 4 mM de glutamina, 44 mM de bicarbonato, rojo fenol), con 10 % de suero fetal bovino
   como fuente de factores de crecimiento. Incubadora: 37 °C, ~~5 %~~ **10 %** de CO₂, 95 % de humedad.
   *Hecho:* `medios.toml` (DMEM con su composición y fuente, el mismo lector que el medio de los
   biomas). Corrección: el DMEM de 3,7 g/L de bicarbonato está formulado para 10 % de CO₂ (pH 7,4);
   con 5 % queda en ~7,7 (Henderson–Hasselbalch con pK′ 6,1), así que la incubadora de la placa
   preparada usa 10 %. El CO₂ se puede cambiar en vivo.
3. 🟡 **Química nueva:** CO₂ atmosférico variable (`MediumParams::co2Atmosphere`; la incubadora fija
   el pH 7,4 con el bicarbonato), glucosa (el campo de materia orgánica actual, en mM), glutamina,
   lactato y factores de crecimiento del suero como campos con difusión. El rojo fenol tiñe el
   medio según el pH (render).
   *Hecho:* CO₂ del aire variable (intervención `Atmosphere`), solubilidad del CO₂ con la
   temperatura (van 't Hoff) y la sal (Weiss), pK′ del carbonato como dato del medio
   (`carbonatePK`), O₂ desplazado por el CO₂ de la incubadora, glucosa inicial en el campo de
   materia orgánica (`MediumParams::organic`), rojo fenol en el render (Beer–Lambert con la
   profundidad del medio). La glutamina y el suero son datos del medio; sus campos con difusión, y
   el del lactato, se suman en 20b, cuando haya células que los consuman o produzcan.
4. ✅ **Recipiente y campo:** el recipiente da el volumen y la profundidad del medio (`gapHeight` y
   química bien mezclada en la vertical, como en la validación de Tilman); el campo da la grilla.
   *Hecho:* `recipientes.toml` (áreas y volúmenes de Corning); `WorldConfig::depth` (la gota sigue
   en 200 µm); el intercambio de gases por la superficie sale de la difusión por la capa quieta
   (D/profundidad²: ~50 min en 2,5 mm); el campo es un círculo del fondo con su anillo de vidrio (el
   borde del campo simulado; antes era periódico: se cambió para que el vidrio se vea siempre).
5. ✅ **Inoculación** (intervención `Inoculate`, reproducible): un ser del catálogo con cantidad o
   densidad (células/cm²) en toda la placa, un punto o una región dibujada. **Mezclas:** cualquier
   ser en cualquier placa (contaminación de un cultivo con bacterias o levaduras, por ejemplo).
   *Hecho:* `World::inoculate` (sin encimar células; las bacterias de a una) y la intervención
   `Inoculate`, en toda la placa o en un círculo alrededor del centro de la vista. La región
   dibujada queda pendiente (la intervención no lleva el polígono).
6. ✅ **Panel Placa:** selector de tipo y secciones según el tipo; el Catálogo ofrece «Inocular…».
7. ✅ **Tests básicos:** `.plate` va y vuelve con tipo, medio, recipiente y campo; la inoculación a
   una densidad siembra la cantidad esperada y se repite bit a bit; el DMEM en la incubadora queda
   en pH 7,4 ± 0,1 y al aire libre se alcaliniza (pérdida de CO₂, como en la realidad).
   *Hecho:* DMEM 7,41 con 10 % de CO₂ y 7,71 con 5 %; al aire sube a 8,7; solubilidad del CO₂ con
   la temperatura y la sal (Sander, Weiss); placa preparada de ida y vuelta; inoculación en toda la
   placa o en un punto, con la misma seed igual; y el criterio de la fase (abajo). Los 60 tests
   (rápidos, largos y validación) pasan con la química nueva. Probado en la interfaz: tipo de placa,
   cultivo creado, 50 levaduras inoculadas, medio teñido por el rojo fenol.

## 20b — Cultivo celular humano

Mecanismos nuevos (código), parametrizados por datos:

*Hecho (pasos 1–7):* `bio/animal_cell` (componentes `AnimalCell` y `Adherent`) y
`game/world_animal.cpp`; `LocomotionType::Crawl` (el cuerpo se traslada con las adhesiones y se
alinea con la dirección; la polaridad difunde con D_r = 1/P en tiempo biológico); ciclo G1/S/G2/M
(con datos; sin dato, la célula típica de Alberts) con punto de restricción (suero, contacto,
energía) y G0; mitosis redondeada que reutiliza el surco y la división existentes; contacto medido
cada 15 pasos en paralelo; captación de glucosa y glutamina (Michaelis–Menten) con lactato que baja
el bicarbonato y el pH; apoptosis (sin energía 4 h, sin suero si la especie lo pide, por edad) que
se encoge y se parte en cuerpos apoptóticos sin derramar nada; neutrófilo con quimiotaxis a fMLP
(liberado por las bacterias, Marasco et al. 1984; detección relativa de ~1 %, Zigmond 1977) y
fagocitosis de bacterias. Datos de fibroblasto dérmico, HeLa y neutrófilo en `seres.toml` y
`fisiologia.toml` (lo no medido en el tipo exacto, como estimación). Simplificaciones documentadas:
la masa celular sale de los aminoácidos y el suero del medio (la glucosa da la energía y el ritmo);
la vía de señalización no se modela, solo sus efectos medidos; el fMLP de una fuente en el fondo
llena una capa de ~200 µm, no toda la profundidad del medio. Tests en `tests/test_animal_cells.cpp`.
Categoría «Células humanas» en el Catálogo.

1. ✅ **Locomoción por arrastre** (`Crawling`, tiempo biológico): lamelipodio en el frente y adhesión
   al sustrato; **paseo aleatorio persistente** con rapidez y tiempo de persistencia medidos.
   Redondeo durante la mitosis (la célula se despega y se redondea, visible).
2. ✅ **Ciclo celular por fases** (G1, S, G2, M) con duraciones de los datos; **punto de restricción**
   en G1 que exige factores de crecimiento; G0 (quiescencia) sin ellos.
3. ✅ **Inhibición por contacto:** de la locomoción (Abercrombie: al tocar a otra célula retrae ese
   frente y se repolariza) y de la proliferación (con la periferia ocupada queda en G0/G1). HeLa no
   tiene inhibición de la proliferación.
4. ✅ **Metabolismo celular:** captación de glucosa y glutamina (Michaelis–Menten, con Km y Vmax
   publicados), respiración y glucólisis aeróbica (Warburg en HeLa) con secreción de lactato que
   baja el pH; consumo de O₂ por célula medido. Masa y volumen celular crecen en el ciclo.
5. ✅ **Apoptosis:** sin factores de crecimiento, sin glucosa o por edad (neutrófilos): la célula se
   encoge, forma vesículas y se fragmenta en cuerpos apoptóticos sin lisis (sin inflamación). La
   lisis actual queda como necrosis.
6. ✅ **Neutrófilo:** quimiotaxis hacia péptidos formilados (fMLP) que liberan las bacterias (campo
   nuevo, con difusión real) y fagocitosis de bacterias con la copa existente; vida corta.
7. ✅ **Datos (paquete de fábrica «células humanas»):** fibroblasto dérmico humano, HeLa, neutrófilo,
   cada valor con fuente (duplicación, duración de fases, tamaño adherido y redondeado, rapidez y
   persistencia de migración, Km y Vmax de glucosa y glutamina, consumo de O₂, densidad de
   confluencia).
8. ✅ **Render, ficha y enciclopedia:** células aplanadas con núcleo y nucléolos, redondeadas en
   mitosis, cuerpos apoptóticos; categoría «Células humanas» en el catálogo; textos de qué es cada
   una y por qué hace lo que hace. El contraste de fase (lente) queda para la fase de lentes.
   *Hecho:* estilos `AdherentCell` (lamelipodio, halo mitótico, cromosomas en M, núcleo picnótico y
   vesículas en la apoptosis) y `Neutrophil` (núcleo lobulado); la ficha muestra la fase del ciclo,
   el contacto y la adhesión; la célula sembrada llega redonda y se extiende en ~1 h.
9. ✅ **Tests básicos:** la célula se adhiere, migra en tiempo biológico, pasa por las fases y se
   divide; sin suero queda en G0 y entra en apoptosis; la inhibición por contacto detiene la
   proliferación en confluencia (fibroblasto) y no en HeLa; el neutrófilo encuentra y fagocita una
   bacteria.
10. ✅ **Validación** (suite `validacion`, contra publicaciones): tiempo de duplicación de HeLa y del
    fibroblasto; rapidez y persistencia del fibroblasto (desplazamiento cuadrático medio del
    paseo persistente); curva de crecimiento hasta confluencia (latencia, fase exponencial y
    meseta en la densidad de confluencia publicada); consumo de glucosa y producción de lactato en
    72 h; rapidez del neutrófilo en quimiotaxis.
    *Medido:* duplicación de HeLa 22 h (dato 22 h) y del fibroblasto 30 h (dato 30 h) con siembra
    asincrónica; desplazamiento cuadrático medio dentro del 10 % de Fürth a 0,5, 1 y 2 h; meseta del
    fibroblasto ~34 000 células/cm² (dato ~40 000); lactato/glucosa de HeLa 1,67; neutrófilo a
    15 µm/min hacia el fMLP. Para llegar ahí se corrigieron artefactos de la mecánica: la forma de
    reposo de una célula que pasa de redonda a huso (o al revés) se reparte por longitud de arco
    (`soft_body::reshapeRestEven`; la proyección radial la dejaba cruzada y la célula se empujaba
    sola a ~8 µm/s); en la mitosis la membrana se pone sobre el círculo (un rulo atrapaba al
    núcleo), la placa metafásica se centra, cada juego de cromosomas mide 1/√2, la célula se alarga
    en la anafase B y el surco entra recién con los cromosomas en los polos (el anillo contráctil es
    la forma de reposo de las dos hijas). Las catástrofes mitóticas de HeLa bajaron de ~50 % a ~3 %.
    Los tests de división corren con kBioTimeScale 600: con 3600 una hora de M dura 0,3 s reales y el
    surco (física, en tiempo real) no alcanza a cerrarse.

## 20c — Preparados (portaobjetos): células vegetales y animales

Lo que se mira en un laboratorio de biología celular además de la gota y el cultivo: un tejido o
unas células montadas entre portaobjetos y cubreobjetos, vivas o fijadas y teñidas. Los más comunes
primero. Cada preparado tiene que verse como en el microscopio real a cada aumento (zoom semántico:
de lejos el tejido como textura; de cerca, cada célula con su pared, su núcleo, sus plastos y sus
gránulos; el LOD como en el resto del render) y lo que hace tiene que ser mecanismo real con datos.

*Hecho:* `bio/osmosis` (osmolaridad del medio con el coeficiente osmótico del NaCl y los
osmolitos; volumen de Boyle–van 't Hoff), `bio/tissue` (disposiciones hileras, poligonal, filas y
rompecabezas; contenidos protoplasto, almidón, mitosis y estomas), `bio/erythrocytes` (glóbulos
rojos y plaquetas como partículas, con volumen crítico de hemólisis por célula),
`game/world_tissue.cpp` (generador de paredes que llena el campo, protoplastos encerrados por su
pared y sin chocar entre sí, gránulos en la capa cortical, núcleo contra la pared, ósmosis en tiempo
real, frotis y sangre), `render/tissue_renderer` (paredes, granos de almidón con sus capas y su hilo,
fases de la mitosis con orceína, estomas, glóbulos bicóncavos, crenados, esferocitos y fantasmas,
plaquetas). Datos: 8 placas en `placas.toml`, medios de montaje (agua destilada, solución
fisiológica), el portaobjetos en `recipientes.toml`, 5 seres nuevos (protoplastos de cebolla y de
*Elodea*, célula escamosa, linfocito, monocito; `locomocion = "ninguna"`). Tests en
`tests/test_preparations.cpp`. El panel Placa elige la solución de montaje (en vivo, como la gota que
se tira bajo el cubreobjetos en clase); la sal y la sacarosa también en Ambiente.
*Medido:* la epidermis de cebolla (jugo de 450 mOsm/L) queda turgente con el medio a 0,9 veces el
jugo y se plasmoliza a 1,15 veces (volumen 0,88); con ~950 mOsm/L el protoplasto queda en ~0,44 del
área de la pared y vuelve a llenarla en agua. Fragilidad osmótica: 0 % de hemólisis con NaCl
0,55 %, 51 % con 0,42 % y 100 % con 0,30 % (Parpart et al. 1947: empieza ~0,50 %, mitad ~0,42 %,
toda ~0,35 %). Punta de raíz: índice mitótico 7,7 % con la profase la fase más frecuente. Frotis:
1,1·10⁶ glóbulos rojos/cm², plaquetas y los tres leucocitos en la fórmula normal. Rendimiento (con
la máquina cargada): cebolla 5,4 ms/paso, *Elodea* 2,5, sangre viva 0,65; mismo checksum con 1 y 8
hilos (`bio_bench` suma al checksum los glóbulos rojos y el tejido). Probado en la interfaz:
plasmólisis de la cebolla con sal y vuelta con agua destilada, hemólisis de la sangre en agua.

**Base común**

1. ✅ **Tipo de placa «preparado»** (`PlateKind::Slide`): portaobjetos y cubreobjetos con una capa fina
   de montaje (~10–20 µm de agua, solución salina o medio), a temperatura ambiente y con la luz de la
   lámpara; vivo o fijado (`World::fix`). Se le puede agregar una solución en vivo (agua, NaCl,
   sacarosa), como en clase: la gota se pone en un borde del cubreobjetos y se tira con un papel
   del otro.
2. ✅ **Ósmosis** (`bio/osmosis`): el volumen de una célula (el protoplasto de una vegetal, un
   eritrocito) sigue la osmolaridad del medio con la conductividad hidráulica de su membrana y la ley
   de Boyle–van 't Hoff (solo cambia el agua; los solutos quedan).
3. ✅ **Pared celular vegetal** (`bio/tissue`): la pared es un anillo rígido y el protoplasto un cuerpo
   aparte adentro; con turgencia lo aprieta, en medio hipertónico se despega (plasmólisis) y se
   recupera en agua (deplasmólisis). Un tejido son paredes compartidas con las vecinas, empaquetadas
   por un generador según el tejido.
4. ✅ **Tinciones nuevas como datos:** orceína o carmín acético (cromosomas, rojo), Giemsa/Wright (núcleos
   violeta, gránulos, eritrocitos rosados) y eosina; con las que ya hay (Lugol: almidón; azul de
   metileno: núcleos).

**Vegetales (los más comunes)**

5. ✅ **Epidermis de cebolla** (*Allium cepa*): monocapa de células alargadas con pared, núcleo y vacuola
   grande; Lugol o azul de metileno; plasmólisis con NaCl.
6. ✅ **Hoja de *Elodea*** (*Egeria densa*): cloroplastos con ciclosis; plasmólisis.
7. ✅ **Tubérculo de papa con Lugol:** amiloplastos (granos de almidón azul-negro, con sus capas).
8. ✅ **Punta de raíz de cebolla aplastada y teñida** (fijada): células en todas las fases de la mitosis;
   índice mitótico y fracción de cada fase de las duraciones publicadas; cromosomas por fase.
9. ✅ **Epidermis de hoja con estomas:** células oclusivas y ostíolo (estructura; la apertura después).

**Animales (los más comunes)**

10. ✅ **Mucosa bucal con azul de metileno:** células escamosas planas y poligonales (~50–80 µm) con
    núcleo chico y bacterias adheridas.
11. ✅ **Frotis de sangre humana** (Giemsa/Wright): eritrocitos bicóncavos sin núcleo, neutrófilos
    (núcleo multilobulado, los de 20b), linfocitos, monocitos y plaquetas en la fórmula leucocitaria
    normal; en sangre viva diluida, la ósmosis da crenación (hipertónico) y hemólisis (hipotónico).

**Cierre**

12. ✅ **Panel Placa:** el preparado elige el tejido, vivo o fijado y la solución de montaje; el
    Catálogo suma las categorías «Células vegetales» y «Células humanas».
13. ✅ **Tests y validación:** volumen del eritrocito según la osmolaridad (Boyle–van 't Hoff), NaCl de
    plasmólisis incipiente en la epidermis de cebolla, índice mitótico y fracción de cada fase en la
    punta de raíz, tamaños de cada tipo celular con su fuente.

## Riesgos

1. **Migración del contenido (20.0):** es el cambio más grande; un error cambia todas las
   dinámicas. Se verifica con checksums iguales en IEEE estricto antes y después.
2. **Escenas viejas:** guardan ids numéricos; se traducen con una tabla fija.
3. **Rendimiento:** una monocapa confluente de fibroblastos son ~40 000 células/cm²: unas 400 por
   mm². El campo simulado tiene que ajustarse a eso (1–2 mm de lado) y las células aplanadas
   necesitan un anillo con la resolución justa.
4. **Datos:** algunos valores (persistencia, Km de algunas líneas) pueden no estar medidos en
   el tipo exacto: quedan como estimación y se dice en la ficha.
5. **Alcance:** la señalización completa (vías de MAPK, Hippo/YAP) no se modela molécula a
   molécula; se modelan sus efectos medidos (punto de restricción, inhibición por contacto) y se
   documenta la simplificación.
6. **Paquetes de terceros:** pueden traer datos sin fuente o absurdos; la validación al cargar
   los rechaza o los marca, y la exportación anota qué paquetes se usaron.
7. **El vidrio siempre visible:** el campo simulado (~2 mm) es mucho más chico que un recipiente o un
   cubreobjetos reales; el anillo de vidrio marca el borde del campo simulado y se dice en la
   descripción de la placa. Las células de la orilla tocan vidrio: las mediciones se hacen lejos.
8. **Rendimiento de los preparados:** un frotis de sangre son miles de eritrocitos por mm²: necesitan
   una representación más barata que el cuerpo blando completo (como las bacterias); un tejido de
   cebolla son ~100–200 células grandes con pared.
9. **Ósmosis:** física nueva; se valida contra el volumen del eritrocito y la plasmólisis de la cebolla.

## Orden y criterios de «hecho»

1. ✅ **20.0** registro y paquetes → migración con checksums iguales → ids de texto en escenas →
   panel *Contenido* (exportar e importar). Hecho cuando un ser nuevo definido solo en un
   `.biopack` (por ejemplo, otra especie de *Paramecium*) aparece en el catálogo, se siembra, vive
   y se exporta e importa en otra instalación sin compilar.
2. ✅ **20a** tipo de placa → medios y química nueva → recipiente y campo → inoculación y mezclas →
   panel. Hecho cuando un cultivo de DMEM en incubadora se crea, se le inocula una levadura del
   catálogo y la escena se repite bit a bit.
3. ✅ **20b** fibroblasto (arrastre, ciclo, inhibición por contacto, metabolismo, apoptosis) → HeLa
   → neutrófilo → render y textos → validación. Hecho cuando pasan los tests de validación de
   20b con sus fuentes y tolerancias.
4. ✅ **20c** preparado y ósmosis → pared celular y tejidos → cebolla, *Elodea*, papa → raíz y estomas
   → mucosa bucal y sangre → panel y validación. Hecho cuando un preparado de epidermis de cebolla
   se plasmoliza al agregar NaCl y se recupera con agua, y en un frotis de sangre teñido con Giemsa
   se distinguen los tipos celulares, con datos con fuente y reproducible.

Después de la fase 20: modo cultivo
(Gause), P1 y lanzamiento.
