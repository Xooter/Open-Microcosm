# Locomotion

Locomotion adds velocity to the body particles **without inertia**: at low Reynolds number a cell
moves only while it propels and stops at once. Speed depends on the current propulsion force plus
the drag of the fluid (`bio/locomotion`).

## Types

| Type (`locomotion`) | Typical speed | How it works |
|---|---|---|
| `cilia` | ~600 µm/s (× `relative_speed`) | Continuous gliding; force dipole in the water. Ciliates and, with a tiny speed, raphid diatoms. |
| `flagellum` | ~80 µm/s, with wobble | Impulses; the flagellum is drawn and pushes the water. Puller by default; `flagellum = "posterior"` is a pusher (choanoflagellates, sperm). |
| `pseudopod` | ~6 µm/s | Amoebae extend lobes (plastic membrane); controlled by click and drag. |
| `crawl` | Biological time (µm/h) | Animal cells: lamellipodium in front, focal adhesions pull the body; persistent random walk ([animal cells](animal-cells.md)). |
| `none` | — | Tissue cells, desquamated squamous cells, smear leukocytes. |

Viscosity follows temperature, so cold water slows swimmers (except where a measured Q10 of the
swimming speed is used).

## Variations (data)

| Variation | Key | Beings |
|---|---|---|
| Helical swimming (a lateral oscillation seen from above) | `helix_radius`, `helix_period` | Dinoflagellates, ciliates, euglenoids, sperm (ALH and BCF) |
| Gliding with a rigid flagellum | `flagellar_gliding` | *Peranema* (Saito et al. 2003) |
| Walking on cirri | `walks` | *Euplotes*, *Stylonychia*, *Chilodonella* |
| Raphe gliding | `raphe_glider` | *Navicula*, *Pinnularia* |
| Long flagellum with thrust along it | `flagellum_length` | Human sperm (50 µm tail) |
| Passive path, no steering | `[being.swim_path]` | Sperm |
| Jumps | `[being.jump]` | *Halteria*, *Mesodinium* |
| Phototaxis | `phototaxis` | *Euglena*, *Chlamydomonas*, volvocines |

Behaviour that steers swimming (avoiding reaction, acid trap, chemotaxis) is in
[behavioural responses](behavioural-responses.md).

## Controlling a being

The Control tool (or `P` over a cell) gives the user a being's locomotion; each species keeps its
physical locomotion:

- **flagellum:** W or left click beats; turns towards the mouse;
- **cilia:** W/S forward and back, A/D turn;
- **pseudopod:** left click extends a pseudopod towards the mouse; touching a smaller cell, it
  engulfs it.

Movement, phagocytic cups and feeding are generic for every cell with `Locomotion`: the controlled
being differs only in where its controls come from. Daughters that are not controlled get
`Autopilot`. When the controlled being divides the camera follows the mother; if it dies another
can be controlled.

## Code

`src/bio/locomotion.*`, `src/game/world_swim.cpp`, `src/ai/autopilot.*`, `src/app/app.cpp`
(`toolInput`).

## Tests

`tests/test_player.cpp`, `tests/test_mechanics.cpp`, `tests/test_sperm.cpp`.
