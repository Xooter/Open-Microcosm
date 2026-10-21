# Ecosystem and behaviour

A drop is a real ecosystem: carnivorous protists (hunters, engulfers, ambushers and parasites),
herbivores (algivores and bacterivores), algae, fungi, bacteria and phages. Nothing is scripted:
populations rise and fall from feeding, growth and death.

## Species as data

Species are data (`SpeciesInfo`, loaded from `beings/*.toml`): locomotion, shape, diet (`preyMask`,
`mouthPreyMask`), flight (`fleeMask`), feeding mode (`FeedingMode`), reproduction, cysts and sessile
traits. **There is no per-species code except in the render**: a being that needs new behaviour gets
a named mechanism that the data select. The enums `SpeciesId`, `MineralKind` and `BiomeId` are
indices into the registry with aliases for the factory ones; never add per-species cases.

## Perception and autopilot

`World::perceive` computes what each cell senses in this step (nearest prey and threat, local
nutrients, light on each side of the anterior pole, excess CO₂, O₂, water shear, collision of the
anterior pole, signals). `ai/autopilot` turns it into behaviour for every non-controlled cell:
wander and eat, chemokinesis, acid trap, hunting, flight, jumps and the phobic responses of
[behavioural responses](behavioural-responses.md). The responses have fixed memory in the autopilot
(no allocations).

## Roles in the drop

| Role | Examples | Mechanism |
|---|---|---|
| Active hunters | *Didinium*, *Lacrymaria*, *Litonotus*, *Dileptus* (toxicysts) | `raptorial`, `neck`, `[being.toxicysts]` |
| Slow engulfers | *Amoeba*, *Chaos* | Phagocytic cup |
| Sessile ambushers | Suctorians | `Captured` drains the prey |
| Filter feeders | *Vorticella*, *Stentor*, *Paramecium* | Oral current in the water |
| Grazers | *Euplotes*, *Chilodonella*, testate amoebae | Walk and scrape surfaces and the biofilm |
| Mixotrophs | *Euglena*, *Ochromonas* | Photosynthesis + feeding |
| Algae | Motile, diatoms, colonial, filamentous | [stalks and colonies](stalks-and-colonies.md) |
| Decomposers | Bacteria, *Saprolegnia* | [bacteria](bacteria-and-phages.md), [fungi](fungi.md) |
| Parasites | Chytrids, phages, *Bdellovibrio* | [fungi](fungi.md), [bacteria](bacteria-and-phages.md) |

Predation is limited by handling time (`handlingHours`, `Digestion::handling`) and satiety by
ingested volume (`satiated`, `World::isSatiated`), and by the mouth width
([feeding](feeding-and-phagocytosis.md)).

Sessile beings (`Anchored`): the stalk holds the posterior pole; the spasmoneme contracts on
disturbance. Their daughters swim as telotrochs (`Telotroch`) and then attach. Ciliates have the
avoiding reaction; *Paramecium* also gathers where there is CO₂ (`acidTrap`, Jennings 1906):
swarms emerge.

## Populations of a drop

`makeDrop(biome, seed)` builds the drop: chemistry, population weights per species
(`WorldConfig::speciesWeight`), fungi, cyanobacteria (by thermal range) and initial patches
([biomes](../content/biomes.md)). An obligate symbiont (tag `obligate_symbiont`) appears only where
the biome names it (*Trichonympha* in the termite gut).

## Records

`World::deathTally()` and `World::observations()` record causes of death and observed behaviours.

## Code

`src/bio/species.*`, `src/ai/autopilot.*`, `src/game/world.cpp` (`perceive`), `src/game/biome.*`.

## Tests

`tests/test_ecosystem.cpp`, `tests/test_interactions.cpp`, `tests/test_biomes.cpp`.

## Limitations

- [`drop-bacteria-collapse`](../bugs/drop-bacteria-collapse.md): a drop does not sustain its
  bacteria for long.
- The volume scale: a plate holds ~10 000 times less water than a classic population experiment
  ([validation](../validation/README.md#the-volume-scale-main-lesson)); use the
  [well-mixed culture](well-mixed-culture.md) for population dynamics.
