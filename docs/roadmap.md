# Open Microcosm — Roadmap

Status as of 17/10/2026. This document collects in one place what used to be spread across the old
roadmap, the per-phase plans, the validation notes, the idea list, the gameplay document and the
laboratory proposal. Finished work is only summarised (the design detail lives in `AGENTS.md` and in
[`docs/mechanics/`](mechanics/README.md)); pending work is ordered into new phases.

Legend: ✅ done · 🟡 partly done (what is missing is stated) · ⬜ to do · ❌ discarded.

---

## 1. Done

### Simulation core ✅

- **Medium:** circular Petri dish (or toroidal world); exact Stokes–Brinkman water solved with FFT,
  without inertia; chemistry by spectral diffusion + advection (O₂, CO₂/pH with carbonate and Henry,
  P, N, Si, Fe, B₁₂, organic matter, toxins, autoinducer, phages, reagent; glutamine, lactate and
  fMLP only with animal cells); light with Beer–Lambert; temperature with Q10, salinity, anoxia,
  alkalinity, variable air CO₂, osmolytes.
- **Soft bodies:** inertia-free XPBD with rates [1/s], plastic amoeba membrane, interleaved contacts,
  three-level LOD, compound bodies (colonies and filaments with bending-stiff links),
  `reshapeRestEven` for large shape changes.
- **Life cycle:** a single physiology for plate and culture (`bio/cell_physiology`); respiration,
  fermentation and photosynthesis; Droop with Liebig (P, N, Si, Fe, B₁₂); spheroid mass; size
  checkpoint before division; physical mitosis, conjugation, cysts, budding, lysis; genome with
  physical organelles.
- **Drop ecosystem:** hunters, engulfers, sessile feeders, suctorians, filter feeders, grazers,
  mixotrophs; motile, diatom, colonial and filamentous algae; cyanobacteria with heterocysts and
  microcystin; chytrids, yeasts, *Saprolegnia*, *Allomyces*, lichen, *Physarum*; phages with resistant
  mutants, *Bdellovibrio*, quorum sensing, biofilm as a refuge. 92 beings in the factory pack.
- **Time:** physics in real time, biology ×`kBioTimeScale` (60), crawling migration in biological
  time.

### Content as data (phase 20.0) ✅

Registry with text ids; beings, body plans, physiology with sources, minerals, biomes, media,
vessels, prepared plates, lessons, glossary and reagents in TOML; diets by tags; user packs and
`.biopack` with validation and size limits; Content panel; compatibility with old plates and scenes;
pack fingerprints in the provenance.

One file per definition in the folder of its type (`beings/paramecium.toml`, with its physiology;
see [`docs/content/`](content/README.md)) and every machine name in English (content, culture
scenes, programs, experiments, parameter keys, data folders). Decided: human-readable text (names,
descriptions, sources, comments) stays in Spanish and is translated by `assets/lang`; data saved
with the old names can still be read (`legacy_ids`, `game/legacy_names`) and old packs are migrated
with `tools/migrate_content.py`.

### Prepared plates, cell culture and slides (phase 20a–c) ✅

- Cell culture in DMEM inside an incubator (37 °C, 10 % CO₂), separate vessel and field,
  reproducible inoculation and mixtures.
- Human cells: dermal fibroblast, HeLa and neutrophil; crawling with a persistent random walk,
  G1/S/G2/M cycle with restriction point and G0, contact inhibition, Michaelis–Menten uptake of
  glucose and glutamine, Warburg effect, apoptosis, chemotaxis to fMLP and phagocytosis. Validated
  (doubling time, Fürth MSD, confluence plateau, lactate/glucose, neutrophil speed).
- Slides: onion, *Elodea*, potato with Lugol, root tip, stomata, buccal mucosa, Giemsa smear and
  diluted blood; Boyle–van 't Hoff osmosis, plasmolysis and haemolysis validated (Parpart).
- Human sperm: fresh semen plate (WHO semen analysis), CASA kinematics, vitality stains
  (eosin–nigrosin), hypo-osmotic swelling test, capacitation and hyperactivation, morphology by
  strict criteria, leukocytospermia.

### Laboratory tool (P0) ✅

- **Reproducibility:** single parameter registry, per-step interventions, scenes that replay bit
  for bit with 1 and 4 threads, full provenance in the export.
- **Measurement (phase 13):** live plots, CSV/JSON, ruler and area, region, growth curve, mass
  budget per compartment and per process, limitation map.
