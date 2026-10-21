# Plan: mutation, evolution and drug resistance

Working plan for one large feature, kept here at the user's request while it is being built (the
[roadmap](../roadmap.md) points to it). Each phase is one commit; when a phase is done, what it built
is described in [`mechanics/`](../mechanics/README.md) and its line here is marked ✅. When the whole
plan is done this file is deleted (no stale plans in `docs/`).

Guiding rules: strict realism with verified sources (an unverified number does not enter the code);
general mechanisms for the whole program (plate, culture, programs, lessons, experiments) rather than
code for one experiment; every rate is configurable so that the user can run an accelerated
evolution experiment on purpose, and the interface says when a value is not realistic.

## Why

Three parts of the current model are far from the measured biology:

1. **Quantitative genes of protists** mutated with probability 0.25 per gene and per division
   (σ = 0.05 for size), and seeding added variation with 0.5 or 0.3. That is a mutational variance
   of ~6·10⁻⁴ per generation in units of the mean (CV ~2.5 %), five times the CV that mutation adds
   to morphological traits per generation (~0.5 %, below). In a Gause tube the size gene fell from
   1.0 to 0.72 in 24 generations ([bug](../bugs/starved-cultures-keep-dividing.md)).
2. **Bacterial resistance to phages** arose at 10⁻³ per division (`BacteriaParams::resistanceMutation`);
   Luria and Delbrück's own data give ~3·10⁻⁸ per cell per generation: some 30 000 times less.
3. **Ciliates** expressed whatever they inherited at every fission. Their phenotype comes from the
   amitotic, highly polyploid macronucleus; new mutations sit in the germline micronucleus and are
   expressed only after sex, and clones age and die without sex.

## Phase 0 — Sources ✅ (21/10/2026)

| Claim | Value | Source | Status |
|---|---|---|---|
| Genomic mutation rate of DNA-based microbes | 0.0033 per genome per replication (range 0.0018–0.0046) | Drake (1991) PNAS 88: 7160–7164; Drake & Hwang (2005) | verified (abstract) |
| Deleterious mutation rate, *E. coli* | > 2·10⁻⁴ per genome per generation (lower bound) | Kibota & Lynch (1996) Nature 381: 694–696 | verified (abstract); mean effect not seen |
| Mutational heritability | Vm/Ve 10⁻⁴ to 5·10⁻² per generation | Lynch (1988) Genet. Res. 51: 137–148 | verified (abstract) |
| Standing vs mutational variance | VG/VM ≈ 100 generations (morphological), ≈ 50 (life history) | Houle, Morikawa & Lynch (1996) Genetics 143: 1467–1483 | verified (abstract) |
| Mutational CV of morphological traits | ~0.4–0.5 % per generation (*Drosophila* bristles) | Houle et al. (1996), as cited by a 2023 GBE paper on yeast rDNA | secondary; no protist data |
| *Paramecium* germline mutation rate | lowest known per site among eukaryotes | Sung et al. (2012) PNAS 109: 19339–19344 | verified (abstract) |
| T1 resistance in *E. coli* | ~3·10⁻⁸ per cell per generation (p₀ method, experiment 16) | Luria & Delbrück (1943) Genetics 28: 491–511, calculation in course notes (S. Carr, Memorial University) | secondary; original not read |
| Clonal lifespan, *P. tetraurelia* | ~200 fissions (150–300) without autogamy or conjugation | Gilley & Blackburn; Sonneborn | verified (secondary summaries) |
| Clonal lifespan, *P. caudatum* | 527 fissions; senescence from ~350 | Japanese J. Protozool. (Yusa et al.) | verified (abstract) |
| Autogamy immaturity, *P. tetraurelia* | ~20 fissions after the last autogamy | same summaries | verified (secondary) |
| Cost of resistance | reviewed | Andersson & Hughes (2010) Nat. Rev. Microbiol. 8: 260–271 | citation verified, values pending |
| To verify before their phase | rifampicin (*rpoB*) and streptomycin (*rpsL*) mutation rates; Lenski et al. (1991) fitness gain; Drlica (2003) mutant selection window; Balaban et al. (2004) persistence switching rates; plasmid transfer rates; macronuclear ploidy of *P. caudatum* | — | pending |

