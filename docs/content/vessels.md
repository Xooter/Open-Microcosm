# Vessels: `vessels/<id>.toml`

A `[[vessel]]` is the container of a cell culture or a well-mixed culture.

| Key | Meaning |
|---|---|
| `id`, `name`, `source`, `legacy_ids` | Identity and reference. |
| `area_cm2` | Culture area. |
| `volume_ml` | Working volume. |

The depth (volume / area) sets how much medium lies above each cell and how fast O₂ enters and CO₂
leaves through the surface (D / depth²).

Factory vessels: `dish_35mm`, `dish_60mm`, `dish_100mm`, `well_6`, `well_24`, `well_96`,
`flask_t25`, `slide`, `gause_tube` (0.5 cm³), `erlenmeyer_250`, `test_tube_10`, `microchemostat`.
