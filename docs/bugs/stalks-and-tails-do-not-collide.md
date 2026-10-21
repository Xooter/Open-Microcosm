# Stalks and sperm tails pass through cells

| Field | Value |
|---|---|
| **ID** | `stalks-and-tails-do-not-collide` |
| **Type** | `model-limitation` |
| **Status** | `open` |
| **Severity** | `minor` |
| **Area** | [`stalks-and-colonies`](../mechanics/stalks-and-colonies.md), [`sperm-and-casa`](../mechanics/sperm-and-casa.md) |
| **Affects** | `vorticella`, `epistylis`, `zoothamnium`, `carchesium`, `gomphonema`, suctorians, `human_sperm` |
| **Code** | `src/bio/stalk.cpp`, `src/game/world_stalk.cpp`, flagellum of `src/bio/locomotion.cpp` |
| **Reported** | 20/10/2026 |
| **Last verified** | 21/10/2026, commit `ae4affb` |
| **Fixed in** | — |

## Summary

Stalks are XPBD rods dragged by the water and sperm tails push the water along their length, but
neither is part of the contact system: cells swim through stalks and tails cross each other and
other cells.

## Steps to reproduce

1. A `pond` plate with *Vorticella*; watch a ciliate swim past a stalk.
2. A `fresh_semen` plate: tails overlap when two sperm cells meet.

## Expected behaviour

Stalks deflect and are deflected by passing cells; tails collide with each other and with cells
(sperm agglutination, accumulation at surfaces).

## Actual behaviour

No contact.

## Evidence

No stalk or tail term in `sim/contacts`.

## Cause

Confirmed: first stage of the stalk mechanism (M9) was implemented without cell–stalk contacts; the
tail is a line of forces, not a body.

## Proposed fix

Add stalk nodes and tail segments as point–edge contact sources (CPU and `solve.comp` on the GPU),
keeping determinism. Measure the cost with `microcosm_bench`.

## Workaround

None.

## References

- Mechanism M9 in [stalks and colonies](../mechanics/stalks-and-colonies.md).

## History

- 20/10/2026: noted when finishing the mechanics plan and the sperm work.
- 21/10/2026: documented here.
