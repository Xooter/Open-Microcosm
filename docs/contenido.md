# Formato del contenido

El contenido de BioGame son datos (`game/content`): seres, planes corporales, minerales, biomas,
medios, recipientes, placas preparadas, lecciones, reactivos y el glosario. Los nombres de máquina
(carpetas, archivos, claves, ids y valores con nombre) están en **inglés**; el texto que lee una
persona (`name`, `description`, `source`, definiciones, pasos de las lecciones, comentarios) está en
español y lo traduce `assets/lang`.

## Un paquete

Un paquete es una carpeta con su manifiesto y una carpeta por tipo, con **un archivo por
definición**, que se llama como su id:

```
assets/content/base/
  pack.toml
  beings/paramecium.toml      [[being]] y su fisiología medida ([physiology.paramecium.*])
  body_plans/ciliate.toml     [[body_plan]]
  minerals/clay.toml          [[mineral]]
  biomes/pond.toml            [[biome]]
  media/wc_guillard.toml      [[medium]]
  vessels/dish_35mm.toml      [[vessel]]
  plates/onion_epidermis.toml [[plate]]
  lessons/jennings_co2.toml   [[lesson]]
  reagents/ampicillin.toml    [[reagent]]
  glossary/chemostat.toml     [[term]]
```

- **Sumar algo es sumar un archivo**. Cada archivo trae una sola definición, y su nombre tiene que
  ser su id: si no, el paquete avisa.
- Las carpetas se cargan en orden de dependencias (`core/pack_files`, `kPackFolders`). Dentro de una
  carpeta va primero lo que lista `[pack.order]` y después el resto por nombre. Los índices de los
  seres, biomas y minerales de fábrica son su orden de carga: las escenas viejas y la enciclopedia
  guardada dependen de él, así que **esa lista no se reordena** y lo nuevo se agrega al final.
- Un `.biopack` es lo mismo en un solo archivo: el `[pack]` y la concatenación de las definiciones,
  más `[[scene]]` (`id`, `plate`: el texto de una placa).
- Los paquetes del usuario viven en `<datos>/biogame/packs/` (carpeta o `.biopack`).
- **Ids renombrados:** `legacy_ids = ["id_viejo"]` mantiene válido un id anterior (lo nombran las
  placas, escenas y experimentos guardados). Al leerlos, se reescriben con el id actual.

### `pack.toml`

`[pack]`: `id` (letras minúsculas, dígitos y `_`), `name`, `version`, `author`, `license`,
`min_program` (versión mínima del programa). `[pack.order]`: una lista de ids por carpeta.

## Seres: `beings/<id>.toml`

`[[being]]`. Cada uno combina mecanismos que implementa el programa (locomoción, alimentación,
reproducción, ciclo de vida) y los parametriza. Las claves que faltan toman el valor por omisión.

- `id`: estable, sin espacios (lo guardan las escenas). `name`: el que se muestra. `description`:
  qué es y qué hace (ficha y enciclopedia).
- `tags`: grupos (taxonómicos o funcionales) para dietas, huidas y hospedadores.
  `obligate_symbiont`: vive solo dentro de su hospedador; en una gota aparece solo si el bioma lo
  nombra en sus `weights`.
- `locomotion`: `cilia` | `flagellum` | `pseudopod` | `crawl` (células animales) | `none` (células
  de tejidos y frotis). `relative_speed`: respecto de la del tipo.
  - `helix_radius` [µm] y `helix_period` [s]: nado helicoidal (dinoflagelados); vista desde arriba,
    una oscilación lateral de ese radio. Sin ellos, el flagelo tiene el bamboleo genérico.
  - `flagellar_gliding`: se desliza sobre el vidrio con el flagelo rígido (*Peranema*): avanza
    recto, el agua no lo arrastra y no deja dipolo en el agua.
- `feeding`: `oral_groove` | `anterior_pole` | `phagocytosis` | `raptorial` | `tentacles` | `neck` |
  `osmotrophy` | `none`.
- `prey`, `mouth_prey` (lo que traga con la corriente oral), `flees_from`: ids de seres o etiquetas.
- `conjugation`: se conjuga con otra célula de su especie y de otro tipo de apareamiento.
- `ph` y `temperature` [°C]: rango tolerado; fuera se paga como mantenimiento extra.
  `max_salinity`.
- `storage`: `none` | `starch` | `glycogen` | `paramylon` | `chrysolaminarin` (tinciones).
- `wall`: `none` | `cellulose` | `silica` | `yeast_glucan`.
- `body_plan`: el plan corporal (qué estructuras tiene y de qué tamaño).
- `genome`: genes que difieren de los de su locomoción (`size`, `motor`, `mitochondria`,
  `chloroplasts`, `lysosomes`, `trichocysts`, `armor`, `rigidity`, `mating_type`).
