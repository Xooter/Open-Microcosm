# Animal cells

Cells of multicellular organisms in culture (`bio/animal_cell`, `game/world_animal.cpp`): real cell
types and lines with published data (doubling time, migration speed, size), with the source in
`[physiology.<id>.*]`. No "generic" cells without a reference.

Catalogue: human dermal fibroblast, HeLa, NIH/3T3, HaCaT, neutrophil, monocyte, lymphocyte, buccal
squamous cell (slide) and the human spermatozoon ([sperm](sperm-and-casa.md)).

What distinguishes them from a protist: they live stuck to the bottom, migrate by crawling, divide
through a cycle with phases and checkpoints, and die by apoptosis (an orderly suicide, without
bursting) when they lack what they need. Everything runs in **biological time** (`World::bioScale`):
migration is actin polymerisation, not water physics.

## Adhesion and migration

- `Adherent` (`[being.cell] adherent`): the water does not drag them. A cell falling from
  suspension adheres and spreads in ~1 h (extension 0 → 1).
- `LocomotionType::Crawl`: lamellipodium in front, focal adhesions pull the body. **Persistent
  random walk** with the speed (`migration_speed`) and persistence (`persistence`) from the data:
  validated against the mean squared displacement of Fürth.
- **Contact inhibition of locomotion** (Abercrombie and Heaysman 1954): colliding cells slow down
  and move apart (`contact_inhibition_locomotion`).

## Cell cycle

Phases G1/S/G2/M with durations from the data (`phase_g1`, `phase_s`, `phase_g2`, `phase_m`);
without data, a typical proliferating human cell (Alberts et al., *Molecular Biology of the Cell*,
6th ed., fig. 17-4: G1 11 h, S 8 h, G2 4 h, M 1 h).

- **Restriction point** (Pardee 1974): without enough serum (`min_serum`) the cell does not commit in
  G1 and goes to **G0**.
- **Contact inhibition of proliferation** (Todaro and Green 1963): with the periphery occupied by
  neighbours, G0; the confluence plateau (`confluence` [cells/cm²]) is validated. HeLa does not stop.
- **Mitosis** rounds the cell (`rounded_radius`) and uses `reshapeRestEven`.

## Metabolism

Michaelis–Menten uptake of **glucose** and **glutamine** (`glucose_vmax`, `glucose_km`,
`glutamine_vmax`, `glutamine_km`), O₂ uptake (`o2_uptake`) and **lactate** secretion
(`lactate_per_glucose`: Warburg effect), which consumes bicarbonate and acidifies the medium (phenol
red turns yellow). Glutamine, lactate and fMLP are fields that only diffuse if there are animal
cells. Incubator: DMEM at 37 °C and 10 % CO₂ ([prepared plates](prepared-plates-and-tissues.md)).

## Apoptosis

Without energy, without serum (`apoptosis_without_serum_h`) or by age (`lifespan`), the cell runs
apoptosis and leaves apoptotic bodies **without lysis** (Kerr et al. 1972). Trypan blue and
eosin stain dead and late-apoptotic cells (`StainTarget::DamagedMembrane`, [stains](stains.md)).
`death = "immotile"` (sperm) dies whole and still: `CellPhase::Dead`, no metabolism, no propulsion,
out of the census.

## Neutrophil

Follows the **fMLP** released by bacteria (`fmlp_chemotaxis`) and phagocytoses them with a phagocytic
cup ([feeding](feeding-and-phagocytosis.md)); in `semen_leukocytes` it phagocytoses sperm. Its speed
is validated.

## Code

`src/bio/animal_cell.hpp`, `src/game/world_animal.cpp`.

## Tests

`tests/test_animal_cells.cpp` (doubling, MSD, confluence plateau, lactate/glucose, neutrophil speed).

## Limitations

- Cell culture is seen in bright field; phase contrast is pending (roadmap C).
- No cell–cell adhesion (epithelial islets of keratinocytes), no Hayflick limit, no monocyte
  differentiation.
- `nih_3t3` and `hacat` are loaded but not yet validated like the fibroblast and HeLa.
- Not supported in the well-mixed culture (`Culture::supports`).
