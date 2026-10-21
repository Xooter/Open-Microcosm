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

## Mutation and variation

All genetic change goes through `bio/genome` (`mutateDivision`, `standingVariation`), used by the
plate and the well-mixed culture alike, with the genetics of each being (`GeneticsSpec`,
`[being.genetics]`).

| Quantity | Default | Source |
|---|---|---|
| Mutational CV per gene and per generation (`mutational_cv`) | 0.5 % | Morphological traits: CVm ≈ 0.4–0.5 % (*Drosophila* bristles; Houle, Morikawa & Lynch 1996, Genetics 143: 1467–1483, as cited). ESTIMATE: no protist measurement. |
| Variation of a wild population (`standing_cv`) | 5 % | The genetic variance kept in a population is ~100 generations of mutation for morphological traits (VG/VM ≈ 100, Houle et al. 1996): √100 × 0.5 %. |
| Ciliates (`macronucleus`, default for the tag `ciliate`) | no change at fission | The phenotype comes from the amitotic, highly polyploid macronucleus; new mutations stay in the germline micronucleus until sex. *Paramecium* has the lowest known mutation rate per site among eukaryotes (Sung et al. 2012, PNAS 109: 19339–19344). |

- **Division:** each allowed gene (not the mating type) changes by N(0, CVm² × scale), bounded by
  its physiological limits. Before 21/10/2026 each gene mutated with probability 0.25 and σ 0.05: a
  CV of ~2.5 % per generation that made the size of a starved culture evolve down within weeks
  ([bug](../bugs/starved-cultures-keep-dividing.md)).
- **Mating type** does not change at fission (in *Paramecium* it is fixed when the macronucleus
  develops); conjugation draws it anew.
- **Seeding:** the populations of a biome drop carry the variation of a wild population (scaled by
  `WorldConfig::geneticDiversity`, Plate panel > Populations). An inoculum is a clone (a pure line,
  like Gause's) unless *Wild population* is ticked (Catalogue > Inoculate…, intervention `Inoculate`
  `v[3]`). A filament and the cells of a colony are a clone of their founder. In the culture,
  `CultureSpec::geneticDiversity` (0 by default). Animal cells in culture stay clonal.
- **User control:** `genetics.mutation_scale` (registry, Processes > Genetics, live; `mutation_scale`
  in the `.plate`) and, in the culture, `mutation_scale` in the spec, the scene, the live operation
  `mutation_scale` and the experiments (key and factor). Above 1 the interface says "accelerated
  evolution (not realistic)".

## Inheritance

- **Mitosis:** each daughter mutates its own copy (above).
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

`tests/test_genome.cpp` (bounds; the variance added per generation equals n·CVm²; the ciliate
macronucleus; the scale; a laboratory inoculum is a clone and a wild one varies ~5 %),
`tests/test_gause.cpp` (a pure line keeps its size: the culture plateaus in individuals),
`tests/test_interactions.cpp`.

## Limitations

- Bacteria have no genome (only phage resistance as a strain flag, at a rate far above the measured
  one): long-term evolution experiments are not possible yet. Plan:
  [genetics, evolution and resistance](../todo/genetics-evolution-resistance.md).
- The ciliate germline (micronucleus), autogamy and clonal ageing are not simulated yet (phase 2 of
  that plan); mutations are not deleterious on average.
- The position of organelles (e.g. anterior or posterior flagellum) is not a gene.
