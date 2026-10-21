# Measurement

Everything that turns a run into data (`game/mass_budget`, `game/timeseries`, `game/growth_fit`,
`game/region`, `game/export`). Measurement only reads the world.

## Mass budget

`World::massBudget()` gives the stocks of **P, N, Si, Fe and C** per compartment (`Pool`: water,
cells, detritus, sediment, frustules, lorica, plasmodium…); `totalPhosphorus()` and the other totals
are their sum (a single accounting).

Fluxes are accumulated by `World::ledger_` (`FluxLedger`): **every exchange with the water goes
through `World::exchange(field, point, pmol, Flow)`** with the process (`Uptake`, `Excretion`,
`Death`, `Spawn`, `Washout`…); `exchangeRaw` does not record. What enters from outside (dropper
`dose`, `setMeanConcentration`, live changes in the Plate panel, N₂ fixation, air CO₂) goes to
`external`. Closure is `budgetResidual` = change of the total − external inputs.

P, N, Si and Fe are conserved. **Carbon has an open boundary by design**: CO₂ exchanges with the air
in real time while biology runs compressed, and organic matter enters from outside; it is shown
without requiring closure.

## Time series

Sampled by `Run` (not `World`) every `sampleInterval` [simulation s] together with the census: time,
populations, chemical means (`Census::mean*`), budget and region. Named columns with units, stored by
column; with a row cap it decimates (keeps 1 of every 2). NaN marks missing values (region not
drawn): with `-ffast-math` use `finiteNumber`, never `std::isfinite`. Columns go by id (they do not
change with the language). Graphs panel (ImPlot).

## Growth curve

`fitGrowth` (deterministic, no raylib):

- µmax [1/h]: the steepest slope of ln N in a sliding window (linear regression), as measured in
  exponential phase; doubling time ln 2 / µmax;
- lag λ: where the tangent of the exponential phase crosses ln N₀ (Pirt; Zwietering et al. 1990);
- carrying capacity K: logistic fit (Verhulst), K found by golden-section search minimising the
  error in ln N.

## Regions

`quantifyRegion` counts individuals by centroid and averages the water cells whose centre is inside a
hand-drawn polygon (Region tool). The region lives in `Run` (`setRegion`) and enters the series.

## Ruler and area

Measure tool: distances and areas in µm on the image.

## Limitation map

`Metabolism::limiting` (Liebig: the factor that kept investment in biomass lowest) is read only by the
render (`CellRenderer::setLimitationMap`, key L), coloured with the Okabe–Ito palette.

## CASA

Sperm kinematics: [sperm and CASA](sperm-and-casa.md).

## Export

Ctrl+E writes to `<data>/openmicrocosm/exports`:

- `seriesCsv`: the time series;
- `runJson`: parameters (the plate with the current environment, metabolism, time scale, the whole
  parameter registry), series, budget, notes and provenance;
- `speciesJson`: the species data.

Numbers are written with `std::to_chars` (decimal point regardless of the locale).

## Code

`src/game/mass_budget.*`, `src/game/timeseries.*`, `src/game/growth_fit.*`, `src/game/region.*`,
`src/game/export.*`, `src/ui/panels/graphs_panel.cpp`, `src/ui/panels/measure_panel.cpp`.

## Tests

`tests/test_budget.cpp`, `tests/test_measure.cpp`, `tests/test_run.cpp`.
