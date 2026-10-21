# Scientific validation

What is compared against published results, with what tolerance, and its current status. The
values and their sources live in `assets/content/base/beings/` (`[physiology.<id>.*]`, see
[content format](../content/beings.md#measured-physiology-physiologyidparameter)); the tests are in
`tests/test_validation.cpp`, `tests/test_gause.cpp`, `tests/test_nutrients.cpp` and others labelled
`validacion` (`ctest -L validacion`, ~28 min with `-j4`).

A test that does not reproduce the literature is **not tuned until it passes**: the discrepancy is
recorded here (and as a [known bug](../bugs/README.md) when it is a model defect) until the real
mechanism is found.

## Loaded physiology data (first batch)

| Species | Published µmax | Temp. | Source | Status |
|---|---|---|---|---|
| *Paramecium* (*P. caudatum*) | 0.946/day | 26 °C (unverified) | Gause 1934, fig. 22 (own fit to the gauseR data) | estimate: verify temperature |
| *Amoeba proteus* | 44 h generation | 20 °C | Rogerson 1980, Can. J. Zool. | direct measurement |
| *Didinium nasutum* | 0.93/day | 26 °C (unverified) | Gause 1934, fig. 32 | estimate |
| *Chlamydomonas reinhardtii* | 20 h (chlorophyll) | ~25 °C (unverified) | PNAS 1997 | estimate (not a cell count) |
| *Euglena gracilis* | 0.047/h | 30 °C | culture in 10 % digest | estimate (non-standard medium) |

Since then 39 beings carry a sourced `max_growth`; each one is listed in its
[being document](../content/beings/README.md). The rest use the general metabolism value
(`metabolism.max_growth`, 0.15/h): **they still need a sourced value.**

Own fits to Gause's (1934) original data digitised in gauseR (Clark et al.): *P. caudatum*
r = 0.946 ± 0.109/day, K = 203; *P. aurelia* r = 0.707 ± 0.090/day, K = 222 (relative volume); in the
mixed culture *P. aurelia* excludes *P. caudatum* (140 vs 23 on day 25). *Didinium* with abundant
prey: +0.93/day; without prey: −0.51/day.

Ciliates without their own measurement use the **allometric estimate** of Müller & Geller (1993,
Arch. Hydrobiol. 126: 315), ln r = 1.52 ln T − 0.27 ln V − 1.44, at 20 °C with the volume of the
loaded spheroid. Checked against those with data: *Colpidium* 1.64/day estimated vs 1.25 measured;
*P. caudatum* 1.19 vs 0.95.

## Status of the validation tests

| Test | Status | Note |
|---|---|---|
| *Paramecium* µmax in unlimited culture (plate) | **PASSES** | Simulated µ 0.024/h vs 0.026/h published (at 20 °C), two doublings, with the species' measured ingestion (formerly 0.026 with the general flow rate, 4× overfed). Culture: ~860 µm plate, 2 individuals, no phages or *Bdellovibrio*, time scale 600. |
| *Amoeba proteus* µmax | pending | Its published µmax uses *Tetrahymena* as prey (hundreds per division); a small plate does not have enough protist prey. Needs a culture with abundant ciliate prey. |
| Gause's competitive exclusion (*P. aurelia* vs *P. caudatum*) on the plate | no result | On the plate it gives no comparable result: with limited food (`organicInflow` 5) about 50 paramecia exhaust the bacteria and crowd into 0.86 mm; Gause had hundreds per 0.5 cm³. Solved in the culture (see below). |
| Predator–prey (*Didinium*–*Paramecium*) without refuge | passes in the culture | See "Gause in the culture". |
| Tilman (*Asterionella* vs *Cyclotella*, P and Si) | **PASSES** | Batch culture, 2 cm water column. With P limiting *Asterionella* wins (0.71 of the biomass); with Si limiting, *Cyclotella* (*Asterionella* 0.22). With the Droop model (formerly 0.52 and 0.47: the published growth K was used for uptake and limitation was weak). In batch there is no complete exclusion (Tilman used semi-continuous cultures). Monod constants **not verified** against Tilman & Kilham (1976). |
| Jennings (CO₂ accumulation) | **PASSES** | 20 *Paramecium* in a ~1.9 mm plate with the CO₂ dropper open at the centre (2 mM/s; ~10 mM and pH 5.3 at the source). Fraction within 0.2 radii (4 % of the area): 0.081 with CO₂ vs 0.031 without (the test requires > 1.5× the area and > 2× the control). Density rises towards the source (3.4× the mean at the centre). Qualitative result: Jennings (1906) published no counts. With stronger acid he saw a ring around the bubble; avoidance of strong acid is not modelled here. |
| *Paramecium* µmax in the well-mixed culture (published ingestion) | **PASSES** | 0.023/h vs 0.026/h at 20 °C, with saturating bacteria (~10⁹/mL) and the measured ingestion (see "Energy budget of *Paramecium*"). |
| Monod: µ vs external P (*Asterionella*) | **PASSES** (basic test) | Droop model since 15/10/2026: at equilibrium, µ = µmax·S/(K+S) with the published K, error below 3 % (`tests/test_nutrients.cpp`, no `World`). On the plate the cell sees less P than the mean (uptake sink: 0.028 vs 0.04 µM) and with that local value the curve matches (0.0142 vs 0.0143 /h). |
| Human cells | **PASSES** | Doubling time, Fürth MSD, confluence plateau, lactate/glucose and neutrophil speed (`tests/test_animal_cells.cpp`). |
| Osmosis | **PASSES** | Plasmolysis of onion epidermis and Parpart's osmotic fragility of erythrocytes. |
| Human sperm | **PASSES** | Swimming at the CASA VAP with its measured Q10, WHO motility classes, vitality and HOS. |

## Energy budget of *Paramecium* (well-mixed culture, phase P1)

The well-mixed culture (`game/culture`) uses the published ingestion and clearance of *P. caudatum*
(Applied Water Science 2013, doi:10.1007/s13201-013-0130-1: ~26·10³ bacteria per individual per hour
at 25 °C, ~106 nL/h). With them the first result did not reach Gause's µmax (0.007/h vs 0.026/h at
20 °C). The causes, in order:

1. **The mass of elongated cells was inflated** (FIXED, 16/10/2026). It came from the sphere of the
   same cross-section seen from above (`cellMassPmol` of the 2D area): a 180 × 50 µm paramecium
   weighed like a 95 µm sphere (~4.5·10⁵ µm³), almost twice the real spheroid (~2.4·10⁵ µm³);
   *Asterionella*, 3.2 times. Now the mass is that of the spheroid with major, minor and vertical
   semi-axes (`thickness` in `[being.shape]`; by default the minor one) times the relative dry
   matter (`cellMassScale`). Human cells, whose factor already carried their measured volume, were
   converted to keep the same mass.
2. **Food-limited cells never divided** (FIXED): metabolism invested in biomass everything above the
   reserve buffer (0.1 of the biomass) and dividing required reserves of 0.1 of the biomass; the cell
   grew without limit past twice its size. Now, at division size, it stops investing and gathers the
   reserves for cytokinesis (cell-cycle checkpoint).
3. **The plate overfed the paramecia** (FIXED): their maximum flow rate was the general cytostome
   one (30 bacteria/s, unsourced: ~108·10³/h, four times what was measured). Now the plate uses the
   species' measured ingestion when it has one (the same as the culture).

With all three: µ = 0.023/h in the culture and 0.024/h on the plate, vs 0.026/h published (both
within 25 % without tuning anything). By hand: a paramecium of ~400 pmol spends ~6 pmol/h on
maintenance and swimming, and growing at 0.026/h needs ~17 pmol/h more (yield 0.6): ~10·10³
bacteria/h, which the published ingestion (18·10³/h at 20 °C) covers with abundant bacteria.

**Gases with Q10** (FIXED): on the plate the O₂ a cell respires and the CO₂ it releases were divided
by `bioScale` (compression × Q10): at 30 °C the cell respired the same as at 20 °C. Now only the
compression is undone (`bioTimeScale`): with heat it respires faster, like the culture.

## Gause in the culture (phase P1, step 5)

Gause's protocol (1934, ch. V): 0.5 cm³ tubes of Osterhout's saline medium at 26 °C; every day the
tube is centrifuged, the medium changed and a fixed ration of bacteria added (in the model,
5·10⁷/mL). Tests in `tests/test_gause.cpp`:

| Test | Status | Note |
|---|---|---|
| Monocultures: plateau | **PASSES** | *P. caudatum* ~220–250 per tube, *P. aurelia* ~550–620 (smaller: more individuals with the same food). Gause: K = 203 and 222 in relative volume. |
| Mixed culture: competitive exclusion | **PASSES** | By day 60 one of the two is below 20 % in 4 of 5 replicates. |
| *P. aurelia* wins | **FAILS** (`should_fail`) | Whichever gets ahead early wins (2 or 3 of 5 replicates). *P. aurelia*'s advantage comes only from feeding allometry (~20 % more clearance per biomass: Hansen et al. 1997) and is not enough against the higher µmax of *P. caudatum*. Gause attributed part of his result to *P. caudatum*'s sensitivity to waste products in the medium, which the model lacks. [Known bug](../bugs/gause-aurelia-does-not-win.md). |
| *Didinium* without refuge | **PASSES** | *Didinium* multiplies by eating and exterminates the paramecia; afterwards none is active (it encysts: Beers 1927). Gause saw them die; with cysts the culture is also left without active predators. |
| *Didinium* with immigration (oscillations) | pending | |

*P. aurelia* has no feeding measurements of its own: its maximum ingestion and clearance come from
those of *P. caudatum* with the zooplankton exponent (V^0.75–0.77). ESTIMATE.

## Chemostat (phase P1, step 7)

| Test | Status | Note |
|---|---|---|
| Monod: residual substrate | **PASSES** | Bacteria with D = 0.5/h: S* = 0.652 µM vs K (D + m) / (µmax − D − m) = 0.647. Before the implicit substrate step, in each 1-min step the bacteria ate everything that came in (S ≈ 0). |
| Tilman in the chemostat: P limiting | **PASSES** | D = 0.25/day (Tilman 1977), 60 days: *Cyclotella* dies out (0 vs 250 *Asterionella*). |
| Tilman in the chemostat: Si limiting | **PASSES** | *Asterionella* dies out (0 vs 1366 *Cyclotella*). Complete exclusion, as in Tilman's semi-continuous cultures (in batch only the trend was visible). |

## Antibiotics: the emergent MIC (phase P1, step 11)

Reagents (`reagents/`, `game/reagent`) use the pharmacodynamic function that Regoes et al. (2004,
Antimicrob. Agents Chemother. 48: 3670–3676) fitted to *E. coli* kill curves (ψmax, ψmin, zMIC and
κ), with killing proportional to how much the bacterium grows (β-lactams do not kill bacteria that
do not grow). The MIC is not an input to the simulation: it comes from a simulated broth dilution
(Mueller-Hinton at 35 °C, 5·10⁵ bacteria/mL, 18 h, two-fold series, visible growth = 10⁷/mL,
threshold ESTIMATED) and is compared with the MIC the same work measured by dilution.

| Test | Status | Note |
|---|---|---|
| Ampicillin MIC | **PASSES** | 4 µg/mL vs 8 published (one dilution: the reproducibility of the test is ±1). |
| Tetracycline MIC | **PASSES** | 1 µg/mL vs 1. |

## Plate ↔ culture cross-calibration (phase P1, step 2)

`measureFunctionalResponse` (`game/calibration`, `bio_bench --respuesta-funcional paramecium`)
measures how many bacteria *Paramecium* eats on a 0.86 mm plate at 20 °C and compares it with the
culture's functional response (type II with the data). Three validation tests (~10⁵, ~10⁶ and ~10⁹
bacteria/mL) require agreement within a factor of 2.

