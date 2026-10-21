# Interface

## Program icon

The window and taskbar icon is set by `App` after `InitWindow` (`SetWindowIcon` with
`assets/icon/icon-256.png`); the Windows executable embeds `icon.ico` as a resource
(`assets/icon/microcosm.rc.in`, configured by CMake); Linux packages carry the launcher
`packaging/linux/openmicrocosm.desktop`. Sources and regeneration: [`assets/icon`](../../assets/icon/README.md)
(the icon is provisional).

## A single workspace

`App` (`app/app.cpp`) opens the window, creates the plate and runs the loop: UI → input on the plate
→ simulation step → world drawing → UI drawn on top. There is no screen state machine and no
mutually exclusive modes. The program starts directly in the laboratory with an **empty Petri dish**.

## Dockspace with a transparent centre

Dear ImGui (docking branch) with its own raylib backend (`ui/imgui_raylib`, no rlImGui). The free
centre (`App::viewport`) is where the plate is seen; the camera, the minimum zoom and the LOD use that
rectangle. Clicks belong to the world only if `!ImGui::GetIO().WantCaptureMouse` (and keys, if not
`WantTextInput`).

## Panels

Plate, Catalogue (any being, with icons), Inspector, Tools, Simulation, Preferences (image,
smoothness and performance), Processes (rates of chemistry, microbiology and fungi), Statistics
(census, conserved totals of P, N, Si and Fe, and the controlled being), Graphs, Measurement,
Content (loaded packs, data warnings, `.biopack` import and export), Cultures, Experiment, Lesson,
Glossary, Notes, Stains, CASA and Help. Plate, Catalogue, Stains, Inspector, Statistics, Tools,
Simulation and Measurement start open; the rest open from *View* (a panel closed by default sets
`open = false` in its constructor).

## Tools

Left toolbar or keys 1–0, M and G: Select, Control, Ink, Detritus, Erase, Lyse, Squeeze, Stir,
Stretch, Dropper, Measure, Region and Spawn (when arming a being from the catalogue). Select is the
default: a click on anything picks it and opens its card.

## Adding a UI feature

- A panel is a class deriving from `ui::Panel` (`ui/panels/*.cpp`) plus a line in
  `ui/panels/panels.cpp`.
- A menu action or shortcut is a `ui::Command` in `App::registerCommands`: menus and shortcuts come
  from the same command table.
- A tool is a value of `Tool` (`app/tools.*`) and its case in `App::toolInput`.
- Any change to the world goes through an intervention ([reproducibility](reproducibility.md)).

## Widgets and style

`ui/widgets`: `slider`, `sliderLog`, `checkbox`, `combo`, `readout`, `bar`, `section`. Label on the
left, help on hover and a typeable value (Ctrl+click). Every colour and spacing comes from `ui/theme`.
The font carries Latin-1 (accents, ñ, µ, °), Greek, punctuation and operators; superscripts and
subscripts (O₂, 10⁻⁵) are derived from Roboto's digits and the arrow → is drawn.

## Languages

Every visible text goes through `ui::tr(key)`, where the key is the Spanish text. Window IDs are
stable (`title###key`) so the saved layout does not depend on the language.

- `assets/lang/en.txt` (English) and, in each content pack, its `lang/<code>.txt`: lines
  `key<TAB>translation`.
- `readout` formats are translated too: the translation keeps the specifiers (checked by
  `tools/claves_tr.py --check`, run by `tools/ci.sh`).
- A new text: `tools/claves_tr.py --check assets/lang/en.txt --faltantes`; texts built at run time:
  `BIO_TR_FALTANTES=file ./microcosm`.
- If the Spanish of a key changes, its translation no longer matches and Spanish is shown (never a
  stale translation).
- Values with numbers built by `game/inspect` (part of `biosim`, without the UI) are translated
  through `core/localize`: the app installs `ui::tr` with `setLocalizer` when it loads the
  language; `fmt` translates the whole format (whose translation keeps the specifiers) and `L()` the
  fragments that go through `%s`. Without a localizer (tests, bench) the card stays in Spanish.
  Tested in `tests/test_inspect.cpp` ("los valores con números salen en el idioma de la interfaz").

## Accessibility

Colours that distinguish categories come from the **Okabe–Ito** palette (`core/palette`: limitation
map, plot series, the theme's good/warning/dead); continuous maps use viridis. The UI scale
(`Options::uiScale`, 0 = the monitor's) is used by `ui::uiScale()`: any new pixel size in the UI is
multiplied by it.

## Options

`Options` (in `game/progress`, `<data>/openmicrocosm/options.txt`) are applied by `App::applyOptions`
(post-process, render scale, detail, VSync, threads, LOD, GPU) and the FPS cap by
`App::applyFrameCap`; MSAA only when creating the window. Quality presets
(`applyGraphicsPreset`) touch the image and the catch-up steps per frame (Low: 1, so the image stays
fluid and the simulation runs slower than the clock), never the simulation's results. The window opens
maximized the first time (`Options::maximized`, remembered on exit; the system fits it to the work
area with its title bar). Unmaximized, it is 1600 × 900 capped at 85 % of the monitor and centred.
Fullscreen (F11) is borderless; leaving it maximizes the window again if it was maximized. The panel layout is saved in
`<data>/openmicrocosm/layout.ini` (*View > Reset layout* rebuilds it). Catalogue icons are drawn once with
raylib into an atlas (`ui/being_icons`).

## Code

`src/app/`, `src/ui/`, `src/ui/panels/`.
