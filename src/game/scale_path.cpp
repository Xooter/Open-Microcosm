#include "game/scale_path.hpp"

#include "bio/body_plan.hpp"
#include "game/world.hpp"
#include "sim/polygon.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

// Lado menor de la vista [µm] a partir del cual se pasa a la escala siguiente.
constexpr float kOrganismsBelow = 1500.0f;     // ya no entra la placa: se ven organismos
constexpr float kCellsBelow = 150.0f;          // células y bacterias sueltas
constexpr float kStructuresBelow = 15.0f;      // organelos e interior bacteriano
constexpr float kSubcellularBelow = 1.5f;      // decenas de nm: ribosomas, membranas, virus

std::string structureUnder(const World& world, const RingBody& body, const CellAnatomy* anatomy, const BodyPlan& plan,
                           Vec2 p) {
    const BodyParticles& bp = world.bodies();
    auto inside = [&](std::uint32_t slot, float scale = 1.0f) {
        if (slot == ~0u)
            return false;
        const float dx = p.x - bp.x[slot], dy = p.y - bp.y[slot], r = bp.radius[slot] * scale;
        return dx * dx + dy * dy <= r * r;
    };
    if (anatomy && inside(anatomy->nucleus))
        return "Nucleo";
    if (anatomy && inside(anatomy->contractileVacuole))
        return "Vacuola contractil";
    for (std::size_t k = body.organelleCount; k < body.interior.size(); ++k) {
        const std::uint32_t s = body.interior[k];
        if (!inside(s, 1.3f))
            continue;
        switch (static_cast<OrganelleTag>(bp.tag[s])) {
        case OrganelleTag::Mitochondrion: // el orgánulo energético: hidrogenosoma en los parabasálidos
            return plan.has(StructureKind::Mitochondrion) || !plan.has(StructureKind::Hydrogenosome) ? "Mitocondria"
                                                                                                  : "Hidrogenosoma";
        case OrganelleTag::Chloroplast: return "Cloroplasto";
        case OrganelleTag::Lysosome: return "Lisosoma";
        case OrganelleTag::Granule: return "Granulo de reserva";
        }
    }
    return "Citoplasma";
}

std::string capitalized(const char* name) {
    std::string s = name;
    if (!s.empty() && s[0] >= 'a' && s[0] <= 'z')
        s[0] = static_cast<char>(s[0] - 'a' + 'A');
    return s;
}

// Lo que ocupa el punto dentro de una bacteria (en sus ejes: x a lo largo, y a lo ancho). Con
// `layers`, la capa de la envoltura según los espesores reales del plan corporal.
std::string bacteriumPart(const World& world, std::size_t i, Vec2 p, bool layers = false) {
    const Bacteria& b = world.bacteria();
    const float c = std::cos(b.heading[i]), s = std::sin(b.heading[i]);
    const float dx = minImage(p.x - b.x[i], world.width()), dy = minImage(p.y - b.y[i], world.height());
    const float lx = c * dx + s * dy, ly = -s * dx + c * dy;
    // Mismo largo que el dibujo: crece con la masa a ancho constante (2/3 a 4/3 del largo medio).
    const float r = b.params.radius, half = 0.5f * b.params.length * (1.0f + std::clamp(b.mass[i] - 1.0f, 0.0f, 1.0f)) / 1.5f;
    const float qx = std::max(0.0f, std::abs(lx) - (half - r));
    const float edge = r - std::sqrt(qx * qx + ly * ly); // distancia al contorno hacia adentro
    if (edge < 0.0f)
        return "";
    const BodyPlan& plan = bacteriumPlan(b.strain[i]);
    float depth = 0.0f;
    for (const StructureSpec& s : plan.structures) {
        if (s.arrangement != Arrangement::Envelope)
            continue;
        if (layers && edge < depth + s.width)
            return capitalized(s.name);
        depth += s.width;
        if (s.kind == StructureKind::OuterMembrane) {
            if (layers && edge < depth + kPeriplasmWidth)
                return "Periplasma";
            depth += kPeriplasmWidth;
        }
    }
    if (edge < std::max(depth, 0.04f))
        return "Envoltura celular";
    const StructureSpec* nuc = plan.find(StructureKind::Nucleoid);
    if (nuc && b.phage[i] < 0.0f && std::abs(lx) < 0.5f * nuc->length && std::abs(ly) < 0.5f * nuc->width)
        return "Nucleoide";
    return "Citoplasma";
}

} // namespace

BioScale bioScaleFor(float viewMicrons) {
    if (viewMicrons >= kOrganismsBelow)
        return BioScale::Environment;
    if (viewMicrons >= kCellsBelow)
        return BioScale::Organisms;
    if (viewMicrons >= kStructuresBelow)
        return BioScale::Cells;
    if (viewMicrons >= kSubcellularBelow)
        return BioScale::CellStructures;
    return BioScale::Subcellular;
}

const char* bioScaleName(BioScale scale) {
    switch (scale) {
    case BioScale::Environment: return "Ambiente y poblaciones";
    case BioScale::Organisms: return "Organismos";
    case BioScale::Cells: return "Celulas";
    case BioScale::CellStructures: return "Estructuras celulares";
    case BioScale::Subcellular: return "Subcelular";
    }
    return "";
}

