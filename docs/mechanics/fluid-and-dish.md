# Fluid and dish

## Water at low Reynolds number

At the scale of protists the Reynolds number is ~10⁻⁴: there is no inertia and the velocity of the
water is the instantaneous response to the forces (Stokes equations). The film of water between
slide and cover slip adds Hele-Shaw friction (the Brinkman term 12ν/h²), which makes currents
local: they die out at ~h/√12 from the force. The film thickness (`gapHeight`) controls how local
the currents are.

The equations are solved **exactly in Fourier space** (`sim/fluid`):
û = P·f̂ / (ν k² + α), with P the incompressible projection. Forces are applied with a Gaussian
profile of a given radius.

### Swimmers and their own flow

A swimmer exerts a pair of opposite forces on the water (zero net force: a force dipole). The water
is solved twice: without the swimmer's forces (`Fluid::backgroundAt`, which is what the swimmer
feels) and with them (what everything else feels). Otherwise the swimmer would be dragged by its own
flow. A posterior flagellum (`flagellum = "posterior"`) is a *pusher*; the default is a *puller*.

## The dish

The world is a **circular glass well** centred on the grid (`sim/dish.hpp`, `World::dish()`):
radius = half the shorter side − `WorldConfig::dishMargin`. A real Petri dish is ~90 mm; at this
scale what really exists is a well or glass ring on a slide (a counting chamber). The wall is the
limit of the world.

The grids are still periodic inside (power-of-two sizes for the FFT), but nothing lives or crosses
outside the circle:

| Thing | How it is kept inside |
|---|---|
| Soft bodies | `soft_body::confineToDish` in each XPBD iteration (also on the GPU, `solve.comp`): a pellicle, wall or theca is moved whole by the displacement of its deepest point (it does not mould itself to the glass; projecting point by point flattened the posterior tip of a ciliate backing into the wall and left the membrane folded), a plastic membrane (amoeba) is projected point by point. Statistical bodies are moved whole too (`pushIntoDish`). Test: `tests/test_dish.cpp` ("un ciliado que retrocede contra el vidrio no se pliega"). |
| Particles (bacteria, detritus, spores, *Bdellovibrio*, cyanobacteria) | `Dish::bounce`: they return and their heading reflects off the wall. |
| Hyphae | Do not grow towards the glass. |
| Spawns | `World::randomPoint`. |
| Water | Velocity mask (`DishMask::flow`): zero in the glass, a ramp next to the wall. |
| Chemistry | Glass cells are ghosts holding the value of their mirror image (no flux by images); mass is conserved by rescaling the water (`ChemField::setMask`). Totals use `ChemField::sum()` (water only), never `grid().sum()`. |

With `WorldConfig::dish = false` the world becomes toroidal (wrapped edges) again.

The render draws the dish wall with `dish_glass.fs` (glass, dark edge, Becke line and meniscus)
before the post-process ([rendering](rendering-and-optics.md)).

## Code

- `src/sim/fluid.hpp/.cpp`, `src/sim/fft.*`, `src/sim/dish.hpp`, `src/sim/grid2d.hpp`.

## Tests

`tests/test_medium.cpp`, `tests/test_dish.cpp`.
