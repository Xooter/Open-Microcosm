# Long, cylindrical diatoms cannot divide; diatoms divide by a waist furrow

| Field | Value |
|---|---|
| **ID** | `diatom-division-geometry` |
| **Type** | `model-limitation` |
| **Status** | `open` |
| **Severity** | `minor` |
| **Area** | [`life-cycle-and-division`](../mechanics/life-cycle-and-division.md) |
| **Affects** | `pinnularia` (simulated as an ellipse), `navicula`, `closterium`, long pennate diatoms not yet loaded (*Ulnaria*, *Nitzschia*) |
| **Code** | `src/bio/division.cpp` (`divisionRestShape`, `furrowClosed`) |
| **Reported** | 17/10/2026 |
| **Last verified** | 21/10/2026, commit `ae4affb` (`pinnularia.toml` still notes the workaround) |
| **Fixed in** | — |

## Summary

With `squareness` 4 (a cylinder) or with more than ~80 µm of length, the division furrow does not
close. *Pinnularia*'s real linear valve with parallel sides was loaded as an elongated ellipse to
avoid it. In addition, every diatom divides by constricting at the waist, while a real diatom
divides in a plane parallel to the valves (daughters lie one over the other in valve view).

## Steps to reproduce

1. Set `squareness = 4.0` in `beings/pinnularia.toml`.
2. Run the being's reproduction test in `tests/test_candidates.cpp` (`largos` suite).

## Expected behaviour

The cell divides into two daughters with the parent's outline; in valve view the division plane is
parallel to the valves, each daughter keeping one parental valve (MacDonald–Pfitzer size reduction).

## Actual behaviour

The furrow stalls before `furrowClosed` and the cell stays dividing.

## Evidence

Comment in `assets/content/base/beings/pinnularia.toml`.

## Cause

Suspected: the daughters' rest shape is derived from the mother's ellipse and the furrow geometry
assumes a waist; in a cylinder the two flat sides resist the constriction.

## Proposed fix

A named division mode for diatoms (`division = "valvar"`) that splits the cell in the girdle view
without a furrow, with size reduction of the daughter that inherits the smaller valve.

## Workaround

Elliptical outlines (`squareness` 2) and lengths below ~80 µm.

## References

- Round F.E., Crawford R.M., Mann D.G. (1990) *The Diatoms*. Cambridge University Press.

## History

- 17/10/2026: found in candidate batch 1.
- 21/10/2026: documented here.
