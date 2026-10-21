# Metabolism and nutrients

Simplified but real metabolism (`bio/metabolism`), in pmol of glucose equivalents of dry matter and
in **biological time** ([time and units](time-and-units.md)).

## Energy

| Process | Model |
|---|---|
| Respiration (with O₂) | ~30 ATP per glucose. The aerobic fraction follows saturation kinetics with the local O₂ (cytochrome oxidase saturates at a few µM). |
| Fermentation (without O₂) | 2 ATP: ~15 times more glucose for the same energy. |
| Maintenance | A fraction of the biomass per hour; swimming and activity raise it; stress (pH, temperature, salt, toxins outside the species' range) adds to it. |
| Growth | Reserves above a buffer are turned into biomass with yield 0.6 and a maximum rate (`max_growth` from the being's data, brought to 20 °C with Q10 = 2; otherwise the general 0.15/h). |
| Photosynthesis | Chloroplasts produce reserves with the local light and release O₂. |
| Starvation | Without reserves the cell consumes itself (autophagy); below half its birth biomass, it dies. |

Gas exchange of each cell with the water runs at the real rate (÷`bioTimeScale`), with Q10.

## Nutrients: Droop quotas and Liebig

Phototrophs take P and N (and, by species, Si, Fe and B₁₂) from the water. N:P ≈ 16 (Redfield).
Since 15/10/2026 the model is **Droop**:

- minimum quota Q₀ (`*Ratio`) and maximum Qmax = L·Q₀ (`luxuryQuota`, L = 1.5);
- growth µ = µ∞(1 − Q₀/Q), µ∞ = µmax·L/(L−1) (at Qmax it grows at µmax);
- Monod uptake with Vm = L·µmax·Q₀ and Ku = L·K, where K is the published **growth**
  half-saturation (`p_half_saturation`, `si_half_saturation`).

At equilibrium this gives exactly µmax·S/(K+S) (own derivation; see Morel 1987, J. Phycol. 23:
137–150). After the nutrient is exhausted the cell keeps growing on its reserve. Growth is limited
by the scarcest factor (**Liebig**); `Metabolism::limiting` records it and the render shows it as the
limitation map (key L).

| Nutrient | Who needs it | Notes |
|---|---|---|
| P, N | All phototrophs | Conserved: `World::totalPhosphorus()`; `totalNitrogen()` changes only by N₂ fixation. |
| Si | Diatoms (`frustule`) | Stays in the frustule after death and redissolves slowly. |
| Fe | Phototrophs; *Trachelomonas* lorica | |
| B₁₂ | Auxotrophs (`b12_auxotroph`: *Euglena*, diatoms) | Supplied by bacteria (`b12_source`). |

## One physiology

`bio/cell_physiology` holds the input of each cell (µmax from the data, traits, pH, temperature,
salt and toxin stress, cyst), bacterial growth (`bacterialDemand`, `bacterialMassChange`) and
vacuole kinetics (`digestionFractions`, `digestVacuole`) as **body-less functions** used by both the
plate and the [well-mixed culture](well-mixed-culture.md). The plate only supplies where the
environment comes from and where exchanges go. **Do not duplicate this biology in another
system.**

Mixotrophs (*Euglena*, *Ochromonas*) both photosynthesise and eat; photosymbionts pass sugar to
their host ([symbioses](symbioses.md)); the stress pigment (astaxanthin) protects from light damage
(`[being.stress_pigment]`, `[being.light_damage]`).

## Code

`src/bio/metabolism.*`, `src/bio/cell_physiology.*`, `src/bio/physiology.*` (loading of
`[physiology.<id>.*]`), `World::updateLifecycle`.

## Tests

`tests/test_physiology.cpp`, `tests/test_cell_physiology.cpp`, `tests/test_nutrients.cpp`,
`tests/test_validation.cpp` (µmax of *Paramecium*, Tilman, Monod).

## Limitations

- L = 1.5 is low for P (in algae Qmax/Q₀ reaches 5–50); not verified (roadmap B).
- Fixed Q10 = 2, no thermal optimum.
- Inorganic carbon is never limiting (roadmap D).
