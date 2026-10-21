# AGENTS.md — Open Microcosm

Guide for agents (and humans) working on this repository. Read it entirely before touching code.

## Vision

Open Microcosm is an **open-source research and teaching tool** that simulates **real** microscopic life as
it is seen through a laboratory microscope: drops of water with their microorganisms, **cell
cultures** (human and animal cells on a dish) and **plant preparations** (tissues with cell walls).
Cells are gelatinous, semi-transparent bodies with visible organelles. They swim or crawl, eat, grow,
divide, conjugate, die and lyse, all emerging from a physical particle simulation rather than from
animations.

Two uses share the same simulation core:
- **Research:** complex, reproducible and measurable simulations (series, budgets, export with
  provenance, replicates), also **headless** (`biosim`, `microcosm_bench`; in the future, Python
  bindings). Graphics are optional: nothing in the simulation depends on the render.
- **Teaching:** show students what happens, with the microscope image, cards that explain what each
  thing is and why, and guided lessons.

Reference documents (all in [`docs/`](docs/README.md), in English):
- [`docs/roadmap.md`](docs/roadmap.md): phases, "done" criteria and decisions taken.
- [`docs/mechanics/`](docs/mechanics/README.md): one document per mechanism of the simulator.
- [`docs/content/`](docs/content/README.md): the TOML format of each content type, and
  [`docs/content/beings/`](docs/content/beings/README.md), one document per being.
- [`docs/validation/`](docs/validation/README.md): comparison against published results.
- [`docs/bugs/`](docs/bugs/README.md): one file per known bug or model limitation.
- [`docs/design/`](docs/design/principles.md): design principles, the idea backlog and the
  candidate beings and experiments (what they reuse from the engine and what they lack).

## Sandbox

There is no game run, score or victory: it is a laboratory. The user prepares a plate, observes it
and intervenes.

- **Plate:** the program starts directly in the laboratory (there is no start screen) with an
  **empty Petri dish**. It is preconfigured in the *Plate* panel (`PlateSpec`: biome + seed, full,
  lifeless or empty content —`DropContent`, `applyDropContent`—, environment, nutrients, sediment,
  populations and geometry) and created with *Create plate* (Ctrl+N). Environment and nutrients are
  applied **live** to the running plate; the rest, when it is created. *Thinning (live)* takes out and
  discards a fraction of what is in suspension, once (also key T) or periodically (`World::thin`,
  intervention `Thin`; in the culture, the `dilution` operation). Plates are saved in
  `<data>/openmicrocosm/plates/*.plate`.
- **Prepared plates** (`game/prepared`; data in `media/`, `vessels/` and `plates/`): besides the
  water drop, plate types as in a real laboratory: the **cell culture** (DMEM in an incubator at
  37 °C and 10 % CO₂: a circular field of the bottom of a vessel, with that vessel's medium depth,
  sterile) and **slides** (`PlateKind::Slide`, see below). **Mixtures:** any being in the catalogue
  can be inoculated into any plate (Catalogue > Inoculate..., intervention `Inoculate`,
  reproducible).
- **Dockable panels** (Dear ImGui, see "Interface"): Plate, Catalogue (any being, with icons),
  Inspector, Tools, Simulation, Preferences (image, smoothness and performance), Processes (rates of
  chemistry, microbiology and fungi), Statistics (census, conserved totals of P, N, Si and Fe, and the
  controlled being), Graphs, Measurement, Content (loaded packs, data warnings,
  `.biopack` import and export), Cultures, Experiment, Lesson, Glossary, Notes, CASA and Help. Plate,
  Catalogue, Stains, Inspector, Statistics, Tools, Simulation and Measurement start open; the rest are
  opened from *View* (a panel closed by default sets `open = false` in its constructor).
- **Tools** (left toolbar or keys 1-0, M and G): Select, Control, Ink, Detritus, Erase, Lyse,
  Squeeze, Stir, Stretch, Dropper, Measure, Region and Spawn (when arming a being from the catalogue).
- **Controlling a being** (Control tool, or `P` over a cell): each species keeps its **physical
  locomotion**: flagellum (impulse), cilia (gliding) and pseudopod (click and drag). The **low
  Reynolds number** is respected: there is no inertia. On division the camera follows the mother; if
  the being dies, another one can be controlled.
- **Select** (default tool, no modes): a click on anything picks the being and opens its card (what
  it is, what it does and why).
- **Eating is physical:** the prey is wrapped with the membrane until a vacuole closes.
- **Real metabolism:** respiration with O₂, fermentation without O₂ and photosynthesis with light,
  limited by P, N and, depending on the species, **silica** (diatoms), **iron** (phototrophs) and
  **vitamin B₁₂** (auxotrophs: *Euglena*, diatoms), which bacteria supply.
- **Genome:** organelles and traits are genes that mutate in mitosis and mix in conjugation.
  Organelles take up physical space inside the cell.
- **Drop ecosystem:** carnivorous protists (hunters, engulfers, ambushers and parasites), herbivores
  (algivores and bacterivores), algae, fungi, bacteria and phages.
- **Human cells in culture** (`bio/animal_cell`, `game/world_animal.cpp`): dermal fibroblast, HeLa,
  NIH/3T3, HaCaT and neutrophil (`[being.cell]` in the data, measured values with their source in
  `[physiology.<id>.*]`). Stuck to the bottom (`Adherent`: water does not drag them), they migrate by
  crawling (`LocomotionType::Crawl`) in a persistent random walk (speed and persistence from the
  data), with contact inhibition of locomotion (Abercrombie) and of proliferation; a cycle by phases
  G1/S/G2/M with a restriction point (serum) and G0; rounded mitosis; Michaelis–Menten uptake of
  glucose and glutamine and lactate (Warburg) that consumes bicarbonate; apoptosis (no energy, no
  serum or by age) that leaves apoptotic bodies without lysis; the neutrophil follows the fMLP that
  bacteria release and phagocytoses them. Glutamine, lactate and fMLP are fields that only diffuse
  if there are animal cells.
- **Human spermatozoon** (`human_sperm`): an animal cell that swims (`flagellum = "posterior"`, a
  tail of `flagellum_length` = 50 µm that is drawn and pushes the water along it), lives on fructose
  and when it dies stays whole and still (`[being.cell] death = "immotile"` → `CellPhase::Dead`: no
  metabolism, no propulsion and out of the census). Prepared plate `fresh_semen` (semen analysis).
  Swimming follows temperature with its measured Q10 (`swim_speed`, `beat_frequency`,
  `lateral_amplitude` in `[physiology.*]`), the path is passive (`[being.swim_path]`,
  `World::steerSwimmer`) and the sample comes by WHO classes (`[being.motility]`, configurable in the
  Plate panel). **CASA** (`game/casa`, CASA panel, `microcosm_bench --casa`): VCL, VSL, VAP, LIN, STR, WOB,
  ALH and BCF of each one; it is measurement (it lives in `Run`, it does not change the fingerprint).
  Vitality plates: eosin–nigrosin (`semen_eosin_nigrosin`) and the hypo-osmotic test (`semen_hos`,
  `[being.osmotic_swelling]`); capacitation (`sperm_capacitation`) and leukocytospermia
  (`semen_leukocytes`).
