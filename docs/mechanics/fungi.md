# Fungi

Aquatic fungi and pseudofungi live in the environmental tier (`bio/fungi`, `game/world_fungi.cpp`):
**hyphae** are chains of nodes fixed to the substrate that grow at the tip, and **zoospores** are
swimming particles. Each fungus is a row of the `FungusInfo` table: no per-species code.

| Fungus | Group | What it does |
|---|---|---|
| Chytrids (*Rhizophydium*-type) | Chytridiomycota | The uniflagellate zoospore (~3 µm) swims towards algae, encysts on the wall, inserts rhizoids and grows as a sporangium at the host's expense (`ChytridInfection`); when mature it releases dozens of zoospores. The infected alga does not divide and dies. |
| *Saprolegnia* | Oomycete ("water mould", not a true fungus) | Coenocytic mycelium on detritus; secretes exoenzymes that accelerate decomposition next to the hyphae and absorbs what dissolves. When food runs out it forms zoosporangia at the tips that release biflagellate zoospores. |
| *Allomyces* | Blastocladiomycota | Hyphae with dichotomous branching (the tip splits in two) and uniflagellate zoospores. When starved it extends its cycle with **resting sporangia** (`Fungi::resting`: thick brown walls) that survive dry and germinate when food returns. |
| Lichenising mycobiont (*Verrucaria*-type) | Ascomycete | Grows slowly and barely feeds alone; its hyphae seek green algae, attach with haustoria and receive their sugar, giving phosphorus and protection from chytrids in return (`Mycobiont`, `Lichenized`, `World::stepLichens`). Only non-motile spores carried by the water. |

Zoospore chemotaxis responds to the target named in each fungus's row. Hyphae do not grow towards
the dish glass.

## Yeasts

Yeasts (*Saccharomyces*, *Candida*) are **soft-body** cells, not part of this table: osmotrophic
species with budding (`budding`, `feeding = "osmotrophy"`). They respire with O₂ and ferment without
it ([metabolism](metabolism-and-nutrients.md)).

## In the biomes

`saprolegnia`, `chytrid_clusters`, `allomyces` and `lichens` per biome. Hosts of chytrids are marked
with the tag `chytrid_host`.

## Code

`src/bio/fungi.*`, `src/game/world_fungi.cpp`.

## Tests

`tests/test_ecosystem.cpp`, `tests/test_interactions.cpp`.

## Limitations

- *Zoophagus* (predatory hyphae trapping rotifers) was removed with the animals.
- No ethanol field (yeast competition, Crabtree effect).
- No secreted antibiotics (Fleming).
- Hyphae do not respond to stains: [bug](../bugs/stains-missing-targets.md).