- Mecanismos: `chemokinesis`, `acid_trap`, `phototaxis`, `relative_photosynthesis`,
  `fissions_per_cycle`, `encysts`, `initial_count`, `sessile`, `stalk_length`, `spasmoneme`,
  `walks`, `tentacle_reach`, `neck_reach`, `handling_hours`, `chloroplasts`, `raphe_glider`,
  `b12_auxotroph`, `frustule`, `filament`, `filament_cells`, `fragmentation_per_hour`, `zoospores`,
  `daughter_colonies`, `colony_cells`, `mother_dies_at_release`, `relative_dry_mass`, `budding`,
  `anaerobe`, `posterior_mouth`, `cyclosis`.
- `[[being.response]]` (hasta 4): respuesta fóbica temporal. `field`: `light` | `oxygen` | `shear`
  (deformación del agua [1/s]) | `touch` (el polo anterior chocó: 1); `kind`: `step_up` | `step_down` (cambio relativo mayor que
  `threshold` respecto de lo que recuerda de los últimos `memory` s, con el nivel arriba de
  `min_level`) | `above` (el nivel pasa `threshold`); `action`: `avoid` (reacción de evitación o
  fotofóbica) | `jump` | `contract`; `refractory` [s].
- `[being.contraction]`: contracción del cuerpo entero (mionemas). La forma de `[being.shape]` es la
  contraída; extendida mide `extension` veces su largo (con el área de la biomasa) y vuelve a
  extenderse en `extension_s` [s]. La dispara una respuesta `contract`; el remolino de la contracción
  entra en el agua y lo sienten las vecinas.
- `[being.jump]`: saltos. `speed` [µm/s], `duration` [s], `interval` [s] medio entre saltos
  espontáneos (0: solo de escape, con una respuesta `jump` o una amenaza a menos de 40 µm).
- `[being.stalk]`: pie o tallo de un ser fijo (lo hace sésil). `kind`: `spasmoneme` (se enrolla) |
  `rigid` | `mucilage` (hialino y flexible) | `foot` (sin tallo, el polo posterior pegado). `length`
  y `width` [µm], `bending_rate` [1/s] (rigidez frente al arrastre del agua), `growth_hours` (en
  secretarlo después de fijarse), `decay_hours` (lo que dura vacío), `contracted_fraction` y
  `extension_s` (espasmonema), `spontaneous_per_min`, `detaches` (con hambre el zooide se suelta y
  deja el tallo). Las claves viejas `sessile`, `stalk_length` y `spasmoneme` se siguen leyendo.
- `[being.light_damage]`: daño fotodinámico. Arriba de `threshold` (irradiancia relativa) se paga
  como estrés, `stress` con la lámpara al máximo; necesita O₂.
- `[being.stress_pigment]`: pigmento de estrés (astaxantina). `rate_per_hour` con luz arriba de
  `light_threshold` o sin N (el doble en el quiste), `decay_per_hour` sin estrés, `protection`
  (fracción del daño por luz que evita al máximo) y `color`.
- `[being.shape]`: `semi_major_axis` y `semi_minor_axis` [µm], `thickness` [µm] (por omisión, el
  menor), `shape_memory` [1/s], `color` RGB del citoplasma, `nucleus` (radio relativo),
  `squareness` (2 elipse, ~6 cilindro), `taper` del polo posterior.
- `[being.appearance]`: `style`, un dibujo con nombre (`ciliate`, `flagellate`, `amoeboid`,
  `flagellar_rostrum`, `girdles_and_proboscis`, `helical_ribbon`, `axial_plate`,
  `stellate_plastids`, `reticulate_plastid`, `frustule`, `spherical_colony`, `flat_colony`,
  `peristomal_crown`, `membranelle_crown`, `capitate_tentacles`, `cirri`, `extensible_neck`,
  `two_flagella`, `yeast`, `adherent_cell`, `neutrophil`, `plant_protoplast`, `elodea_leaf`,
  `squamous_cell`, `lymphocyte`, `monocyte`, `coccoid`, `plate_colony`, `compact_colony`,
  `hollow_colony`; por omisión, el de su locomoción) y `plastid_color` [r, g, b].
