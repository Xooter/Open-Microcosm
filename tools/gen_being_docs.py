#!/usr/bin/env python3
"""Fichas de los seres en docs/content/beings/ (una por ser del paquete de fábrica).

Cada ficha tiene partes escritas a mano (descripción, literatura, lo que falta, mecánicas) y bloques
que salen de los datos (resumen, fisiología medida, biomas y placas). Este script reescribe solo los
bloques generados, entre <!-- GENERATED:nombre --> y <!-- /GENERATED -->, y deja el resto como está.

  tools/gen_being_docs.py           crea las fichas que faltan y actualiza los bloques generados
  tools/gen_being_docs.py --check   falla si falta una ficha, sobra una, un bloque quedó viejo o
                                    queda un "TODO:" en el texto a mano (lo corre tools/ci.sh)
"""
import argparse
import re
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PACK = ROOT / "assets" / "content" / "base"
OUT = ROOT / "docs" / "content" / "beings"

# Qué documento de docs/mechanics explica cada clave de un ser.
MECHANISMS = [
    ("conjugation", "conjugation", "life-cycle-and-division"),
    ("encysts", "cysts", "life-cycle-and-division"),
    ("divides_in_cyst", "reproductive cyst", "life-cycle-and-division"),
    ("budding", "budding", "life-cycle-and-division"),
    ("fissions_per_cycle", "several daughters per cycle", "life-cycle-and-division"),
    ("division_axis", "longitudinal division", "life-cycle-and-division"),
    ("filament", "filament", "stalks-and-colonies"),
    ("daughter_colonies", "daughter colonies", "stalks-and-colonies"),
    ("star_colony", "star colony", "stalks-and-colonies"),
    ("sliding_colony", "sliding colony", "stalks-and-colonies"),
    ("algal_conjugation", "scalariform conjugation", "stalks-and-colonies"),
    ("stalk", "stalk or foot", "stalks-and-colonies"),
    ("helix_radius", "helical swimming", "locomotion"),
    ("flagellum", "posterior flagellum (pusher)", "locomotion"),
    ("flagellum_length", "long flagellum", "locomotion"),
    ("flagellar_gliding", "flagellar gliding", "locomotion"),
    ("walks", "walking on cirri", "locomotion"),
    ("raphe_glider", "raphe gliding", "locomotion"),
    ("jump", "jumps", "behavioural-responses"),
    ("response", "phobic responses", "behavioural-responses"),
    ("phototaxis", "phototaxis", "behavioural-responses"),
    ("acid_trap", "acid trap (Jennings)", "behavioural-responses"),
    ("chemokinesis", "chemokinesis", "behavioural-responses"),
    ("light_damage", "photodynamic damage", "behavioural-responses"),
    ("bioluminescence", "bioluminescence", "behavioural-responses"),
    ("release", "released signal", "chemical-signals"),
    ("morph", "induced morph", "chemical-signals"),
    ("relay", "signal relay", "chemical-signals"),
    ("chemotaxis", "chemotaxis to a signal", "chemical-signals"),
    ("signal_toxicity", "sensitivity to a toxic signal", "chemical-signals"),
    ("photosymbiont", "photosymbionts", "symbioses"),
    ("toxicysts", "toxicysts", "feeding-and-phagocytosis"),
    ("histophagous", "histophagy", "feeding-and-phagocytosis"),
    ("mouth_width", "mouth width", "feeding-and-phagocytosis"),
    ("tentacle_reach", "tentacles / axopodia reach", "feeding-and-phagocytosis"),
    ("neck_reach", "extensible neck", "shape-changes"),
    ("perforation_hours", "wall perforation", "feeding-and-phagocytosis"),
    ("process", "processes and notches", "shape-changes"),
    ("contraction", "whole-body contraction", "shape-changes"),
    ("shape_wave", "shape wave", "shape-changes"),
    ("semicell", "semicell regrowth", "shape-changes"),
    ("somersault", "somersaults", "shape-changes"),
    ("contractile_vacuole", "contractile vacuole cycle", "genome-and-organelles"),
    ("stress_pigment", "stress pigment", "metabolism-and-nutrients"),
    ("chloroplasts", "photosynthesis", "metabolism-and-nutrients"),
    ("b12_auxotroph", "B₁₂ auxotrophy", "metabolism-and-nutrients"),
    ("frustule", "silica frustule", "metabolism-and-nutrients"),
    ("lorica_iron_ratio", "iron lorica", "metabolism-and-nutrients"),
    ("anaerobe", "anaerobic life", "metabolism-and-nutrients"),
    ("testate", "rigid test", "soft-bodies"),
    ("cyclosis", "cyclosis", "genome-and-organelles"),
    ("cell", "multicellular-organism cell", "animal-cells"),
    ("swim_path", "passive swimming path", "sperm-and-casa"),
    ("motility", "WHO motility classes", "sperm-and-casa"),
    ("osmotic_swelling", "hypo-osmotic swelling", "sperm-and-casa"),
    ("capacitation", "capacitation", "sperm-and-casa"),
    ("morphotype", "morphotypes", "sperm-and-casa"),
]