- **Slides** (`bio/osmosis`, `bio/tissue`, `bio/erythrocytes`, `game/world_tissue.cpp`,
  `render/tissue_renderer`): onion epidermis, *Elodea* leaf, potato with Lugol, root tip with
  mitoses, leaf epidermis with stomata, buccal mucosa, Giemsa blood smear, diluted blood and fresh
  semen. A tissue (`[plate.tissue]`) is cellulose walls generated by an arrangement (rows, polygonal,
  files, jigsaw) and a living protoplast (soft body, component `Protoplast`) in each cell, enclosed by
  its wall (`confineProtoplasts`; protoplasts do not collide with each other) with the granules in
  the cortical layer. **Osmosis** (Boyle–van 't Hoff, in real time): the medium's osmolarity comes
  from salt and from `MediumParams::osmolytes` (sucrose); the protoplast plasmolyses in a medium more
  concentrated than its cell sap and recovers in water; red blood cells (particles, thousands per
  field) crenate or swell and haemolyse when they pass their critical volume (Parpart's osmotic
  fragility). Fixed (`fixed`), the preparation does not move and the smear flattens the leukocytes
  against the glass (`World::flattenSmear`). Cells that do not move on their own use
  `LocomotionType::None` (`locomotion = "none"`).
- **Cells of multicellular organisms:** human and animal cells in culture (real lines and types:
  fibroblasts, keratinocytes, neutrophils…) and plant tissues, with the biology of each (adhesion to
  the substrate, migration, contact inhibition, apoptosis, cell wall). Simple cells and tissues are
  simulated, not whole organisms: complete multicellular animals (rotifers, nematodes) only when
  their simulation is realistic.
- **Variety:** procedural drops by seed and different biomes (puddle, pond, moss, termite gut,
  hydrothermal spring, ice); prepared plates of each type.
- **Visual style:** **bright-field** microscope (light background, semi-transparent cells). Cell
  cultures are really looked at with **phase contrast** (pending lens): until then, bright field.

## Guiding principle: strict realism

- Only **real** species and mechanisms. Before implementing a behaviour, check that it exists in
  biology; if it is simplified, leave a comment with what the real organism does.
- Human and animal cells are **real cell types or lines** with published data (doubling time,
  migration speed, size), with their source in `[physiology.<id>.*]` (in the being's file) like the
  rest of the physiology. No "generic" cells without a reference.
- **Time:** physics (swimming, water, diffusion, gas exchange) runs in REAL time
  (`units::kTimeScale = 1`). Biology (metabolism, growth, digestion, division) is compressed with
  **a single constant**, `units::kBioTimeScale` (60: 1 real min = 1 biological h), respecting the
  proportions between processes and species. Consequences that must be kept:
  - what is digested yields ×`bioTimeScale` (feeding is mechanical, in real time);
  - each cell's O₂ and CO₂ exchange with the water is done at the real rate (÷`bioTimeScale`, not
    ÷`bioScale`: Q10 does accelerate respiration);
  - the bacterial ingestion of a species with measured data (`max_ingestion`) is that, on the plate
    and in the culture;
  - exception: the **crawling migration** of animal cells (lamellipodium, focal adhesions) is
    biology —actin polymerisation— and runs in biological time (a fibroblast at 40 µm/h is seen at
    ~0.7 µm/s with kBioTimeScale 60). Swimming is still physics, in real time.
- **Masses:** pmol of glucose equivalents of DRY matter (`units::kDryMassFraction` = 0.3).
- Internal units: **micrometres (µm)** and **simulation seconds**. Conversions to pixels only in the
  render.
- There is no inertia at cell scale: velocity depends on the current propulsion force, plus the
  fluid's drag.

## Libraries

Use **only** these. To add another one, propose it first and wait for approval. The project is
**GPL-3.0-or-later** (`LICENSE`): a new library or asset must have a licence compatible with GPLv3
(MIT, BSD, zlib, Apache 2.0 yes; GPLv2-only or non-commercial, no).

| Lib | Version | Use |
|---|---|---|
| raylib | 5.5 | Window, input, 2D render, GLSL 330 shaders and audio |
| Dear ImGui | 1.91.9b (docking branch) | Laboratory UI: dockable panels, menus, tools. Own raylib backend (`ui/imgui_raylib`), no rlImGui |
| ImPlot | 1.0 | Live plots of the Graphs panel (compiled inside `imgui_lib`) |
| EnTT | 3.14 | ECS for organisms and high-level entities |
| OpenMP | system | Parallelism of the simulation loops |
| Tracy | 0.11 | Profiling (`profile` preset) |
| doctest | 2.4.11 | Tests of physical invariants (`tests/`, target `microcosm_tests`) |
| toml++ | 3.4.0 | Content as data, with its sources (`assets/content/`, [`docs/content/`](docs/content/README.md)) |

Approvable candidate when its phase requires it: **FastNoiseLite** (procedural generation).

Every dependency is fetched with `FetchContent` at a fixed version in `CMakeLists.txt`. No system
packages are used, except OpenMP. Repository tools in `tools/` are Python 3 scripts using only the
standard library.

## Build

```sh
cmake --preset release && cmake --build --preset release && ./build/release/microcosm
```

There are `debug`, `release`, `profile` (Tracy) and `lite` (per-scope timings without Tracy)
presets. Code must compile **without warnings** with `-Wall -Wextra -Wpedantic`, and
`ctest --test-dir build/release` must pass (`ctest -L rapidos`: ~2 min; `ctest -L largos -j8`:
~16 min; `validacion`: against published results, ~28 min with `-j4`, almost all of it the Tilman
case on the plate; measured on 17/10/2026 with 16 logical threads). Long and validation suites run
one ctest process per case (4 threads each): a case that repeats something independent several times
(prey, species, cell types) goes in separate cases, so they run in parallel.

**Distribution presets:** `dist` (Linux, without `-march=native`, base x86-64-v2) and `windows`
(cross-compiled with MinGW, `cmake/mingw-w64.cmake`); `cpack` in its folder builds the package.
Assets are looked for next to the executable (`core/paths`) and, in development, in the source tree;
user data in the platform's folder (`userDataDir`). `tools/ci.sh` is the local CI (`--todo`,
`--paquetes`); `ctest -L rapidos` is the quick check. macOS is not tested (community supported, see
the README).

**Measuring:** `microcosm_bench` runs a fixed drop headless and gives ms/step, the census and a checksum
of the state (`--casa`: sperm kinematics; `--wildlife 3` ≈ 700 cells, `--gpu` uses the GPU solver
like the app; with the `lite` preset it breaks down by `PROFILE_SCOPE`; `--shot` saves the drawn
frame, `--stain <id>` draws it with a factory stain; `--timelapse <bio_min>` saves a photo every so many biological minutes; by default it uses one
thread per physical core, like the app: with SMT, 16 threads are slower than 8). `BIO_GPU_TIMING=1`
gives the time of each GPU kernel with timestamps. An optimisation that must not change results is
verified by comparing checksums of the old and new versions built with `-DBIO_STRICT_FP=ON`: with
`-ffast-math` any code change may reassociate operations and alter the last bits, and the dynamics
amplify it. Prefer truthfulness to speed: an optimisation that changes the physical result is not
accepted.

