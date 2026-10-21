# Well-mixed culture

A tube or flask of millilitres (Gause, Tilman) where populations do not fit in a simulated plate
(`game/culture`, phase P1). No space or soft bodies, but **the same physiology as the plate**
(`bio/cell_physiology`, `updateMetabolism`, `digestContents`, `bacterialDemand`).

## Individuals and cohorts

- Each protist or alga is an individual (`CultureCell`: `Metabolism`, `Heredity`, `Digestion`,
  `Lineage`).
- Bacteria are **cohorts** (`BacterialCohort`: super-individuals of identical bacteria), exact up to
  `maxBacterialCohorts`.

## Encounters from physical theory

What emerges from space on the plate comes here from encounter theory:

- **Filter feeders:** type II functional response with the measured `max_ingestion` and `clearance`
  (`[physiology.<id>.*]`); without data, the oral current and cytostome of the plate.
- **Hunters:** the encounter kernel of Gerritsen and Strickler (1977, J. Fish. Res. Board Can. 34:
  73–82) with the swimming speeds and radii from the data.
- The medium is well mixed: the concentration is the same in the whole volume.

The plate and the culture are **cross-calibrated**: `microcosm_bench --respuesta-funcional paramecium`
compares what *Paramecium* eats in both ([validation](../validation/README.md#plate--culture-cross-calibration-phase-p1-step-2)).

## Time

There is no real-time physics to compress, so there is no `bioTimeScale`: `step(dt)` advances `dt`
seconds and biology `dt · Q10` (the same 20 °C reference as the plate). Gases exchange with the air at
their real rate, set by how mixed the medium is (`gasExchangeRate`):

- **Still medium** (`gas_transfer = 0`): O₂ and CO₂ diffuse through the whole column, rate
  D / depth². Right for a static dish, wrong for a deep tube: in a 2.8 cm column it takes ~4 days, and
  a dense *Paramecium* culture runs out of O₂ within a day.
- **Mixed medium** (`gas_transfer` = k > 0, cm/h): the bulk is uniform (swimmers, stirring) and the
  resistance is the film at the surface (two-film theory), rate k / depth. Without wind a still water
  surface has k ≈ 2.07 cm/h (k₆₀₀ of Cole and Caraco 1998, Limnol. Oceanogr. 43: 647–656, measured
  in a small lake: an ESTIMATE for a tube); stirring or bubbling raise it.

The vessel brings its value (`vessels/`, [format](../content/vessels.md)); the culture can override
it when created (`CultureSpec::gasTransfer`, `gas_transfer=` in the scene) or change it live
(operation `gas_transfer`, recorded in the scene); experiments take it in `[culture]` or as a
factor.

## Light and phages

Light: Beer–Lambert down the column from the phototrophic biomass; each cell receives the column's
mean. Lytic phages with mass-action adsorption, latent period, burst and resistant mutants, with the
plate's parameters (infected cells are their own cohorts).

## Operations

| Operation | Effect |
|---|---|
| `renewMedium` | Centrifuged medium change: protists stay (Gause's protocol). |
| Dilution or thinning | A fraction of the culture (cells drawn one by one) is taken out with everything in it and replaced by fresh medium ([thinning](thinning.md)). |
| Gas transfer (`setGasTransfer`) | Mixing of the medium: surface transfer velocity [cm/h]; 0 = still. |
| Chemostat (`setDilutionRate`) | Continuous dilution with binomial washout. Bacteria take the substrate with an **implicit step** (end-of-step concentration), otherwise the chemostat does not reach S*. |
| Perfusion, inflow medium | Continuous supply. |
| Sample drop, passages (`withdraw`, `receive`) | What leaves goes to the budget as `Flow::Washout`. |
| Seeding by density, serial dilutions | |

The mass budget (`massBudget`, `FluxLedger`) is the same as on the plate. The census carries the
**biovolume** of each species (`CultureCensus::biovolume`, `cellVolumeUm3`), which also goes into the
series (`biovolume:<id>`, µm³/mL): the measure to compare species of different sizes, as Gause's
"volume".

## Culture session

`game/culture_run`: like `Run` for the plate. Every operation goes through `CultureRun::apply`, is
recorded with its step and the `.culture` scene replays it ([user files](../content/user-files.md)).
The **sample drop** (`sampleDrop`) builds a `PlateSpec` v3 with the sampled cells and their state
(`WorldConfig::sampled`): a drop from the tube can be looked at under the microscope. Cultures panel.

## What is not supported yet

`Culture::supports` rejects animal cells, filaments, colonies and zoospores. There is no
*Bdellovibrio*, quorum sensing or conjugation either (roadmap phase E).

## Validation

Gause (plateaus, exclusion, *Didinium*), Tilman (batch and chemostat), Monod in the chemostat,
*Paramecium* µmax, emergent MIC: see [validation](../validation/README.md).

## Code

`src/game/culture.*`, `src/game/culture_run.*`, `src/game/calibration.*`,
`src/ui/panels/culture_panel.cpp`.

## Tests

`tests/test_culture.cpp`, `tests/test_culture_run.cpp`, `tests/test_chemostat.cpp`,
`tests/test_gause.cpp`.
