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
| **Last verified** | 21/10/2026, commit `bded214` (the test still carries `doctest::should_fail()`) |
| **Fixed in** | — |

## Summary

In Gause's (1934) mixed cultures *P. aurelia* excludes *P. caudatum*. In the simulated culture the
competitive exclusion happens, but the winner is whichever species gets ahead early, so the classic
result is not reproduced.

## Steps to reproduce

1. `ctest --test-dir build/release -L validacion -R gause` or
   `./build/release/microcosm_tests -tc="*aurelia*"`.
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

Partly revised on 21/10/2026 after reading Gause (1934, ch. V, sections II–III) directly. The
model's protocol differs from Gause's in three ways, and the first is the mechanism Gause himself
gives for the displacement:

1. **Daily thinning.** Every day Gause stirred the culture and took out a 0.5 cm³ sample of the
   5 cm³ that was counted "and then destroyed": a tenth of the population. In section III(5) he
   says this "elementary movement" of thinning and refilling is what produces the displacement,
   and that *P. aurelia* gains each time "primarily owing to its high coefficient of
   multiplication". The model's protocol has no thinning, so once the food is shared the outcome
   is fixed by whoever got ahead early.
2. **Volume and inoculum.** The tube held 5 cm³ (10 c.c. centrifuge tubes filled to 5 c.c.) with
   20 individuals, or 20 + 20; 0.5 cm³ was the counted sample. `vessels/gause_tube` has 0.5 mL and
   the tests inoculate 5 + 5.
3. **Units.** Gause's numbers are relative volumes: *P. caudatum* individuals per 0.5 cm³ and
   *P. aurelia* individuals × 0.39 (Table X). The test compares individuals with 23 / 163, which is a
   fraction of volume.

His advantage figures for *P. aurelia* (Table XI, buffered medium, "half-loop": b = 1.1244 vs
0.7944 per day, K = 105 vs 64) contradict the model's µmax, which comes from logistic fits to the
unbuffered Osterhout curves and gives *P. caudatum* the higher rate. Gause attributes the better
yield of *P. aurelia* to its resistance to the waste products of the food bacterium, *Bacillus
pyocyaneus* (footnote 1; Gause, Nastukova and Alpatov 1934). That paper also says *P. caudatum* has
the higher coefficient of geometric increase under its conditions, while *P. aurelia* resists
waste products better.

Trials on 21/10/2026 (not committed):

- Adding the daily 10 % dilution before the medium change: *P. aurelia* ends up excluding
  *P. caudatum* in 5 of 5 replicates, but in 3 of them only after day 25 (day 25: 3/5).
- Also moving to 5 mL with 20 + 20: the populations boom and crash from one day to the next.
  Probably the well-mixed gas exchange (D / depth²) leaves a 2.5 cm column short of O₂. Gause's
  tubes did not do this, so oxygenation of a deep tube needs its own review before the vessel
  changes.

## Workaround

None. The lesson `gause_competition` shows exclusion without asserting the winner.

## References

- Gause G.F. (1934) *The Struggle for Existence*, ch. V (sections II–III, Tables IX–XI, Fig. 21–25).
- Gause G.F., Nastukova O.K., Alpatov W.W. (1934) The influence of biologically conditioned media
  on the growth of a mixed population of *Paramecium caudatum* and *P. aurelia*. J. Anim. Ecol. 3:
  222–230.
- Hansen P.J., Bjørnsen P.K., Hansen B.W. (1997) Limnol. Oceanogr. 42: 687–704.
- Roadmap phase B.

## History

- 16/10/2026: found when the well-mixed culture reproduced Gause's protocol.
- 21/10/2026: documented here.
- 21/10/2026: protocol compared with the original text; daily thinning and vessel volume found
  missing.