## Architecture

The detail of each mechanism is in [`docs/mechanics/`](docs/mechanics/README.md); this section is the
summary every change must respect.

### World

- **Top view** (looked at from above, as in the microscope).
- **Dish** (`sim/dish.hpp`, `World::dish()`): the world is a circular glass well centred on the grid
  (radius = half the shorter side − `WorldConfig::dishMargin`); its wall is the limit of the world.
  The grids are still periodic inside (power-of-two sizes, the FFT needs them), but nothing lives or
  crosses outside the circle:
  - soft bodies: `soft_body::confineToDish` in each XPBD iteration (also on the GPU, `solve.comp`);
    statistical ones are moved whole (`pushIntoDish`);
  - particles (bacteria, detritus, spores, *Bdellovibrio*, cyanobacteria): `Dish::bounce`; hyphae do
    not grow towards the glass; spawns use `World::randomPoint`;
  - water: velocity mask (`DishMask::flow`), zero in the glass and a ramp next to the wall;
  - chemistry: glass cells are ghosts with the value of their mirror image (no flux by images) and
    mass is conserved by rescaling the water (`ChemField::setMask`). Totals use `ChemField::sum()`
    (water only), never `grid().sum()`.
  - With `WorldConfig::dish = false` the world becomes toroidal again (wrapped edges).
- **Water:** Stokes–Brinkman solved exactly with FFT (`sim/fluid`). There is no inertia: velocity is
  the instantaneous response to forces. The film thickness (`gapHeight`) controls how local the
  currents are.
- **Chemistry:** exact diffusion in Fourier + semi-Lagrangian advection (`sim/chem_field`). O₂ enters
  by exchange with the air over the whole surface; CO₂ equilibrates with that of the air
  (`MediumParams::co2Atmosphere`: the incubator sets 5–10 %) by Henry's law at the medium's
  temperature and salinity, and pH comes from bicarbonate with the medium's pK′ (`carbonatePK`).
  Macronutrients (P, N, Si) and micronutrients (Fe, B₁₂) are dissolved fields; each cell takes its
  share with Monod kinetics (`bio/metabolism`) and growth is limited by the scarcest one (Liebig). The
  Si of a dead diatom stays in its frustule (`AmbientParticles::mass`) and redissolves slowly.
- **Light:** the lamp illuminates uniformly and objects attenuate it (Beer–Lambert).

### Simulation layers (tiers by scale)

| Tier | What | Representation |
|---|---|---|
| Medium | Water, currents and chemistry | Stokes–Brinkman fluid grid (FFT) + scalar fields (O₂, CO₂/pH, light, organic matter, phosphate, nitrogen, toxins, autoinducer, phages) |
| Environmental | Detritus, bacteria, cyanobacteria, fungi | Simple particles (bacteria, *Bdellovibrio*, zoospores), chains (cyanobacteria) and trees of fixed nodes (hyphae) |
| Cell | Protists, algae and yeasts | Soft body: membrane ring + interior particles, solved with XPBD |
| Compound | Colonies and filaments | Soft bodies joined by adhesions (`World::AdhesionLink`) |

LOD (`World::setFocus`, three levels): near the view, full detail; farther, 1 of every `lodStride`
XPBD sub-steps with a longer step (rates [1/s] keep their meaning); off screen, statistical (moved as
solids, without XPBD or contacts). Division, phagocytosis, conjugation and held prey are always in
detail. Without a focus (tests), everything detailed. Contacts: only membrane points within their
contact radius (+ slack) of another body, measured once per sub-step; two passes per sub-step
(`SoftBodyTuning::contactPasses`).

### Soft bodies (`sim/soft_body`, `sim/contacts`)

- **Inertia-free XPBD:** each step predicts x* = x + v_external·dt, with no accumulated velocity,
  and then projects the constraints. Each constraint relaxes its error with a **rate [1/s]**:
  correction = 1 − exp(−rate·h). So the result does not depend on the sub-step and the behaviour is
  viscoelastic.
- **Constraints per body:**
  - edge length;
  - bending by local *shape matching* in 5-point windows (the rotation never reflects, so it undoes
    folds; an i↔i+2 distance constraint does not);
  - area (incompressible water);
  - global shape memory (`shapeRate`: rigid pellicle high, amoeboid cortex ~0);
  - anchoring of organelles to the cytoskeleton.
- **Contacts:** point against edge, with a Jacobi scheme. They are **interleaved** with the
  constraints in each iteration of the sub-step. At the end of the step, a pass removes any membrane
  particle left inside another cell. Only the plastic membrane collides with itself (it folds when
  making pseudopods): a wall, theca or pellicle does not fold, and the two sides of a thin horn are
  closer than two particle radii.
- Bacteria and detritus are excluded from the interior of cells.
- **Plastic membrane** (amoeba, `soft_body::makePlastic`): no edge tension or shape memory; cortical
  tension (curvature flow) that smooths wrinkles without stopping lobes; area conserved in ~1 s (the
  cytoplasm flows); remeshing that adds or removes membrane. Its membrane has more inverse mass, so it
  yields in contacts.
- **Large shape changes** (an animal cell going from round to spindle, or rounding for mitosis):
  `reshapeRestEven` distributes the rest shape by arc length. The radial projection of `reshapeRest`
  bunches up points and leaves the shape crossed; with window bending that makes the cell push itself.
- **Rest shape that changes in life** (`game/rest_shape`): contraction (`[being.contraction]`:
  *Stentor*'s trumpet, the peritrichs' bell) and the shape wave (`[being.shape_wave]`: euglenoid
  metaboly, the proboscis sway) write the rest shape point by point with `rest_shape::apply`, which
  centres it on the mean of its points (otherwise the shape memory pushes the whole body) and carries
  the organelles.
- **More shapes that change in life:** the new semicell of desmids (`world_semicell`), the neck of
  *Lacrymaria* in the membrane (`world_neck`) and the somersaults of *Closterium*
  (`world_somersault`) use `rest_shape::apply`; the rest area follows the shape with
  `World::restShapeAreaScale`.
- **Topology:** `replaceRing` / `resizeRing` change the ring (fusion, membrane insertion). Topology
  events are applied at the end of the step.

### Life cycle (`bio/metabolism`, `bio/division`, `World::updateLifecycle`)

- **Metabolism** (component `Metabolism`): biomass, reserves and age. Respiration (~30 ATP per
  glucose) or fermentation (2 ATP, 15× more glucose) depending on local O₂; maintenance proportional
  to biomass and activity; growth with yield 0.6 and a maximum rate; photosynthesis with local light.
  Without reserves there is autophagy; below half the birth biomass, it dies.
