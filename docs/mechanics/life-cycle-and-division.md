# Life cycle and division

Every cell with a `Metabolism` component has biomass, reserves and age, and goes through growth,
reproduction and death (`bio/division`, `World::updateLifecycle`).

## Size and mass

- The rest area is that of the biomass plus the content of the food vacuoles.
- Mass is that of the **spheroid** with major, minor and vertical (`thickness`) semi-axes times the
  relative dry matter (`cellMassScale`), not that of the sphere of the 2D silhouette. Before
  16/10/2026 an elongated paramecium weighed almost twice its real mass.

## Cell-cycle checkpoint

At division size the cell stops investing in biomass and gathers the reserves for cytokinesis
(otherwise a food-limited cell would never divide).

## Mitosis (binary fission)

Component `Dividing`, triggered with biomass ×2 and reserves for the cost (the controlled being pays
the most):

1. Nucleus and contractile vacuole duplicate and separate towards the two poles.
2. The rest shape transforms from the mother into two daughters (`divisionRestShape`, or
   `divisionRestShapeOutline` with a silhouette) and the actomyosin furrow constricts the cell
   perpendicular to its long axis (transverse fission, as in *Paramecium*). Peritrichs divide
   longitudinally (`division_axis = "longitudinal"`).
3. When both sides of the furrow touch (`furrowClosed`, 3 membrane radii) the ring splits.
4. Daughters are resampled (`resampleRing`) with the perimeter of their species' shape and return to
   it (`CellShape`). Each daughter has at least 20 % of the total area. Each one mutates its own
   copy of the genome ([genome](genome-and-organelles.md)).

When the controlled being divides, the camera follows the mother.

## Other ways to reproduce

| Mode | Beings | Data |
|---|---|---|
| Autospores (several daughters inside the mother) | *Chlorella* | `fissions_per_cycle` |
| Reproductive cyst (2–4 daughters, still and not eating) | *Colpoda* | `divides_in_cyst` |
| Budding | *Saccharomyces*, *Candida*, *Euglypha* | `budding` |
| Daughter colonies released by the mother | *Volvox*, *Gonium*, *Pandorina*, *Eudorina* | `daughter_colonies`, `colony_cells`, `mother_dies_at_release` |
| Filament growth and septation, fragmentation | *Spirogyra*, *Oedogonium*, *Ulothrix*… | `filament`, `fragmentation_per_hour` |
| Colony branch on the mother's stalk | *Zoothamnium*, *Carchesium*, *Epistylis*, *Gomphonema* | `[being.stalk] colony` |
| Conjugation (sexual) | Ciliates with mating types; *Spirogyra* (scalariform) | `conjugation`, `[being.algal_conjugation]` |
| Cell cycle with phases | Animal cells | [animal cells](animal-cells.md) |

## Cysts

Under bad conditions (starvation, desiccation, temperature) beings with `encysts` form a resting
cyst: no locomotion, minimal metabolism, resistant; they excyst when conditions return. *Didinium*
encysts when the prey is gone (Beers 1927).

## Death and lysis

- Below half the birth biomass, or outside tolerance for long, the cell dies.
- **Lysis:** ~40 % of the content dissolves into nutrients (organic matter, P, N…) and the rest
  remains as detritus. A diatom leaves its frustule with its Si.
- Animal cells die by **apoptosis** (no lysis: apoptotic bodies) and the sperm cell stays whole and
  immotile (`death = "immotile"`).
- `World::deathTally()` records causes of death.

## Code

`src/bio/division.*`, `src/game/world.cpp` (`updateLifecycle`), `src/game/world_algal_conjugation.cpp`.

## Tests

`tests/test_lifecycle.cpp`, `tests/test_algae.cpp`, `tests/test_candidates.cpp` (every being
reproduces by its normal route; `largos` suite).

## Limitations

- Diatoms and desmids divide by a waist furrow: [bug](../bugs/diatom-division-geometry.md).
- Volvocines release 4 daughter colonies (really one per cell: 16 or 32); biomass is shared the same,
  so the growth rate does not change.