What showed up when measuring:

1. **The plate forgot the third dimension of small prey.** Every bacterium overlapping the mouth in
   the top view was swallowed, as if it were at the same height as the ciliate. Now each bacterium
   has a height in the film (`World::filmHeight`, which changes randomly each time it swims across)
   and the mouth only reaches those passing at its height; the amoeba, those swimming at the height
   of its body and those in the biofilm.
2. **The published clearance is at high density.** The ingestion study (106 nL/h) computes it at
   ~2.5·10⁸ bacteria/mL, where ingestion is already almost saturated; used as the initial slope, the
   half-saturation fell at 2.5·10⁸/mL and *Paramecium* could not grow in Gause's cultures. Clearance
   at low prey density comes from the general zooplankton scaling (~10⁵ body volumes per hour in
   ciliates; Hansen et al. 1997, Kiørboe 2011): ~24 µL/h for *P. caudatum*. It is consistent with
   the study: at 2.5·10⁸/mL ingestion is 0.996 of the maximum and at ~10⁶/mL it drops to half, as
   reported for the low concentrations. ESTIMATE.
3. **The mouth's bacterial capture radius** (6 µm, unsourced) was separated from the reach for cells
   and detritus and **calibrated** with that functional response: `bacteriaCaptureRadius` = 75 µm
   (the oral current drags the water around the body).

