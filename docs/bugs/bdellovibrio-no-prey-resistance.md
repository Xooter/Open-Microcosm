# *Bdellovibrio* can exterminate its prey: there is no phenotypic resistance

| Field | Value |
|---|---|
| **ID** | `bdellovibrio-no-prey-resistance` |
| **Type** | `model-limitation` |
| **Status** | `open` |
| **Severity** | `major` |
| **Area** | [`bacteria-and-phages`](../mechanics/bacteria-and-phages.md) |
| **Affects** | Drop plates with *Bdellovibrio*; validation cultures disable it (`WorldConfig::bdellovibrioFraction = 0`) |
| **Code** | `src/bio/bacteria.cpp` |
| **Reported** | 14/10/2026 |
| **Last verified** | 21/10/2026, commit `ae4affb` (no resistance in the code) |
| **Fixed in** | — |

## Summary

*Bdellovibrio* swimming at ~100 µm/s covers a 0.86 mm plate and finds every bacterium; the counters
showed 177 000 bursts until no prey was left, with plenty of organic matter. In reality the prey
never goes extinct.

## Steps to reproduce

1. A small plate (~0.86 mm) with bacteria and the default *Bdellovibrio* fraction, no phages.
2. Watch the bacteria count and the *Bdellovibrio* bursts in the export.

## Expected behaviour

A fraction of the prey population becomes transiently, phenotypically resistant, so predator and
prey coexist (Shemesh and Jurkevitch 2004).

## Actual behaviour

The prey goes extinct, then the predator.

## Evidence

[Validation, model problem 1](../validation/README.md#model-problems-found-while-validating-mvp-phase).

## Cause

Confirmed: the resistance mechanism is not modelled.

## Proposed fix

A per-bacterium resistant state entered at a published rate and lost after a published time, with
its own `Rng` stream (keeps determinism).

## Workaround

Set `bdellovibrioFraction` to 0 for population experiments.

## References

- Shemesh Y., Jurkevitch E. (2004) Plastic phenotypic resistance to predation by *Bdellovibrio* and
  like organisms in bacterial prey. Environ. Microbiol. 6: 12–18.
- Related: [`drop-bacteria-collapse`](drop-bacteria-collapse.md).

## History

- 14/10/2026: found while validating the *Paramecium* µmax.
- 21/10/2026: documented here.
