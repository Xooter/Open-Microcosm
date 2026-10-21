# Minerals: `minerals/<id>.toml`

A `[[mineral]]` is an inorganic sediment particle as it looks in bright field. Biomes count their
sediment per mineral id ([biomes.md](biomes.md)). Registry: `game/mineral`.

| Key | Meaning |
|---|---|
| `id`, `name`, `description` | Identity. |
| `grain_name` | Name of a fixed sand grain of this mineral. |
| `radius` [µm] | `[min, max]`. |
| `density_difference` [kg/m³] | With water (Stokes sedimentation). |
| `opacity` | 0–1. |
| `flocculates` | Whether particles clump into flocs. |
| `style` | `floc`, `angular_grain`, `flake`, `opaque_grain`, `refractile_globule`, `frustule`. |

Factory minerals: `clay`, `quartz`, `mica`, `iron_oxide`, `sulfur`, `frustule` (empty diatom
frustules: their Si redissolves slowly).