Result (`bio_bench --respuesta-funcional paramecium`):

| bacteria/mL | plate [1/h] | culture [1/h] | ratio |
|---|---|---|---|
| 7.4·10⁴ | 901 | 1595 | 0.56 |
| 1.1·10⁵ | 1621 | 2257 | 0.72 |
| 3.5·10⁵ | 4053 | 5664 | 0.72 |
| 1.9·10⁶ | 10329 | 13069 | 0.79 |
| 1.4·10⁷ | 18375 | 17412 | 1.06 |
| 8.5·10⁸ | 18375 | 18368 | 1.00 |

At low density the plate eats somewhat less: the paramecia deplete the bacteria around them (an
uptake sink the well-mixed culture does not have). Pending: a functional response of *P. caudatum*
measured at several densities (today there is a single published point).

## Model problems found while validating (MVP phase)

1. **Bacteria went extinct because of *Bdellovibrio*, not hunger.** (At first we attributed it,
   without checking, to synchronous starvation; the counters showed 177 000 *Bdellovibrio* bursts
   until no bacterium was left, with plenty of organic matter.) Swimming at 100 µm/s on a 0.86 mm
   plate they cover everything and find every bacterium. In reality the prey does not go extinct: a
   fraction develops transient phenotypic resistance (Shemesh and Jurkevitch 2004). **Pending
   proposal:** model that resistance. Validation cultures carry no *Bdellovibrio*
   (`WorldConfig::bdellovibrioFraction = 0`).
   - Starvation was fixed anyway: a bacterium without enough food lowers its maintenance to 0.2 of
     normal and stops growing (stationary phase; Schink et al. 2019, Cell Systems 9: 64–73). Before,
     it spent full maintenance until it died.
