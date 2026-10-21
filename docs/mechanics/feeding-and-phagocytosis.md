# Feeding and phagocytosis

**Eating is physical**: what is ingested forms a **food vacuole** (an organelle) that closes, is
digested and is expelled (`bio/feeding`, `bio/phagocytosis`).

## Feeding modes (`feeding`)

| Mode | Beings | Mechanism |
|---|---|---|
| `oral_groove` | *Paramecium*, *Tetrahymena*, *Vorticella*, *Stentor* | Filter feeder: generates its oral current in the water and swallows what passes through the cytostome, with a maximum flow rate. |
| `anterior_pole` | Flagellates, *Salpingoeca* | Swallows through the anterior pole, slowly. *Salpingoeca*'s collar retains bacteria pushed by its posterior flagellum; the collar and mouth are at the rear pole. |
| `phagocytosis` | *Amoeba*, *Chaos*, *Acanthamoeba*, neutrophil | Engulfs on contact with a phagocytic cup. |
| `raptorial` | *Didinium*, *Litonotus*, *Frontonia*, *Peranema* | Hunts and swallows whole prey. |
| `tentacles` | Suctorians (*Podophrya*, *Tokophrya*, *Acineta*) | `Captured` holds the prey and drains its biomass in biological time. |
| `neck` | *Lacrymaria* | Extensible neck reaching for prey ([shape changes](shape-changes.md)). |
| `axopodia` | *Actinophrys* | Axopodia trap prey on contact (`tentacle_reach`), then a phagocytic cup. |
| `perforation` | *Vampyrella* | Attaches to a *Spirogyra* cell, perforates the wall (`perforation_hours`) and empties it, leaving the empty wall. |
| `osmotrophy` | Yeasts, fungi | Takes dissolved organic matter. |

`prey`, `mouth_prey` and `flees_from` name ids or tags. Histophagous beings (`histophagous`,
*Coleps*) go towards damaged cells and eat them in bites, several at once.

## Bacteria and the third dimension

Bacteria have a **height in the film** (`World::filmHeight`, changing at random each time they
swim across): the mouth only swallows those passing at its height (the view is 2D, the film is
not); an amoeba, those at the height of its body and those in the biofilm. Attached bacteria
(biofilm) cannot be reached by filter feeders. The bacterial capture radius of the mouth
(`bacteriaCaptureRadius` = 75 µm) is **calibrated** with the measured clearance
(`microcosm_bench --respuesta-funcional paramecium`): the plate and the culture must eat the same.

A species with a measured `max_ingestion` eats at that rate, on the plate and in the culture.
Feeding of many mouths uses a bacteria grid; eaten bacteria are marked and removed together at the
end of the step.

## Digestion

- Small prey (bacteria, detritus) are eaten at a **mechanical** (real-time) rate and yield
  ×`bioTimeScale`; whole cells are limited by digestion, which already runs in biological time, and
  yield ×1.
- Vacuole kinetics (`digestionFractions`, `digestVacuole`) are shared with the culture.
- Satiety by ingested volume (`World::isSatiated`) and handling time between whole prey
  (`handling_hours`, `Digestion::handling`).
- Vacuoles acidify while digesting (Congo red and neutral red show it: [stains](stains.md)).

## Mouth width

`mouth_width` [µm] is the maximum opening (dilated cytostome). A prey enters only if its minimum
Feret width, including horns or arms, does not exceed it: horns protect from grazing. It is a pure
function of `bio/feeding`, also used by the culture.

## Phagocytic cup (zipper mechanism)

Component `PhagocyticCup` (amoeba, heliozoan, neutrophil):

1. Contact with an eligible prey starts the cup; the prey is held by ligand–receptor binding (it
   barely yields in contacts, otherwise the cup squeezes it out like a seed).
2. The front of each lip advances, accumulating point by point; membrane is added at the lip.
3. Lips cannot slip between two cells stuck together.
4. When the lips touch, `findClosure` checks that the outer ring keeps its orientation and area, and
   the prey passes into a phagosome.

Armour ≥ 0.5 prevents phagocytosis and trichocysts can reject a cup ([genome](genome-and-organelles.md)).
Phagocytoses (which destroy entities) are applied after traversing the EnTT view.

## Code

`src/bio/feeding.*`, `src/bio/phagocytosis.*`, `src/game/world.cpp`, `src/game/calibration.*`.

## Tests

`tests/test_player.cpp`, `tests/test_interactions.cpp`, `tests/test_validation.cpp`
(functional-response calibration; long test "la membrana de la ameba no se cruza al comer presas de
distinta forma").
