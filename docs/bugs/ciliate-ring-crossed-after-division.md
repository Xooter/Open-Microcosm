# Swimming ciliates sometimes have a crossed membrane ring after dividing

| Field | Value |
|---|---|
| **ID** | `ciliate-ring-crossed-after-division` |
| **Type** | `bug` |
| **Status** | `investigating` |
| **Severity** | `minor` |
| **Area** | [`soft-bodies`](../mechanics/soft-bodies.md), [`life-cycle-and-division`](../mechanics/life-cycle-and-division.md) |
| **Affects** | `paramecium`, `paramecium_aurelia`, `tetrahymena`, `colpidium` |
| **Code** | `src/bio/division.cpp`, `src/sim/contacts.cpp` |
| **Reported** | 17/10/2026 |
| **Last verified** | Unknown: not re-checked after the silhouette and division fixes of 19–20/10/2026 |
| **Fixed in** | — |

## Summary

About 20 s after dividing, the membrane ring of a swimming ciliate was sometimes seen crossed
(self-intersecting). It is not known whether it is a transient crossing during collisions or a fold
that stays.

## Steps to reproduce

Reproduced on 21/10/2026 with a test counting self-intersections of `RingBody::poly` every 0.5 s
(trial code, not committed):

1. 15 *Paramecium* inoculated on an empty plate (seed 17), 20 000 bacteria, all forced to divide.
2. Count the samples with a crossed ring over 60 s.

## Expected behaviour

The ring never self-intersects (a pellicle does not fold).

## Actual behaviour

Before the fix of 21/10/2026: most samples crossed, also without division. After it: a single
swimmer no longer folds; with 15 dividing cells, ~15 % of the samples are crossed and a fold can last
~30 s.

## Evidence

- The folds are local: edges *i* and *i*+2 (a zigzag of one or two edges), not a crossed body.
- Only the fast *Paramecium* (cilia at full speed): *Tetrahymena*, *Colpidium*, *Euglena*,
  *Chlamydomonas*, *Peranema*, *Stentor* and *Amoeba* did not fold. At 60 % of its speed it did not
  fold either. Neither the helical swimming nor the oral notch is the cause (removed one at a time).
- Cause 1 (fixed): every first fold of a lone cell happened with its posterior tip against the glass
  (1.8 µm, the particle radius) while backing up at 300 µm/s in the avoiding reaction.
- Cause 2 (open): in a crowd, the first folds happen at the posterior pole (rest x ≈ −107 to
  −110 µm: mothers elongated by `divisionRestShape`) with another cell 3–8 µm away, mostly while
  reversing.
- Once deep, a fold can outlast the contact that made it (seen 4 s, 200 µm away from the wall).

## Cause

1. Confirmed and fixed: `confineToDish` projected each membrane point onto the glass, so a sharp
   pole pushed into it was flattened into a zigzag.
2. Suspected: the cell–cell contacts (point against edge, Jacobi) do the same to a sharp pole
   squeezed between two cells; and the restoring constraints (window bending, shape memory) do not
   always undo a deep fold.

## Proposed fix

1. Done: a pellicle, wall or theca is moved whole out of the glass (`confineToDish`, CPU and GPU);
   only a plastic membrane moulds itself to it.
2. Pending: resolve contacts of non-plastic bodies with a rigid component (as for the wall) or
   limit the per-point correction at a pole; then check why the window bending does not unfold a
   deep zigzag.

## Workaround

None needed for results (mass and area are conserved).

## References

- Related fixes: furrow threshold and silhouettes in [validation](../validation/README.md#being-batches-1719102026).

## History

- 17/10/2026: observed.
- 21/10/2026: documented here; needs reproduction on the current code.
- 21/10/2026: reproduced; the wall cause fixed, the contact cause open.
