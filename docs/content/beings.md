# Beings: `beings/<id>.toml`

A being file holds one `[[being]]` table and, optionally, the measured physiology of that being
(`[physiology.<id>.<parameter>]`). Each being combines mechanisms that the program implements
(locomotion, feeding, reproduction, life cycle) and parametrises them. Missing keys take their
default value. The parser is `bio/species.cpp` (`SpeciesInfo`); unknown keys produce a warning.

Every being also has its own document in [`beings/`](beings/README.md), with its description,
literature, what is not implemented yet and its unique mechanics.

## Identity

| Key | Type | Meaning |
|---|---|---|
| `id` | string | Stable id without spaces (scenes store it). Must match the file name. |
| `name` | string | Display name. |
| `description` | string | What it is and what it does (inspector card). |
| `legacy_ids` | [string] | Previous ids that remain valid. |
| `tags` | [string] | Taxonomic or functional groups used by diets, flight and hosts (`ciliate`, `alga`, `diatom`, `phototroph`, `chytrid_host`…). `obligate_symbiont`: lives only inside its host; in a drop it appears only if the biome names it in its `weights`. |
| `body_plan` | id | The body plan: which structures it has and how large ([body_plans.md](body_plans.md)). |
| `initial_count` | int | Individuals in a reference drop (scaled by the biome weights and the plate area). |

## Locomotion

| Key | Type | Meaning |
|---|---|---|
| `locomotion` | `cilia` \| `flagellum` \| `pseudopod` \| `crawl` \| `none` | `crawl`: animal cells; `none`: tissue and smear cells. |
| `relative_speed` | float | Relative to the speed of the locomotion type (cilia ~600 µm/s, flagellum ~80 µm/s, pseudopod ~6 µm/s). |
| `division_axis` | `transverse` (default) \| `longitudinal` | Peritrichs: the furrow runs along the axis, from mouth to scopula. |
| `helix_radius` [µm], `helix_period` [s] | float | Helical swimming (dinoflagellates, ciliates, euglenoids); seen from above, a lateral oscillation of that radius. Without them the flagellum has the generic wobble. |
| `flagellum` | `"posterior"` | The flagellum pushes from behind (choanoflagellates, sperm): a *pusher* force pair in the water and the flagellum drawn behind. |
| `flagellum_length` [µm] | float | Flagellum length when it is not ~1 body length (the sperm tail, ~50 µm): drawn so and its thrust enters the water along it. |
| `flagellar_gliding` | bool | Glides on the glass with a rigid flagellum (*Peranema*): moves straight, is not dragged by the water and leaves no dipole. |
| `walks` | bool | Walks on cirri (hypotrichs). |
| `raphe_glider` | bool | Diatom gliding along the raphe. |
| `phototaxis` | bool | Swims towards moderate light. |
| `chemokinesis` | bool | Changes speed with the chemical environment. |
| `acid_trap` | bool | Accumulates where there is CO₂ (Jennings): reverses on a step-down of acidity. |

## Feeding

| Key | Type | Meaning |
|---|---|---|
| `feeding` | `oral_groove` \| `anterior_pole` \| `phagocytosis` \| `raptorial` \| `tentacles` \| `neck` \| `osmotrophy` \| `axopodia` \| `perforation` \| `none` | Feeding mode (see [feeding](../mechanics/feeding-and-phagocytosis.md)). |
| `prey`, `mouth_prey`, `flees_from` | [id or tag] | What it eats, what it swallows with the oral current, what it flees from. |
| `mouth_width` [µm] | float | Maximum mouth opening (dilated cytostome) at the typical size. A prey enters if its minimum width (the silhouette with horns or arms) does not exceed it; 0 = no limit. |
| `handling_hours` | float | Handling time between whole prey items. |
| `tentacle_reach`, `neck_reach` [µm] | float | Reach of suctorian tentacles / axopodia (`axopodia`) / perforation (`perforation`) and of the *Lacrymaria* neck. |
| `perforation_hours` | float | Time to open the wall before emptying it (*Vampyrella*). |
| `histophagous` | bool | Goes towards damaged cells (paralysed, trapped, dying) and, if large, eats them in bites together with others. |
| `posterior_mouth` | bool | The mouth is at the posterior pole. |
| `chloroplasts`, `relative_photosynthesis` | bool, float | Photosynthesis and its relative rate. |
| `b12_auxotroph` | bool | Needs vitamin B₁₂ (supplied by bacteria). |
| `anaerobe` | bool | Lives without O₂. |