- `[being.cell]`: mecanismos de una célula de organismo pluricelular: `adherent`, `rounded_radius`
  [µm], `contact_inhibition_locomotion`, `contact_inhibition_proliferation`, `min_serum`
  [fracción], `apoptosis_without_serum_h`, `divides`, `fmlp_chemotaxis`. `relative_dry_mass`: la
  célula extendida es mucho más delgada que el esferoide de su silueta.
- `[[being.process]]` (una por prolongación): **prolongaciones** de la célula o de su cubierta.
  `kind`: `horn` | `spine` | `arm` | `seta`; `at`: dirección desde el polo anterior del cuerpo [°];
  `length` desde la superficie, `base_width` y `tip_width` [µm], para el tamaño típico (escalan con la
  célula); `curve`: giro total a lo largo [°, positivo hacia la izquierda]; `count`: copias, repartidas
  en `spread` [°] alrededor de `at` o, sin `spread`, en toda la vuelta (simetría radial); `body`: si es
  parte de la célula (por omisión, `true` para `horn` y `arm`, `false` para `spine` y `seta`);
  `fillet` [µm]: radio del redondeo de la unión (por omisión, medio ancho de la base).
  - **Del cuerpo** (`body = true`: cuernos de *Ceratium*, brazos de *Staurastrum*, espina caudal de
    *Phacus*): tienen citoplasma, así que son parte de la célula. La silueta (`bio/silhouette`) une el
    cuerpo y las prolongaciones con un redondeo y es la forma del anillo de membrana: chocan, crecen
    y se dividen con la célula, y la memoria de forma de la pared (`shape_memory`) las mantiene
    rígidas. La masa sigue saliendo del esferoide del cuerpo (los cuernos casi no tienen citoplasma).
  - **De la cubierta** (`body = false`: espinas de la lórica de *Trachelomonas*, de las placas de
    *Coleps*, de la testa de *Euglypha*): sin citoplasma y más finas que la resolución de la membrana
    (3 µm); se dibujan sobre la cubierta, con la base redondeada, y no son parte del cuerpo blando.

### Fisiología medida: `[physiology.<id>.<parámetro>]`

En el archivo del ser. Cada valor como se publicó: `value`, `unit`, `temperature` de la medición
[°C], `source` y `estimate` (true: no es una medición directa en condiciones comparables; la ficha
lo dice). El programa lo lleva a 20 °C con Q10 = 2. Los seres sin dato usan el valor general del
metabolismo (`metabolism.max_growth`).

- `max_growth`: `"1/h"` o `"1/day"` (tasa), `"h"` o `"day"` (tiempo de duplicación).
- `max_ingestion`: `"bacteria/h"` o `"bacteria/min"`. `clearance`: `"nL/h"`, `"uL/h"` o `"mL/h"`.
- `p_half_saturation`, `si_half_saturation`: `"µM"`.
- Células animales: `migration_speed` (`"um/h"`, `"um/min"`), `persistence`, `phase_g1`,
  `phase_s`, `phase_g2`, `phase_m`, `lifespan` (`"h"`, `"min"`, `"day"`), `confluence` (`"1/cm2"`),
  `glucose_vmax`, `glutamine_vmax`, `o2_uptake` (`"fmol/h"`, `"amol/s"`, `"pmol/h"`),
  `glucose_km`, `glutamine_km` (`"uM"`, `"mM"`), `lactate_per_glucose` (`"mol/mol"`).

## Planes corporales: `body_plans/<id>.toml`

`[[body_plan]]`: qué estructuras tiene cada tipo de célula y de qué tamaño real son. Los recorre el
zoom semántico y la ficha. Nada se agrega porque "se vea bien": cada estructura tiene que existir en
el organismo real. `id`, `name`, `morphology` (`eukaryote`, `rod`, `vibrio`, `tailed_phage`,
`icosahedral`, `helical`) y `structures`, cada una con:

- `kind`: `plasma_membrane`, `outer_membrane`, `peptidoglycan`, `cell_wall`, `capsule`,
  `nucleoid`, `ribosomes`, `flagellum`, `pili`, `nucleus`, `nucleolus`, `mitochondrion`,
  `hydrogenosome`, `chloroplast`, `pyrenoid`, `eyespot`, `endoplasmic_reticulum`, `golgi`,
  `contractile_vacuole`, `food_vacuole`, `lysosome`, `storage_granule`, `capsid`, `viral_genome`,
  `viral_envelope`, `tail`, `baseplate`, `tail_fibers`.
- `source`: `simulated` (tiene estado en la simulación) o `declared` (existe pero no se simula: solo
  se dibuja en la vista de modelo, salvo `brightfield`).
