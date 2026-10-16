#include "game/stain.hpp"

#include "game/progress.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <tuple>

const char* stainTargetName(StainTarget t) {
    switch (t) {
    case StainTarget::Cytoplasm: return "Citoplasma";
    case StainTarget::Nucleus: return "Núcleo (ADN)";
    case StainTarget::Starch: return "Almidón";
    case StainTarget::Glycogen: return "Glucógeno";
    case StainTarget::Paramylon: return "Paramilón";
    case StainTarget::Acidic: return "Compartimentos ácidos";
    case StainTarget::GramPositive: return "Pared grampositiva";
    case StainTarget::GramNegative: return "Pared gramnegativa";
    case StainTarget::Chlorophyll: return "Clorofila";
    case StainTarget::Nucleoid: return "Nucleoide bacteriano";
    case StainTarget::Bacteria: return "Bacteria entera";
    case StainTarget::Hemoglobin: return "Hemoglobina";
    case StainTarget::Count: break;
    }
    return "";
}

const char* stainResponseName(StainResponse r) {
    switch (r) {
    case StainResponse::Fixed: return "Fijo";
    case StainResponse::PhIndicator: return "Indicador de pH";
    case StainResponse::GramRetention: return "Retención (Gram)";
    }
    return "";
}

namespace {

float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }
Rgba mix(Rgba a, Rgba b, float t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
}

} // namespace

bool stainHasTarget(const StainSpec& spec, StainTarget target) {
    for (const StainRule& r : spec.rules)
        if (r.target == target)
            return true;
    return false;
}

StainResult evaluateStain(const StainSpec& spec, StainTarget target, float amount, float ph) {
    StainResult out;
    for (const StainRule& r : spec.rules) {
        if (r.target != target)
            continue;
        Rgba c = r.color;
        switch (r.response) {
        case StainResponse::Fixed: break;
        case StainResponse::PhIndicator:
            // Los indicadores viran en un intervalo de ~1 unidad de pH alrededor de su pKa.
            c = mix(r.color, r.color2, clamp01(0.5f + (ph - r.pivot) / (2.0f * std::max(0.05f, r.width))));
            break;
        case StainResponse::GramRetention: {
            // El decolorante (alcohol) arrastra el cristal violeta: la pared fina lo pierde en
            // segundos; la gruesa lo retiene mucho más. Sobre-decolorar vuelve rosa a la grampositiva.
            const float kept = std::exp(-std::max(0.0f, spec.time) / std::max(0.01f, r.tau));
            c = mix(r.color2, r.color, kept);
            break;
        }
        }
        const float a = clamp01(r.strength * spec.dose * amount);
        // "over": el colorante nuevo se apila sobre lo que ya había.
        const float total = out.alpha + a * (1.0f - out.alpha);
        if (total > 1e-6f) {
            const float wNew = a * (1.0f - out.alpha) / total;
            out.color = mix(out.color, c, out.alpha <= 0.0f ? 1.0f : wNew);
        }
        out.alpha = total;
    }
    return out;
}

Rgba stainOver(Rgba base, const StainResult& dye) {
    Rgba o = mix(base, dye.color, clamp01(dye.alpha));
    o.a = base.a;
    return o;
}

Rgba stainBackground(const StainSpec& spec, Rgba natural) {
    Rgba o = mix(natural, spec.background, clamp01(spec.backgroundStrength));
    o.a = 1.0f;
    return o;
}

// ---------------------------------------------------------------------------
// Tinciones de fábrica
// ---------------------------------------------------------------------------

