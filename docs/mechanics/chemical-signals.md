# Chemical signals

Beings that talk to each other through the water: kairomones, prey cues, relay waves and toxins.

## M3 — Released signal

`[being.release]`: each cell releases `rate` pmol per biological hour of a signal (`signal` id),
which diffuses with `diffusion` [µm²/s] and decays with `half_life_hours`. There is **one chemistry
field per signal** (up to 4), and it only diffuses once someone has released it. Signals are outside
the P/N budget. In the [well-mixed culture](well-mixed-culture.md) they are a scalar.

Time scale: a signal released and degraded in biological time would spread too little in real-time
diffusion. With `bio_time = true` it diffuses with D × bioTimeScale, so √(D·τ) is the real length
(used for every released signal, including cAMP relay).

## M4 — Induced morph

`[being.morph]`: above a `threshold` of a signal, the cell changes gradually, over `hours` in both
directions, to another shape (`width_scale`, `length_scale`, `area_scale`) and, once complete, to
its own `feeding`, `prey` and `mouth_width` (`reshapeRestEven`).

| Pair | Real biology | Source |
|---|---|---|
| *Lembadion* → *Euplotes octocarinatus* | The predator's kairomone makes *Euplotes* grow lateral "wings" wider than *Lembadion*'s mouth. | Kuhlmann & Heckmann 1985 |
| *Tetrahymena* → *T. vorax* | A cue released by the prey (stomatin) turns the microstome (bacterivore) into the macrostome (eats ciliates). | Smith-Somerville et al. 2000 |

## cAMP relay (*Dictyostelium*)

`[being.relay]` + `[being.chemotaxis]`: starved amoebae become competent after `competence_hours`;
pacemakers (`pacemaker_fraction`, every `pacemaker_minutes`) emit cAMP pulses; a cell above
`threshold` emits its own `pulse` and stays refractory for `refractory_minutes`. Cells climb the
gradient while the signal rises. Spiral waves and aggregation streams emerge (Bonner 1947; Konijn et
al. 1967). The slug and the fruiting body are multicellular and are not simulated.

## Signal toxicity (killer *Paramecium*)

`[being.signal_toxicity]`: *P. tetraurelia* killers release R bodies (from their *Caedibacter*
endosymbionts); a sensitive cell that exceeds `ec50` dies (Sonneborn 1938).

## Other dissolved signals

- Bacterial autoinducer (quorum sensing): [bacteria](bacteria-and-phages.md).
- fMLP released by bacteria, followed by neutrophils: [animal cells](animal-cells.md).
- Progesterone as a reagent of mechanism `signal`, followed by capacitated sperm:
  [reagents](reagents.md), [sperm](sperm-and-casa.md).

## Code

`src/game/world_signals.cpp`, `src/bio/species.hpp`.

## Tests

`tests/test_mechanics.cpp`, `tests/test_candidates.cpp`.

## Limitations

- *Ochromonas* is mixotrophic, but the *Chlorella* colonies of Boraas et al. (1998) appear by
  selection over ~100 generations, not by a signal: they need a coloniality gene.
- *Salpingoeca* rosettes induced by *Algoriphagus* (Alegado et al. 2012) are missing: bacteria do not
  release signals.
