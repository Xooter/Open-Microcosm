# Chemistry and light

## Dissolved fields

Every dissolved substance is a concentration field [µM] on the grid (`sim/chem_field`):

- **Diffusion** is exact in Fourier space, ĉ·exp(−D k² dt): stable for any dt and mass-conserving.
- **Advection** by the water is semi-Lagrangian: stable, but not exactly conservative. Several
  fields on the same grid share the departure points and interpolation weights.
- In a dish the glass cells are mirror-image ghosts (see [fluid and dish](fluid-and-dish.md)).

| Field | Notes |
|---|---|
| O₂ | Enters by exchange with the air over the whole surface. |
| CO₂ / pH | Carbonate system with fixed alkalinity; CO₂ equilibrates with the air (`MediumParams::co2Atmosphere`: the incubator sets 5–10 %) following Henry's law at the medium's temperature and salinity. pH comes from bicarbonate with the medium's pK′ (`carbonatePK`). |
| Organic matter | Food of the bacteria; decomposes; arrives with `organic_inflow` and detritus. |
| Phosphate, nitrogen, silicate | Macronutrients. |
| Iron, vitamin B₁₂ | Micronutrients; B₁₂ is released by bacteria. |
| Microcystin | Toxin released by dying cyanobacteria ([cyanobacteria](cyanobacteria.md)). |
| Autoinducer (AHL) | Quorum sensing ([bacteria](bacteria-and-phages.md)). |
| Phages | Lytic phage concentration. |
| Reagent | A single field, diffusing only once used ([reagents](reagents.md)). |
| Glutamine, lactate, fMLP | Diffuse only if there are animal cells ([animal cells](animal-cells.md)). |
| Signals | One field per released signal, up to 4 ([chemical signals](chemical-signals.md)). |
| Slime | *Physarum*'s extracellular slime ([*Physarum*](physarum.md)). |

Each cell takes its share with Monod kinetics and growth is limited by the scarcest nutrient
(Liebig): see [metabolism](metabolism-and-nutrients.md). The Si of a dead diatom stays in its
frustule (`AmbientParticles::mass`) and redissolves slowly.

**Every exchange with the water goes through `World::exchange(field, point, pmol, Flow)`** with the
process that caused it, so the mass budget closes ([measurement](measurement.md)).

## Medium parameters

`MediumParams` (in `WorldConfig::medium`): temperature, salinity, anoxia, alkalinity, P, N, Si, Fe,
B₁₂, air CO₂, osmolytes (sucrose) and the rest of the keys of `[biome.medium]`
([biomes](../content/biomes.md)). Environment and nutrients can be changed live from the Plate
panel; the change is an intervention ([reproducibility](reproducibility.md)).

## Light

The lamp illuminates uniformly and objects attenuate it with **Beer–Lambert**. Photosynthesis uses
the local light; phototaxis compares the light on either side of the anterior pole. In the
well-mixed culture the phototrophic biomass attenuates the column and each cell receives the mean.

## Code

- `src/sim/chem_field.*`, `src/game/world_chem.cpp` (pH, quorum, phages, *Bdellovibrio*).

## Tests

`tests/test_medium.cpp`, `tests/test_budget.cpp`, `tests/test_nutrients.cpp`.

## Limitations

- Temperature, base pH and salinity are global, not fields (roadmap phase D).
- Organic matter after a lysis in a small plate: [bug](../bugs/organic-matter-after-lysis.md).
