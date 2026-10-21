# Program icon

| File | Use |
|---|---|
| `icon.svg` | Source of the icon (1024 × 1024): a *Paramecium* in a glass well under bright-field light. |
| `icon.png` | 1024 px render of the SVG. |
| `icon-256.png` | Window and taskbar icon (`SetWindowIcon` in `App`) and the Linux launcher icon. |
| `icon.ico` | Windows executable icon (16–256 px), embedded through `microcosm.rc.in`. |

Regenerate everything after editing the SVG:

```sh
rsvg-convert -w 1024 -h 1024 icon.svg -o icon.png
magick icon.png -resize 256x256 icon-256.png
magick icon.png -define icon:auto-resize=256,128,64,48,32,16 icon.ico
```

The Linux launcher is [`packaging/linux/openmicrocosm.desktop`](../../packaging/linux/openmicrocosm.desktop).

## TODO

- [ ] **Definitive icon.** The current one is provisional, generated as a simple SVG. A designed
  icon is welcome: keep the motif readable at 16 × 16 px, licence it GPL-3.0-or-later (or a
  compatible licence) and regenerate the files above.
- [ ] **macOS icon** (`.icns` with `iconutil` and `MACOSX_BUNDLE_ICON_FILE`), together with the rest of
  macOS support (community supported).
