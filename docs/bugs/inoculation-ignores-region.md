# Inoculating into a drawn region ignores the region's shape

| Field | Value |
|---|---|
| **ID** | `inoculation-ignores-region` |
| **Type** | `bug` |
| **Status** | `fixed` |
| **Severity** | `minor` |
| **Area** | [`reproducibility`](../mechanics/reproducibility.md) |
| **Affects** | Catalogue > Inoculate… with a region drawn |
| **Code** | `src/game/intervention.hpp` (`Inoculate`: `v[0]` species, `v[1]` count, `v[2]` radius) |
| **Reported** | 17/10/2026 |
| **Last verified** | 21/10/2026, commit `ae4affb` |
| **Fixed in** | commit "Inocular en la región dibujada" (21/10/2026) |

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

## Fix

The intervention carries the polygon (`Intervention::area`, serialised as `area=x,y;...` at the end
of the event line; old scenes are read as before). `World::inoculate` samples inside the polygon's
bounding box and keeps the points inside it and inside the dish. Catalogue > Inoculate… > *Where*:
whole plate, around the centre of the view or in the drawn region (the option appears once a region
exists). Test: `tests/test_scene.cpp` ("inocular en la región dibujada").

## Workaround

None needed.

## References

- Roadmap phase A.

## History

- 17/10/2026: noted in the roadmap.
- 21/10/2026: documented here.
- 21/10/2026: fixed.
