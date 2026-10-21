# Thinning

Taking a fraction out of a stirred culture, discarding it and topping the volume up with fresh
medium. It is a laboratory operation in its own right (harvesting, a disturbance, keeping a
population below its carrying capacity) and the daily sample of Gause's experiments, available on
the plate and in the well-mixed culture, by hand or programmed.

## Real biology

Gause (1934, *The Struggle for Existence*, ch. V, section II) counted a 0.5 cm³ sample of each
5 cm³ culture every day "and then destroyed" it: a tenth of the population. In section III(5) he
explains that this "elementary movement" of thinning and refilling is what produces the
displacement of one species by another once the food is shared: each time the room left is taken
up faster by the species with the higher coefficient of multiplication. Without it, a well-mixed
culture with a constant ration has no turnover and the first species to take the food keeps it.
The same holds for any periodic harvest or dilution: it is the semi-continuous version of the
chemostat (Tilman's cultures were semi-continuous too).

## What is simulated

- **Well-mixed culture** (`Culture::dilute`, operation `dilution`): every protist leaves with
  probability *f* (a binomial draw per cell), and *f* of the medium, bacteria, detritus and phages
  is replaced with fresh medium. The volume does not change.
- **Plate** (`World::thin`, intervention `Thin`): the drop is stirred and *f* of what is in
  suspension leaves at random: soft-bodied cells (colonies and filaments joined by adhesions leave
  whole), free bacteria (with any *Bdellovibrio* inside), free *Bdellovibrio*, zoospores,
  cyanobacterial filaments (whole), and ink, detritus and fine mineral particles. What is stuck to
  the glass stays: sessile cells with a stalk or foot (`Anchored`), adherent animal cells, the
  protoplasts of a tissue, sand grains, biofilm bacteria (`Bacteria::attached`), hyphae, the
  plasmodium and settled sediment (radius ≥ `kSettledRadius`). A cell held by a predator stays or
  leaves with it. Then *f* of the dissolved medium is replaced with the plate's medium
  (`World::renewDissolved`, the same as perfusion). Removed cells are not deaths: they do not enter
  `deathTally`.
- **Mass budget:** what leaves with the liquid is recorded as `Flow::Washout` and as an external
  output, so the budget still closes. On the plate it is measured as the change of
  `massBudget()` across the removal, which covers every compartment.
- **Randomness:** `rng_stream::kThin`, one draw per item in a fixed order (cells sorted by entity),
  so a scene replays it bit for bit.

## Controls

- Plate: *Plate > Thinning (live)*, with the fraction, *Thin now* (an intervention; also
  *Simulation > Thin the plate* or the **T** key, with the same fraction) and *Thin periodically* (adds a `dilution` event to the plate's program, every so many biological hours).
- Culture: *Cultures > Medium*, "Fraction to dilute" and *Dilute*; the *Gause protocol* program
  button includes the daily 10 % thinning before the medium change.
- Programs, lessons and experiments: the `dilution` action (fraction) in a program, on the plate and
  in the culture ([programs](programs.md)); a lesson action `dilution <f>` in a culture or a `Thin`
  intervention on a plate.

## Code

`src/game/world_thin.cpp` (`World::thin`), `World::renewDissolved` and `World::removeCell` in
`src/game/world.cpp`, `Culture::dilute` in `src/game/culture.cpp`, `InterventionKind::Thin`,
`src/ui/panels/plate_panel.cpp`, `src/ui/panels/culture_panel.cpp`.

## Tests

`tests/test_plate_program.cpp` (the plate removes about half of what is suspended with *f* = 0.5,
keeps the biofilm and closes the P and N budget; a programmed `dilution` repeats it),
`tests/test_gause.cpp` (Gause's protocol with the daily 10 %).

## Limitations

- On the plate the draw is uniform over the whole dish: a real pipette takes a local volume of a
  liquid that is never perfectly mixed.
- Swimmers do not escape the pipette.