# Grupos del índice: el primero cuya etiqueta tenga el ser.
GROUPS = [
    ("Cells of multicellular organisms", ("animal_cell", "plant_cell")),
    ("Yeasts", ("yeast",)),
    ("Diatoms", ("diatom",)),
    ("Green algae", ("green_alga",)),
    ("Dinoflagellates", ("dinoflagellate",)),
    ("Ciliates", ("ciliate",)),
    ("Amoebae and heliozoans", ("amoeboid", "amoeba", "heliozoan")),
    ("Flagellates and euglenoids", ("flagellate",)),
]

FEEDING_DEFAULT = "none"


def load_toml(path):
    with open(path, "rb") as f:
        return tomllib.load(f)


def fmt(x):
    if isinstance(x, float):
        return f"{x:g}"
    return str(x)


def load_world():
    beings = {}
    for f in sorted((PACK / "beings").glob("*.toml")):
        d = load_toml(f)
        b = d["being"][0]
        b["_file"] = f
        b["_physiology"] = d.get("physiology", {}).get(b["id"], {})
        beings[b["id"]] = b
    biomes = [load_toml(f)["biome"][0] for f in sorted((PACK / "biomes").glob("*.toml"))]
    plates = [load_toml(f)["plate"][0] for f in sorted((PACK / "plates").glob("*.toml"))]
    return beings, biomes, plates


def biome_weight(biome, being):
    w = biome.get("weights", {})
    if being["id"] in w:
        return w[being["id"]]
    return 0.0 if "obligate_symbiont" in being.get("tags", []) else biome.get("base_weight", 0.0)


def summary_block(b):
    shape = b.get("shape", {})
    a, c = shape.get("semi_major_axis", 0.0), shape.get("semi_minor_axis", 0.0)
    rows = [
        ("Id", f"`{b['id']}`"),
        ("Data file", f"[`{b['_file'].relative_to(ROOT)}`](../../../{b['_file'].relative_to(ROOT)})"),
        ("In-app name", b.get("name", "")),
        ("Tags", ", ".join(f"`{t}`" for t in b.get("tags", []))),
        ("Body plan", f"`{b.get('body_plan', '')}`"),
        ("Locomotion", f"`{b.get('locomotion', '')}`"
         + (f" × {fmt(b['relative_speed'])}" if "relative_speed" in b else "")),
        ("Feeding", f"`{b.get('feeding', FEEDING_DEFAULT)}`"
         + (f" (prey: {', '.join(f'`{p}`' for p in b['prey'])})" if b.get("prey") else "")),
        ("Simulated size", f"{fmt(2 * a)} × {fmt(2 * c)} µm"
         + (f", thickness {fmt(2 * shape['thickness'])} µm" if "thickness" in shape else "")),
        ("Storage / wall", f"`{b.get('storage', 'none')}` / `{b.get('wall', 'none')}`"),
    ]
    if "ph" in b:
        rows.append(("Tolerated pH", f"{fmt(b['ph'][0])}–{fmt(b['ph'][1])}"))
    if "temperature" in b:
        rows.append(("Tolerated temperature", f"{fmt(b['temperature'][0])}–{fmt(b['temperature'][1])} °C"))
    if "max_salinity" in b:
        rows.append(("Maximum salinity", f"{fmt(b['max_salinity'])} PSU"))
    mech = []
    for key, label, doc in MECHANISMS:
        v = b.get(key)
        if v is None or v is False or v == 0 or v == [] or v == "":
            continue
        mech.append(f"[{label}](../../mechanics/{doc}.md) (`{key}`)")
    if mech:
        rows.append(("Mechanisms", ", ".join(mech)))
    out = ["| | |", "|---|---|"]
    out += [f"| **{k}** | {v} |" for k, v in rows]
    return "\n".join(out)


def physiology_block(b):
    phys = b["_physiology"]
    if not phys:
        return ("No measured physiology in the data: the being uses the general metabolism values "
                "(`metabolism.max_growth`, 0.15/h).")
    out = ["| Parameter | Value | Measured at | Estimate | Source |", "|---|---|---|---|---|"]
    for name, p in phys.items():
        value = f"{fmt(p.get('value', ''))} {p.get('unit', '')}".strip()
        if "q10" in p:
            value += f" (Q10 {fmt(p['q10'])})"
        temp = f"{fmt(p['temperature'])} °C" if "temperature" in p else "—"
        est = "yes" if p.get("estimate") else "no"
        src = str(p.get("source", "")).replace("|", "\\|").replace("\n", " ")
        out.append(f"| `{name}` | {value} | {temp} | {est} | {src} |")
    return "\n".join(out)


