#!/usr/bin/env python3
"""Migra un paquete de contenido de Open Microcosm al formato de carpetas.

El formato viejo es un archivo por tipo (seres.toml, biomas.toml...) o un .biopack con todo
adentro. El nuevo es `pack.toml` y una carpeta por tipo con un archivo por definición
(beings/paramecium.toml), con la fisiología medida de cada ser en su propio archivo.

    tools/migrate_content.py <carpeta vieja> <carpeta nueva>
    tools/migrate_content.py <viejo.biopack> <nuevo.biopack>

Los comentarios se conservan: el que está justo antes de una definición viaja con ella; el
encabezado de cada archivo viejo (que documenta las claves) queda en `<carpeta nueva>/_headers.txt`
para revisarlo a mano.
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import content_names as N  # noqa: E402

# Archivo viejo → (tabla de arreglo, carpeta nueva).
FILES = [
    ("planes.toml", "plan", "body_plans"),
    ("minerales.toml", "mineral", "minerals"),
    ("seres.toml", "ser", "beings"),
    ("biomas.toml", "bioma", "biomes"),
    ("medios.toml", "medio", "media"),
    ("recipientes.toml", "recipiente", "vessels"),
    ("placas.toml", "placa", "plates"),
    ("lecciones.toml", "leccion", "lessons"),
    ("reactivos.toml", "reactivo", "reagents"),
    ("glosario.toml", "termino", "glossary"),
]
ARRAY_FOLDER = {array: folder for _, array, folder in FILES}

ID_LINE = re.compile(r'^id\s*=\s*"([a-z0-9_]+)"')


def is_comment_or_blank(line):
    s = line.strip()
    return not s or s.startswith("#")


def split_blocks(lines, header_re):
    """Parte un archivo en (encabezado, [(línea de cabecera, líneas del bloque)]).

    Los comentarios que preceden a una cabecera (el último párrafo antes de ella, o todo lo que
    hay después del último dato del bloque anterior) van con el bloque que sigue."""
    starts = [i for i, l in enumerate(lines) if header_re.match(l)]
    if not starts:
        return "".join(lines), []
    blocks = []
    cut = []  # índice donde empieza cada bloque contando sus comentarios previos
    for n, s in enumerate(starts):
        lo = starts[n - 1] + 1 if n > 0 else 0
        j = s
        while j > lo and is_comment_or_blank(lines[j - 1]):
            j -= 1
        if n == 0:
            # Encabezado del archivo: todo menos los comentarios propios de la primera definición
            # (las líneas "# Texto" justo antes de la cabecera; las del encabezado que documentan
            # claves están sangradas, "#   clave: ...", o separadas por un "#" solo).
            k = s
            while k > 0 and lines[k - 1].strip() == "":
                k -= 1
            top = k
            while k > 0 and re.match(r"^# \S", lines[k - 1]):
                k -= 1
            # Un encabezado de un solo párrafo pegado a la cabecera es todo encabezado.
            j = top if k == 0 else k
        cut.append(j)
    header = "".join(lines[: cut[0]])
    for n, s in enumerate(starts):
        end = cut[n + 1] if n + 1 < len(starts) else len(lines)
        blocks.append((s, lines[cut[n] : end]))
    return header, blocks


def clean(block_lines):
    """Sin líneas en blanco al principio y al final; termina con un salto de línea."""
    text = "".join(block_lines).strip("\n")
    return text + "\n"


def block_id(block_lines, where):
    for l in block_lines:
        m = ID_LINE.match(l)
        if m:
            return m.group(1)
    sys.exit(f"{where}: un bloque sin id")


def read_sources(src):
    """{nombre de archivo viejo: líneas}. Un .biopack se reparte por tabla."""
    if os.path.isdir(src):
        out = {}
        for name in os.listdir(src):
            if name.endswith(".toml"):
                with open(os.path.join(src, name), encoding="utf-8") as f:
                    out[name] = f.readlines()
        return out
    with open(src, encoding="utf-8") as f:
        return {"<biopack>": f.readlines()}


def migrate(src, dst):
    sources = read_sources(src)
    os.makedirs(dst, exist_ok=True)
    headers = []
    order = {}
    files = {}  # ruta relativa → texto

    def add(folder, ident, text, where):
        path = f"{folder}/{ident}.toml"
        if path in files:
            sys.exit(f"{where}: '{ident}' repetido en {folder}")
        files[path] = text
        order.setdefault(folder, []).append(ident)

    if "<biopack>" in sources:
        sys.exit("un .biopack se migra a otro .biopack: migrate_content.py viejo.biopack nuevo.biopack")

    for name, array, folder in FILES:
        if name not in sources:
            continue
        header_re = re.compile(r"^\[\[" + array + r"\]\]\s*$")
        header, blocks = split_blocks(sources[name], header_re)
        if header.strip():
            headers.append(f"===== {name}\n{header}")
        for _, lines in blocks:
            add(folder, block_id(lines, name), clean(lines), name)

    # Fisiología: cada tabla [ser.parametro] va al archivo de su ser, como [fisiologia.ser.parametro].
    if "fisiologia.toml" in sources:
        header_re = re.compile(r"^\[([a-z0-9_]+)\.([a-z0-9_]+)\]\s*$")
        header, blocks = split_blocks(sources["fisiologia.toml"], header_re)
        headers.append(f"===== fisiologia.toml\n{header}")
        per_being = {}
        for start, lines in blocks:
            text = "".join(lines)
            m = None
            out = []
            for l in lines:
                hm = header_re.match(l)
                if hm and m is None:
                    m = hm
                    out.append(f"[fisiologia.{hm.group(1)}.{hm.group(2)}]\n")
                else:
                    out.append(l)
            per_being.setdefault(m.group(1), []).append(clean(out))
            del text
        for being, tables in per_being.items():
            path = f"beings/{being}.toml"
            if path not in files:
                sys.exit(f"fisiologia.toml: '{being}' no es un ser del paquete")
            files[path] = files[path] + "\n" + "\n".join(tables)

    manifest = "".join(sources.get("paquete.toml", []))
    manifest = manifest.rstrip("\n") + "\n\n# Orden de carga de cada carpeta (los índices de los seres, biomas y minerales\n" \
        "# de fábrica son el orden de carga). Lo que no está listado se carga después, por nombre.\n[paquete.orden]\n"
    for _, _, folder in FILES:
        if folder in order:
            ids = ", ".join(f'"{i}"' for i in order[folder])
            manifest += f"{folder} = [{ids}]\n"
    files["pack.toml"] = manifest

    for path, text in files.items():
        full = os.path.join(dst, path)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "w", encoding="utf-8") as f:
            f.write(text)
    with open(os.path.join(dst, "_headers.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(headers))
    print(f"{len(files)} archivos en {dst}")




# --- Renombrar al inglés ------------------------------------------------------------------------
#
# Un lexer de TOML mínimo (lo que usan los paquetes: tablas, arreglos de tablas, tablas y arreglos
# en línea, textos básicos, literales y multilínea, comentarios) que reescribe cada clave y cada
# valor con nombre según su contexto, sin tocar los comentarios ni el formato.

TOKEN = re.compile(
    r'(?P<ml>"""(?:.|\n)*?""")|(?P<str>"(?:\\.|[^"\\\n])*")|(?P<lit>\'[^\'\n]*\')'
    r"|(?P<comment>#[^\n]*)|(?P<nl>\n)|(?P<ws>[ \t\r]+)|(?P<punct>\[\[|\]\]|[\[\]{}=,.])"
    r"|(?P<bare>[A-Za-z0-9_+\-:]+)"
)

unknown = set()  # valores con nombre que no están en el diccionario (para revisar)


def tokens(text):
    pos = 0
    out = []
    while pos < len(text):
        m = TOKEN.match(text, pos)
        if not m:
            sys.exit(f"no se entiende el TOML cerca de: {text[pos:pos + 40]!r}")
        out.append([m.lastgroup, m.group()])
        pos = m.end()
    return out


def unquote(tok):
    kind, text = tok
    if kind == "str":
        return text[1:-1]
    if kind == "lit":
        return text[1:-1]
    return text


def requote(tok, value):
    kind, text = tok
    if kind == "lit":
        tok[1] = "'" + value + "'"
    else:
        tok[1] = '"' + value + '"'


def being_ref(v):
    return N.IDS["beings"].get(v, N.VALUES.get(v, v) if v in N.VALUES else v)


def event_line(v):
    """Una línea de intervención de la placa: el último campo puede ser una clave o un id."""
    parts = v.split(" ")
    last = parts[-1]
    pk = N.param_key(last)
    if pk:
        parts[-1] = pk
    elif last in N.IDS["beings"]:
        parts[-1] = N.IDS["beings"][last]
    return " ".join(parts)


def culture_event(v):
    f = v.split(" ")
    if len(f) >= 2:
        op = f[1]
        f[1] = N.CULTURE_OPS.get(op, op)
        if len(f) >= 4:
            key = f[3]
            if op in ("inocular", "inocular_densidad"):
                f[3] = N.IDS["beings"].get(key, key)
            elif op in ("concentracion", "medio_que_entra"):
                f[3] = N.SOLUTES.get(key, key)
            elif op == "reactivo":
                f[3] = N.IDS["reagents"].get(key, key)
    return " ".join(f)


def program_entry(v):
    f = v.split(" ")
    target = f[0]
    if ":" in target:
        pre, arg = target.split(":", 1)
        if pre in ("agregar", "soluto"):
            arg = N.SOLUTES.get(arg, arg)
        elif pre == "inocular":
            arg = N.IDS["beings"].get(arg, arg)
        target = N.PROGRAM_PREFIXES.get(pre, pre) + ":" + arg
    else:
        target = N.PROGRAM_TARGETS.get(target, target)
    f[0] = target
    if len(f) > 1:
        f[1] = N.PROGRAM_SHAPES.get(f[1], f[1])
    return " ".join(f)


def culture_scene(text):
    """El texto de una escena de cultivo (.cultivo)."""
    out = []
    for line in text.split("\n"):
        if "=" not in line or line.startswith("#"):
            out.append(line)
            continue
        k, v = line.split("=", 1)
        nk = N.CULTURE_KEYS.get(k, k)
        if k == "recipiente":
            v = N.IDS["vessels"].get(v, v)
        elif k == "medio":
            v = N.IDS["media"].get(v, v)
        elif k == "programa":
            v = program_entry(v)
        elif k == "evento":
            v = culture_event(v)
        out.append(nk + "=" + v)
    return "\n".join(out)


def plate_text(text):
    """El texto de una placa (.plate): las claves ya están en inglés; los ids y los programas, no."""
    out = []
    for line in text.split("\n"):
        if "=" not in line:
            out.append(line)
            continue
        k, v = line.split("=", 1)
        if k == "biome":
            v = N.IDS["biomes"].get(v, v)
        elif k == "prepared":
            v = N.IDS["plates"].get(v, v)
        elif k in ("seeding", "sampled"):
            f = v.split(" ")
            f[0] = N.IDS["beings"].get(f[0], f[0])
            v = " ".join(f)
        elif k == "programa":
            k, v = "program", program_entry(v)
        elif k == "event":
            v = event_line(v)
        elif k.startswith("w."):
            k = "w." + N.IDS["beings"].get(k[2:], k[2:])
        elif k.startswith("sediment."):
            k = "sediment." + N.IDS["minerals"].get(k[9:], k[9:])
        out.append(k + "=" + v)
    return "\n".join(out)


def map_value(key, path, folder, v):
    """El valor (texto) de la clave `key` (en español) dentro de la tabla `path`."""
    if key == "fuente" and v in ("simulada", "declarada"):
        return N.VALUES[v]
    if key in N.TEXT_KEYS:
        return v
    if path[:2] == ["paquete", "orden"]:
        return N.IDS.get(key, {}).get(v, v)
    if key == "id":
        return N.IDS.get(folder, {}).get(v, v)
    if key in N.ID_KEYS:
        return N.IDS[N.ID_KEYS[key]].get(v, v)
    if key in N.BEING_REF_KEYS or key == "etiquetas":
        return being_ref(v)
    if key == "clave":
        return ",".join(N.param_key(p) or N.IDS["beings"].get(p, N.SOLUTES.get(p, p)) for p in v.split(","))
    if key == "controles":
        return N.param_key(v) or v
    if key in ("preparar", "solucion", "evento"):
        return event_line(v)
    if key == "cultivo":
        return culture_scene(v)
    if key == "unidad":
        return N.VALUES.get(v, v)  # las unidades en inglés ("1/h", "um/h") no cambian
    if key in N.VALUES_BY_KEY and v in N.VALUES_BY_KEY[key]:
        return N.VALUES_BY_KEY[key][v]
    if v in N.VALUES:
        return N.VALUES[v]
    if re.fullmatch(r"[a-z_0-9/]+", v):
        unknown.add((key, v))
    return v


def map_key(key, path, inline_owner):
    """El nombre de una clave dentro de la tabla `path` (claves del dueño de una tabla en línea)."""
    owner = inline_owner or (path[-1] if path else None)
    if owner == "pesos":
        return N.IDS["beings"].get(key, key)
    if owner == "sedimento" and key not in ("granos", "grano_radio"):
        return N.IDS["minerals"].get(key, key)
    if path[:2] == ["paquete", "orden"]:
        return key  # carpetas
    if key in N.KEYS:
        return N.KEYS[key]
    if key not in N.SAME_KEYS:
        unknown.add(("<clave>", key))
    return key


def rename_toml(text, folder):
    toks = tokens(text)
    out = toks
    i = 0
    n = len(toks)
    path = []  # tabla actual (nombres en español)
    stack = []  # contextos en línea: ("table", dueño) o ("array", clave)

    def skip_ws(j):
        while j < n and toks[j][0] in ("ws",):
            j += 1
        return j

    at_line_start = True
    while i < n:
        kind, text_ = toks[i]
        if kind in ("ws", "comment"):
            i += 1
            continue
        if kind == "nl":
            if not stack:
                at_line_start = True
            i += 1
            continue
        if not stack and at_line_start and kind == "punct" and text_ in ("[", "[["):
            # Cabecera de tabla: cada segmento.
            close = "]]" if text_ == "[[" else "]"
            j = i + 1
            segs = []
            while j < n and not (toks[j][0] == "punct" and toks[j][1] == close):
                if toks[j][0] in ("bare", "str", "lit"):
                    segs.append(j)
                j += 1
            names = [unquote(toks[s]) for s in segs]
            for k, s in enumerate(segs):
                name = names[k]
                if k == 1 and names[0] == "fisiologia":
                    new = N.IDS["beings"].get(name, name)
                elif k == 2 and names[0] == "fisiologia":
                    new = N.KEYS.get(name, name)
                elif k == 1 and names[0] == "paquete" and name == "orden":
                    new = "order"
                else:
                    new = map_key(name, names[:k], None) if k > 0 else N.KEYS.get(name, name)
                toks[s][1] = new if toks[s][0] == "bare" else '"' + new + '"'
            path = names
            i = j + 1
            at_line_start = False
            continue
        in_key_pos = (not stack and at_line_start) or (stack and stack[-1][0] == "table" and stack[-1][2])
        if in_key_pos and kind in ("bare", "str", "lit"):
            # Clave (con puntos) hasta el '='.
            j = i
            parts = []
            while j < n and not (toks[j][0] == "punct" and toks[j][1] == "="):
                if toks[j][0] in ("bare", "str", "lit"):
                    parts.append(j)
                j += 1
            owner = stack[-1][1] if stack else None
            key_es = unquote(toks[parts[-1]])
            for p_ in parts:
                name = unquote(toks[p_])
                new = map_key(name, path, owner)
                toks[p_][1] = new if toks[p_][0] == "bare" else '"' + new + '"'
            current_key = key_es
            at_line_start = False
            if stack:
                stack[-1][2] = False
                stack[-1][3] = current_key
            else:
                top_key[0] = current_key
            i = j + 1
            continue
        governing = stack[-1][3] if stack else top_key[0]
        if kind == "punct":
            if text_ == "{":
                stack.append(["table", governing, True, None])
            elif text_ == "[":
                stack.append(["array", governing, False, governing])
            elif text_ in ("}", "]"):
                stack.pop()
            elif text_ == "," and stack and stack[-1][0] == "table":
                stack[-1][2] = True
            i += 1
            continue
        if kind in ("str", "lit"):
            requote(toks[i], map_value(governing, path, folder, unquote(toks[i])))
        elif kind == "ml":
            body = text_[3:-3]
            new = map_value(governing, path, folder, body)
            toks[i][1] = '"""' + new + '"""'
        i += 1
    return "".join(t[1] for t in out)


top_key = [None]
# Tipos cuyos ids guardan las placas, las escenas y los experimentos (los demás solo los nombra
# el contenido, que se migra entero).
LEGACY_FOLDERS = {"beings", "biomes", "minerals", "media", "vessels", "plates", "lessons", "reagents"}


def rename_pack(dst):
    """Renombra al inglés un paquete en carpetas (en el lugar)."""
    for folder in os.listdir(dst):
        full = os.path.join(dst, folder)
        if not os.path.isdir(full):
            continue
        for name in sorted(os.listdir(full)):
            if not name.endswith(".toml"):
                continue
            path = os.path.join(full, name)
            with open(path, encoding="utf-8") as f:
                text = f.read()
            new = rename_toml(text, folder)
            ident = name[:-5]
            new_id = N.IDS.get(folder, {}).get(ident, ident)
            if new_id != ident and folder in LEGACY_FOLDERS:
                # El id viejo sigue valiendo: lo nombran las placas y escenas guardadas.
                new = re.sub(r'^(id = "' + new_id + r'"\n)', r'\g<1>legacy_ids = ["' + ident + r'"]\n', new,
                             count=1, flags=re.M)
            os.remove(path)
            with open(os.path.join(full, new_id + ".toml"), "w", encoding="utf-8") as f:
                f.write(new)
    manifest = os.path.join(dst, "pack.toml")
    with open(manifest, encoding="utf-8") as f:
        text = f.read()
    with open(manifest, "w", encoding="utf-8") as f:
        f.write(rename_toml(text, None))
    for k, v in sorted(unknown):
        print(f"sin traducir: {k} = {v}")


def migrate_biopack(src, dst):
    """Un .biopack viejo → uno nuevo (las mismas definiciones, con los nombres en inglés)."""
    with open(src, encoding="utf-8") as f:
        lines = f.readlines()
    # Bloques por cabecera de primer nivel; las subtablas ([ser.forma]) van con su definición.
    top = re.compile(r"^\[\[([a-z_]+)\]\]\s*$|^\[(paquete|fisiologia)[\].]")
    blocks, kind = [[None, []]], None
    for line in lines:
        m = top.match(line)
        if m:
            kind = m.group(1) or m.group(2)
            if not (kind == "fisiologia" and blocks[-1][0] == "fisiologia"):
                blocks.append([kind, []])
        blocks[-1][1].append(line)
    out = []
    for kind, block in blocks:
        text = "".join(block)
        if kind == "escena":
            top_key[0] = None
            new = rename_toml(re.sub(r'^placa = ', "plate_text = ", text, flags=re.M), None)
            # La escena es el texto de una placa: sus ids también cambian.
            new = re.sub(r'(?s)^plate_text = (""".*?"""|"(?:\\.|[^"\\])*")',
                         lambda m: "plate = " + plate_literal(m.group(1)), new, flags=re.M)
            out.append(new)
            unknown.discard(("<clave>", "plate_text"))
        else:
            top_key[0] = None
            out.append(rename_toml(text, ARRAY_FOLDER.get(kind)))
    with open(dst, "w", encoding="utf-8") as f:
        f.write("".join(out))
    for k, v in sorted(unknown):
        print(f"sin traducir: {k} = {v}")


def plate_literal(lit):
    """El texto de una placa dentro de un literal de TOML (básico o multilínea)."""
    if lit.startswith('"""'):
        return '"""' + plate_text(lit[3:-3]) + '"""'
    body = lit[1:-1].encode("utf-8").decode("unicode_escape").encode("latin-1").decode("utf-8")
    new = plate_text(body)
    return '"' + new.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if len(args) != 2:
        sys.exit(__doc__)
    if args[0].endswith(".biopack"):
        migrate_biopack(args[0], args[1])
        sys.exit(0)
    migrate(args[0], args[1])
    if "--split-only" not in sys.argv:
        rename_pack(args[1])
