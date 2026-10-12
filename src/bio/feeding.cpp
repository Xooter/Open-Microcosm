#include "bio/feeding.hpp"

#include "core/units.hpp"
#include "sim/soft_body.hpp"

#include <algorithm>
#include <cmath>

namespace {

float vacuoleRadius(float contentArea) {
    // El fagosoma lleva agua además de la presa: ~30% más de área.
    return std::max(1.5f, std::sqrt(1.3f * contentArea / units::kPi));
}

std::size_t interiorIndex(const RingBody& body, std::uint32_t slot) {
    return static_cast<std::size_t>(std::find(body.interior.begin(), body.interior.end(), slot) -
                                    body.interior.begin());
}

} // namespace

float cellMassPmol(float area) {
    const float r = std::sqrt(std::abs(area) / units::kPi);
    return units::kDryMassFraction * 4.0f / 3.0f * units::kPi * r * r * r / 180.0f;
}

float cellAreaForMass(float pmol) {
    const float r = std::cbrt(std::max(0.0f, pmol) * 180.0f * 3.0f /
                              (4.0f * units::kPi * units::kDryMassFraction));
    return units::kPi * r * r;
}

float vacuoleContentArea(const Digestion& dig) {
    float a = 0.0f;
    for (const FoodVacuole& v : dig.vacuoles)
        a += v.contentArea;
    return a;
}

void ingest(Digestion& dig, RingBody& body, BodyParticles& bp, Vec2 where, float area, float pmol,
            std::uint8_t r, std::uint8_t g, std::uint8_t b, bool forceNew, float phosphorus,
            float nutritionScale) {
    FoodVacuole* target = nullptr;
    if (!forceNew) {
        for (FoodVacuole& v : dig.vacuoles)
            if (v.open)
                target = &v;
    }
    if (!target) {
        const std::uint32_t slot = bp.allocate(BodyKind::Interior, where.x, where.y,
                                               vacuoleRadius(area), 0.5f, body.id);
        // En una ameba (sin forma propia) el marco del cuerpo no significa nada: las vacuolas
        // flotan libres con la corriente citoplasmática en vez de anclarse.
        const float anchor = body.membraneReservoirRate > 0.0f ? 0.0f : 2.0f;
        soft_body::addOrganelle(body, slot, soft_body::toLocal(body, where), anchor);
        dig.vacuoles.push_back({slot});
        target = &dig.vacuoles.back();
        target->r = r;
        target->g = g;
        target->b = b;
        target->open = !forceNew;
        target->wholeCell = forceNew;
        target->nutritionScale = nutritionScale;
    }
    target->contentArea += area;
    target->contentPmol += pmol;
    target->initialPmol += pmol;
    target->phosphorus += phosphorus;
    ++target->items;
    bp.radius[target->slot] = vacuoleRadius(target->contentArea);
    dig.pmolIngested += pmol;
}

float digest(Digestion& dig, RingBody& body, BodyParticles& bp, const FeedingParams& params,
             float dt, float bioDt, float& excreted) {
    const float fraction = 1.0f - std::exp(-params.digestionRate * dig.rateScale * bioDt);
    const float wholeFraction = 1.0f - std::exp(-dig.rateScale * bioDt / (params.wholeCellDigestionHours * 3600.0f));
    float digested = 0.0f;
    for (std::size_t k = 0; k < dig.vacuoles.size();) {
        FoodVacuole& v = dig.vacuoles[k];
        v.age += dt;
        const std::size_t idx = interiorIndex(body, v.slot);
        if (v.open && (v.age > params.vacuoleOpenTime || v.items >= params.vacuoleCapacity)) {
            // La vacuola se cierra y se suelta del citostoma: queda flotando en el citoplasma.
            v.open = false;
            if (idx < body.anchorRate.size()) {
                body.anchorRate[idx] = body.membraneReservoirRate > 0.0f ? 0.0f : 0.3f;
                body.restInterior[idx] = soft_body::toLocal(body, {bp.x[v.slot], bp.y[v.slot]});
            }
        }
        if (!v.open) {
            const float f = v.wholeCell ? wholeFraction : fraction;
            const float d = v.contentPmol * f;
            const float before = v.contentPmol;
            v.contentPmol -= d;
            dig.pmolDigested += d;
            digested += d * v.nutritionScale;
            // El fósforo de la presa se excreta al agua a medida que se digiere.
            const float pOut = v.phosphorus * f;
            v.phosphorus -= pOut;
            excreted += pOut;
            v.contentArea *= before > 0.0f ? v.contentPmol / before : 0.0f;
            bp.radius[v.slot] = vacuoleRadius(v.contentArea);
        }
        if (!v.open && v.contentPmol < 0.02f * v.initialPmol) {
            excreted += v.phosphorus; // lo que quedaba se expulsa con la vacuola
            if (idx < body.interior.size())
                soft_body::removeInterior(body, idx);
            bp.release(v.slot);
            dig.vacuoles.erase(dig.vacuoles.begin() + static_cast<std::ptrdiff_t>(k));
            continue;
        }
        ++k;
    }
    return digested;
}

bool satiated(const Digestion& dig) {
    for (const FoodVacuole& v : dig.vacuoles)
        if (v.wholeCell && v.contentPmol > 0.2f * v.initialPmol)
            return true;
    return false;
}