2. **Phages in small populations:** with ~20 000 bacteria and the real frequency of resistant
   mutants there is almost surely no resistant one and the lytic epidemic wipes them out. Plausible
   for such a small volume; validation cultures carry no phages (as in the lab).
3. **NaN in the soft body under extreme compression** (FIXED): the cause was `limitApproach`, which
   corrected each pair's approach twice; chained between packed cells, velocities reached
   ~10⁵ µm/s. There is also a safety net (`recoverNonFinite`, `fallas_numericas` in the export).
   What was seen: ten paramecia of ~180 µm in a 350 µm plate: one was left stretched against the
   glass (ring of 220 points, 240 × 247 µm box) and its positions became NaN. After the fix, the
   dense soak gives 0 numerical failures.
4. After a lysis in a small volume organic matter reaches hundreds of mM (conservation is correct,
   but whether the film volume is the right one should be reviewed).
   [Known bug](../bugs/organic-matter-after-lysis.md).
5. **The amoeba's membrane crossed itself while phagocytosing** (FIXED; long test "la membrana de la
   ameba no se cruza al comer presas de distinta forma"). Two cup defects that appeared or not
   depending on the trajectory:
   - the advance of each lip was measured with the wrapped angle at ±π from the anchor; when the
     anchor relocated near one lip, the other lining passed the antipode, seemed to be behind, and
     the new membrane was inserted 8 µm behind the lip, folded ("three" case);
   - in a thin pseudopod pressed against the prey, the lining continued around the tip to the
     opposite face and pulled it towards the prey through the other face ("paramecium" case).
   Now the advance accumulates point by point, the lining ends where the membrane turns and each lip
   can advance until it meets the other. The safety net (`untangle`) no longer acts in these cases.
6. **Rings that grew without limit when dividing** (FIXED): the daughter was resampled with the
   perimeter of its current geometry; a stretched daughter was born with thousands of points and its
   daughters more (300 → 5000 points, crossing the whole plate). Now: perimeter of its species'
   shape.
