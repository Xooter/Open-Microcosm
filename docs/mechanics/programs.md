# Programs

What a laboratory changes by the clock, stored as data of the plate or the culture instead of loose
interventions: a 16:8 photoperiod, a temperature ramp, a phosphate pulse, feeding bacteria every day
or changing the medium (Gause's protocol). Programs are saved as their **definition** (not as
thousands of events) and applied **once per step**, so the result does not depend on the frame rate
and a scene replays them (`game/program`).

## Clock

Laboratory time [h]: in the culture, its time; on the plate, **biological hours** (biology is
compressed and a photoperiod is biology).

## Format

One line per program: `target shape start end from to period cycle amount`
([user files](../content/user-files.md#program-program-in-plates-and-cultures)).

| Shape | Meaning |
|---|---|
| `ramp` | Linear from `from` to `to` between `start` and `end`. |
| `cycle` | Square cycle: `to` for `cycle` of each `period`, `from` the rest (photoperiod). |
| `sine` | Sinusoid between `from` and `to` with `period`. |
| `event` | An action every `period` (or once). |

Targets: `temperature`, `light`, `solute:<solute>` (`phosphate`, `nitrogen`, `silicate`, `iron`,
`b12`, `organic`, `o2`, `co2`), `parameter:<registry key>`, or an action: `bacteria` (per mL added),
`medium` (fraction renewed: culture only), `dilution` (fraction replaced: culture only), `add:<solute>`
(µM added), `inoculate:<being id>` (individuals). On the plate there is no `medium` or `dilution`
(there is perfusion).

## Where they live

- Culture: in the culture scene (`program=` lines).
- Plate: `WorldConfig::program`, applied by `World::applyProgram`; live changes are the
  `Program` intervention ([reproducibility](reproducibility.md)).
- Lessons and experiments can carry programs.

## Code

`src/game/program.*`, `src/game/world_program.cpp`.

## Tests

`tests/test_program.cpp`, `tests/test_plate_program.cpp`.