## Phase 1 — Realistic quantitative mutation ✅ when committed

- One place for genetic change (`bio/genome`): `mutateDivision` (per division) and
  `standingVariation` (a wild population), replacing the six fixed rates in `world.cpp` and
  `culture.cpp`.
- Per division each allowed gene changes by N(0, CVm²) (relative to the species value 1), with
  CVm = 0.5 % by default (estimate: morphological traits). Mating type no longer flips at fission (in
  *Paramecium* it is fixed when the macronucleus develops).
- Data per being: `[being.genetics]` `mutational_cv`, `standing_cv`, `macronucleus`, `source`.
  Default `standing_cv` = √100 × 0.5 % = 5 % (Houle et al. 1996: VG/VM ≈ 100). Beings tagged
  `ciliate` default to `macronucleus = true`: their asexual fission does not change the expressed
  genome (the germline arrives in phase 2).
- Seeding: a drop of a biome has standing variation (a wild population); an inoculum from the
  catalogue or a culture is a clone (pure line), unless the user asks for a wild inoculum.
- User control: `genetics.mutation_scale` (multiplies the mutational variance; 1 = published) and
  `genetics.standing_scale` in the parameter registry (plate, programs, lessons); in the culture,
  `mutation_scale` and `standing_scale` in the spec, the scene and the experiments. Values above 1
  are labelled "accelerated (not realistic)".
- Tests: the variance added per generation equals CVm²; a Gause monoculture plateaus in individuals
  (closes the bug); determinism with 1 and 4 threads.

## Phase 2 — Ciliate genetics

Germline (micronucleus) and expressed (macronucleus) genomes; mutations accumulate in the germline;
conjugation (meiosis, exchange of pronuclei, new macronucleus) and autogamy (*P. aurelia* complex:
homozygosity) express the new genotype; clonal age in fissions since the last sex, with the decline
of the division rate and death from data (`[being.clonal_life]`), and rejuvenation by sex.
Validation: clonal lifespan ~200 fissions (*P. tetraurelia*) and ~527 (*P. caudatum*).

## Phase 3 — Bacterial genotypes and realistic mutation

A bacterial genotype in `bio/` (resistance per target — phage receptor and each reagent —, cost,
compensatory mutations): an index into a genotype table on the plate, the cohort key in the culture
(identical genotypes merged under `maxBacterialCohorts`). Phage resistance at the published rate. A
general **fluctuation assay** tool (N parallel cultures, p₀ and Lea–Coulson estimators). Validation:
Luria–Delbrück (variance ≫ mean, the input rate recovered).

## Phase 4 — Antibiotic resistance

`[reagent.resistance]` with real mechanisms: target mutation (rate, MIC factor, cost), enzymatic
inactivation (β-lactamase degrading the drug in the medium: a public good), efflux. Regoes
pharmacodynamics per genotype (zMIC × factor, ψmax reduced by the cost). Non-heritable persistence.
Emergent: mutant selection window, MIC rising under increasing doses, interrupted treatments (dose
programs). Validation: mutant MICs, competition cost, mutant frequency at 4× MIC. New reagents
(rifampicin, streptomycin) once their data are verified.

## Phase 5 — Horizontal transfer (roadmap phase F)

Plasmid conjugation (contact on the plate, mass action in the culture), plasmid cost and segregation
loss, transduction by the existing phages, transformation (*B. subtilis* competence).

## Phase 6 — Measuring evolution

`Lineage` with id, parent and birth time (lineage tree, roadmap phase G) and a bounded mutation log;
series of the mean and variance of each trait per species and of resistance frequencies; a general
**competition assay** (relative fitness W, as Lenski); genotype, fissions since sex and clonal age
in the card; trait histograms and frequencies in the export.

## Phase 7 — Classic experiments and lessons

Luria–Delbrück, Sonneborn's clonal ageing, complete versus interrupted antibiotic treatment;
mutation accumulation with single-cell bottlenecks; daily 1:100 transfers (Lenski) if the run time
allows; MIC rising with serial passages. Each one in `validation/` with its tolerance and source.

## Phase 8 — Closing

Bugs closed, `AGENTS.md` (Genome section) and roadmap updated, full test suites run once.
