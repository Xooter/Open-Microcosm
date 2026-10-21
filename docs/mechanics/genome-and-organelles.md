# Genome and organelles

## Genome

Component `Heredity` = `Genome` (quantitative genes, alleles with a gradual effect, **relative to
the species**: 1 is typical) + `Traits` (physiological multipliers derived with `traitsOf`).
`World::applyTraits` writes them into `Locomotion::power`, `Digestion::rateScale`, `Metabolism` and
the shape (`CellShape`).

Genes: `size`, `motor`, `mitochondria`, `chloroplasts`, `lysosomes`, `trichocysts`, `armor`,
`rigidity`, `mating_type`. A being's `genome` table sets those that differ from its locomotion type.

`geneAllowed` enforces biology: chloroplasts only in beings that have them, trichocysts only in
ciliates, armour only in flagellates (lorica) and amoebae (test). Without endosymbiosis an absent
gene never appears by mutation: a chloroplast is never "invented".

## Inheritance

- **Mitosis:** each daughter mutates its own copy.
- **Conjugation** (`Conjugating`): requires different mating types; both exconjugants end with the
  same recombined genotype (Sonneborn 1937).
- Component `Lineage`: session and generation of each cell.

## Organelles

Organelles are interior particles with a `BodyParticles::tag`. They **take up physical space**:
`Traits::volume` enlarges the cell, which becomes slower and more visible; and they cost
maintenance. Nucleus, contractile vacuole, food vacuoles, mitochondria, chloroplasts, lysosomes and
storage granules are simulated; finer structures are declared in the body plan and drawn in the
model view ([rendering](rendering-and-optics.md)).

The contractile vacuole has its own cycle (`[being.contractile_vacuole]`): it fills slowly and empties at once, beating faster in a more dilute medium and stopping
in a medium as concentrated as the cytosol (Kitching 1938; Stock et al. 2002, J. Exp. Biol. 205:
1171). It is physics and runs in real time (`game/world_osmoregulation.cpp`).

## Defences

| Defence | Effect |
|---|---|
| Armour (`armor` ≥ 0.5: lorica, test, theca) | Prevents phagocytosis. |
| Trichocysts | Can reject a phagocytic cup (component `Discharge`: the predator backs off). A paralysed cell cannot discharge. |
| Body processes and mouth width | A prey wider than the predator's mouth is safe ([feeding](feeding-and-phagocytosis.md)). |
| Induced morphs | *Euplotes* grows wings against *Lembadion* ([signals](chemical-signals.md)). |

## Code

`src/bio/genome.*`, `src/bio/organelles.*`, `src/game/world.cpp` (`applyTraits`).

## Tests

`tests/test_genome.cpp`, `tests/test_interactions.cpp`.

## Limitations

- Bacteria have no genome (only phage resistance as a strain flag): long-term evolution
  experiments are not possible yet.
- The position of organelles (e.g. anterior or posterior flagellum) is not a gene.