## Reproduction and life cycle

| Key | Type | Meaning |
|---|---|---|
| `conjugation` | bool | Conjugates with another cell of its species and a different mating type. |
| `fissions_per_cycle` | int | Daughters per cycle (autospores, reproductive cyst). |
| `encysts` | bool | Forms resting cysts under bad conditions. |
| `divides_in_cyst` | bool | Divides inside a reproductive cyst (*Colpoda*): still, not eating, with a thin wall during the cycle. |
| `budding` | bool | Reproduces by budding (yeasts, *Euglypha*). |
| `filament`, `filament_cells`, `fragmentation_per_hour` | | Filamentous algae: grow lengthwise and divide by septa. |
| `daughter_colonies`, `colony_cells`, `mother_dies_at_release` | | Colonies that release daughter colonies (volvocines). |
| `zoospores` | bool | Releases zoospores. |
| `relative_dry_mass` | float | Dry mass relative to the default (hollow or vacuolated bodies; flat animal cells). |
| `frustule` | bool | Siliceous frustule (uses Si; it remains when the cell dies). |
| `lorica_iron_ratio` [pmol Fe / pmol biomass], `lorica_hours` | float | Lorica mineralised with iron from the water (*Trachelomonas*); the daughter leaves and builds a new one. |
| `testate` | `lobose` \| `filose` | Testate amoeba: the soft body is the rigid test (no plastic membrane or phagocytic cups); pseudopods leave through the aperture, drag it and are drawn. |
| `cyclosis` [µm/s] | float | Cytoplasmic streaming speed of the granules. |

## Tolerances and chemistry

