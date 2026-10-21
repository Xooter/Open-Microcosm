# Rendering and optics

The render only **reads** simulation state (`render/`); nothing in the simulation depends on it.

## Bright field

The cells' coverage goes to a shader with an optical edge and halo (`cell_brightfield.fs`). The dish
wall is drawn with `dish_glass.fs` (glass, dark edge, Becke line and meniscus) before the
post-process. With a dish there is a single copy of the world (no tiling when zooming out).
Everything happens in one plane (no focal plane).

The field is drawn into a texture and passes through `microscope_post.fs`: vignette, chromatic
aberration, grain and, with `Options::opticFlaws`, uneven Köhler illumination and dust on the
eyepiece and the camera (they belong to the optics, so they stay fixed to the screen and multiply the
light). A photo's grain comes from the simulated time.

Cell cultures are really looked at with **phase contrast**; until that lens exists, bright field
(roadmap C).

## Semantic zoom

`render/lod.hpp` (`RenderLodSettings`: all configuration in one place): each being chooses its level
(`DetailLevel`: density, simplified, normal, detail, subcellular) by its **real size projected in
pixels**, not by the zoom.

- Bacteria and *Bdellovibrio*: a linear pass clips by the view and splits between the density
  (`DensityGrid` in `game/`, power-of-two cells anchored to the world → `PopulationLayer`,
  Beer–Lambert turbidity) and instances (`InstancedParticles`, only the visible ones), with a
  cross-fade.
- Up close, the ones nearest the centre (cap `maxDetailed`) and the selected one are drawn with
  structures (`render/structure_renderer`).

## Body plan

`bio/body_plan` lists which structures each being has and their real size (`Simulated` or
`Declared`, [format](../content/body_plans.md)). Each structure appears when its projected size
reaches a few pixels; what a light microscope cannot resolve is shown only in the **model view**
(`ViewMode`). Declared structures are placed with a hash of the stable id (`Bacteria::id`), never at
random. `game/scale_path` gives the biological scale and the breadcrumb (Plate > organism > cell >
structure).

## Stains and limitation map

Stains are evaluated per target in the render ([stains](stains.md)); the limitation map colours each
cell by its limiting factor ([measurement](measurement.md)).

## Photo and time-lapse

- **Photo** (`render/photo`): the central field without the interface, with a scale bar of a round
  length in µm and a JSON with the field width in µm (no "magnification": there are no simulated
  objectives), µm per pixel, time, seed, plate, stain and provenance. Saved in
  `<data>/biogame/photos`.
- **Time-lapse** (`game/timelapse`): one frame per interval of **biological** time
  (`TimelapseClock`), never per screen frame. Headless: `bio_bench --timelapse <bio_min>`.

## Performance

Own batched rendering (`render/triangle_batch`), instanced particles, a render scale that draws the
scene smaller and rescales it at the end (`Renderer`). Quality presets touch only the image.

## Code

`src/render/*`, `assets/shaders/*.fs`, `src/bio/body_plan.*`, `src/game/scale_path.*`.

## Tests

`tests/test_render_lod.cpp`, `tests/test_body_plan.cpp`, `tests/test_timelapse.cpp`.

## Pending lenses

Phase contrast, dark field, fluorescence (with chlorophyll fluorescence), India ink, objectives,
polarisation (roadmap C).
