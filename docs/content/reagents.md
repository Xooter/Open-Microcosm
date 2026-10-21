# Reagents: `reagents/<id>.toml`

A `[[reagent]]` is a substance with a named **mechanism** and real targets
([reagents](../mechanics/reagents.md)). Engine: `game/reagent`, `game/dose_fit`.

| Key | Meaning |
|---|---|
| `id`, `name`, `description`, `legacy_ids` | Identity. |
| `mechanism` | `pharmacodynamic` (Regoes et al. 2004) or `signal` (a signal with no killing of its own, with its `source`, whose effect is set by the receiving being's data, like progesterone in `[being.capacitation]`). |
| `targets` | `bacteria` or being ids. |
| `growth_dependent` | The added killing scales with how much the bacterium grows (β-lactams). |
| `diffusion` [µm²/s] | Diffusion coefficient on the plate. |
| `source` | Reference (for `signal`). |

## `[reagent.pharmacodynamics]`

`psi_max` and `psi_min` [1/h], `zmic` [µg/mL], `kappa`, `source`.

## `[reagent.published_mic]`

`value` [µg/mL], `source`. It only validates the emergent MIC: the IC50 and MIC are **not inputs**;
they come out of a dose batch (`reactivo:<id>`, `fitDoseResponse`, `minimumInhibitory`).

Factory reagents: `ampicillin`, `tetracycline`, `progesterone`.