def presence_block(b, biomes, plates):
    lines = []
    count = b.get("initial_count", 0)
    found = [(bi["id"], biome_weight(bi, b)) for bi in biomes]
    found = [(i, w) for i, w in found if w > 0]
    if count and found:
        lines.append("Drops: " + ", ".join(f"`{i}` (weight {fmt(w)})" for i, w in found)
                     + f"; {count} individuals in a reference drop at weight 1.")
    else:
        lines.append("Drops: not seeded in the default drops (inoculate it from the Catalogue).")
    used = []
    for p in plates:
        seeded = any(s.get("being") == b["id"] for s in p.get("seeding", []))
        tissue = p.get("tissue", {}).get("being") == b["id"]
        if seeded or tissue:
            used.append(f"`{p['id']}`")
    if used:
        lines.append("Prepared plates: " + ", ".join(used) + ".")
    return "\n\n".join(lines)


def skeleton(b):
    title = b.get("name", b["id"])
    return f"""<!-- Partly generated by tools/gen_being_docs.py: edit only the text outside the GENERATED blocks. -->
# {title}

<!-- GENERATED:summary -->
<!-- /GENERATED -->

## Description

TODO: what the organism is, where it lives and how it looks under the microscope.

## In the simulation

<!-- GENERATED:presence -->
<!-- /GENERATED -->

TODO: how Open Microcosm simulates it (shape, locomotion, feeding, reproduction).

## Literature

TODO: the references behind its data and behaviour.

### Measured physiology

<!-- GENERATED:physiology -->
<!-- /GENERATED -->

## Not implemented yet

TODO: real features of the organism that the simulation does not have.

## Unique and shared mechanics

TODO: what only this being does and which mechanisms it shares with others.
"""


BLOCK = re.compile(r"(<!-- GENERATED:(\w+) -->\n)(.*?)(<!-- /GENERATED -->)", re.S)


def render(text, blocks):
    def sub(m):
        name = m.group(2)
        if name not in blocks:
            return m.group(0)
        return f"{m.group(1)}{blocks[name]}\n{m.group(4)}"
    return BLOCK.sub(sub, text)


def group_of(b):
    tags = set(b.get("tags", []))
    for title, keys in GROUPS:
        if tags & set(keys):
            return title
    return "Others"


def index(beings, titles):
    out = ["<!-- Generated by tools/gen_being_docs.py: do not edit by hand. -->",
           "# Beings", "",
           "One document per being of the factory pack: description, literature, measured physiology,",
           "what is not implemented yet and its unique and shared mechanics. The data format is in",
           "[`../beings.md`](../beings.md). Each document mixes hand-written text with blocks generated",
           "from the TOML by `tools/gen_being_docs.py`; `tools/gen_being_docs.py --check` (run by",
           "`tools/ci.sh`) fails if a being has no document or a generated block is stale. The",
           "physiology sources are copied from the TOML as they are (some are still in Spanish).", ""]
    groups = {}
    for b in beings.values():
        groups.setdefault(group_of(b), []).append(b)
    order = [g for g, _ in GROUPS] + ["Others"]
    for g in order:
        if g not in groups:
            continue
        out += [f"## {g}", "", "| Being | Id | Size | Locomotion | Feeding |", "|---|---|---|---|---|"]
        for b in sorted(groups[g], key=lambda x: titles[x["id"]].replace("*", "").lower()):
            s = b.get("shape", {})
            size = f"{fmt(2 * s.get('semi_major_axis', 0))} × {fmt(2 * s.get('semi_minor_axis', 0))} µm"
            out.append(f"| [{titles[b['id']]}]({b['id']}.md) | `{b['id']}` | {size} | "
                       f"`{b.get('locomotion', '')}` | `{b.get('feeding', FEEDING_DEFAULT)}` |")
        out.append("")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    beings, biomes, plates = load_world()
    OUT.mkdir(parents=True, exist_ok=True)
    problems, titles = [], {}
    for bid, b in beings.items():
        path = OUT / f"{bid}.md"
        old = path.read_text(encoding="utf-8") if path.exists() else None
        if old is None and args.check:
            problems.append(f"{path.relative_to(ROOT)}: falta la ficha de '{bid}'")
            titles[bid] = b.get("name", bid)
            continue
        text = old if old is not None else skeleton(b)
        new = render(text, {"summary": summary_block(b), "physiology": physiology_block(b),
                            "presence": presence_block(b, biomes, plates)})
        m = re.search(r"^# (.+)$", new, re.M)
        titles[bid] = m.group(1).strip() if m else bid
        if args.check:
            if new != old:
                problems.append(f"{path.relative_to(ROOT)}: bloques generados viejos (correr tools/gen_being_docs.py)")
            if "TODO:" in new:
                problems.append(f"{path.relative_to(ROOT)}: quedan secciones sin escribir (TODO:)")
        elif new != old:
            path.write_text(new, encoding="utf-8")
    for path in OUT.glob("*.md"):
        if path.name != "README.md" and path.stem not in beings:
            problems.append(f"{path.relative_to(ROOT)}: ficha de un ser que no existe")
    readme = OUT / "README.md"
    text = index(beings, titles)
    if args.check:
        if not readme.exists() or readme.read_text(encoding="utf-8") != text:
            problems.append(f"{readme.relative_to(ROOT)}: índice viejo (correr tools/gen_being_docs.py)")
    else:
        readme.write_text(text, encoding="utf-8")
    for p in problems:
        print(p, file=sys.stderr)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
