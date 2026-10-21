# Vessels: `vessels/<id>.toml`

A `[[vessel]]` is the container of a cell culture or a well-mixed culture.

| Key | Meaning |
|---|---|
| `id`, `name`, `source`, `legacy_ids` | Identity and reference. |
| `area_cm2` | Culture area. |
| `volume_ml` | Working volume. |
| `gas_transfer` | Optional. Gas transfer velocity through the surface [cm/h] when the medium is mixed (swimmers, stirring): the well-mixed culture exchanges O₂ and CO₂ at k / depth. 0 or absent: still medium, D / depth². |

The depth (volume / area) sets how much medium lies above each cell and how fast O₂ enters and CO₂
leaves through the surface ([well-mixed culture](../mechanics/well-mixed-culture.md#time)).

Factory vessels: `dish_35mm`, `dish_60mm`, `dish_100mm`, `well_6`, `well_24`, `well_96`,
`flask_t25`, `slide`, `gause_tube` (5 cm³ in a 10 c.c. centrifuge tube, mixed:
`gas_transfer` = 2.07), `erlenmeyer_250`, `test_tube_10`, `microchemostat`.
