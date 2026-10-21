# Cell size evolves down within weeks in a food-limited culture

| Field | Value |
|---|---|
| **ID** | `starved-cultures-keep-dividing` |
| **Type** | `model-limitation` |
| **Status** | `investigating` |
| **Severity** | `minor` |
| **Area** | [`genome-and-organelles`](../mechanics/genome-and-organelles.md) |
| **Affects** | Well-mixed cultures at their carrying capacity (`paramecium`, `paramecium_aurelia`); probably the plate too |
| **Code** | `src/bio/genome.cpp` (`mutate`, `kGenes`), `src/game/culture.cpp` (rate 0.25 per division, 0.5 on seeding), `src/game/world.cpp` |
| **Reported** | 21/10/2026 |
| **Last verified** | 21/10/2026 |
| **Fixed in** | — |

## Summary

In a food-limited culture the biovolume of each species reaches a plateau, but the number of
individuals keeps growing for weeks. The cells do not divide below their division size: the size
gene itself evolves down, by mutation and selection, far faster than in a real culture.

## Steps to reproduce

1. A Gause tube (`gause_tube`, program of `tests/test_gause.cpp`) with 20 *P. caudatum*.
2. Every 8 days, look at the mean `Traits::sizeScale`, the biomass of each cell relative to its own
   `targetBirthBiomass` and the mean generation.

## Expected behaviour

A pure line (Gause used them) keeps its size: Gause (1934, fig. 21) shows the plateau both in
individuals and in volume. *Paramecium* has the lowest known mutation rate per site among
eukaryotes (Sung et al. 2012), and polygenic mutation adds per generation a variance of 10⁻⁴ to
5·10⁻² of the environmental variance (Lynch 1988).

## Actual behaviour

Trial of 21/10/2026 (seed 22): mean `sizeScale` 0.95, 0.81, 0.74, 0.72, 0.72 on days 8, 16, 24, 32
and 40 (generations 8 to 24), piled against the gene's lower bound (0.7). Each cell holds 0.4–1.6
times the birth biomass of its own size (no cell divides small), but the mean cell volume falls from
3.4·10⁵ to 1.06·10⁵ µm³, so with the same biovolume there are three times more individuals.

## Evidence

The measurements above. The plateau test of `tests/test_gause.cpp` uses biovolume for this reason.

## Cause

Confirmed: every division mutates each quantitative gene with probability 0.25 (σ = 0.05 for size),
and seeding adds variation with probability 0.5. That is a mutational variance of 6·10⁻⁴ per
generation in units of the mean, about 0.06 of an environmental variance with a 10 % coefficient of
variation: above the top of Lynch's range and some 60 times its typical values. With the daily
thinning and a fixed ration, smaller cells reach their division size sooner and are selected.

## Proposed fix

Make the mutation rate a parameter of the plate, the culture and the experiments, with a default
within the published range, so that evolution experiments can still raise it on purpose; seed cultures
as pure lines (no added variation) unless asked.

## Workaround

Compare species by biovolume (series `biovolume:<id>`, condition `biovolume_fraction`).

## References

- Gause G.F. (1934) *The Struggle for Existence*, ch. V, fig. 21.
- Lynch M. (1988) The rate of polygenic mutation. Genet. Res. 51: 137–148.
- Sung W. et al. (2012) Extraordinary genome stability in the ciliate *Paramecium tetraurelia*.
  PNAS 109: 19339–19344.

## History

- 21/10/2026: seen while reproducing Gause's protocol with the daily thinning.
- 21/10/2026: the cause is the size gene evolving down (mutation rate), not small divisions.
