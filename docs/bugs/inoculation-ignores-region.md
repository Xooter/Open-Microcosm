# Inoculating into a drawn region ignores the region's shape

| Field | Value |
|---|---|
| **ID** | `inoculation-ignores-region` |
| **Type** | `bug` |
| **Status** | `open` |
| **Severity** | `minor` |
| **Area** | [`reproducibility`](../mechanics/reproducibility.md) |
| **Affects** | Catalogue > Inoculate… with a region drawn |
| **Code** | `src/game/intervention.hpp` (`Inoculate`: `v[0]` species, `v[1]` count, `v[2]` radius) |
| **Reported** | 17/10/2026 |
| **Last verified** | 21/10/2026, commit `ae4affb` |
| **Fixed in** | — |

## Summary

The `Inoculate` intervention stores a centre and a radius, not a polygon, so beings cannot be seeded
inside an arbitrary drawn region.

## Steps to reproduce

1. Draw a region with the Region tool.
2. Catalogue > Inoculate… a being.

## Expected behaviour

The inoculum lands inside the drawn polygon and the scene replays it the same way.

## Actual behaviour

It lands in a disc (or the whole plate).

## Evidence

`Intervention::Kind::Inoculate` carries only scalar values.

## Cause

Confirmed: the intervention format has no room for a polygon.

## Proposed fix

Reference the region stored in `Run` (it is already part of the scene) or serialise the polygon in
the event line; keep old scenes readable.

## Workaround

Inoculate with a radius centred on the region.

## References

- Roadmap phase A.

## History

- 17/10/2026: noted in the roadmap.
- 21/10/2026: documented here.