| Key | Type | Meaning |
|---|---|---|
| `ph`, `temperature` [°C] | [min, max] | Tolerated range; outside it the cell pays extra maintenance (stress). |
| `max_salinity` [PSU] | float | Maximum salinity tolerated. |
| `storage` | `none` \| `starch` \| `glycogen` \| `paramylon` \| `chrysolaminarin` | Storage compound (used by stains: only true starch turns blue-black with Lugol). |
| `wall` | `none` \| `cellulose` \| `silica` \| `yeast_glucan` | Cell wall. |
| `genome` | inline table | Genes that differ from those of its locomotion: `size`, `motor`, `mitochondria`, `chloroplasts`, `lysosomes`, `trichocysts`, `armor`, `rigidity`, `mating_type` ([genome](../mechanics/genome-and-organelles.md)). |
| `[being.genetics]` | table | Optional. `mutational_cv` (per gene and per generation, default 0.005), `standing_cv` (wild population, default 0.05), `macronucleus` (default true for the tag `ciliate`), `source` ([genome](../mechanics/genome-and-organelles.md#mutation-and-variation)). |

## Shape and appearance

### `[being.shape]`

| Key | Meaning |
|---|---|
| `semi_major_axis`, `semi_minor_axis` [µm] | The cell's semi-axes. |
| `thickness` [µm] | Vertical semi-axis (default: the minor one). Mass is that of the spheroid. |
| `shape_memory` [1/s] | Rate of the global shape memory (rigid pellicle high, amoeboid cortex ~0). |
| `color` [r, g, b] | Cytoplasm colour. |
| `nucleus` | Relative nucleus radius. |
| `squareness` | 2 ellipse, ~6 cylinder. |
| `taper`, `taper_front` | Narrowing of the posterior / anterior pole, 0 to < 1; with both, a spindle. |
| `bend` [°] | The axis curves in an arc of that angle (*Closterium* crescent; the cell becomes a silhouette). |
| `cytoplasm_granules` | `false`: no free granules in the cytoplasm (the mature sperm cell has almost no cytoplasm). |

### `[being.appearance]`

| Key | Meaning |
|---|---|
| `style` | A named drawing: `ciliate`, `flagellate`, `amoeboid`, `flagellar_rostrum`, `girdles_and_proboscis`, `helical_ribbon`, `axial_plate`, `stellate_plastids`, `reticulate_plastid`, `frustule`, `spherical_colony`, `flat_colony`, `peristomal_crown`, `membranelle_crown`, `capitate_tentacles`, `cirri`, `extensible_neck`, `two_flagella`, `yeast`, `adherent_cell`, `neutrophil`, `plant_protoplast`, `elodea_leaf`, `squamous_cell`, `lymphocyte`, `monocyte`, `coccoid`, `plate_colony`, `compact_colony`, `hollow_colony`, `spermatozoon`. Default: the one of its locomotion. |
| `plastid_color` [r, g, b] | Plastid colour. |
| `roll_hz` | Rotation about the long axis of a flattened swimming cell (the sperm head narrows edge-on; drawing only). |
| `pigment_stripes` | Bands of pigment granules along the body (*Stentor*, *Blepharisma*). Saturated cytoplasm looks more opaque (the pigment absorbs). |

### `[[being.process]]` (one per process)

**Processes** of the cell or of its covering.

| Key | Meaning |
|---|---|
| `kind` | `horn` \| `spine` \| `arm` \| `seta` \| `notch`. |
| `at` [°] | Direction from the anterior pole of the body. |
| `length`, `base_width`, `tip_width` [µm] | From the surface, for the typical size (they scale with the cell). |
| `curve` [°] | Total turn along it (positive to the left). |
| `count`, `spread` [°] | Copies, spread over `spread` around `at` or, without `spread`, over the whole turn (radial symmetry). |
| `body` | Whether it is part of the cell (default `true` for `horn` and `arm`, `false` for `spine` and `seta`). |
| `fillet` [µm] | Radius of the rounding at the junction (default: half the base width). |

- **Body processes** (`body = true`: horns of *Ceratium*, arms of *Staurastrum*, caudal spine of
  *Phacus*) contain cytoplasm, so they are part of the cell. The silhouette (`bio/silhouette`) joins
  body and processes with a fillet and is the shape of the membrane ring: they collide, grow and
  divide with the cell, and the wall's shape memory keeps them rigid. Mass still comes from the
  body's spheroid.
- **Notches** (`notch`, always of the body: the sinus of *Cosmarium*, the lobe incisions of
  *Micrasterias*, the oral side of *Colpoda*, the oral groove of *Paramecium*) are subtracted from
  the silhouette with a fillet. `length` is the depth, `base_width` the opening, `tip_width` the
  bottom.
- **Covering processes** (`body = false`: spines of the *Trachelomonas* lorica, the *Coleps* plates,
  the *Euglypha* test) have no cytoplasm and are thinner than the membrane resolution (3 µm); they
  are drawn on the covering and are not part of the soft body.

## Behaviour

### `[[being.response]]` (up to 4)

Transient phobic response ([behavioural responses](../mechanics/behavioural-responses.md)).

| Key | Meaning |
|---|---|
| `field` | `light` \| `oxygen` \| `shear` (water deformation [1/s]) \| `touch` (the anterior pole collided: 1). |
| `kind` | `step_up` \| `step_down` (relative change larger than `threshold` compared with what it remembers of the last `memory` s, with the level above `min_level`) \| `above` (the level exceeds `threshold`). |
| `action` | `avoid` (avoiding or photophobic reaction) \| `jump` \| `contract` \| `flash`. |
| `refractory` [s] | Time before it can respond again. |

### Other behaviour tables

| Table | Keys | Meaning |
|---|---|---|
| `[being.jump]` | `speed` [µm/s], `duration` [s], `interval` [s] | Jumps; `interval` is the mean between spontaneous jumps (0: escape only, with a `jump` response or a threat within 40 µm). |
| `[being.bioluminescence]` | `flash_s`, `recharge_s` [s], `circadian`, `day_capacity`, `color` | Flash on a `flash` response; with `circadian`, a 24-biological-hour clock: by day it shines `day_capacity` of the night level. |
| `[being.contraction]` | `extension`, `extension_s`, `profile`, `flare`, `foot_width`, `swimming_extension`, `swimming_squareness`, `aboral_first`, `spontaneous_per_min` | Whole-body contraction (myonemes). The shape in `[being.shape]` is the contracted one; extended it is `extension` times longer and re-extends in `extension_s`. `profile`: `uniform`, `trumpet` (oral disc, cone and thin peduncle ending in the foot) or `bell` (peritrich zooid). Triggered by a `contract` response; its eddy enters the water ([contraction](../mechanics/shape-changes.md)). |
| `[being.shape_wave]` | `kind`, `amplitude`, `period_s`, `wavelength`, `episode_s`, `episodes_per_min`, `reach` | The rest shape changes in time without changing area. `metaboly`: a widening wave runs from the anterior to the posterior pole, in episodes during which it does not swim. `sway`: the anterior fraction `reach` bends from side to side. Not combined with `[being.contraction]`. |
| `[being.somersault]` | `interval_s`, `duration_s` | Somersaults (*Closterium*): fixes one tip, stands on it and falls to the other side, alternating tips. |
| `[being.semicell]` | `regrowth_hours` | Desmids (with silhouette): hours for the new semicell to sprout from the isthmus and take its shape after division. |
| `[being.contractile_vacuole]` | `period_s`, `cytosol_mosm` | Contractile vacuole cycle: period in fresh water (~16 mOsm); beats faster in a more dilute medium and stops when the medium is as concentrated as the cytosol. |

### Stalks and colonies

| Table | Keys | Meaning |
|---|---|---|
| `[being.stalk]` | `kind` (`spasmoneme` \| `rigid` \| `mucilage` \| `foot`), `length`, `width` [µm], `bending_rate` [1/s], `growth_hours`, `decay_hours`, `contracted_fraction`, `extension_s`, `spontaneous_per_min`, `detaches`, `colony`, `max_zooids`, `branch_angle` [°], `shared_spasmoneme` | Foot or stalk of a sessile being (makes it sessile). `detaches`: when hungry the zooid leaves and the empty stalk remains. `colony`: the daughter stays on a new branch of the mother's stalk. `shared_spasmoneme`: the whole colony contracts together. The old keys `sessile`, `stalk_length` and `spasmoneme` are still read ([stalks](../mechanics/stalks-and-colonies.md)). |
| `[being.star_colony]` | `max_cells` | Star colony (*Asterionella*): cells joined by the posterior pole at equal angles. |
| `[being.sliding_colony]` | `max_cells`, `amplitude`, `period_s` | Sliding colony (*Bacillaria*): fraction of the length each cell slides relative to its neighbour and period of the stretch–close cycle. |
| `[being.algal_conjugation]` | `reach` [µm], `tube_hours`, `transfer_hours`, `zygote_hours` | Scalariform conjugation (*Spirogyra*): with nitrogen limiting, cells of filaments of different types side by side pair up; the receptor becomes a dormant zygospore and the donor an empty wall. |

### Predation and defence

| Table | Keys | Meaning |
|---|---|---|
| `[being.toxicysts]` | `reach` [µm], `paralysis_hours`, `lethal` | Toxicysts from the anterior pole (proboscis). What they touch is paralysed (does not swim, flee or discharge trichocysts) and, if lethal, dies when the paralysis ends. |
| `[being.morph]` | `name`, `signal`, `threshold` [µM], `hours`, `width_scale`, `length_scale`, `area_scale`, `feeding`, `prey`, `mouth_width` | Morph induced by a signal: gradual transition in both directions; once complete, its own feeding, prey and mouth width ([signals](../mechanics/chemical-signals.md)). |
| `[being.signal_toxicity]` | `signal`, `ec50` [µM] | Sensitivity to another being's toxic signal (R bodies of a killer); kills above the reference dose. |

### Chemical signals

| Table | Keys | Meaning |
|---|---|---|
| `[being.release]` | `signal` (id), `rate` [pmol/biological h], `diffusion` [µm²/s], `half_life_hours`, `bio_time` | Signal released by each cell. With `bio_time` it diffuses with D × bioTimeScale (a fast biological process, such as cAMP relay, so the wave has its real length). |
| `[being.relay]` | `threshold` [µM], `pulse` [pmol], `refractory_minutes`, `pacemaker_fraction`, `pacemaker_minutes`, `competence_hours` | Signal relay (*Dictyostelium*), with competence acquired by starvation. |
| `[being.chemotaxis]` | `signal` | Signal it follows (climbs the gradient while the signal rises). One field per signal (up to 4), diffusing only after someone released it. |

### Light and symbionts

| Table | Keys | Meaning |
|---|---|---|
| `[being.light_damage]` | `threshold`, `stress` | Photodynamic damage: above `threshold` (relative irradiance) it is paid as stress, `stress` with the lamp at maximum; needs O₂. |
| `[being.stress_pigment]` | `rate_per_hour`, `light_threshold`, `decay_per_hour`, `protection`, `color` | Stress pigment (astaxanthin): accumulates with strong light or without N (twice as fast in the cyst) and protects from light damage. |
| `[being.photosymbiont]` | `kind` (`endosymbionts` \| `kleptoplasts`), `source`, `per_prey`, `max`, `initial`, `growth_per_hour`, `decay_per_hour`, `photosynthesis` | Endosymbiotic algae or kleptoplasts taken from prey; shared at division ([symbioses](../mechanics/symbioses.md)). |

## Cells of multicellular organisms

| Table | Keys | Meaning |
|---|---|---|
| `[being.cell]` | `adherent`, `rounded_radius` [µm], `contact_inhibition_locomotion`, `contact_inhibition_proliferation`, `min_serum` [fraction], `apoptosis_without_serum_h`, `divides`, `fmlp_chemotaxis`, `death` (`apoptosis` default \| `immotile`) | Mechanisms of a cell of a multicellular organism ([animal cells](../mechanics/animal-cells.md)). `immotile`: the sperm cell dies without falling apart and stays whole and still. |
| `[being.swim_path]` | `curvature_radius` [µm], `rotational_diffusion` [rad²/s], `noise_s` [s], `rheotaxis_alignment`, `rheotaxis_chirality` | Passive path of a flagellated swimmer that does not choose its heading (sperm: no senses, no avoiding reaction, no autopilot). Positive rheotaxis of Kantsler et al. (2014). |
| `[being.motility]` | `progressive`, `non_progressive`, `immotile`, `dead`, `speed_cv`, `non_progressive_speed`, `non_progressive_radius` [µm] | A sample of swimmers by WHO classes (they add up to 1). With `[physiology.<id>.motility_half_life]` swimmers become immotile before dying. |
| `[being.osmotic_swelling]` | `isotonic`, `full_osmolarity` [mOsm/L], `inactive`, `seconds` | The tail swells and coils in a hypotonic medium if the membrane is intact (hypo-osmotic test). |
| `[being.capacitation]` | `min_bicarbonate` [µM], `min_albumin`, `min_temperature` [°C], `hyperactivated_fraction`, `hyper_lateral_amplitude` [µm], `hyper_beat_scale`, `hyper_curvature_radius` [µm], `hyper_rotational_diffusion`, `spontaneous_reaction_per_hour`, `inducer`, `induced_reaction_per_hour`, `inducer_ec50` [µg/mL], `chemotaxis_threshold` [µg/mL] | Capacitation (hours in `[physiology.<id>.capacitation_time]`), hyperactivation and acrosome reaction ([sperm and CASA](../mechanics/sperm-and-casa.md)). |
| `[[being.morphotype]]` | `id`, `name`, `fraction`, `length`, `width`, `taper`, `neck_bend` [°], `coiled_tail`, `droplet`, `acrosome`, `speed`, `normal` | Morphology of a population by strict criteria (one table per form; fractions add up to 1). |

## Measured physiology: `[physiology.<id>.<parameter>]`

In the being's file. Each value as published:

| Key | Meaning |
|---|---|
| `value` | The number. |
| `unit` | Its unit (see below). |
| `temperature` [°C] | Temperature of the measurement. |
| `source` | Bibliographic reference. |
| `estimate` | `true`: not a direct measurement in comparable conditions; the inspector says so. |
| `q10` | Measured Q10 (mandatory for the swimming parameters). |

The program brings each value to 20 °C with Q10 = 2. Beings without data use the general metabolism
value (`metabolism.max_growth`).

| Parameter | Units |
|---|---|
| `max_growth` | `"1/h"` or `"1/day"` (rate), `"h"` or `"day"` (doubling time) |
| `max_ingestion` | `"bacteria/h"` or `"bacteria/min"` |
| `clearance` | `"nL/h"`, `"uL/h"` or `"mL/h"` |
| `p_half_saturation`, `si_half_saturation` | `"µM"` |
| `migration_speed` (animal cells) | `"um/h"`, `"um/min"` |
| `persistence`, `phase_g1`, `phase_s`, `phase_g2`, `phase_m`, `lifespan`, `capacitation_time`, `motility_half_life` | `"h"`, `"min"`, `"day"` |
| `confluence` | `"1/cm2"` |
| `glucose_vmax`, `glutamine_vmax`, `o2_uptake` | `"fmol/h"`, `"amol/s"`, `"pmol/h"` |
| `glucose_km`, `glutamine_km` | `"uM"`, `"mM"` |
| `lactate_per_glucose` | `"mol/mol"` |
| `swim_speed`, `beat_frequency`, `lateral_amplitude` (sperm) | `"um/s"`, `"Hz"`, `"um"`, each with its `q10`: not brought to 20 °C but to the medium's temperature from the measurement's. With `swim_speed`, `relative_speed` and viscosity are not used (the measured Q10 already includes it); with `beat_frequency`, the helix period is its inverse. |

## Example

```toml
[[being]]
id = "pinnularia"
name = "Pinnularia"
tags = ["alga", "diatom", "phototroph", "chytrid_host"]
description = "Large (~120 µm) rod-shaped diatom with a striated frustule..."
locomotion = "cilia"
body_plan = "diatom"
relative_speed = 0.012
chloroplasts = true
raphe_glider = true
frustule = true
ph = [5.0, 9.0]
temperature = [0.0, 30.0]
storage = "chrysolaminarin"
wall = "silica"
genome = { chloroplasts = 1.0, trichocysts = 0.0, armor = 0.3 }

[being.shape]
semi_major_axis = 60.0
semi_minor_axis = 11.0
shape_memory = 40.0
color = [202, 184, 122]
nucleus = 0.3

[being.appearance]
style = "frustule"

# Illustrative only: Pinnularia has no published µmax in the pack yet.
[physiology.pinnularia.max_growth]
value = 0.5
unit = "1/day"
temperature = 20.0
source = "Author Year, Journal volume: page"
estimate = true
```