- **A single physiology** (`bio/cell_physiology`, `bacterialDemand`/`bacterialMassChange`,
  `digestionFractions`/`digestVacuole`): each cell's input (µmax from the data, traits, pH,
  temperature, salt and toxin stress, cyst), bacterial growth and vacuole kinetics are body-less
  functions used by the plate and the well-mixed culture (phase P1). The plate only supplies where
  the environment comes from and where exchanges go: do not duplicate that biology in another system.
- **Size:** the rest area is that of the biomass plus the content of the vacuoles. Mass is that of
  the **spheroid** with major, minor and vertical semi-axes (`thickness` in `[being.shape]`; by
  default, the minor one) times the relative dry matter (`cellMassScale`), not that of the sphere of
  the 2D silhouette.
- **Cell-cycle checkpoint:** at division size the cell stops investing in biomass and gathers the
  reserves for cytokinesis (otherwise a cell with little food never divides).
- **Mitosis** (component `Dividing`): with biomass ×2 and reserves for the cost (the controlled
  being's is the most expensive), nucleus and contractile vacuole duplicate, the rest shape
  transforms from the mother into two daughters (`divisionRestShape`), the furrow constricts and,
  when it closes, the ring splits. Daughters are resampled (`resampleRing`) and return to the shape
  of their species (`CellShape`).
- **Lysis:** ~40 % of the content dissolves into nutrients and the rest stays as detritus.

### Genome (`bio/genome`, `bio/organelles`)

- Component `Heredity` = `Genome` (quantitative genes, relative to the species) + `Traits`
  (physiological multipliers derived with `traitsOf`). `World::applyTraits` writes them into
  `Locomotion::power`, `Digestion::rateScale`, `Metabolism` and the shape (`CellShape`).
- `geneAllowed` imposes biology: chloroplasts only in beings that have them, trichocysts only in
  ciliates, armour only in flagellates (lorica) and amoebae (test). Without endosymbiosis, an absent
  gene does not appear by mutation.
- Mitosis: each daughter mutates its own copy. Conjugation (`Conjugating`): requires different mating
  types; both exconjugants end with the same recombined genotype.
- Organelles are interior particles with `BodyParticles::tag`; they take up space (`Traits::volume`
  enlarges the cell) and cost maintenance.
- Defences: armour ≥ 0.5 prevents phagocytosis; trichocysts can reject a cup (component
  `Discharge`, the predator backs off).

### Ecosystem (`bio/species`, `ai/autopilot`, `World::perceive`)

- Species are data (`SpeciesInfo`): locomotion, shape, diet (`preyMask`, `mouthPreyMask`), flight
  (`fleeMask`), feeding mode (`FeedingMode`), reproduction, cysts and sessile traits (`sessile`,
  `stalkLength`, `contractileStalk`, `tentacleReach`, `neckReach`, `walker`). No per-species code
  except in the render.
- Sessile (`Anchored`): the stalk holds the posterior pole; the spasmoneme contracts on disturbance.
  Their daughters swim as telotrochs (`Telotroch`) and then attach.
- Suctorians: `Captured` holds the prey and drains its biomass in biological time.
- All filter feeders (`FeedingMode::OralGroove`) generate their oral current in the water; motile
  cells only feel the background flow.
- Ciliates: avoiding reaction to threats and collisions of the anterior pole (mechanoreception).
  *Paramecium* also gathers where there is CO₂ (`acidTrap`, Jennings): swarms emerge.
- Predation: `handlingHours` (handling time between whole prey, `Digestion::handling`) and satiety by
  ingested volume (`satiated`, `World::isSatiated`).
- **Processes** (`ProcessSpec`, `[[being.process]]`), as in reality:
  - **of the body** (horns of *Ceratium*, arms of *Staurastrum*, caudal spine of *Phacus*): the
    species' silhouette (`bio/silhouette`: smooth union by distance fields, contour by marching
    squares) is the shape of the membrane ring (`createCell`, `reshapeRestToOutline`,
    `divisionRestShapeOutline`). It is centred on the mean of its points, which is the centre of
    `soft_body::bestFit` (not the area centroid). Mass comes from the body
    (`SpeciesInfo::outlineAreaFactor`), the division furrow and `smallPrey` measure the body, and the
    mouth and flagellum go at its anterior pole (`placeMouth`);
  - **of the covering** (spines of loricas, plates and tests): thinner than the membrane, they are
    only drawn (`forEachProcess` in the render) and the card names them.
- Algae: filaments (`filament`: grow lengthwise with `setRestAreaAxial` and divide by septa),
  colonies (`colonyDaughters`), diatoms (`glider`, `frustule`), `dryMassScale` for hollow or
  vacuolated bodies. Phototrophs count P and N (`Metabolism::phosphorus/nitrogen`), conserved:
  `World::totalPhosphorus()`; `totalNitrogen()` only changes by N₂ fixation.
- Cyanobacteria (`bio/cyanobacteria`): heterocysts that fix N₂ (inhibited by O₂) and release
  ammonium; they die and release microcystin (`toxin_`), harmful to consumers.
- Fungi (`bio/fungi`, `game/world_fungi.cpp`): chytrids (`ChytridInfection` on algae),
  *Saprolegnia* (exoenzymes accelerate decomposition next to the hyphae); yeasts as an osmotrophic
  species with budding (`budding`).
- Chemistry and microbial interactions (`game/world_chem.cpp`): carbonate-system pH with fixed
  alkalinity; pH range per species (`phMin/phMax`); quorum sensing of *C. violaceum*
  (`Bacteria::strain/quorum`, violacein in `Digestion::toxin`); lytic phages as a concentration field
  (`phage_`) with resistant mutants; free *Bdellovibrio* and bdelloplasts.
- Biofilm (`Bacteria::attached`): bacteria stick to the glass when they hit it and come off slowly;
  attached ones do not swim and filter feeders cannot reach them (amoebae can).
- More fungi: `FungusInfo` (data table of `bio/fungi`) defines each type; *Allomyces* (dichotomous
  branching, resting sporangia in `Fungi::resting`) and the lichenising fungus (`Mycobiont`,
  component `Lichenized` on the alga: it gives sugar, the fungus returns phosphorus and protects it
  from chytrids; `World::stepLichens`).
- *Physarum polycephalum* (`bio/plasmodium`, `game/world_plasmodium.cpp`): a slime mould, not a
  fungus. `PlasmodiumSystem` = graph of nodes and tubes with conductivity D; Kirchhoff flow (serial
  conjugate gradient: deterministic) and Tero adaptation; tips that follow the gradient of organic
  matter and avoid slime (`World::slime_`) and light; anastomosis; oat flakes (`OatFlakes`) and
  sclerotia. The phosphorus of the plasmodium, flakes and sclerotia is part of `totalPhosphorus()`.
- `World::deathTally()` and `World::observations()` record causes of death and observed behaviours
  (Statistics panel, tests).

### Well-mixed culture (`game/culture`, phase P1)

