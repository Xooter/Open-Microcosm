# Starved protist cultures keep dividing into smaller cells

| Field | Value |
|---|---|
| **ID** | `starved-cultures-keep-dividing` |
| **Type** | `model-limitation` |
| **Status** | `investigating` |
| **Severity** | `minor` |
| **Area** | [`life-cycle-and-division`](../mechanics/life-cycle-and-division.md) |
| **Affects** | Well-mixed cultures at their carrying capacity (`paramecium`, `paramecium_aurelia`); probably the plate too |
| **Code** | `src/bio/cell_physiology.cpp`, `src/bio/metabolism.cpp`, `src/game/culture.cpp` |
| **Reported** | 21/10/2026 |
| **Last verified** | 21/10/2026 |
| **Fixed in** | — |

## Summary

In a food-limited culture the biovolume of each species reaches a plateau, but the number of
individuals keeps growing for weeks: cells divide while getting smaller.

## Steps to reproduce

1. A Gause tube (`gause_tube`, program of `tests/test_gause.cpp`) with 20 *P. caudatum*.
2. Count individuals and biovolume every few days up to day 40.

## Expected behaviour

Gause (1934, fig. 21) shows the plateau both in individuals and in volume, reached around day 10.

## Actual behaviour

*P. caudatum*: biovolume 261, 279, 283, 310·10⁶ µm³ on days 8, 16, 20 and 40, while individuals go
1089, 1969, 2589, 2930. On day 40 the mean cell has ~45 % of the species' typical volume
(*P. aurelia*: ~38 %).

## Evidence

The measurements above (trial run of 21/10/2026). The plateau test of `tests/test_gause.cpp` uses
biovolume for this reason.

## Cause

Unknown. Candidate: the division checkpoint lets a cell divide at a biomass well below the species'
normal division size when reserves allow it, so starvation produces more and smaller cells.

## Proposed fix

Check the division size against the published size of starved *Paramecium* and, if needed, require
the species' division size (with its genetic variation) before cytokinesis.

## Workaround

Compare species by biovolume (series `biovolume:<id>`, condition `biovolume_fraction`).

## References

- Gause G.F. (1934) *The Struggle for Existence*, ch. V, fig. 21.

## History

- 21/10/2026: seen while reproducing Gause's protocol with the daily thinning.
