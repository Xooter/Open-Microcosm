# Idea backlog

Fifty ideas for an educational, scientific, open-source sandbox. `[x]` marks the ideas that were
chosen (most are done or scheduled in the [roadmap](../roadmap.md)); `[ ]` are open for discussion.
Proposing a new idea: open an issue or a pull request that adds it here.

## Experiments and laboratory control

- [x] 1. **Saveable scenes:** export and import a drop (biome, seed, medium parameters and beings
      placed by hand) as a text file, to share and replay it. *(Done: scenes.)*
- [x] 2. **Timeline with replays:** record the session by seed and events and rewind or skip to any
      instant (the simulation is already deterministic).
- [ ] 3. **Batch experiments:** run N drops with a swept parameter (for example Si from 0 to
      300 µM) headless and export the results. *(Done for cultures and plates: `microcosm_batch`.)*
- [ ] 4. **Ramps and programs:** vary temperature, light or nutrients over time (day/night cycle,
      phosphate pulse, season). *(Done: programs.)*
- [ ] 5. **Controls and replicates:** two drops side by side with the same seed, changing a single
      variable, to compare.
- [x] 6. **Antibiotics, salt, acids and toxins** as reagents added with the dropper, with a
      dose–response per species.
- [ ] 7. **Dilution and seeding:** transfer an aliquot to a new drop (serial cultures), as in a
      microbiology lab. *(Done in the culture.)*
- [ ] 8. **Chemostat:** continuous flow of fresh medium and outflow, to study steady-state growth
      (Monod). *(Done in the culture.)*
- [ ] 9. **Optical tweezers:** hold and drag a cell with the mouse without damaging it, with the
      real force in pN.
- [ ] 10. **Micropipette:** aspirate a cell or a group of bacteria at one point and release them at
      another.

## Measurement and analysis

- [x] 11. **Live plots:** population curves per species, nutrients, O₂ and pH, with zoom and CSV
      export.
- [x] 12. **Visible mass budget:** fluxes of P, N, Si, Fe and C between water, cells, detritus and
      sediment, closing at 100 %.
- [x] 13. **Ruler and scale:** measure distances and areas in µm directly on the image.
- [ ] 14. **Trajectory tracking:** draw a cell's trail and compute its mean speed, mean squared
      displacement and type of motion. *(Done for sperm: CASA.)*
- [x] 15. **Region quantification:** count cells and measure concentrations inside a hand-drawn
      area.
- [x] 16. **Automatic growth curve:** doubling time, lag phase and carrying capacity fitted to the
      drop's data.
- [x] 17. **Limitation map:** colour each cell by the nutrient currently limiting its growth (P, N,
      Si, Fe, B₁₂, light).
- [ ] 18. **Cell quota panel:** see how many times the minimum quota of each nutrient a selected
      cell holds.
- [ ] 19. **Lineage tree:** reconstruct the genealogy of a population, with mutations per branch
      and selection rate.
- [x] 20. **Open data export:** CSV/JSON of time series and of each species' card, with units and
      parameters used.

## Microscopy and optics

- [x] 21. **Interchangeable lenses:** bright field, phase contrast, dark field and fluorescence,
      each with the information the real one shows.
- [x] 22. **Real stains:** Gram, Lugol (starch), DAPI (nuclei), neutral red, methylene blue and
      India ink for capsules.
- [x] 23. **Chlorophyll fluorescence:** algae glow red under blue light, dimmer when iron-starved.
- [ ] 24. **10×, 40× and 100× oil objectives:** magnification changes the field and depth; at 100×
      individual bacteria are visible.
- [ ] 25. **Microscope photo with scale bar**, date, magnification and seed, ready to publish or use
      in class. *(Done, without magnification: there are no simulated objectives.)*
- [ ] 26. **Time-lapse video:** compress biological hours into a clip of seconds. *(Done.)*
- [ ] 27. **Tyndall dark field:** tiny particles (clay, bacteria) scatter light and look bright.
- [ ] 28. **Polarisation:** crystals (frustules, quartz, starch) change colour as the polariser
      rotates.
- [ ] 29. **Overhead loupe:** a floating higher-magnification inset over what is under the cursor.
- [ ] 30. **Data ghost mode:** overlay a nutrient's concentration only inside cells, as a halo.

## Educational content

- [x] 31. **Guided lessons:** short walkthroughs with an observable goal ("why does the diatom not
      divide without silica?") and a quiz.
- [x] 32. **Classroom mode:** the teacher prepares a drop with a fixed seed and students compare it,
      independent of chance.
- [ ] 33. **Notes on the card:** the user writes observations anchored to a being, a place or an
      instant, exported with the experiment. *(Done.)*
- [ ] 34. **Cards with references:** each card cites the scientific source of its parameters
      (rates, sizes, quotas) and marks what was simplified. *(Partly: the being docs in
      `docs/content/beings/`.)*
- [ ] 35. **Interactive glossary:** terms such as Reynolds, chemotaxis or Redfield with a minimal
      in-program animation. *(Done: the animation is the simulation.)*
- [ ] 36. **Hypothesis and prediction:** before running, the user predicts the result and the
      program compares it with what happened.
- [ ] 37. **Historical scenarios:** Great Oxidation, Snowball Earth, lake eutrophication and
      cyanobacterial bloom.
- [x] 38. **Real cases:** reproduce classic experiments (Gause with *Paramecium*, Tilman with
      diatoms and silica, Jennings with CO₂).
- [ ] 39. **Interface translation:** strings in a separate file to add languages. *(Done.)*
- [x] 40. **Accessible mode:** colour-blind palette, large text and event subtitles.

## New biology and chemistry

- [x] 41. **Complete silicon cycle:** pH- and temperature-dependent dissolution and frustule
      sedimentation out of the field of view.
- [x] 42. **Iron chelation:** siderophores from bacteria and algae competing for Fe, with
      precipitation as oxide at high pH.
- [x] 43. **More vitamins and micronutrients:** thiamine (B₁), biotin (B₇), zinc, cobalt,
      molybdenum and manganese, with real auxotrophies per species.
- [x] 44. **Inorganic carbon:** limiting CO₂, carbon-concentrating mechanisms and their effect on
      pH.
- [ ] 45. **Endosymbiosis and kleptoplasty:** swallow an alga and keep it, with the constraints of
      each real case, as an experiment rather than an upgrade. *(Done for* P. bursaria *and*
      Mesodinium*.)*
- [x] 46. **Biofilms and extracellular matrix:** bacterial mats that change the flow and retain
      nutrients.
- [ ] 47. **Epidemics and resistance:** phages, antibiotics and the evolution of resistance with a
      cost, visible in the lineage tree.
- [x] 48. **Real soil and water:** load a sample with measured parameters (T, pH, conductivity,
      nutrients) and see which community it supports.

## Technology and community

- [ ] 49. **Species as data:** TOML files per species so the community can add organisms without
      touching C++ (with validation of units and biological coherence). *(Done: content packs.)*
- [x] 50. **Headless core and bindings:** expose `biosim` as a library with a minimal interface (C
      or Python) for research and classroom notebooks.