- A tube or flask of millilitres (Gause, Tilman) where populations do not fit in a plate. No space or
  soft bodies, but with **the same physiology** as the plate: each cell is an individual
  (`CultureCell`: `Metabolism`, `Heredity`, `Digestion`, `Lineage`); bacteria, cohorts
  (`BacterialCohort`, super-individuals: exact up to `maxBacterialCohorts`).
- Encounters come from physical theory: filter feeders with a type II functional response
  (`max_ingestion` and `clearance` in `[physiology.<id>.*]`; without data, the plate's oral current
  and cytostome) and hunters with the kernel of Gerritsen and Strickler (1977).
- There is no `bioTimeScale`: `step(dt)` advances `dt` seconds and biology `dt · Q10`; gases exchange
  with the air at their real rate: D / depth² with a still medium, k / depth with a mixed one
  (`gas_transfer` [cm/h] of the vessel, overridable and live: `gasExchangeRate`). The budget (`massBudget`, `FluxLedger`) is the same
  as the plate's.
- Light: Beer–Lambert down the column from the phototrophic biomass (each cell receives the mean).
  Mass-action lytic phages with the plate's parameters (infected cells are their own cohorts).
- Operations: centrifuged medium change (`renewMedium`: protists stay), dilution or thinning, chemostat
  (`setDilutionRate`, binomial washout), sample drop and passages (`withdraw`, `receive`); what leaves
  goes to the budget as `Flow::Washout`. Bacteria take the substrate with an implicit step (the
  end-of-step concentration), otherwise the chemostat does not reach S*.
- **Programs** (`game/program`): ramps, cycles, sines and events by laboratory time, saved as a
  definition (`program=` in scenes); applied once per step. On the plate (`WorldConfig::program`,
  `World::applyProgram`) laboratory time is biological hours and live changes are the `Program`
  intervention.
- **Culture session** (`game/culture_run`): like `Run` for the plate: every operation goes through
  `CultureRun::apply`, is recorded with its step and the `.culture` scene replays it. The sample drop
  (`sampleDrop`) builds a v3 `PlateSpec` with the cells and their state (`WorldConfig::sampled`).
- **Batch experiments** (`game/experiment`, `microcosm_batch`, Experiment panel): TOML with replicates and
  factors; one run per thread (OpenMP at 1, preloaded registries); long format, summary with mean, SD
  and 95 % CI and JSON with the provenance.
- What is not yet simulated well mixed is not seeded (`Culture::supports`): animal cells, filaments,
  colonies and zoospores. There is no *Bdellovibrio*, quorum or conjugation either.

### Reagents (`game/reagent`, `game/dose_fit`, phase P1)

- A reagent is data with a named **mechanism** (`pharmacodynamic`: the function of Regoes et al. 2004
  with measured ψmax, ψmin, zMIC and κ; `signal`: a signal whose effect is set by the receiving
  being) and real targets (`bacteria`). The killing it adds scales with how much the bacterium grows
  (`growth_dependent`). The IC50 and MIC **are not input data**: they come out of a dose batch
  (`reactivo:<id>`, `fitDoseResponse`, `minimumInhibitory`); `published_mic` only validates.
- On the plate, a single reagent field (`World::reagentField`) that diffuses only if used; the
  dropper releases it (solute 7, the key says which). In the culture, one concentration per reagent.

### Lessons and protocols (`game/lesson`, Lesson panel, phase P1)

- A lesson is a protocol with steps, as data (`lessons/<id>.toml`, `[[lesson]]` in packs): what is
  prepared (prepared plate, a biome drop or a culture scene), the controls the student sees (registry
  keys), the parameters it fixes (with their reason), named actions (an intervention or a culture
  operation) and steps with a goal, question or prediction.
- Goals are **named conditions** (`time_h`, `population`, `bacteria`, `concentration`, `ph`,
  `plasmolysis`, `hemolysis`, `near_center`, `fraction`, `biovolume_fraction`...): code provides the
  mechanism and the data
  choose which. A new condition is added in `lessonQuantity`.
- No score: questions show their explanation when answered and the report (Markdown) is marked by
  the teacher. A fixed parameter is shown locked (`App::protocolLocks`, `parameterControl`); it can
  be unlocked, it is recorded and `runJson` marks `fuera_de_protocolo`.
- Each lesson carries its **solution** (interventions or operations with their step) and
  `tests/test_lessons.cpp` runs it: if a model change breaks a lesson, the test finds out.

### Glossary and notes (`game/glossary`, `game/notes`, phase P1)

- The glossary is data (`glossary/<id>.toml`, `[[term]]`): definition with a source, `alias` (forms
  in the text, lowercase), related terms and where to see it (`lesson`, `plate`: the term's
  "animation" is the simulation, not a separate drawing). `ui::glossaryChips` shows as buttons the
  terms that appear in a text (the card, lesson steps), in Spanish and in the translation.
- User notes are anchored to a being (`Picked::Kind` + entity or stable id: the same when the scene
  is replayed), a place or an instant. They live in `Run::notes()`, go into the scene (`nota=`) and
  the export, and **are not interventions**: they do not change the world or the fingerprint.

### Environment and biomes (`game/biome`)

- `MediumParams` (in `WorldConfig::medium`): temperature (Q10 = 2 in everything biological via
  `World::bioScale()`, viscosity and O₂ saturation), salinity, anoxia, alkalinity, P, N. Tolerances
  are species data and are paid as extra maintenance (stress).
- `makeDrop(biome, seed)` builds the drop: chemistry, population weights per species
  (`WorldConfig::speciesWeight`), fungi, cyanobacteria (thermal range) and initial patches.
- Biomes and minerals are pack data (`biomes/`, `minerals/`; registries `game/biome` and
  `game/mineral`, like beings): the medium is read with `game/medium_toml` (the same reader as for
  culture media), the sediment is a count per mineral and weights go by being id. An obligate
  symbiont (tag `obligate_symbiont`) only appears where the biome names it. Pack readers use
  `core/toml_reader` (errors with a location, unknown keys warned).
- The card's "what it is" text of each being is its `description`; the environmental organisms
  (bacteria, fungi, phages, *Physarum*) take theirs from `game/organism_text`.

### Render (`render/`)

- Bright field: cell coverage → shader with an optical edge and halo. The dish wall is drawn with
  `dish_glass.fs` (glass, dark edge, Becke line and meniscus) before the post-process. With a dish
  there is a single copy of the world (no repetition when zooming out without a torus). Everything
  happens in one plane (there is no focal plane). The field is drawn into a texture and passes through
  `microscope_post.fs` (vignette, chromatic aberration, grain and, with `Options::opticFlaws`, uneven
  Köhler illumination and dust on the eyepiece and camera: they are in the optics, so they stay fixed
  to the screen, and multiply the light). A photo's grain comes from the simulated time.
- **Photo and time-lapse** (`render/photo`, `game/timelapse`, phase P1): the photo is the central
  field without the interface, with a scale bar and a JSON with the field width in µm (there is no
  "magnification": there are no simulated objectives), the time, the seed, the plate, the stain and
  the provenance. The time-lapse takes a frame per interval of BIOLOGICAL time (`TimelapseClock`),
  never per screen frame; headless, `microcosm_bench --timelapse <bio_min>`. Photos go in
  `<data>/openmicrocosm/photos`.
