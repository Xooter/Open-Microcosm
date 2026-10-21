# Reagents

Antibiotics and other compounds added to the plate (dropper) or to the culture. The code provides
the **mechanism**; the data provide its measured parameters, with a source
([reagent format](../content/reagents.md)). The IC50 and MIC are **not inputs**: they come out of a
dose–response experiment, as in the laboratory.

## Pharmacodynamic mechanism

The pharmacodynamic function of Regoes et al. (2004, Antimicrob. Agents Chemother. 48: 3670–3676),
fitted to measured kill curves:

ψ(a) = ψmax − (ψmax − ψmin) (a/zMIC)^κ / ((a/zMIC)^κ − ψmin/ψmax)

is the net growth rate at concentration a. The killing added by the reagent is ψmax − ψ(a). With
`growth_dependent` (β-lactams break the wall while it is being synthesised: Tuomanen et al. 1986,
J. Infect. Dis. 153: 312) that killing scales with how much the bacterium grows now relative to
ψmax: a bacterium that does not grow does not die (stationary-phase tolerance).

## Emergent MIC

The MIC comes from a simulated broth dilution (Mueller-Hinton at 35 °C, 5·10⁵ bacteria/mL, 18 h,
two-fold series, visible growth = 10⁷/mL, threshold ESTIMATED) and is compared with
`published_mic`:

| Reagent | Emergent MIC | Published |
|---|---|---|
| Ampicillin | 4 µg/mL | 8 µg/mL (one dilution away: the test's reproducibility is ±1) |
| Tetracycline | 1 µg/mL | 1 µg/mL |

## Signal mechanism

`mechanism = "signal"`: a signal with no pharmacodynamics of its own (progesterone for sperm). Its
effect is set by the being that receives it, with its own data (`[being.capacitation]`): the reagent
only diffuses ([sperm](sperm-and-casa.md)).

## On the plate and in the culture

- Plate: a single reagent field (`World::reagentField`) that diffuses only if used; the dropper
  releases it (solute 7, the key says which reagent).
- Culture: one concentration per reagent.

## Code

`src/game/reagent.*`, `src/game/dose_fit.*`.

## Tests

`tests/test_reagents.cpp`, `tests/test_validation.cpp` (MIC).

## Limitations

Only bacterial targets with pharmacodynamics; metals, salt and eukaryotic targets are pending
(roadmap E). No resistance with a cost or horizontal transfer (roadmap F).
