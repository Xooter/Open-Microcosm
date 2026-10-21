# In Gause's competition, whichever *Paramecium* gets ahead wins, not *P. aurelia*

| Field | Value |
|---|---|
| **ID** | `gause-aurelia-does-not-win` |
| **Type** | `validation-gap` |
| **Status** | `open` |
| **Severity** | `major` |
| **Area** | [`well-mixed-culture`](../mechanics/well-mixed-culture.md) |
| **Affects** | `paramecium`, `paramecium_aurelia`; lesson `gause_competition`; test `tests/test_gause.cpp` |
| **Code** | `src/game/culture.cpp`, `src/bio/cell_physiology.cpp` |
| **Reported** | 16/10/2026 |
| **Last verified** | 21/10/2026, commit `ae4affb` (the test still carries `doctest::should_fail()`) |
| **Fixed in** | — |

## Summary

In Gause's (1934) mixed cultures *P. aurelia* excludes *P. caudatum*. In the simulated culture the
competitive exclusion happens, but the winner is whichever species gets ahead early, so the classic
result is not reproduced.

## Steps to reproduce

1. `ctest --test-dir build/release -L validacion -R gause` or
   `./build/release/bio_tests -tc="*aurelia*"`.
2. The test is marked `should_fail`: it "passes" while the bug exists and fails when it is fixed.

## Expected behaviour

Gause (1934, fig. 23; data digitised in gauseR by Clark et al.): on day 25, *P. aurelia* 140 vs
*P. caudatum* 23 (relative volume). *P. aurelia* wins in every replicate.

## Actual behaviour

*P. aurelia* wins in 2 or 3 of 5 replicates. The monoculture plateaus and the exclusion itself are
correct (see [validation](../validation/README.md#gause-in-the-culture-phase-p1-step-5)).

## Evidence

`tests/test_gause.cpp`, test marked `doctest::should_fail()` with the comment "DISCREPANCIA
CONOCIDA".

## Cause

Confirmed by analysis: *P. aurelia*'s only advantage in the model is feeding allometry (~20 % more
clearance per biomass, Hansen et al. 1997), which is not enough against the higher µmax of
*P. caudatum*. Gause attributed part of his result to the sensitivity of *P. caudatum* to waste
products of the medium, which the model lacks. *P. aurelia* has no feeding measurements of its own
(they are scaled from *P. caudatum*).

## Proposed fix

- Model the real mechanism: accumulation of a waste product (or the bacterial metabolites Gause
  described) and a published sensitivity per species.
- Find measured ingestion and clearance for *P. aurelia*.
- Do **not** tune µmax or clearance to force the winner.

## Workaround

None. The lesson `gause_competition` shows exclusion without asserting the winner.

## References

- Gause G.F. (1934) *The Struggle for Existence*, ch. V.
- Hansen P.J., Bjørnsen P.K., Hansen B.W. (1997) Limnol. Oceanogr. 42: 687–704.
- Roadmap phase B.

## History

- 16/10/2026: found when the well-mixed culture reproduced Gause's protocol.
- 21/10/2026: documented here.
