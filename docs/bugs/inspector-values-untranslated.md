# Numeric values in the inspector card stay in Spanish

| Field | Value |
|---|---|
| **ID** | `inspector-values-untranslated` |
| **Type** | `bug` |
| **Status** | `fixed` |
| **Severity** | `cosmetic` |
| **Area** | [`interface`](../mechanics/interface.md) |
| **Affects** | Inspector (Ficha) panel in English and any other language |
| **Code** | `src/game/inspect.cpp` |
| **Reported** | 17/10/2026 |
| **Last verified** | 21/10/2026, commit `c8b0bc3` |
| **Fixed in** | commit "La ficha traduce sus valores con números" (21/10/2026) |

## Summary

The inspector translates titles, labels, names and descriptions, but the values that
`game/inspect` builds with numbers ("12 µm de largo", units with words) are assembled in Spanish and
do not pass through `ui::tr`.

## Steps to reproduce

1. Preferences > Language: English.
2. Select any cell and open its card.

## Expected behaviour

Every visible string in the selected language.

## Actual behaviour

The formatted value rows are in Spanish.

## Evidence

Run with `BIO_TR_FALTANTES=missing.txt ./build/release/microcosm` and open a card: the value strings are
not listed as keys because they never go through `tr`.

## Cause

Confirmed: `game/inspect` is part of `biosim` (no UI) and formats text directly.

## Proposed fix

Return structured values (number, unit, format key) and format them in the UI with `ui::tr`, whose
translations keep the format specifiers (`tools/claves_tr.py --check`).

## Fix

`biosim` cannot call `ui::tr`, so `core/localize` holds a translation hook that the app installs
with `setLocalizer(&ui::tr)` after loading the language. In `game/inspect`, `fmt` translates the
whole format string (`tools/claves_tr.py --check` checks that the translation keeps the
specifiers) and `L()` translates the fixed fragments passed through `%s` and the pieces joined by
concatenation. The hover tooltip translates the card's title and headline too.

## Workaround

None needed.

## References

- Roadmap phase A.

## History

- 17/10/2026: noted in the roadmap.
- 21/10/2026: documented here.
- 21/10/2026: fixed with `core/localize` (test in `tests/test_inspect.cpp`).