- **Semantic zoom** (`render/lod.hpp`, `RenderLodSettings`: all configuration in one place): each
  being chooses its level (`DetailLevel`: density, simplified, normal, detail, subcellular) by its real
  projected size in pixels, not by the zoom. Bacteria and *Bdellovibrio*: a linear pass clips by the
  view and splits between density (`DensityGrid` in `game/`, power-of-two cells anchored to the
  world → `PopulationLayer`, Beer–Lambert turbidity) and instances (`InstancedParticles`, only the
  visible ones), with a cross-fade. Up close, the ones nearest the centre (cap `maxDetailed`) and the
  selected one are drawn with structures (`render/structure_renderer`).
- **Body plan** (`bio/body_plan`): which structures each being has and their real size (`Simulated`
  or `Declared`). Each structure appears on its own when its projected size reaches a few pixels;
  what the light microscope does not resolve is only seen in the **model view** (`ViewMode`).
  Declared structures are placed with a hash of the stable id (`Bacteria::id`), never at random.
  `game/scale_path` gives the biological scale and the breadcrumb (Plate > organism > cell >
  structure).

### Interface (`app/`, `ui/`)

- **A single workspace:** `App` (`app/app.cpp`) opens the window, creates the plate and runs the
  loop: UI → input on the plate → simulation step → world drawing → UI drawn on top. There is no
  screen state machine or mutually exclusive modes.
- **Dockspace with a transparent centre:** the free centre (`App::viewport`) is where the plate is
  seen; the camera, the minimum zoom and the LOD use that rectangle. Clicks belong to the world only
  if `!ImGui::GetIO().WantCaptureMouse` (and keys, if not `WantTextInput`).
- **To add a UI feature:** a panel is a class deriving from `ui::Panel` (`ui/panels/*.cpp`) and a
  line in `ui/panels/panels.cpp`; a menu action or shortcut is a `ui::Command` in
  `App::registerCommands`; a tool is a value of `Tool` (`app/tools.*`) and its case in
  `App::toolInput`. Menus and shortcuts come from the same command table.
- **Laboratory widgets** (`ui/widgets`): `slider`, `sliderLog`, `checkbox`, `combo`, `readout`,
  `bar`, `section`. Label on the left, help on hover and a typeable value (Ctrl+click). Every visible
  text goes through `ui::tr(key)` (the key is the Spanish text); each window's ID is stable
  (`title###key`) so the saved layout does not depend on the language.
- **Languages** (phase P1): `assets/lang/en.txt` (English) and, in each content pack, its
  `lang/<code>.txt`. `readout` formats are translated too: the translation keeps the specifiers
  (checked by `tools/claves_tr.py --check`, which runs in `tools/ci.sh`). A new text is added with
  `tools/claves_tr.py --check assets/lang/en.txt --faltantes`; what is built at run time comes out
  with `BIO_TR_FALTANTES=file ./microcosm`. If the Spanish of a key changes, its translation no longer
  matches and it is shown in Spanish (never a stale translation). Series and CSV columns go by id
  (they do not change with the language). The card translates titles, labels, names and
  descriptions; the values `game/inspect` builds with numbers too: `biosim` does not know the UI,
  so the UI installs `ui::tr` with `setLocalizer` (`core/localize`) and `fmt` translates the whole
  format and `L()` the fragments that go through `%s`.
- **Style:** every colour and spacing comes from `ui/theme`. The font carries Latin-1 (accents, ñ,
  µ, °), Greek, punctuation and operators; superscripts and subscripts (O₂, 10⁻⁵) are derived from
  Roboto's digits and the arrow → is drawn (`ui/imgui_raylib`).
