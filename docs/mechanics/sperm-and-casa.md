# Sperm and CASA

The human spermatozoon (`human_sperm`) is an animal cell that swims. Prepared plates reproduce the
WHO semen analysis (spermiogram) and its classic tests.

## The cell

- Head of 4.2 × 2.8 µm (the soft body) without a lamellipodium or granules; it rolls about its axis
  (~6 Hz, `roll_hz`) and narrows edge-on.
- Tail of 50 µm (`flagellum_length`) with its midpiece: drawn, and its thrust enters the water along
  it. `flagellum = "posterior"`: a pusher.
- Lives on fructose (Mann's fructolysis); does not eat or divide.
- When it dies it stays whole and immotile (`[being.cell] death = "immotile"` → `CellPhase::Dead`:
  no metabolism, no propulsion, out of the census) and deflates.

## Swimming

- Speed (`swim_speed`: VAP 54.5 µm/s at 37 °C and 37.4 at 23 °C, Q10 1.31), beat frequency
  (`beat_frequency`: BCF 15.3 Hz, Q10 1.15) and lateral head amplitude (`lateral_amplitude`: ALH
  2.15 µm, Q10 1.20) are CASA values from García-Molina et al. (2023, Rev. Int. Androl. 21(2),
  table 2), each with its **own measured Q10**: brought to the medium's temperature from the
  measurement's, not to 20 °C. Heat speeds up swimming.
- The path is **passive** (`[being.swim_path]`, `World::steerSwimmer`): no senses, no avoiding
  reaction, no autopilot. The heading follows an Ornstein–Uhlenbeck process with a curvature radius
  (each individual turns its own way) and rotational diffusion. **Positive rheotaxis** (Kantsler et
  al. 2014): it turns against the current with the shear at the glass.
- The sample comes by **WHO motility classes** (`[being.motility]`): progressive, non-progressive
  (small circles), immotile and dead, with speed variation between individuals; configurable in the
  Plate panel. With `motility_half_life` swimmers become immotile before dying (two-stage loss).

## CASA (computer-assisted semen analysis)

`game/casa`, CASA panel, `microcosm_bench --casa`: it follows each swimmer's head frame by frame and
measures its kinematics like laboratory equipment (WHO 2021; Mortimer 1997, Hum. Reprod. Update 3:
403). It is **measurement**: it lives in `Run`, only reads the world and does not change the
fingerprint.

| Metric | Definition |
|---|---|
| VCL | Curvilinear speed (point-to-point path). |
| VSL | Straight-line speed (first to last point). |
| VAP | Speed of the average path (5-point moving average). |
| LIN, STR, WOB | VSL/VCL, VSL/VAP, VAP/VCL. |
| ALH | Amplitude of lateral head displacement (peak to peak). |
| BCF | Beat-cross frequency (pairs of crossings of the average path). |

Classes by thresholds: rapid progressive ≥ 25 µm/s, slow 5–25 µm/s, non-progressive without
advance; immotile below a VCL of 10 µm/s.

## Vitality and other tests

| Plate | Test | Mechanism |
|---|---|---|
| `fresh_semen` | Wet preparation (WHO): concentration, motility | Medium `seminal_plasma`, 37 °C, ~170 per field. |
| `semen_eosin_nigrosin` | Vitality | Exclusion stain: dead cells (damaged membrane) turn pink ([stains](stains.md)). |
| `semen_hos` | Hypo-osmotic swelling test (Jeyendran 1984) | `[being.osmotic_swelling]`: the tail of living cells swells and coils at ~150 mOsm. |
| `sperm_capacitation` | Capacitation, hyperactivation, acrosome reaction, chemotaxis to progesterone | `[being.capacitation]`: needs bicarbonate, albumin and temperature; hyperactivated cells beat wider with star-shaped paths; progesterone (reagent of mechanism `signal`) induces the acrosome reaction and is followed by temporal comparison. |
| `semen_leukocytes` | Leukocytospermia | Neutrophils phagocytose sperm. |

**Morphology** by strict criteria (`[[being.morphotype]]`): normal (14 %), amorphous, tapered,
pyriform, round (no acrosome), small and large heads, bent neck, coiled tail and cytoplasmic droplet,
each with its fraction and relative speed; the fraction of normal forms is set in the Plate
panel (`normal_forms=`).

The lesson `spermiogram` covers motility and temperature.

## Code

`src/game/casa.*`, `src/game/world_swim.cpp`, `src/game/world_animal.cpp`,
`src/ui/panels/` (CASA panel).

## Tests

`tests/test_sperm.cpp`.

## Limitations

- Tails do not collide with each other or with cells: [bug](../bugs/stalks-and-tails-do-not-collide.md).
- No agglutination; no vertical axis (accumulation at surfaces appears only at the dish wall).