7. **Degenerate daughters** (FIXED): a furrow cutting almost at the edge left one half with no area
   and the daughter was born NaN (*Cyclotella* in a dense culture). Each daughter has at least 20 %
   of the total area.
8. **Inocula without nutrients** (FIXED): the seeded cell took its quota from the surrounding water
   and in poor water was born without P or Si. Now it brings it from outside (recorded external
   input); internal births still take it from the water.
9. **Fixing the mean concentration blew up with almost depleted water** (FIXED, while measuring
   Monod): `setMeanConcentration` multiplied the whole field by target / mean, including the
   negative ripples of spectral diffusion next to an uptake sink and the glass cells (which add up
   as mass: ~1 % error). With the mean almost zero the factor was ~10⁶, the sign alternated every
   step and after 220 steps phosphate was NaN (with NaN uptake never limits). Now it touches only
   the water, clips negatives and, if no distribution remains, fills evenly.
10. **Filament joints** (FIXED, 15/10/2026): the 0.7–0.8 of `bio_bench` was not from the solver. A
    filament that grew longer than its room buckled by bending **at the joints**: they were two
    links near the rounded tip (a ~5 µm pin that rotated like a hinge) and fragmentation cut loose
    links (half walls). Now the joint is the entire cross wall (5 links across and 2 diagonal) and
    it is cut as a whole. *Spirogyra* growing for 20 h on the plate: mean joint deviation
    0.40 → 0.06 and the filament bends spread along its length.

## Collapse of the bacteria in the drop (30-min test)

The test "30 min sin jugador: ninguna especie se extingue" was in `skip` and had never been run with
the current model. Run (seed 66, 50 000 bacteria): bacteria go from 50 000 to 0 in 3 min (3
biological hours); bacterivores and algae follow (eaten by predators and chytrids) and *Candida*
(13 → 815) and *Naegleria* (6 → 361) grow unchecked and fill the plate (the step becomes very slow:
~4 h for 30 min). Causes, measured with a separate diagnosis:

1. The test configuration has no organic matter (no detritus or inflow): bacteria cannot grow and
   grazing exhausts them. That is consistent with the configuration.
2. In the Puddle drop (with organic inflow), a phage epidemic lyses 62 933 bacteria in ~100 s (from
   ~80 000 to ~120). About 45 resistant ones survive (mutation frequency 10⁻³).
3. The resistant ones, healthy and with 2–7 mM of organic matter, do not recover: grazing imposes a
   mortality of ~0.05/s (estimated from the drop 50 000 → 31 000 in 20 s without phages) and their
   maximum growth is 0.023/s (1.4/h biological at ×60). It is the volume scale again: 20 paramecia
   in ~0.06 µL is ~3×10⁵ per mL, some 300 times a dense culture.

Chosen (15/10/2026, "the most realistic"): **refuge in the biofilm**. Bacteria hitting the glass can
attach; attached ones do not swim, grow on local food and their daughters stay in the biofilm; they
detach slowly. Filter feeders cannot reach them, amoebae can (Matz and Kjelleberg 2005, Trends
Microbiol. 13: 302–307). Attachment probability (0.01 per collision) and detachment (0.2/h) are
ESTIMATES.

Result in the Puddle drop (seed 66, 6 biological hours): the biofilm forms quickly (3 480 attached,
~5 %, after 1 h); the phage epidemic still kills 99.9 % (the biofilm does not protect from phages in
the model), but there is no longer extinction: 16 resistant ones remain, almost all attached, and
from there the population grows again (16 → 176 in 3 h, doubling every ~50 min). Meanwhile the
paramecia drop from 17 to 3 for lack of bacteria. The 30-min test is still in `skip`: its plate has
no organic matter, and without food bacteria do not grow anyway.
[Known bug](../bugs/drop-bacteria-collapse.md).

Other remaining options: realistic initial protist densities for the plate volume; a deeper water
column for the bacteria (as in Tilman); refuges for bacteria (biofilm, sediment) and loss of phages
(adsorption to particles); the phenotypic resistance to *Bdellovibrio* already proposed; or the
culture mode for population dynamics.

## Monod and Droop (chosen: Droop)