namespace {

StainRule rule(StainTarget t, Rgba c, float strength) {
    StainRule r;
    r.target = t;
    r.color = c;
    r.strength = strength;
    return r;
}

std::vector<StainSpec> makeBuiltins() {
    std::vector<StainSpec> v;
    {
        StainSpec s;
        s.id = "none";
        s.name = "Sin tinción";
        s.note = "Campo claro sin colorantes.";
        s.vital = true;
        v.push_back(s);
    }
    {
        StainSpec s;
        s.id = "gram";
        s.name = "Gram";
        s.note = "Cristal violeta, yodo, alcohol y safranina. Las bacterias de pared gruesa de peptidoglicano (grampositivas) retienen el violeta; las de pared fina con membrana externa (gramnegativas) lo pierden con el alcohol y toman la safranina (rosa). Fija y mata a las células.";
        s.vital = false;
        s.time = 10.0f;
        const Rgba violet = rgb8(78, 36, 132), pink = rgb8(228, 92, 124);
        for (const auto& [t, strength, tau] : {std::tuple{StainTarget::GramPositive, 0.97f, 60.0f},
                                              std::tuple{StainTarget::GramNegative, 0.92f, 3.0f}}) {
            StainRule r;
            r.target = t;
            r.response = StainResponse::GramRetention;
            r.color = violet;
            r.color2 = pink;
            r.strength = strength;
            r.tau = tau;
            s.rules.push_back(r);
        }
        s.rules.push_back(rule(StainTarget::Cytoplasm, rgb8(232, 128, 158), 0.22f));
        s.rules.push_back(rule(StainTarget::Nucleus, rgb8(196, 66, 108), 0.38f));
        v.push_back(s);
    }
    {
        StainSpec s;
        s.id = "lugol";
        s.name = "Lugol (almidón)";
        s.note = "Yodo-yoduro de potasio. Se intercala en la amilosa del almidón y lo vuelve azul-negro; el glucógeno da marrón rojizo; el paramilón de Euglena y la crisolaminarina de las diatomeas no reaccionan. Tiñe de amarillo lo demás. Fija y mata a las células.";
        s.vital = false;
        s.dose = 1.0f;
        s.background = rgb8(232, 196, 96);
        s.backgroundStrength = 0.22f;
        s.rules.push_back(rule(StainTarget::Starch, rgb8(22, 18, 58), 1.0f));
        s.rules.push_back(rule(StainTarget::Glycogen, rgb8(142, 62, 30), 0.8f));
        s.rules.push_back(rule(StainTarget::Cytoplasm, rgb8(214, 166, 58), 0.38f));
        s.rules.push_back(rule(StainTarget::Nucleus, rgb8(170, 118, 40), 0.45f));
        s.rules.push_back(rule(StainTarget::Bacteria, rgb8(200, 156, 64), 0.5f));
        v.push_back(s);
    }
    {
        StainSpec s;
        s.id = "dapi";
        s.name = "DAPI (núcleos)";
        s.note = "Fluorocromo que se une al surco menor del ADN y emite azul bajo luz UV. Marca núcleos y nucleoides bacterianos sobre fondo oscuro; la clorofila emite roja por sí sola. Atraviesa mal las membranas vivas: en la práctica se fija la muestra.";
        s.mode = StainMode::Fluorescence;
        s.vital = false;
        s.background = rgb8(6, 8, 18);
        s.backgroundStrength = 1.0f;
        s.rules.push_back(rule(StainTarget::Nucleus, rgb8(92, 150, 255), 1.0f));
        s.rules.push_back(rule(StainTarget::Nucleoid, rgb8(110, 165, 255), 0.85f));
        s.rules.push_back(rule(StainTarget::Chlorophyll, rgb8(190, 34, 26), 0.32f));
        v.push_back(s);
    }
    {
        StainSpec s;
        s.id = "neutral_red";
        s.name = "Rojo neutro";
        s.note = "Colorante vital: la forma sin carga entra a la célula viva y se acumula, protonado, en los compartimentos ácidos (vacuolas digestivas, lisosomas). Es indicador: rojo en medio ácido y amarillo en alcalino (viraje a pH 6,8 a 8,0).";
        s.vital = true;
        StainRule r;
        r.target = StainTarget::Acidic;
        r.response = StainResponse::PhIndicator;
        r.color = rgb8(214, 36, 66);
        r.color2 = rgb8(240, 198, 58);
        r.strength = 0.92f;
        r.pivot = 7.4f;
        r.width = 0.6f;
        s.rules.push_back(r);
        s.rules.push_back(rule(StainTarget::Cytoplasm, rgb8(232, 150, 150), 0.1f));
        s.rules.push_back(rule(StainTarget::Nucleus, rgb8(214, 120, 130), 0.18f));
        s.rules.push_back(rule(StainTarget::Bacteria, rgb8(200, 112, 112), 0.3f));
        v.push_back(s);
    }
    {
        StainSpec s;
        s.id = "methylene_blue";
        s.name = "Azul de metileno";
        s.note = "Colorante básico (catiónico): se une a los ácidos nucleicos y tiñe de azul el núcleo y las bacterias. En dosis baja es vital: las células vivas lo reducen a la forma incolora.";
        s.vital = true;
        s.rules.push_back(rule(StainTarget::Nucleus, rgb8(26, 58, 172), 1.0f));
        s.rules.push_back(rule(StainTarget::Cytoplasm, rgb8(120, 150, 222), 0.26f));
        s.rules.push_back(rule(StainTarget::Bacteria, rgb8(46, 78, 182), 0.72f));
        v.push_back(s);
    }
    {
        StainSpec s;
        s.id = "congo_red";
        s.name = "Rojo Congo";
        s.note = "Indicador de pH: rojo en medio neutro y azul por debajo de pH 5. Las vacuolas digestivas de Paramecium se acidifican (azul) y después se neutralizan (rojo).";
        s.vital = true;
        StainRule r;
        r.target = StainTarget::Acidic;
        r.response = StainResponse::PhIndicator;
        r.color = rgb8(40, 62, 172);
        r.color2 = rgb8(204, 40, 60);
        r.strength = 0.95f;
        r.pivot = 5.0f;
        r.width = 1.0f;
        s.rules.push_back(r);
        v.push_back(s);
    }
    {
        StainSpec s;
        s.id = "giemsa";
        s.name = "Giemsa (sangre)";
        s.note = "Mezcla de azur B y azul de metileno (básicos) con eosina (ácida), sobre un frotis fijado con metanol. El ADN y los gránulos azurófilos toman violeta; el citoplasma rico en ARN, celeste; la hemoglobina, rosa anaranjado. Distingue los leucocitos: neutrófilo de núcleo lobulado, linfocito casi todo núcleo, monocito de citoplasma gris azulado; las plaquetas, violetas. Fija y mata.";
        s.vital = false;
        s.rules.push_back(rule(StainTarget::Nucleus, rgb8(86, 38, 128), 0.92f));
        s.rules.push_back(rule(StainTarget::Cytoplasm, rgb8(150, 172, 222), 0.35f));
        s.rules.push_back(rule(StainTarget::Hemoglobin, rgb8(226, 140, 150), 0.8f));
        s.rules.push_back(rule(StainTarget::Bacteria, rgb8(96, 50, 140), 0.8f));
        s.rules.push_back(rule(StainTarget::Nucleoid, rgb8(96, 50, 140), 0.6f));
        v.push_back(s);
    }
    {
        StainSpec s;
        s.id = "orceina_acetica";
        s.name = "Orceína acética (cromosomas)";
        s.note = "Orceína en ácido acético: fija y tiñe la cromatina de rojo vinoso intenso. Sobre una punta de raíz aplastada deja ver los cromosomas en cada fase de la mitosis; el citoplasma queda casi incoloro. Fija y mata.";
        s.vital = false;
        s.rules.push_back(rule(StainTarget::Nucleus, rgb8(132, 24, 66), 0.95f));
        s.rules.push_back(rule(StainTarget::Cytoplasm, rgb8(230, 190, 200), 0.08f));
        v.push_back(s);
    }
    return v;
}

} // namespace

