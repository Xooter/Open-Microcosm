# Shape changes

How a being gets its outline and how that outline changes while it lives.

## Species shape

`[being.shape]` gives an ellipse or superellipse (`squareness`) with optional tapering of either
pole (`taper`, `taper_front`) and a curved axis (`bend`). Mass is that of the **spheroid** with the
major, minor and vertical (`thickness`) semi-axes, not that of the 2D silhouette.

## Silhouettes and processes

Beings with **body processes** (horns of *Ceratium*, arms of *Staurastrum*, caudal spine of
*Phacus*) and **notches** (sinus of *Cosmarium*, lobes of *Micrasterias*) have a silhouette
(`bio/silhouette`): the distance field of the body is joined with that of each process (a capsule
tapering along its curved axis) with a smooth minimum; the contour comes out of marching squares,
resampled densely by arc length. It is built once per species and is deterministic.

The silhouette is the shape of the membrane ring (`createCell`, `reshapeRestToOutline`,
`divisionRestShapeOutline`): processes collide, grow and divide with the cell and the wall's shape
memory keeps them rigid. It is centred on the mean of its points, which is the centre of
`soft_body::bestFit` (not the area centroid). Mass comes from the body (`outlineAreaFactor`); the
division furrow and `smallPrey` measure the body; mouth and flagellum go at the body's anterior pole
(`placeMouth`).

**Covering processes** (spines of loricas, plates and tests) are thinner than the membrane: they are
only drawn (`forEachProcess` in the render) and named on the card. Keys: `[[being.process]]` in
[beings](../content/beings.md#beingprocess-one-per-process).

## Rest shapes that change in life

`game/rest_shape` writes the rest shape point by point with `rest_shape::apply`, centres it on the
mean of its points (otherwise the shape memory would push the whole body) and carries the organelles
(each keeps its volume fraction along the axis). The rest area follows the shape with
`World::restShapeAreaScale`.

| Mechanism | Beings | Data | Code |
|---|---|---|---|
| Whole-body contraction (myonemes, Ca²⁺): shortens in ms, re-extends in seconds | *Stentor* (trumpet), peritrich zooids (bell) | `[being.contraction]` | `game/world_contraction.cpp` |
| Shape wave: metaboly (a widening wave runs from the anterior to the posterior pole) or sway | *Euglena*, *Peranema* (metaboly), *Dileptus* (proboscis sway) | `[being.shape_wave]` | `game/world_shape_wave.cpp` |
| New semicell growing from the isthmus after division | Desmids (*Cosmarium*, *Micrasterias*) | `[being.semicell]` | `game/world_semicell.cpp` |
| Extensible neck in the membrane | *Lacrymaria* | `neck_reach` | `game/world_neck.cpp` |
| Somersaults: stands on one tip and falls to the other side | *Closterium* | `[being.somersault]` | `game/world_somersault.cpp` |
| Induced morph (wider body, other mouth) | *Euplotes octocarinatus*, *Tetrahymena vorax* | `[being.morph]` | `game/world_signals.cpp` ([signals](chemical-signals.md)) |
| Rounding for mitosis, spreading after adhesion | Animal cells | `[being.cell]` | `game/world_animal.cpp` |

The contraction's eddy enters the water and the neighbours feel it. Contracted *Stentor* is the
shape of `[being.shape]`; extended it is `extension` times longer, with the radius going from
`foot_width`·R at the foot to R at the oral disc as a power of the length (`flare`), and R from
conserving the volume of the contracted cell (the cytoplasm is incompressible).

## Sources

- Contraction: Newman 1972; Mathijssen et al. 2019 (*Stentor*); Amos 1971 (spasmoneme).
- Metaboly: euglenoid movement as described for *Euglena gracilis*.

## Tests

`tests/test_mechanics.cpp`, `tests/test_silhouette.cpp`, `tests/test_candidates.cpp`.

## Limitations

- Daughters are born with every process; in *Ceratium* each daughter really regenerates the half of
  the theca it lacks.
- Desmids and diatoms really divide in a plane parallel to the view; here they constrict in the view
  ([bug](../bugs/diatom-division-geometry.md)).
