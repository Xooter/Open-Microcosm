# Bacteria in a drop collapse within a few biological hours

| Field | Value |
|---|---|
| **ID** | `drop-bacteria-collapse` |
| **Type** | `model-limitation` |
| **Status** | `open` |
| **Severity** | `major` |
| **Area** | [`bacteria-and-phages`](../mechanics/bacteria-and-phages.md), [`ecosystem-and-behaviour`](../mechanics/ecosystem-and-behaviour.md) |
| **Affects** | Every drop plate; test "30 min sin jugador: ninguna especie se extingue" (`tests/test_ecosystem.cpp`, in `skip`) |
| **Code** | `src/bio/bacteria.cpp`, `src/game/world.cpp` |
| **Reported** | 15/10/2026 |
| **Last verified** | 21/10/2026, commit `ae4affb` (the test is still `doctest::skip()`) |
| **Fixed in** | — |

## Summary

Left alone, a drop loses almost all its bacteria in a few biological hours (phage epidemics,
*Bdellovibrio* and grazing), the bacterivores starve and fast osmotrophs (*Candida*, *Naegleria*)
take over. A real pond drop sustains its community for days.

## Steps to reproduce

1. `./build/release/microcosm_tests -tc="*30 min*" --no-skip` (very slow: hours).
2. Or create a `puddle` plate with seed 66 and watch the Statistics panel for 6 biological hours.

## Expected behaviour

A drop that keeps its bacterial population (orders of 10⁶–10⁷/mL) and its grazers for days, as in a
real infusion.

## Actual behaviour

Seed 66, 50 000 bacteria: 50 000 → 0 in 3 min (3 biological hours) in the test configuration; in
the Puddle drop a phage epidemic lyses 62 933 bacteria in ~100 s. Since the biofilm refuge was added
(15/10/2026), ~16 resistant attached bacteria survive and the population regrows (16 → 176 in 3 h),
but paramecia drop from 17 to 3.

## Evidence

See [validation: collapse of the bacteria](../validation/README.md#collapse-of-the-bacteria-in-the-drop-30-min-test).

## Cause

Confirmed by separate diagnostics:

1. The test plate has no organic matter: bacteria cannot grow at all.
2. **Volume scale:** 20 paramecia in ~0.06 µL is ~3×10⁵/mL, ~300 times a dense culture; grazing
   mortality (~0.05/s) exceeds maximum growth (0.023/s at ×60).
3. Phages: with ~20 000 bacteria and the real mutation frequency there is almost never a resistant
   one before the epidemic.
4. *Bdellovibrio* has no prey resistance (see [`bdellovibrio-no-prey-resistance`](bdellovibrio-no-prey-resistance.md)).
5. The biofilm does not protect from phages in the model.

## Proposed fix

Options already evaluated: realistic initial protist densities for the plate volume; a deeper water
column for bacteria (as for Tilman); more refuges (sediment) and phage loss by adsorption to
particles; phenotypic resistance to *Bdellovibrio*. Then re-enable the 30-min test with organic
inflow.

## Workaround

Use the well-mixed culture for population dynamics; on the plate, add organic inflow and lower the
protist weights, or disable phages and *Bdellovibrio*.

## References

- Matz C., Kjelleberg S. (2005) Trends Microbiol. 13: 302–307 (biofilm refuge).
- Shemesh Y., Jurkevitch E. (2004) Environ. Microbiol. 6: 12–18.
- Roadmap phase B ("sustainable drop").

## History

- 15/10/2026: found when running the skipped test; biofilm refuge added.
- 21/10/2026: documented here.
