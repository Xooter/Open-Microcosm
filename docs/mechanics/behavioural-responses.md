# Behavioural responses

How cells react to what they sense. All of it is data-driven (`[[being.response]]`, `[being.jump]`
and a few flags); the code provides named mechanisms.

## M1 — Transient phobic response

The cell compares what it senses **now** with what it sensed a moment ago (memory of `memory`
seconds) and reacts to a sudden change, like the avoiding reaction of ciliates (Jennings) or the
photophobic response of flagellates (Diehn 1973). Up to 4 responses per being
(`kMaxResponses`, `ResponseSpec` in `bio/species.hpp`).

| `kind` | Fires when |
|---|---|
| `step_up` | The relative rise exceeds `threshold`, with the level above `min_level`. |
| `step_down` | The relative fall exceeds `threshold`, from a level above `min_level`. |
| `above` | The absolute level exceeds `threshold`. |

| `field` | What is sensed |
|---|---|
| `light` | Relative irradiance at the centre. |
| `oxygen` | O₂ [µM]. |
| `shear` | Water deformation around the cell [1/s] (M2). |
| `touch` | The anterior pole collided (1). |

| `action` | Effect |
|---|---|
| `avoid` | Avoiding reaction: backs off and turns (ciliates) or photophobic stop and turn (flagellates). |
| `jump` | A jump ([jumps](#jumps)). |
| `contract` | Whole-body or stalk contraction ([shape changes](shape-changes.md), [stalks](stalks-and-colonies.md)). |
| `flash` | Bioluminescent flash (`[being.bioluminescence]`). |

After responding, the cell waits `refractory` seconds.

## M2 — Hydrodynamic mechanoreception

`field = "shear"` reads the deformation of the water next to the cell from the fluid: an
approaching predator, a contraction of a neighbour or a stirring tool trigger escape jumps
(*Halteria*), contractions (*Vorticella*, *Stentor*) or flashes (*Lingulodinium*, whose capacity
follows a circadian clock: Hastings and Sweeney).

## Avoiding reaction and acid trap

- Ciliates reverse on threats and on collisions of the anterior pole (mechanoreception).
- `acid_trap` (*Paramecium*): it reverses when the acidity drops, so it stays where there is CO₂
  and swarms emerge (Jennings 1906). Validated: [Jennings test](../validation/README.md).

## Phototaxis, photophobia and light damage

- `phototaxis`: compares the light on either side of the anterior pole and swims towards moderate
  light (*Euglena*, *Chlamydomonas*, volvocines).
- A `step_up` of `light` with `avoid` is the photophobic response (*Blepharisma*: Kraml & Marwan
  1983).
- `[being.light_damage]`: blepharismin is photodynamic under intense light (Giese 1946): extra
  maintenance, needs O₂. The stress pigment of *Haematococcus* protects from it.

## Jumps

`[being.jump]`: cirri or bristles launch the cell several body lengths in tens of ms; spontaneous
(mean `interval`) and escape jumps (a `jump` response or a threat within 40 µm). Without inertia: the
velocity is that of the stroke while it lasts. *Halteria* (Jakobsen 2001), *Mesodinium*.

## Chemokinesis and chemotaxis

- `chemokinesis`: changes speed with the local food.
- Chemotaxis to a released signal is described in [chemical signals](chemical-signals.md).
- Bacteria do run and tumble ([bacteria](bacteria-and-phages.md)); neutrophils follow fMLP
  ([animal cells](animal-cells.md)); sperm follow progesterone ([sperm](sperm-and-casa.md)).

## Code

`src/bio/species.hpp` (`ResponseSpec`, `JumpSpec`), `src/ai/autopilot.*`, `src/game/world.cpp`
(`perceive`).

## Tests

`tests/test_mechanics.cpp`, `tests/test_validation.cpp` (Jennings).

## Limitations

- No habituation or response hierarchy (*Stentor*, Dexter et al. 2019).
- No avoidance of strong acid (Jennings saw a ring around the bubble).
- No gravitaxis: there is no vertical axis.