- `arrangement`: `envelope`, `interior`, `polar`, `peritrichous`, `perinuclear`, `anterior`.
- `length` y `width` [µm] (0: los da la simulación), `count` por célula, `per_um3` (lo que se
  reparte por volumen, como los ribosomas), `brightfield` (se distingue sin teñir) y `name` en la
  ficha.

## Minerales: `minerals/<id>.toml`

`[[mineral]]`: partículas inorgánicas del sedimento, como se ven en campo claro. `id`, `name`,
`description`, `grain_name` (un grano de arena fijo de este mineral), `radius` [mínimo, máximo] µm,
`density_difference` [kg/m³] con el agua (sedimentación de Stokes), `opacity` (0–1), `flocculates`
y `style` (`floc`, `angular_grain`, `flake`, `opaque_grain`, `refractile_globule`, `frustule`).

## Biomas: `biomes/<id>.toml`

`[[biome]]`: un ambiente real con su química, su temperatura y su elenco de seres; la gota de cada
placa sale de un bioma con variaciones por seed (`makeDrop`).

- `id`, `name`, `description`.
- `bacteria`, `detritus`: individuos en la gota de referencia (2048 × 1024 µm); en otra placa se
  escalan por el área.
- `nitrogen_fixing_filaments` (cianobacterias con heterocistos), `oscillatoria_filaments`,
  `cyanobacteria_temperature` [°C].
- Hongos y mohos al empezar: `saprolegnia`, `chytrid_clusters`, `allomyces`, `lichens`, `physarum`.
- `patchiness`: manchas iniciales de materia orgánica y detritos (0–1).
- `base_weight` y `weights` (por id de ser): el multiplicador de la población inicial.
- `[biome.medium]`: química y ambiente; lo que falta queda en el valor general del agua dulce:
  `temperature` [°C], `salinity` [PSU], `light` (0–1), `o2_exchange` [1/s], `anoxic`, `alkalinity`
  (bicarbonato) [µM], `phosphate`, `nitrogen`, `silicate`, `iron`, `vitamin_b12` [µM], `b12_source`
  [µM/h], `b12_decay` [1/h], `frustule_dissolution` [1/h], `phages` [viriones/µm³],
  `phage_inactivation` [1/h], `toxin_decay` [1/h], `ahl_half_life` [h], `organic_inflow` [µM/h
  biológica], `decomposition` [1/s biológico], `detritus_inflow` [1/s], `nutrient_uptake` [1/s],
  `dust` [1/s], `osmolytes` [mOsm/L].
- `[biome.sediment]`: partículas por gota de referencia, por id de mineral, más `grains` (arena:
  obstáculos fijos) y `grain_radius` [µm] (por omisión [50, 120]).

## Medios de cultivo: `media/<id>.toml`

`[[medium]]`: una composición publicada (`source` es obligatoria). `[medium.composition]` usa las
claves de `[biome.medium]` (en µM salvo que se diga otra cosa), más `glucose` [µM] (carbono orgánico
disuelto), `glutamine` [µM], `serum` (fracción de suero fetal bovino), `phenol_red` [mg/L],
`carbonate_pka` (pK′ aparente de CO₂/HCO₃⁻) y `air_co2`. La temperatura, el CO₂ y la luz no son del
medio sino del ambiente de la placa.

## Recipientes: `vessels/<id>.toml`

`[[vessel]]`: `id`, `name`, `area_cm2` (de cultivo), `volume_ml` (de trabajo), `source`. La
profundidad (volumen / área) fija cuánto medio hay sobre cada célula y qué tan rápido entra el O₂ y
sale el CO₂ por la superficie (D / profundidad²).

## Placas preparadas: `plates/<id>.toml`

`[[plate]]`: `id`, `name`, `description`, `kind` (`cell_culture` o `slide`), `medium` y `vessel`
(ids), `field_um` (lado del campo simulado), `cell_um` (resolución de la grilla), `fixed` (muerto y
quieto: frotis, corte), `stain` (id de la tinción al crearla), `erythrocytes_cm2`, `platelets_cm2`,
`bacteria_cm2`.

- `[plate.environment]`: `temperature` [°C], `co2` (fracción del aire), `light`.
- `[[plate.seeding]]`: `being` (id) y `density_cm2` [células/cm²].
- `[plate.tissue]`: `arrangement` (`rows`, `polygonal`, `files`, `jigsaw`), `content`
  (`protoplast`, `starch`, `mitosis`, `stomata`), `being` (el protoplasto), `length_um` y
  `width_um` de las células, `wall_um`, `sap_osmolarity` [mOsm/L], `inactive` (fracción
  osmóticamente inactiva), `equilibration_s` y `mitotic_phases` [profase, metafase, anafase,
  telofase].

