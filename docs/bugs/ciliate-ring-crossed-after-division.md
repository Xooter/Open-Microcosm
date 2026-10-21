# Swimming ciliates sometimes have a crossed membrane ring after dividing

| Field | Value |
|---|---|
| **ID** | `ciliate-ring-crossed-after-division` |
| **Type** | `bug` |
| **Status** | `needs-reproduction` |
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

1. A plate with 10–20 *Paramecium* and abundant bacteria; let them divide.
2. Check the rings of the daughters with the model view or a self-intersection test on
   `body.ring`.

## Expected behaviour

The ring never self-intersects (a pellicle does not fold).

## Actual behaviour

Occasionally crossed after division (17/10/2026).

## Evidence

Observation during candidate batch 1. No automated test yet.

## Cause

Unknown.

## Proposed fix

First write a test that counts self-intersections over time after divisions; if it reproduces,
check the daughters' resampling and the self-contact (only plastic membranes self-collide).

## Workaround

None needed for results (mass and area are conserved).

## References

- Related fixes: furrow threshold and silhouettes in [validation](../validation/README.md#being-batches-1719102026).

## History

- 17/10/2026: observed.
- 21/10/2026: documented here; needs reproduction on the current code.