- **Distribution:** `dist` and `windows` presets, CPack, GPL-3.0-or-later with licences.
- **Stability:** autosave with recovery, soak test, `fallas_numericas`, local CI.
- **Performance (phase 19):** deterministic XPBD and contacts on the GPU, `limitApproach` 8.5×
  faster, own batched rendering: 551 cells in 16.1 ms per frame.

### Experiments and classroom (P1) ✅

Individual-based well-mixed culture with plate ↔ culture cross-calibration; programs (ramps, cycles,
events) in culture and plate; Cultures panel with sample drop; Gause (plateaus, exclusion,
*Didinium*); replicates and batches (`microcosm_batch`, Experiment panel, 95 % CI); chemostat (Monod and
Tilman with complete exclusion) and perfusion; seeding by density, passages and serial dilutions;
English; lessons as protocols with a tested solution (Jennings, Tilman, Gause, plasmolysis, osmotic
fragility, semen analysis); reagents with pharmacodynamics and an emergent MIC; photo and time-lapse
by biological time; glossary and anchored notes; accessibility (UI scale, HiDPI, Okabe–Ito).

### Optics ✅ / 🟡

- ✅ Bright field with post-processing (vignette, aberration, grain), dish glass, semantic zoom by
  projected size, body plan and model view.
- ✅ Rule-based stains: Gram, Lugol, DAPI, neutral red, methylene blue, Congo red, Giemsa, aceto-orcein,
  eosin–nigrosin, trypan blue; fixing the sample; preview.
- ✅ Optics phase A: uneven Köhler illumination and dust on the eyepiece and camera.

### Scientific validation 🟡

Passing: *Paramecium* µmax (plate and culture), Tilman (batch and chemostat), Jennings, Monod/Droop,
Monod in the chemostat, functional-response calibration, ampicillin and tetracycline MIC, human cells
and osmosis. What is missing is in phase B below. Details in
[`docs/validation/`](validation/README.md).

---

## 2. To do

### Phase A — Clean-up and open debt

Small or already decided items, before adding anything new.

- ✅ **Remove the encyclopedia** (done on 21/10/2026): panel, discovery by camera and
  `progress.txt` are gone; the card's texts moved to `game/organism_text`. `deathTally()` and
  `observations()` stay (Statistics panel and tests).
- ✅ Numeric values of the inspector (`game/inspect`) in English (done on 21/10/2026,
  [bug](bugs/inspector-values-untranslated.md)).
- ⬜ Inoculation inside a drawn region ([bug](bugs/inoculation-ignores-region.md)).
- ⬜ Stains: hyphae, cyanobacteria and oat flakes do not respond to Lugol/Gram
  ([bug](bugs/stains-missing-targets.md)).
- ⬜ Position of organelles (anterior or posterior flagellum) is not modelled for every being.
- ✅ Archive the superseded documents (done on 21/10/2026: documentation reorganised in `docs/`).

### Phase B — Close validation and release

The P0 "done" criterion that is still open.

- 🟡 **Gause:** *P. aurelia* wins with the full protocol (daily thinning, 5 cm³, mixed medium,
  biovolume; done on 21/10/2026). Missing: the speed of displacement (`should_fail` test,
  [bug](bugs/gause-displacement-too-slow.md)); do not tune parameters.
- ⬜ *Didinium*–*Paramecium* with immigration (oscillations).
- ⬜ µmax of *Amoeba proteus* (culture with *Tetrahymena* as prey).
- ⬜ Load µmax with a source for the species that use the general value (0.15/h).
- ⬜ Check the Gause temperatures and the Tilman and Kilham (1976) constants against the
  publications; review Droop's L = 1.5 for P.
- ⬜ Functional response of *P. caudatum* measured at several densities.
- ⬜ **Sustainable drop:** phenotypic resistance to *Bdellovibrio* (Shemesh and Jurkevitch 2004),
  realistic initial densities for the volume; re-enable the 30-min test
  ([bug](bugs/drop-bacteria-collapse.md)).
- ⬜ Review the hundreds of mM of organic matter after a lysis in a small volume
  ([bug](bugs/organic-matter-after-lysis.md)).
- ⬜ **Packages:** test the Linux package on another distribution and the Windows one on a clean
  machine; multi-hour soak with no failures.
- **Done when:** every `validacion` test passes without `should_fail` or `skip` and both packages run
  on clean machines. Then: **release**.

### Phase C — Lenses

What is needed to really look at cell cultures (AGENTS.md: "phase contrast, pending lens").

- ⬜ **Phase contrast:** greys with a halo; the default lens for cell culture.
- ⬜ **Dark field** (including Tyndall: tiny bright particles).
- ⬜ **Fluorescence**, with chlorophyll fluorescence (red under blue light, lower with iron
  deficiency).