## Lecciones: `lessons/<id>.toml`

`[[lesson]]`: un protocolo con pasos. No hay puntaje: el informe lo corrige el docente.

- `id`, `title`, `level`, `goal`, `source`.
- Qué se prepara: `plate` (id), o `biome` + `seed` + `content` (0 completa, 1 sin vida, 2 vacía), o
  `culture` (el texto de una escena de cultivo). `setup`: intervenciones al empezar, como en una
  escena (`"paso tipo activo índice x y v0..v5 [clave]"`).
- `controls`: claves del registro de parámetros que ve el alumno. `[[lesson.locked]]`: `key`,
  `reason`.
- `[[lesson.actions]]`: `text`, `event` (una intervención de la placa o una operación del cultivo,
  `"op valor [clave]"`).
- `[[lesson.step]]`: `text`; `goal` {`kind`, `key`, `op`, `value`}; `question` {`text`, `options`,
  `correct`, `explanation`}; `prediction` {`text`, `quantity` {`kind`, `key`}, `unit`}.
- Condiciones (`kind`): `time_h`, `population`, `bacteria`, `concentration`, `ph`, `plasmolysis`,
  `hemolysis`, `near_center`, `fraction`.
- `solution` (intervenciones u operaciones con su paso que cumplen todos los objetivos; las usa
  `tests/test_lessons.cpp`) y `solution_hours`.

## Reactivos: `reagents/<id>.toml`

`[[reagent]]`: `id`, `name`, `description`, `mechanism` (`pharmacodynamic`: Regoes et al. 2004),
`targets` (por ahora `bacteria`), `growth_dependent`, `diffusion` [µm²/s];
`[reagent.pharmacodynamics]`: `psi_max` y `psi_min` [1/h], `zmic` [µg/mL], `kappa`, `source`;
`[reagent.published_mic]`: `value` [µg/mL], `source` (solo valida la CIM que emerge).

## Glosario: `glossary/<id>.toml`

`[[term]]`: `id`, `term`, `definition`, `source`, `alias` (otras formas en el texto, en minúsculas:
están en español porque se buscan en los textos), `related` (ids), `lesson`, `plate`.

## Otros archivos

- **Escena de cultivo** (`.culture`, `<datos>/biogame/cultures/`): líneas `clave=valor`:
  `culture_version`, `name`, `vessel`, `medium`, `temperature`, `light`, `seed`, `program=` y
  `event=paso op valor [clave]` (op: `inoculate`, `bacteria`, `medium`, `dilution`,
  `concentration`, `phages`, `sample`, `withdraw`, `receive`, `inoculate_density`, `chemostat`,
  `inflow_medium`, `reagent`).
- **Programa** (`program=` en placas y cultivos): `destino forma inicio fin desde hasta período
  ciclo cantidad`. Forma: `ramp`, `cycle`, `sine`, `event`. Destino: `temperature`, `light`,
  `solute:<soluto>`, `parameter:<clave>` o una acción (`bacteria`, `medium`, `dilution`,
  `add:<soluto>`, `inoculate:<id>`). Solutos: `o2`, `co2`, `phosphate`, `nitrogen`, `silicate`,
  `iron`, `b12`, `organic`.
- **Experimento** (`<datos>/biogame/experiments/*.toml`): `[experiment]` (`name`, `kind` =
  `culture` o `plate`, `replicates`, `seed`, `hours`, `sample_h`, `threads`), `[culture]`
  (`vessel`, `medium`, `temperature`, `light`, `program`, `events`) o `[plate]` (`scene`), y
  `[[factor]]` (`key`, `values`).
- Las placas (`.plate`), tinciones (`.stain`) y opciones ya usaban claves en inglés.
- **Datos del usuario** (`<datos>/biogame/`): `packs/`, `plates/`, `cultures/`, `experiments/`,
  `stains/`, `photos/`, `exports/`, `autosave.plate`, `layout.ini`, `options.txt` y
  `progress.txt`. Las carpetas y archivos con el nombre viejo (`paquetes/`, `cultivos/`,
  `experimentos/`, `fotos/`, `autoguardado.plate`) se renombran solos la primera vez, y la
  disposición guardada de los paneles se migra a sus ids nuevos.

Lo guardado con los nombres viejos (en español) se sigue leyendo (`game/legacy_names`, `legacy_ids`)
y se vuelve a guardar con los nuevos. Un paquete viejo se migra con
`tools/migrate_content.py <paquete viejo> <carpeta nueva>`; los nombres están en
`tools/content_names.py`.
