# Time and units

## Units

- Lengths in **micrometres (µm)**, time in **simulation seconds (s)**, concentrations in **µM**.
  Conversions to pixels happen only in the render (`core/units.hpp`).
- Masses are **pmol of glucose equivalents of dry matter**. `units::kDryMassFraction` = 0.3: about
  70 % of a cell is water.
- Brownian motion uses Stokes–Einstein, D = kT / (6π μ r): a bacterium of 0.5 µm radius gets
  ~0.4 µm²/s, real but subtle.

## Two clocks

Real microorganisms live on very different time scales: water flows and a ciliate swims in
milliseconds to seconds, while a *Paramecium* divides about once a day. Open Microcosm keeps physics in
real time and compresses biology with **a single constant**, so the proportions between processes
and between species are respected.

| Process | Clock |
|---|---|
| Swimming, water, diffusion, gas exchange with the air, osmosis | Real time (`units::kTimeScale = 1`) |
| Metabolism, growth, digestion, division, signal decay | Biological time: `units::kBioTimeScale` = 60 (1 real minute = 1 biological hour) |
| Crawling migration of animal cells (lamellipodium, focal adhesions) | Biological time: it is actin polymerisation. A fibroblast at 40 µm/h is seen at ~0.7 µm/s |

Consequences that every new mechanism must keep:

- What is digested yields ×`bioTimeScale`: feeding is mechanical (real time) but the energy it
  provides belongs to biology.
- A cell's O₂ and CO₂ exchange with the water is done at the real rate: divided by `bioTimeScale`,
  **not** by `bioScale` (Q10 does accelerate respiration).
- The bacterial ingestion of a species with a measured value (`max_ingestion`) is that value, on the
  plate and in the culture.
- Swimming is physics and stays in real time.

A well-fed *Paramecium* divides in ~5 real minutes (real: ~5–8 h). The [well-mixed
culture](well-mixed-culture.md) has no real-time physics, so it has no compression: `step(dt)`
advances `dt` seconds of both.

## Temperature

Everything biological is scaled with **Q10 = 2** around 20 °C through `World::bioScale()`; the
medium temperature also changes viscosity and O₂ saturation. Published rates are brought to 20 °C
with Q10 = 2 when they are loaded. Exception: the measured swimming parameters of the sperm cell
carry their own Q10 ([sperm](sperm-and-casa.md)).

Limitation: there is no thermal optimum or maximum per species (experiment 29 in
[candidates](../design/candidates.md)); tolerances outside the `temperature` range are paid as
maintenance stress.

## Fixed time step

The simulation advances with a fixed step (`kFixedDt`), decoupled from rendering; XPBD may take
sub-steps inside each step. Order of a step: fluid → chemistry → forces and propulsion → XPBD
constraints → collisions → metabolism and life cycle → AI → topology events (phagocytosis,
division, lysis, fusion) applied at the end, never in the middle of a step.

## Code

- `src/core/units.hpp`: constants and conversions.
- `World::bioScale()`, `World::step` (`src/game/world.cpp`).

## Tests

`tests/test_physiology.cpp`, `tests/test_validation.cpp` (growth rates measured in biological
time).
