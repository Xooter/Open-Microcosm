# Organic matter reaches hundreds of mM after a lysis in a small plate

| Field | Value |
|---|---|
| **ID** | `organic-matter-after-lysis` |
| **Type** | `model-limitation` |
| **Status** | `investigating` |
| **Severity** | `minor` |
| **Area** | [`chemistry-and-light`](../mechanics/chemistry-and-light.md), [`life-cycle-and-division`](../mechanics/life-cycle-and-division.md) |
| **Affects** | Small plates where a large cell lyses |
| **Code** | `src/game/world.cpp` (lysis), `src/sim/chem_field.cpp` |
| **Reported** | 14/10/2026 |
| **Last verified** | Unknown: not re-measured since it was reported |
| **Fixed in** | — |

## Summary

When a cell lyses, ~40 % of its content dissolves into the water. In a small plate the local organic
matter concentration then reaches hundreds of mM. Mass is conserved correctly, but such a value is
far above natural waters and may mean the film volume used for the conversion is not the right one.

## Steps to reproduce

1. A small plate (≤ 0.5 mm) with a large cell (*Amoeba*, *Paramecium*).
2. Lyse it with the Lyse tool and read the organic matter concentration with the Measure tool.

## Expected behaviour

Local concentrations comparable with a real lysed cell diluting into the surrounding water column.

## Actual behaviour

Hundreds of mM next to the lysis.

## Evidence

[Validation, model problem 4](../validation/README.md#model-problems-found-while-validating-mvp-phase).

## Cause

Suspected: the volume of each chemistry cell (`gapHeight` × cell area) is smaller than the volume a
lysed cell really dilutes into, or the dry-mass to concentration conversion.

## Proposed fix

Check the conversion against a hand calculation; decide whether the film depth should be larger
than the cell thickness.

## Workaround

None needed for mass budgets (they close).

## References

- Roadmap phase B.

## History

- 14/10/2026: observed during the MVP validation.
- 21/10/2026: documented here.
