# Soft bodies

Protists, algae, yeasts and animal cells are **soft bodies**: a ring of membrane particles plus
interior particles (organelles, granules), solved with XPBD (`sim/soft_body`, `sim/contacts`). They
are gelatinous and semi-transparent and every shape change emerges from the constraints, not from
animation.

## Inertia-free XPBD

Each step predicts x* = x + v_external·dt, with **no accumulated velocity**, and then projects the
constraints. Each constraint relaxes its error with a **rate [1/s]**: correction = 1 − exp(−rate·h).
The result does not depend on the sub-step h and the behaviour is viscoelastic: the cell deforms
and returns to its shape without bouncing.

Constraints per body:

| Constraint | Purpose |
|---|---|
| Edge length | Membrane tension. |
| Bending by local *shape matching* in 5-point windows | The rotation never reflects, so it undoes folds (an i↔i+2 distance constraint does not). |
| Area | Incompressible water. |
| Global shape memory (`shapeRate`, `shape_memory`) | Rigid pellicle high, amoeboid cortex ~0. |
| Organelle anchoring | Organelles held by the cytoskeleton. |

## Contacts

Point-against-edge, with a Jacobi scheme, **interleaved** with the constraints in each iteration of
the sub-step. Only the membrane points within their contact radius (+ slack) of another body are
considered, measured once per sub-step; two passes per sub-step (`SoftBodyTuning::contactPasses`).
At the end of the step a pass pushes out any membrane particle left inside another cell.

Only the **plastic membrane** collides with itself (it folds when making pseudopods): a wall, theca
or pellicle does not fold, and the two sides of a thin horn are closer than two particle radii.
Bacteria and detritus are excluded from the interior of cells. Parts of the same compound organism
share a contact group and do not collide with each other.

## Plastic membrane (amoebae)

`soft_body::makePlastic`: no edge tension and no shape memory; cortical tension (curvature flow)
that smooths wrinkles without stopping lobes; area conserved in ~1 s (the cytoplasm flows);
remeshing that adds or removes membrane (membrane reservoir). Its membrane has a higher inverse
mass, so it yields in contacts. Small plastic amoebae (perimeter below 48 membrane spacings) use
finer remeshing and a relaxed cortex during cytokinesis.

## Large shape changes

- `reshapeRestEven` distributes the rest shape by arc length (a cell going from round to spindle,
  or rounding up for mitosis). The radial projection of `reshapeRest` bunches up points and leaves
  the shape crossed; with window bending, the cell then pushes itself.
- Shapes that change during life use `rest_shape::apply` ([shape changes](shape-changes.md)).

## Topology

`replaceRing` / `resizeRing` change the ring (fusion, membrane insertion). Topology events are
applied at the end of the step.

## Compound bodies

Colonies and filaments are soft bodies joined by adhesions (`World::AdhesionLink`). A filament joint
is the entire cross wall (5 links across and 2 diagonal) and is cut as a whole
([stalks and colonies](stalks-and-colonies.md)).

## Statistical level and GPU

Far from the view bodies are solved with fewer XPBD sub-steps; off-screen they are moved as solids.
XPBD and contacts can run on the GPU (`solve.comp`), deterministically. See
[performance and GPU](performance-and-gpu.md).

## Safety net

`recoverNonFinite` restores bodies whose positions became non-finite and counts them
(`fallas_numericas` in the export). With `-ffast-math`, use `finiteNumber` (`core/finite`), never
`std::isfinite`.

## Code

`src/sim/soft_body.*`, `src/sim/contacts.*`, `src/sim/substep_solver.hpp`, `src/gpu/`,
`assets/shaders/*.comp`.

## Tests

`tests/test_cells.cpp`, `tests/test_silhouette.cpp`, `tests/test_lifecycle.cpp`.

## Limitations

- [`ciliate-ring-crossed-after-division`](../bugs/ciliate-ring-crossed-after-division.md).
- Stalks and sperm tails are not part of the contacts: [bug](../bugs/stalks-and-tails-do-not-collide.md).
