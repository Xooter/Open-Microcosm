# Hyphae, cyanobacteria and oat flakes do not respond to stains

| Field | Value |
|---|---|
| **ID** | `stains-missing-targets` |
| **Type** | `bug` |
| **Status** | `open` |
| **Severity** | `minor` |
| **Area** | [`stains`](../mechanics/stains.md) |
| **Affects** | Lugol, Gram and the other stains on plates with fungi, cyanobacteria or *Physarum* oat flakes |
| **Code** | `src/render/renderer.cpp` (hyphae drawn with fixed colours `kHypha`, `kHyphaWall`), `src/game/stain.cpp` |
| **Reported** | 17/10/2026 |
| **Last verified** | 21/10/2026, commit `ae4affb` |
| **Fixed in** | — |

## Summary

The render asks `evaluateStain` for each target it draws, but hyphae, cyanobacterial filaments and
oat flakes are drawn with fixed colours, so a stain never changes them.

## Steps to reproduce

1. Create a `pond` plate (it has *Saprolegnia*, *Allomyces* and cyanobacteria).
2. Apply Gram or Lugol in the Stains panel.

## Expected behaviour

- Cyanobacteria are gram-negative: pink with Gram; they store cyanophycean starch (glycogen-like),
  brownish with Lugol.
- Oomycete and fungal hyphae have glucan/cellulose walls; oat flakes are mostly starch: blue-black
  with Lugol.

## Actual behaviour

They keep their bright-field colour.

## Evidence

No `evaluateStain` call in the hyphae, filament or oat flake drawing paths.

## Cause

Confirmed: those structures have no `StainTarget` mapping.

## Proposed fix

Add their targets (`CellWall`, `Storage`) to `speciesChemistry` / the fungus and cyanobacterium
tables and call `evaluateStain` where they are drawn. Stains are render-only, so results do not
change.

## Workaround

None.

## References

- Roadmap phase A.

## History

- 17/10/2026: noted in the roadmap.
- 21/10/2026: documented here.
