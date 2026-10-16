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
   (D/profundidad²: ~50 min en 2,5 mm); el campo es periódico (sin vidrio: un recorte del fondo).
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

1. **Locomoción por arrastre** (`Crawling`, tiempo biológico): lamelipodio en el frente y adhesión
   al sustrato; **paseo aleatorio persistente** con rapidez y tiempo de persistencia medidos.
   Redondeo durante la mitosis (la célula se despega y se redondea, visible).
2. **Ciclo celular por fases** (G1, S, G2, M) con duraciones de los datos; **punto de restricción**
   en G1 que exige factores de crecimiento; G0 (quiescencia) sin ellos.
3. **Inhibición por contacto:** de la locomoción (Abercrombie: al tocar a otra célula retrae ese
   frente y se repolariza) y de la proliferación (con la periferia ocupada queda en G0/G1). HeLa no
   tiene inhibición de la proliferación.
4. **Metabolismo celular:** captación de glucosa y glutamina (Michaelis–Menten, con Km y Vmax
   publicados), respiración y glucólisis aeróbica (Warburg en HeLa) con secreción de lactato que
   baja el pH; consumo de O₂ por célula medido. Masa y volumen celular crecen en el ciclo.
5. **Apoptosis:** sin factores de crecimiento, sin glucosa o por edad (neutrófilos): la célula se
   encoge, forma vesículas y se fragmenta en cuerpos apoptóticos sin lisis (sin inflamación). La
   lisis actual queda como necrosis.
6. **Neutrófilo:** quimiotaxis hacia péptidos formilados (fMLP) que liberan las bacterias (campo
   nuevo, con difusión real) y fagocitosis de bacterias con la copa existente; vida corta.
7. **Datos (paquete de fábrica «células humanas»):** fibroblasto dérmico humano, HeLa, neutrófilo,
   cada valor con fuente (duplicación, duración de fases, tamaño adherido y redondeado, rapidez y
   persistencia de migración, Km y Vmax de glucosa y glutamina, consumo de O₂, densidad de
   confluencia).
8. **Render, ficha y enciclopedia:** células aplanadas con núcleo y nucléolos, redondeadas en
   mitosis, cuerpos apoptóticos; categoría «Células humanas» en el catálogo; textos de qué es cada
   una y por qué hace lo que hace. El contraste de fase (lente) queda para la fase de lentes.
9. **Tests básicos:** la célula se adhiere, migra en tiempo biológico, pasa por las fases y se
   divide; sin suero queda en G0 y entra en apoptosis; la inhibición por contacto detiene la
   proliferación en confluencia (fibroblasto) y no en HeLa; el neutrófilo encuentra y fagocita una
   bacteria.
10. **Validación** (suite `validacion`, contra publicaciones): tiempo de duplicación de HeLa y del
    fibroblasto; rapidez y persistencia del fibroblasto (desplazamiento cuadrático medio del
    paseo persistente); curva de crecimiento hasta confluencia (latencia, fase exponencial y
    meseta en la densidad de confluencia publicada); consumo de glucosa y producción de lactato en
    72 h; rapidez del neutrófilo en quimiotaxis.

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

## Orden y criterios de «hecho»

1. ✅ **20.0** registro y paquetes → migración con checksums iguales → ids de texto en escenas →
   panel *Contenido* (exportar e importar). Hecho cuando un ser nuevo definido solo en un
   `.biopack` (por ejemplo, otra especie de *Paramecium*) aparece en el catálogo, se siembra, vive
   y se exporta e importa en otra instalación sin compilar.
2. ✅ **20a** tipo de placa → medios y química nueva → recipiente y campo → inoculación y mezclas →
   panel. Hecho cuando un cultivo de DMEM en incubadora se crea, se le inocula una levadura del
   catálogo y la escena se repite bit a bit.
3. **20b** fibroblasto (arrastre, ciclo, inhibición por contacto, metabolismo, apoptosis) → HeLa
   → neutrófilo → render y textos → validación. Hecho cuando pasan los tests de validación de
   20b con sus fuentes y tolerancias.

Después de la fase 20: preparado vegetal (Elodea con ciclosis, epidermis de cebolla), modo cultivo
(Gause), P1 y lanzamiento.
