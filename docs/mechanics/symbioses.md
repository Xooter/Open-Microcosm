# Symbioses

Mutualisms and stolen organelles, each with the constraints of the real case.

## Photosymbionts and kleptoplasts

`[being.photosymbiont]` (`cell_physiology`, shared by plate and culture):

| `kind` | Being | Real biology |
|---|---|---|
| `endosymbionts` | *Paramecium bursaria* with *Chlorella* | The algae live in perialgal vacuoles, divide with light, are lost in the dark and pass sugar to the host; they are shared between the daughters at division (binomially). Green cells grow without prey in light (Karakashian 1963). |
| `kleptoplasts` | *Mesodinium rubrum* | Retains chloroplasts (and the nucleus) of the cryptophytes it eats (`source`, `per_prey`); they photosynthesise but decay (`decay_per_hour`) and must be renewed by eating. |

`photosynthesis` is the sugar passed at full load, relative to an alga of the host's biomass.

## Lichen

A lichenising fungus (`Mycobiont`, like *Verrucaria*) grows slowly and barely feeds on its own: its
hyphae seek green algae, attach with haustoria and receive their sugar; in exchange they pass
phosphorus and protect the alga with secondary metabolites (chytrids cannot infect it). Component
`Lichenized` on the alga; `World::stepLichens`. Without an alga the fungus barely grows
([fungi](fungi.md)).

## Nitrogen-fixing mutualism

Cyanobacterial heterocysts fix N₂ and release ammonium; the nitrogenase is inhibited by O₂, so they
fix more where neighbouring bacteria consume O₂ ([cyanobacteria](cyanobacteria.md)).

## Gut symbionts

*Trichonympha* (an obligate symbiont) digests cellulose in the anoxic termite gut
(`termite_gut` biome); it appears only where the biome names it.

## Killer endosymbionts

*P. tetraurelia* killers carry *Caedibacter*, whose R bodies kill sensitive paramecia
([chemical signals](chemical-signals.md#signal-toxicity-killer-paramecium)).

## Code

`src/bio/cell_physiology.*`, `src/game/world_fungi.cpp` (lichen), `src/bio/cyanobacteria.*`.

## Tests

`tests/test_mechanics.cpp`, `tests/test_interactions.cpp`.

## Limitations

- No free endosymbiosis: a swallowed alga cannot be adopted by any being; only the real cases.
- *Amoeba proteus*'s dependence on its bacteria (Jeon 1972) is not modelled.