ScalePath scalePath(const World& world, Vec2 center, float viewMicrons, const Picked& focus) {
    ScalePath out;
    out.scale = bioScaleFor(viewMicrons);
    out.crumbs.emplace_back(world.config().cellCulture ? "Cultivo" : world.dish().active() ? "Placa" : "Gota");
    if (out.scale == BioScale::Environment)
        return out;
    const float half = 0.5f * viewMicrons;
    auto onScreen = [&](Vec2 q) {
        return std::abs(minImage(q.x - center.x, world.width())) < 2.0f * half &&
               std::abs(minImage(q.y - center.y, world.height())) < 2.0f * half;
    };
    // El punto que se mira: el centro, salvo que el ser elegido esté a la vista.
    Picked target = focus;
    Vec2 at = center;
    if (target) {
        Vec2 where;
        if (locatePicked(world, target, where) && onScreen(where))
            at = target.kind == Picked::Kind::Cell ? center : where;
        else
            target = {};
    }
    if (!target)
        target = world.pick(center, 0.0f);
    const entt::registry& reg = world.registry();
    if (target.kind == Picked::Kind::Cell && reg.valid(target.entity) && reg.all_of<RingBody>(target.entity)) {
        const Species* sp = reg.try_get<Species>(target.entity);
        const RingBody& body = reg.get<RingBody>(target.entity);
        out.crumbs.emplace_back(sp ? speciesInfo(sp->id).name : "Celula");
        // Protistas y levaduras son una sola célula: organismo y célula coinciden.
        const float size = std::max(body.aabbMax.x - body.aabbMin.x, body.aabbMax.y - body.aabbMin.y);
        if (viewMicrons < size)
            out.crumbs.emplace_back("Celula");
        const Vec2 q = body.centroid + Vec2{minImage(center.x - body.centroid.x, world.width()),
                                            minImage(center.y - body.centroid.y, world.height())};
        if (viewMicrons < 0.5f * size && polygon::contains(body.poly.data(), static_cast<int>(body.poly.size()), q))
            out.crumbs.push_back(structureUnder(world, body, reg.try_get<CellAnatomy>(target.entity),
                                                speciesPlan(sp ? sp->id : SpeciesId::Amoeba), q));
        return out;
    }
    if (target.kind == Picked::Kind::Bacterium && target.index < world.bacteria().size()) {
        const Bacteria& b = world.bacteria();
        out.crumbs.emplace_back("Bacterias");
        out.crumbs.emplace_back(bacteriumPlan(b.strain[target.index]).name);
        if (viewMicrons < 4.0f * b.params.length) {
            const std::string part = bacteriumPart(world, target.index, at);
            if (!part.empty())
                out.crumbs.push_back(part);
            if (b.phage[target.index] >= 0.0f)
                out.crumbs.emplace_back(std::string("infectada por ") + virusPlan(kSimulatedPhage).name);
        }
        return out;
    }
    if (target.kind == Picked::Kind::Bdellovibrio) {
        out.crumbs.emplace_back("Bacterias");
        out.crumbs.emplace_back(bdellovibrioPlan().name);
        return out;
    }
    if (target)
        out.crumbs.push_back(buildInspection(world, target).title);
    else
        out.crumbs.emplace_back("Agua");
    return out;
}

std::string structureAt(const World& world, const Picked& hit, Vec2 at, float viewMicrons, bool model) {
    if (bioScaleFor(viewMicrons) < BioScale::Cells)
        return "";
    const entt::registry& reg = world.registry();
    if (hit.kind == Picked::Kind::Cell && reg.valid(hit.entity) && reg.all_of<RingBody>(hit.entity)) {
        const RingBody& body = reg.get<RingBody>(hit.entity);
        const float size = std::max(body.aabbMax.x - body.aabbMin.x, body.aabbMax.y - body.aabbMin.y);
        if (viewMicrons >= size)
            return "";
        const Vec2 q = body.centroid + Vec2{minImage(at.x - body.centroid.x, world.width()),
                                            minImage(at.y - body.centroid.y, world.height())};
        if (!polygon::contains(body.poly.data(), static_cast<int>(body.poly.size()), q))
            return "";
        const Species* sp = reg.try_get<Species>(hit.entity);
        const BodyPlan& plan = speciesPlan(sp ? sp->id : SpeciesId::Amoeba);
        std::string part = structureUnder(world, body, reg.try_get<CellAnatomy>(hit.entity), plan, q);
        // Con luz, mitocondrias, lisosomas e hidrogenosomas no se distinguen del citoplasma.
        if (!model && (part == "Mitocondria" || part == "Lisosoma" || part == "Hidrogenosoma"))
            part = "Citoplasma";
        return part;
    }
    if (hit.kind == Picked::Kind::Bacterium && hit.index < world.bacteria().size())
        return bacteriumPart(world, hit.index, at, model);
    return "";
}

float niceLength(float target) {
    const float p = std::pow(10.0f, std::floor(std::log10(std::max(target, 1e-4f))));
    for (float m : {1.0f, 2.0f, 5.0f, 10.0f})
        if (m * p >= target)
            return m * p;
    return 10.0f * p;
}

std::string formatLength(float microns) {
    char text[32];
    if (microns >= 1000.0f)
        std::snprintf(text, sizeof text, "%g mm", static_cast<double>(microns / 1000.0f));
    else if (microns >= 1.0f)
        std::snprintf(text, sizeof text, "%g µm", static_cast<double>(microns));
    else
        std::snprintf(text, sizeof text, "%g nm", static_cast<double>(std::round(microns * 1000.0f)));
    return text;
}