- ⬜ India ink for capsules (new rule in the stain engine).
- ⬜ Stains on the live plate with a split view and real-time exposure.
- ⬜ Later: 10×/40×/100× objectives (they change the field and what is resolved; the photo records
  the magnification), polarisation (frustules, starch).
- **Done when:** the same plate can be seen through the four lenses and each shows something the
  others do not.

### Phase D — Extended chemistry (old phase 15)

- ⬜ Complete silicon cycle (dissolution with pH and temperature, frustule sedimentation).
- ⬜ Iron chelation (siderophores, oxide at high pH).
- ⬜ More micronutrients (B₁, B₇, Zn, Co, Mo, Mn) with real auxotrophies.
- ⬜ Limiting inorganic carbon and carbon-concentrating mechanisms.
- ⬜ Biofilms with matrix (they change the flow, retain nutrients, internal gradients, shear).
- ⬜ Real samples: load measured T, pH, conductivity and nutrients.
- ⬜ Temperature, base pH and salinity as fields (global today; cost: `bioScale()` stops being a
  scalar).
- **Done when:** each new nutrient has a conservation test and a species visibly limited by it.

### Phase E — Complete well-mixed culture

What `Culture::supports` still rejects.

- ⬜ Animal cells, filaments, colonies and zoospores in the culture.
- ⬜ Conjugation, *Bdellovibrio*, quorum and B₁₂ release by bacteria.
- ⬜ Reagents with other mechanisms: metals, salt and eukaryotic targets.
- **Done when:** every prepared plate has a well-mixed equivalent with the same physiology.

### Phase F — More cell biology

- ⬜ More real cell types with published data (keratinocyte, other lines).
- ⬜ Opening and closing of stomata.
- ⬜ Endosymbiosis and kleptoplasty as an experiment (*P. bursaria* with *Chlorella*).
- ⬜ Antimicrobial resistance with a cost and horizontal transfer (plasmids, transduction).
- ⬜ Phage lysogeny.

### Phase G — Analysis

- ⬜ Trajectories, mean speed and mean squared displacement of any being.
- ⬜ Cell quota panel (how many times Q₀ the selected cell holds).
- ⬜ Lineage tree with mutations per branch.
- ⬜ Two plates side by side with the same seed and one different variable.
- ⬜ Timeline: rewind by re-simulating from a checkpoint.
- ⬜ Compare with the analytical model (Monod, Lotka–Volterra) in the same plot.
- ⬜ Laboratory units in the interface (mg/L, mmol/L).

### Phase H — Headless core and bindings (rest of phase 18)

- ✅ `biosim` without raylib, `microcosm_bench`, `microcosm_batch`.
- ⬜ Minimal C or Python interface (propose the bindings library before adding it).
- **Done when:** a notebook sweeps a parameter over N runs and plots the result.

### Phase I — Performance (rest of phase 19)

- ⬜ `limitApproach` (~8 ms, serial), GPU wait (`collect`, `pairs` kernel), chemistry + FFT,
  `applyMotion`, `resolvePenetrations`, `perceive`.
- **Done when:** 60 FPS with 540+ cells and the GPU on mid-range desktop hardware, with identical
  checksums.

### Phase J — Interface and teaching

- ⬜ Graphical preview of the plate before creating it.
- ⬜ Vector icons in the toolbar; command palette.
- ⬜ Definitive program icon (the current one is provisional) and the macOS `.icns`
  ([`assets/icon`](../assets/icon/README.md)).
- ⬜ Student mode / scientist mode (same simulation, different interface).
- ⬜ Hypothesis and prediction outside the lessons.
- ⬜ Historical scenarios (eutrophication, cyanobacterial bloom).
- ⬜ Declared fidelity per process (conceptual, quantitative, experimental) in the inspector.

---

## 3. Discarded ❌

- Game run, score, victory, roguelite and inheritance between runs (vision of 15/10/2026: it is a
  laboratory).
- Encyclopedia and discovery (17/10/2026).
- Multicellular animals (rotifers, *Hydra*) and *Zoophagus*, until their simulation is realistic.
- Focal plane: everything happens in one plane.
- Vulkan: raylib 5.5 does not support it and the frame is CPU-bound.
- Arcade mode, speculative biology, exaggerated realism.

## 4. Proposed order

1. **A** (clean-up: small debt).
2. **B** (validation and packages) → **release**.
3. **C** (lenses: phase contrast first, for the cultures).
4. **H** (bindings) and **G** (analysis), which build on what already exists.
5. **D**, **E** and **F** (new biology and chemistry), in the order the lessons need.
6. **I** and **J** when needed.