- **Accessibility** (phase P1): colours that distinguish categories come from the Okabe–Ito palette
  (`core/palette`: limitation map, plot series, the theme's good/warning/dead); continuous maps, from
  viridis. The UI scale (`Options::uiScale`, 0 = the monitor's) is used by `ui::uiScale()`: a new
  pixel size in the UI is multiplied by it.
- **Options** (`Options` in `game/progress`, `<data>/openmicrocosm/options.txt`): applied by
  `App::applyOptions` (post-process, render scale, detail, VSync, threads, LOD, GPU) and the FPS cap
  by `App::applyFrameCap`; MSAA only when creating the window. Quality presets
  (`applyGraphicsPreset`) touch the image and the steps per frame, never the simulation results. The render scale lives in
  `Renderer`: the scene is drawn smaller with the scaled camera and rescaled at the end.
- The panel layout is saved in `<data>/openmicrocosm/layout.ini` (*View > Reset layout* rebuilds it).
  Catalogue icons are drawn once with raylib into an atlas (`ui/being_icons`).
- `PlateSpec` (`game/plate_spec`, without raylib) is the source of truth for what is preconfigured;
  `parsePlate`/`serializePlate` are versioned key=value text (`plate_version`) and what is missing
  stays as in the biome.

### Measurement (`game/mass_budget`, `game/timeseries`, `game/growth_fit`, `game/region`, `game/export`)

- **Mass budget:** `World::massBudget()` gives the stocks of P, N, Si, Fe and C per compartment
  (`Pool`); `totalPhosphorus()` and the other totals are its sum (a single accounting). Fluxes are
  accumulated by `World::ledger_` (`FluxLedger`): **every exchange with the water goes through
  `World::exchange(field, point, pmol, Flow)`** with the corresponding process (`Uptake`,
  `Excretion`, `Death`, `Spawn`…); `exchangeRaw` does not record. What enters from outside (dropper
  `dose`, `setMeanConcentration`, N₂ fixation, air CO₂) goes to `external`. The closure is
  `budgetResidual` = change of the total − external inputs. C has an open boundary (the air and the
  incoming organic matter are not accounted as closed): closure is not required for it.
- **Time series:** sampled by `Run` (not `World`) every `sampleInterval` [simulation s] together with
  the census: time, populations, chemical means (`Census::mean*`), budget, region and the biovolume
  of each species (`biovolume:<id>`, `cellVolumeUm3`). Named columns
  with units, stored by column; with a row cap it decimates (1 of every 2). NaN marks what is missing
  (region not drawn): with `-ffast-math` use `finiteNumber`, never `std::isfinite`.
- **Growth curve:** `fitGrowth` (µmax by sliding window over ln N, Pirt's lag, logistic K by golden
  section). Deterministic and without raylib.
- **Region:** `quantifyRegion` counts individuals by centroid and averages the water cells with
  their centre inside. The region lives in `Run` (`setRegion`) and enters the series.
- **Export** (Ctrl+E): `seriesCsv`, `runJson` (parameters: the plate with the current environment,
  metabolism, time scale; series; budget) and `speciesJson`, in `<data>/openmicrocosm/exports`. Numbers
  with `std::to_chars` (decimal point regardless of the locale).
- **Limitation map:** `Metabolism::limiting` (Liebig: the factor that left the investment in biomass
  lowest) is only read by the render (`CellRenderer::setLimitationMap`, key L).

### Stains (`game/stain`, `render/cell_renderer`, `ui/panels/stain_panel`)

- **Data, not code per dye:** `StainSpec` = list of rules {target, response, colours, intensity}.
  Targets are real compounds or structures (`StainTarget`: nucleus, nucleoid, starch, glycogen,
  paramylon, acidic compartments with their pH, gram-positive and gram-negative wall, chlorophyll…).
  Responses: `Fixed`, `PhIndicator` (turns with pH) and `GramRetention` (loses the dye with
  decolouriser time: the thin wall in ~3 s, the thick one in ~60 s; over-decolourising turns the
  gram-positive pink). Factory: Gram, Lugol, DAPI (fluorescence, dark background), neutral red,
  methylene blue, Congo red, Giemsa (blood: violet nuclei, pink haemoglobin), aceto-orcein
  (chromosomes) and the exclusion ones, eosin–nigrosin (sperm vitality) and trypan blue (cultures):
  `DamagedMembrane` stains `Dead` cells and those in late apoptosis.
- **Real biochemistry in the simulation:** `speciesChemistry(id)` gives the storage (`Storage`:
  starch, glycogen, paramylon, chrysolaminarin) and the wall (`CellWall`) of each species; only true
  starch (chlorophytes) turns blue-black with Lugol and *Euglena* (paramylon) does not react.
  `Bacteria::strain` 2 is *Bacillus subtilis* (gram-positive); the rest are gram-negative.
- **Visual only:** the render asks `evaluateStain` for each target it draws; it does not touch
  `World` or `Rng`. `vital` = false marks those that in reality fix or kill (Gram, Lugol, DAPI): for
  those the panel offers *Fix the sample* (`World::fix`), which kills everything without lysis and
  stops `step`.
- **Preview:** `ui::StainPreview` draws a fixed test specimen (a small `World`) with the SAME
  `Renderer` as the plate (`Renderer::drawWorldTo` into a texture), unstained and stained, in cell,
  detail and bacteria views. Adding a target: a value of `StainTarget`, its evaluation in the render
  and, if needed, a datum in `SpeciesChemistry`.
- User stains are saved in `<data>/openmicrocosm/stains/*.stain` (versioned key=value text,
  `stain_version`).

### Reproducibility (`game/parameters`, `game/intervention`, `game/scene`)

- **Parameter registry** (`parameters()`): every adjustable parameter of the running plate is in a
  single table (stable key, unit, range, accessor). Panels draw their controls from it
  (`ui/param_controls`) and `runJson` exports it whole. A new parameter is added there, not in a
  panel.
- **Interventions:** **every action that changes the world** (tools, parameters, environment,
  solutes, controlled being, fixing, LOD focus, engine) goes through `App::intervene` /
  `interveneState` → `Run::interventions()`: it is applied between steps and recorded with its step.
  Never call `World` directly from the interface. What is held down (ink, dropper) is a brush that
  `InterventionLog::beforeStep` applies **once per step** (not per frame: the result cannot depend
  on the FPS).
- **Scene** = `.plate` + `event=step type …` lines. Loading it and creating the plate replays the
  run (`InterventionLog::schedule`); the export includes it. Promised level: bit for bit with the
  same binary, machine, engine and options; between machines, statistical (replicates). The
  provenance (version, commit, flags, engine, GPU, CPU, fingerprint of the interventions) goes in
  `runJson`.
- Floats in text files are written with round-trip precision (`max_digits10` or `std::to_chars`):
  with 6 digits, a plate read back is no longer the same.

### Session and control (`game/run`, `game/progress`, `ai/autopilot`)

- `World` knows nothing of session rules: it emits `WorldEvent` (the controlled being divided,
  conjugated or died) and offers `census()`. `Run` counts them and keeps the census; there is no
  victory or defeat.
- Component `Lineage` (session and generation). Daughters that are not controlled get `Autopilot`
  (wander and eat). Movement, phagocytic cups and feeding are generic for every cell with
  `Locomotion`; the controlled being only differs in where its controls come from.
- Feeding of many mouths uses a bacteria grid; what is eaten is marked and removed together at the
  end of the step. Phagocytoses (which destroy entities) are applied after traversing the EnTT view.
- Nothing is unlocked or recorded between sessions: the whole catalogue is available from the
  start. `game/progress` only holds the program `Options`.

### Controlled being (`bio/locomotion`, `bio/feeding`, `bio/phagocytosis`)

- **Locomotion:** adds velocity to the body particles (no inertia), with laboratory speeds: cilia
  ~600 µm/s, flagellum ~80 µm/s with wobble, pseudopod ~6 µm/s.
- **Water:** the swimmer exerts a pair of opposite forces (zero net force). The water is solved twice:
  without the swimmer's forces (`Fluid::backgroundAt`, which is what it feels) and with them (what
  everything else feels). Otherwise the swimmer would be dragged by its own flow.
- **Eating:** the ciliate through the cytostome (with oral current and maximum flow rate), the
  flagellate through the anterior pole (slowly) and the amoeba by contact. Bacteria have a **height
  in the film** (`World::filmHeight`): the mouth only swallows those passing at its height (the view
  is 2D, but the film is not). The bacterial capture radius is calibrated with the measured clearance
  (`microcosm_bench --respuesta-funcional`: the plate and the culture must eat the same). What is ingested
  forms **food vacuoles** (organelles) that close, are digested and are expelled.
- **Phagocytosis of cells** (amoeba, component `PhagocyticCup`): contact with an eligible prey starts
  the cup; the prey is held (ligand–receptor binding); the front of each lip advances and membrane is
  added at the lip; when the lips touch, `findClosure` validates that the outer ring keeps its
  orientation and area, and the prey passes into a phagosome.

### Data

- **Particles in SoA**, in two pools:
  - `sim/particles.hpp` (environmental: ink, detritus): compact, with swap-and-pop.
  - `sim/body_particles.hpp` (membranes, organelles, granules): **stable slots** with a free list,
    so the index serves as an ID in the constraints. Each particle has a type and an owner (cell
    entity). A body's positions are contiguous (not wrapped per particle); the whole body is moved
    when its centroid crosses an edge.
- **Organisms in EnTT:** a cell is an entity with components (membrane: particle range and target
  area; metabolism; genome; species; AI). **Never** put individual particles as ECS entities.
- **Stable handles:** particles move in memory (swap-and-pop, reordering by cell), so references
  between systems use stable IDs with an indirection table, never raw indices kept between frames.
- **Content packs** (`game/content`): the factory one (`assets/content/base/`) and the user's
  (`<data>/openmicrocosm/packs/`, a folder or a single `.biopack`), loaded at start-up; imported and
  exported from the Content panel and `microcosm_bench --paquete` loads them headless. They are data only,
  bounded (8 MB, ids `[a-z0-9_]`); each registry keeps the source TOML of each definition to export
  it unchanged. Plates and scenes name everything by text id and list the packs (`pack=`), and
  `runJson` records their fingerprints.
- **Content as data** (`assets/content/base/`, format in [`docs/content/`](docs/content/README.md)):
  one file per definition in the folder of its type (`beings/paramecium.toml`, with its measured
  physiology; `biomes/`, `minerals/`, `body_plans/`…), with the load order in `pack.toml`. Machine
  names (files, keys, ids, named values) are in **English**; human-readable text (`name`,
  `description`, `source`, comments) is in Spanish today and is translated by `assets/lang`; English
  is accepted in new content and existing text will be migrated over time. A renamed id leaves the old
  one in `legacy_ids` and what was saved with old names is read with `game/legacy_names`. Beings,
  body plans, physiology, minerals and biomes: the parameters of each species (size, speed, diet,
  reproduction, behaviour, appearance) live in data, not in code. The enums (`SpeciesId`,
  `MineralKind`, `BiomeId`) are indices in the registry with aliases for the factory ones; do not add
  cases per species or per mineral in the code: add a mechanism or a named style that the data choose.

### Loop

- **Fixed** timestep (`kFixedDt`), decoupled from rendering. XPBD sub-steps are allowed inside each
  step.
- Order of each step: fluid → chemistry → forces and propulsion → XPBD constraints → collisions →
  metabolism and life cycle → AI → topology events (phagocytosis, division, lysis, fusion) applied at
  the end, never in the middle of a step.
- **Determinism by seed:** all randomness comes from `Rng` (`core/rng.hpp`), computed from (seed,
  stream, index, step), so it does not depend on the thread that runs it. No `rand()` or
  `std::random_device` inside the simulation. Parallel loops only write to their own index. The
  determinism test compares runs with 1 and 4 threads.

### Folder structure

```
src/
  main.cpp          creates the `App` and runs it
  app/              `App` (loop, camera, input on the plate), tools and selection
  ui/               UI framework: ImGui backend, theme, widgets, commands, panels, `tr()`, icons
  ui/panels/        one file per panel
  core/             profiling, units, rng, vec2, paths, TOML reader
  sim/              fft, fluid, chem_field, grid2d, particles, body_particles, soft_body (XPBD),
                    contacts, polygon, spatial_grid
  bio/              cell, metabolism, physiology, division, genome, organelles, species, locomotion,
                    feeding, phagocytosis, bacteria, cyanobacteria, fungi, plasmodium, animal cells,
                    osmosis, tissue, erythrocytes, silhouette, stalk, body plan
  ai/               autopilot (wander, hunt, flee, taxes)
  game/             world, sandbox session, culture, experiments, lessons, content, biomes, measurement
  gpu/              GPU XPBD solver (compute shaders)
  render/           renderer, membrane shaders, microscope post-process, photo
bench/              microcosm_bench and microcosm_batch (headless)
assets/
  content/base/     factory content pack (TOML)
  fonts/            UI font (Roboto, Apache 2.0)
  icon/             program icon (SVG source, PNG, ICO, Windows resource)
  lang/             UI languages (`<code>.txt`, lines key<TAB>translation)
  shaders/          GLSL 330 (and compute shaders)
docs/               documentation (English): roadmap, mechanics, content, beings, validation, bugs,
                    design, img
packaging/          desktop integration (Linux launcher)
tests/              doctest
tools/              CI, translations, content migration, documentation generators and checks
```

`biosim` (everything except `main`, `app/`, `ui/` and `render/`) is a library without raylib: the
program and the tests use it.

`sim/` does not know about `bio/`, and `bio/` does not know about `render/`. The render only reads
simulation state.

## Performance rules

- In hot loops there is no memory allocation per frame: buffers are reused.
- Parallelise with OpenMP only when each iteration writes to its own index. With crossed writes, use
  per-thread buffers and a reduction.
- Every new simulation function gets a `PROFILE_SCOPE`. Measure with Tracy before optimising.
- Goal: **60 FPS** with the content of the current phase on mid-range desktop hardware.

## Code conventions

- C++20. Formatting with `.clang-format` (LLVM, 4 spaces, 100 columns).
- Types in `PascalCase`, functions and variables in `camelCase`, private members with an `_`
  suffix, constants `kPascalCase`.
- Code comments are in Spanish (the current convention of the code base), short, explaining the
  **why** or the real biology behind it. Existing comments are not translated.
- Medium and large features are planned first with the `plan` skill and implemented in roadmap
  order.

## Documentation policy

Documentation is part of the change, not a follow-up. A pull request (or an agent's commit) that
changes behaviour without updating its documentation is incomplete.

- **Language:** all documentation (`README.md`, `AGENTS.md`, `CONTRIBUTING.md`, everything in
  `docs/`) is written in **English**. Code comments and the human-readable text of the TOML files
  follow the rules above.
- **Every important feature is documented** in [`docs/mechanics/`](docs/mechanics/README.md): one
  file per mechanism, with what is simulated, the real biology or physics and its sources, the data
  keys, the code, the tests and the limitations. A new mechanism gets a new file and a line in the
  mechanics index; a changed one updates its file in the same commit.
- **Every being, experiment, lesson or other content item is documented in its folder of `docs/`:**
  - a being: its file in [`docs/content/beings/`](docs/content/beings/README.md), created with
    `tools/gen_being_docs.py` and completed by hand (description, simulation, literature, what is not
    implemented yet, unique and shared mechanics). Never edit the `GENERATED` blocks: run the
    generator after changing the TOML;
  - a new TOML key or content type: [`docs/content/`](docs/content/README.md) (the format file of its
    type);
  - an experiment or a validation case: [`docs/validation/`](docs/validation/README.md) (what is
    compared, with what tolerance, the source and its status) and, if it is a candidate,
    [`docs/design/candidates.md`](docs/design/candidates.md);
  - a lesson, plate, medium, vessel, reagent or glossary term: listed in its format file in
    `docs/content/`.
- **Known bugs and model limitations** get one file each in [`docs/bugs/`](docs/bugs/README.md),
  following [`docs/bugs/TEMPLATE.md`](docs/bugs/TEMPLATE.md), and a line in the index. When fixed,
  the file stays with `Status: fixed` and the commit. A test marked `should_fail` or `skip` must have
  its bug file.
- **The roadmap** ([`docs/roadmap.md`](docs/roadmap.md)) is the only place for plans and the project's
  direction. Plans of a single feature live in the conversation or the pull request; once done, what
  matters moves to the mechanics documents and the plan is deleted (no stale plan files in `docs/`).
- **Describe what the code does today.** Mark estimates as estimates, cite sources (author, year,
  journal) and never invent a reference: check it.
- **`AGENTS.md`** is updated when an architectural rule, a convention or the folder structure changes.
- **Checks:** `tools/gen_being_docs.py --check` (every being has an up-to-date document) and
  `tools/check_doc_links.py` (relative links and anchors) run in `tools/ci.sh` and must pass.
- Images for the documentation go in `docs/img/` ([index](docs/img/README.md)).
