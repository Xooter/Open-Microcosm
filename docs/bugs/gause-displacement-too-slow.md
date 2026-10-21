# In Gause's competition *P. aurelia* displaces *P. caudatum* more slowly than Gause saw

| Field | Value |
|---|---|
| **ID** | `gause-displacement-too-slow` |
| **Type** | `validation-gap` |
| **Status** | `fixed` |
| **Severity** | `minor` |
| **Area** | [`well-mixed-culture`](../mechanics/well-mixed-culture.md) |
| **Affects** | `paramecium`, `paramecium_aurelia`; lesson `gause_competition`; test `tests/test_gause.cpp` |
| **Code** | `src/game/culture.cpp`, `src/bio/cell_physiology.cpp`, `[physiology.*.max_growth]` of both beings |
| **Reported** | 21/10/2026 |
| **Last verified** | 21/10/2026 |
| **Fixed in** | commit "Mutación realista (fase 1)" (21/10/2026) |

## Summary

With Gause's full protocol (5 cm³, 20 + 20, daily 10 % thinning) *P. aurelia* wins in every
replicate, but on day 25 *P. caudatum* still holds about a third of the biovolume; Gause had 23 of
163 (0.14).

## Steps to reproduce

1. `./build/release/microcosm_tests -tc="*desplazamiento*" -s`.
2. Look at the mean biovolume fraction of *P. caudatum* on day 25.

## Expected behaviour

Gause (1934, fig. 22; data in gauseR, `gause_1934_book_f22`): on day 25, *P. aurelia* 140 and
*P. caudatum* 23 in relative volume.

## Actual behaviour

Mean fraction of *P. caudatum* on day 25: 0.33 (5 replicates); *P. aurelia* ahead in 5 of 5. By
day 60 *P. caudatum* is below 1 % in every replicate.

## Evidence

`tests/test_gause.cpp`, "el desplazamiento llega al de Gause al día 25" (`should_fail`).

## Cause

Suspected, not confirmed:

- The µmax of both species are logistic fits to Gause's own curves (`[physiology.*.max_growth]`,
  0.946 and 0.707 per day). Those curves already include the daily 10 % thinning, so the fitted r is
  net of a loss of ~0.105 per day; and with Gause's Table XI (buffered medium) *P. aurelia* has the
  higher coefficient (1.12 vs 0.79), while the Osterhout fits give it the lower one. Gause says the
  displacement is driven "primarily" by that coefficient, so its value sets the speed.
- *P. aurelia*'s feeding is scaled from *P. caudatum* by allometry (no own measurement).

## Proposed fix

- Refit µmax from Gause's curves with the thinning in the model (µ = r + D), from independent
  sources if possible (division rates of *P. aurelia* at 26 °C).
- Find measured ingestion and clearance for *P. aurelia*.
- Do **not** tune µmax or clearance to force the speed.

## Fix

Neither of the suspected causes: the size gene of both species evolved down during the experiment,
because mutation was far above the published rates
([bug](starved-cultures-keep-dividing.md)). With realistic mutation and pure-line inocula, the mean
fraction of *P. caudatum* on day 25 is 0.10 (Gause: 0.14) and the test no longer carries
`should_fail`. The µmax fitted to Gause's curves (net of the daily thinning) remain an estimate.

## Workaround

None needed.

## References

- Gause G.F. (1934) *The Struggle for Existence*, ch. V (sections II–III, Table XI, Fig. 22).
- [`gause-aurelia-does-not-win`](gause-aurelia-does-not-win.md) (fixed: the missing thinning).

## History

- 21/10/2026: split from `gause-aurelia-does-not-win` once the full protocol was in place.
- 21/10/2026: fixed by the realistic mutation of phase 1 of the genetics plan.
