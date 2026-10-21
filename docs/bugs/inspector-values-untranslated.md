# Numeric values in the inspector card stay in Spanish

| Field | Value |
|---|---|
| **ID** | `inspector-values-untranslated` |
| **Type** | `bug` |
| **Status** | `open` |
| **Severity** | `cosmetic` |
| **Area** | [`interface`](../mechanics/interface.md) |
| **Affects** | Inspector (Ficha) panel in English and any other language |
| **Code** | `src/game/inspect.cpp` |
| **Reported** | 17/10/2026 |
| **Last verified** | 21/10/2026, commit `ae4affb` |
| **Fixed in** | — |

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

Run with `BIO_TR_FALTANTES=missing.txt ./build/release/bio` and open a card: the value strings are
not listed as keys because they never go through `tr`.

## Cause

Confirmed: `game/inspect` is part of `biosim` (no UI) and formats text directly.

## Proposed fix

Return structured values (number, unit, format key) and format them in the UI with `ui::tr`, whose
translations keep the format specifiers (`tools/claves_tr.py --check`).

## Workaround

None.

## References

- Roadmap phase A.

## History

- 17/10/2026: noted in the roadmap.
- 21/10/2026: documented here.