const std::vector<StainSpec>& builtinStains() {
    static const std::vector<StainSpec> list = makeBuiltins();
    return list;
}

// ---------------------------------------------------------------------------
// Texto
// ---------------------------------------------------------------------------

namespace {

std::string rgbaText(const Rgba& c) {
    std::ostringstream o;
    o << c.r << ',' << c.g << ',' << c.b << ',' << c.a;
    return o.str();
}

bool parseRgba(const std::string& s, Rgba& c) {
    std::istringstream in(s);
    char comma;
    return static_cast<bool>(in >> c.r >> comma >> c.g >> comma >> c.b >> comma >> c.a);
}

} // namespace

std::string serializeStain(const StainSpec& s) {
    std::ostringstream out;
    out << "stain_version=1\n"
        << "id=" << s.id << '\n'
        << "name=" << s.name << '\n'
        << "note=" << s.note << '\n'
        << "mode=" << static_cast<int>(s.mode) << '\n'
        << "vital=" << s.vital << '\n'
        << "dose=" << s.dose << '\n'
        << "time=" << s.time << '\n'
        << "background=" << rgbaText(s.background) << '\n'
        << "background_strength=" << s.backgroundStrength << '\n';
    for (const StainRule& r : s.rules)
        out << "rule=" << static_cast<int>(r.target) << ',' << static_cast<int>(r.response) << ',' << rgbaText(r.color)
            << ',' << rgbaText(r.color2) << ',' << r.strength << ',' << r.pivot << ',' << r.width << ',' << r.tau << '\n';
    return out.str();
}