Before, Tilman and Kilham's (1976) K, which is a **growth** constant, was used as an **uptake** K with
a fixed maximum uptake of 0.3 quotas/h (~9× µmax): with P = K, *Asterionella* grew at 82 % of µmax
instead of 50 %. Now (`bio/metabolism`):

- minimum quota Q₀ (`*Ratio`) and maximum Qmax = L·Q₀ (`luxuryQuota`, L = 1.5);
- Droop growth: µ = µ∞(1 − Q₀/Q), µ∞ = µmax·L/(L−1) (at Qmax it grows at µmax);
- Monod uptake with Vm = L·µmax·Q₀ and Ku = L·K.

At equilibrium (µ·Q = uptake) this gives exactly µmax·S/(K+S) (own derivation; the general relation
between uptake and growth constants is in Morel 1987, J. Phycol. 23: 137–150). After the nutrient is
exhausted, the cell keeps growing on its reserve. Unverified: L = 1.5 is low for P (in algae
Qmax/Q₀ reaches 5–50; Droop 1973 gives values for B₁₂ and P); it was left so as not to change the
stoichiometry of the rest of the model.

## The volume scale (main lesson)

Classic population experiments are in millilitres (Gause: 0.5 cm³; Tilman: cultures with
10⁴–10⁵ cells/mL). The simulated plate is a ~0.1 mm film over ~0.6 mm²: some 10 000 times less
volume. With real concentrations **the whole plate holds less phosphorus than a single diatom**, and
a dozen paramecia already fill it. To validate population experiments:

- chemistry can represent a deeper, well-mixed water column (`gapHeight`): that was done for Tilman;
- competition for bacteria (particles in 2D) is not fixed that way: food must be limited so that the
  equilibrium population fits the plate (Gause), or a larger plate used;
- a well-mixed **culture mode** (no soft bodies, only metabolism and chemistry) validates population
  dynamics at the mL scale. Done: [well-mixed culture](../mechanics/well-mixed-culture.md).

## Being batches (17–19/10/2026)

New beings were added in batches, each with its size, its µmax when published, its sources and a
test that makes it reproduce (`tests/test_candidates.cpp`, `largos` suite). The per-being values and
sources are in each [being document](../content/beings/README.md). Fixes found while validating the
batches:

- **Furrow closure threshold** (`furrowClosed`, `bio/division.cpp`): from 2.5 to 3 membrane radii.
  Self-contact of the membrane leaves the two faces of the furrow ~2.6 radii apart between vertices,
  so almost round cells (*Cosmarium*, *Micrasterias*) were left with the furrow closed but not split.
- ***Euglypha*** reproduces by **budding**: the real testate amoeba does not constrict; the daughter
  leaves through the aperture and builds its test.
- ***Closterium*** and ***Ceratium***: without `taper`, which narrowed only the posterior pole and
  crossed the ring when dividing. *Closterium* is now a crescent silhouette with `bend` and both
  tips tapered, and divides with the silhouette.
- **Performance** (`bio_bench --wildlife 3`, 300 steps, vs 52ca0f0): puddle 31.7 → 43.1 ms/step
  (+36 %, 848 → 875 cells), pond 40.6 → 46.7 ms/step (+15 %). Large new beings (*Chaos*,
  *Micrasterias*, *Stylonychia*, *Frontonia*, *Closterium*, *Blepharisma*) have long membrane rings;
  their weights in the default drops were lowered afterwards.

Still pending:

- Well-mixed culture: check that the loaded µmax give the expected rate (per-species validation with
  measured data: `tetrahymena`, `colpidium`, `saccharomyces`, `chlorella`, `raphidocelis`,
  `desmodesmus`, `stylonychia`, `cryptomonas`, `ceratium`).
- Animal cells: `nih_3t3` and `hacat` with their cycle, migration and confluence (like
  `tests/test_animal_cells.cpp` with the fibroblast and HeLa).
- Data to confirm: estimated widths (*Blepharisma*, *Colpoda*, *Chilodonella*, *Peranema*),
  temperature of the µmax of *Stylonychia*, *Cryptomonas*, *Ceratium* and *Acanthamoeba*, and the
  HaCaT table.

## Pending

- Decide how to sustain the bacteria in the drop ("Collapse of the bacteria") and re-enable the
  30-min test.
- Verify the Gause temperatures and the Tilman values against the publications.
