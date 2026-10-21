# Bacteria and phages

Bacteria are the base of the drop's food web. They are **simple particles** in the environmental
tier (structure-of-arrays, `bio/bacteria`), not soft bodies, and number in the tens of thousands.

## Growth

- Monod growth on dissolved organic matter, in biological time; parameters of *E. coli* (Berg
  2004): doubling ~30 min with abundant food (`growthPerHourMax` 1.4), yield 0.5.
- **Stationary phase** (stringent response): without food covering maintenance, a bacterium lowers
  its maintenance to a fraction and stops growing instead of starving at the full rate; viability
  then falls exponentially (Schink, Biselli, Ammar and Gerland 2019, Cell Systems 9: 64–73).
- The physiology (`bacterialDemand`, `bacterialMassChange`) is shared with the
  [well-mixed culture](well-mixed-culture.md), where bacteria are cohorts.
- Performance cap: `maxCount` 150 000 (food usually limits earlier).

## Movement

Run and tumble with **temporal** chemotaxis like *E. coli*: they do not measure the gradient in
space, they compare the current concentration with that of ~1 s ago; if it improves, they tumble
less. Each bacterium has a height in the film (`World::filmHeight`) that decides which mouths can
reach it ([feeding](feeding-and-phagocytosis.md)).

## Strains

| Strain (`Bacteria::strain`) | Organism | Features |
|---|---|---|
| 0 | Generic gram-negative rod | Default. |
| 1 | *Chromobacterium violaceum* | Quorum sensing: releases an AHL autoinducer; above the threshold it produces violacein (a violet pigment toxic to the protists that eat it, `Digestion::toxin`) and aggregates. |
| 2 | *Bacillus subtilis* | Gram-positive (thick peptidoglycan wall): distinguished with Gram staining. |
| 3 | *Spirillum volutans* | Giant helical bacterium (~25 µm), microaerophilic: aerotaxis towards a low O₂ forming a band (Caraway & Krieg 1974); bipolar flagella reverse instead of tumbling. |

## Biofilm

A bacterium hitting the glass may attach (probability 0.01 per collision, ESTIMATE); attached ones
detach slowly (0.2/biological h, ESTIMATE), do not swim, are not dragged by the water, grow on local
food and their daughters stay in the biofilm. Filter feeders cannot reach them; amoebae, which graze
surfaces, can (refuge from grazing: Matz and Kjelleberg 2005).

## Quorum sensing

Each *C. violaceum* releases AHL (`ahlPerHour`); above `quorumThreshold` it switches behaviour
(violacein, aggregation, slower swimming). The AHL field has its own half-life (`ahl_half_life` in
the medium). In the culture quorum sensing is not available yet.

## Phages

Lytic phages (T4-like) are a concentration field (`phage_`): adsorption proportional to phage
concentration, latent period ~30 min, burst of ~50 virions. Resistant mutants (altered receptor)
arise at 10⁻³ per division and grow somewhat slower. In the culture: mass-action phages with the same
parameters (infected cells are their own cohorts).

## *Bdellovibrio*

*Bdellovibrio bacteriovorus* swims at ~100 µm/s, enters the periplasm of a prey (bdelloplast), grows
for ~3 h and leaves with 4–6 daughters. Free cells and bdelloplasts are particles. Validation runs
disable it (`WorldConfig::bdellovibrioFraction = 0`).

## Other prokaryote products

- Bacteria release vitamin B₁₂ (`b12_source`) needed by auxotrophs.
- They release fMLP, followed by neutrophils ([animal cells](animal-cells.md)).

## Code

`src/bio/bacteria.*`, `src/game/world_chem.cpp`, `src/game/population_density.*` (density grid for
the render).

## Tests

`tests/test_ecosystem.cpp`, `tests/test_interactions.cpp`, `tests/test_culture.cpp`,
`tests/test_chemostat.cpp`.

## Limitations

- [`bdellovibrio-no-prey-resistance`](../bugs/bdellovibrio-no-prey-resistance.md),
  [`drop-bacteria-collapse`](../bugs/drop-bacteria-collapse.md).
- Strains are code, not data yet (plan M8): a new bacterium needs C++.
- No named *E. coli* strain with its own data, no bacterial genome, no individual bacterial size
  (adder), no spores, no lysogeny.
- The biofilm does not protect from phages; no matrix.