bool parseStain(const std::string& text, StainSpec& spec) {
    StainSpec s;
    bool versioned = false;
    std::istringstream in(text);
    std::string line;
    auto num = [](const std::string& v, float fallback) {
        try {
            return std::stof(v);
        } catch (...) {
            return fallback;
        }
    };
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "stain_version") versioned = true;
        else if (k == "id") s.id = v;
        else if (k == "name") s.name = v;
        else if (k == "note") s.note = v;
        else if (k == "mode") s.mode = num(v, 0) > 0.5f ? StainMode::Fluorescence : StainMode::BrightField;
        else if (k == "vital") s.vital = num(v, 1) != 0.0f;
        else if (k == "dose") s.dose = std::clamp(num(v, 1), 0.0f, 4.0f);
        else if (k == "time") s.time = std::clamp(num(v, 10), 0.0f, 600.0f);
        else if (k == "background") parseRgba(v, s.background);
        else if (k == "background_strength") s.backgroundStrength = clamp01(num(v, 0));
        else if (k == "rule") {
            StainRule r;
            std::istringstream rs(v);
            int target = 0, response = 0;
            char c;
            Rgba a, b;
            float st = 1, pivot = 7, width = 0.6f, tau = 10;
            if (!(rs >> target >> c >> response >> c >> a.r >> c >> a.g >> c >> a.b >> c >> a.a >> c >> b.r >> c >> b.g >> c >>
                  b.b >> c >> b.a >> c >> st >> c >> pivot >> c >> width >> c >> tau))
                continue;
            if (target < 0 || target >= static_cast<int>(StainTarget::Count) || response < 0 || response > 2)
                continue;
            r.target = static_cast<StainTarget>(target);
            r.response = static_cast<StainResponse>(response);
            r.color = a;
            r.color2 = b;
            r.strength = clamp01(st);
            r.pivot = pivot;
            r.width = std::max(0.05f, width);
            r.tau = std::max(0.01f, tau);
            s.rules.push_back(r);
        }
    }
    if (!versioned)
        return false;
    if (s.id.empty())
        s.id = "tincion";
    spec = s;
    return true;
}

std::string defaultStainsDir() {
    namespace fs = std::filesystem;
    return (fs::path(defaultProgressPath()).parent_path() / "stains").string();
}

bool saveStain(const StainSpec& spec, const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::ofstream out(path);
    if (!out)
        return false;
    out << serializeStain(spec);
    return static_cast<bool>(out);
}

bool loadStain(const std::string& path, StainSpec& spec) {
    std::ifstream in(path);
    if (!in)
        return false;
    std::stringstream ss;
    ss << in.rdbuf();
    return parseStain(ss.str(), spec);
}

std::vector<StainSpec> loadUserStains(const std::string& dir) {
    namespace fs = std::filesystem;
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec))
        if (e.is_regular_file() && e.path().extension() == ".stain")
            files.push_back(e.path());
    std::sort(files.begin(), files.end());
    std::vector<StainSpec> out;
    for (const auto& f : files) {
        StainSpec s;
        if (loadStain(f.string(), s))
            out.push_back(std::move(s));
    }
    return out;
}
