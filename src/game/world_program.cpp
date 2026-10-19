// Programa temporal de la placa (fase P1, paso 12): lo que en el laboratorio se cambia con el reloj
// (fotoperíodo, rampas de temperatura, pulsos de nutrientes, inóculos periódicos), aplicado una vez
// por paso con las horas biológicas. Es parte de la placa (`WorldConfig::program`): una escena lo
// repite sin guardar un evento por paso.
#include "game/world.hpp"

#include "core/profiling.hpp"
#include "core/units.hpp"
#include "game/parameters.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

ChemField* World::soluteByKey(const std::string& key) {
    return key == "phosphate"     ? &phosphate_
           : key == "nitrogen" ? &nitrogen_
           : key == "silicate"    ? &silicate_
           : key == "iron"    ? &iron_
           : key == "b12"       ? &vitamin_
           : key == "organic"  ? &nutrients_
           : key == "co2"       ? &co2_
                                : nullptr;
}

void World::applyProgram(float dt) {
    PROFILE_SCOPE("World::applyProgram");
    // La hora de laboratorio de la placa: las horas biológicas (sin el Q10: es el reloj).
    const double from = simTime_ * bioTimeScale_ / 3600.0;
    const double to = (simTime_ + dt) * bioTimeScale_ / 3600.0;
    bool medium = false;
    for (const ProgramEntry& e : program_.entries) {
        if (e.isAction()) {
            const int times = programEvents(e, from, to);
            for (int k = 0; k < times; ++k) {
                if (e.target.rfind("inoculate:", 0) == 0) {
                    if (const std::optional<SpeciesId> id = findSpecies(e.target.substr(10)))
                        inoculate({BeingKind::Species, *id}, static_cast<int>(e.amount), {}, 0.0f);
                } else if (e.target == "bacteria") {
                    // Por mL de la gota: la placa tiene el volumen de su película.
                    const double ul = units::kPi * dish_.radius * dish_.radius * fluid_.gapHeight * 1e-9;
                    inoculate({BeingKind::Bacterium, SpeciesId{}}, static_cast<int>(e.amount * ul * 1e-3), {}, 0.0f);
                } else if (e.target.rfind("add:", 0) == 0) {
                    if (ChemField* f = soluteByKey(e.target.substr(4)))
                        setMeanConcentration(*f, std::max(0.0f, meanConcentration(*f) + e.amount));
                }
                // "medio" y "dilucion" son del cultivo bien mezclado; en la placa está la perfusión.
            }
            continue;
        }
        float v;
        if (!programValue(e, to, v))
            continue;
        if (e.target == "temperature") {
            medium_.temperature = v;
            medium = true;
        } else if (e.target == "light") {
            medium_.surfaceLight = std::max(0.0f, v);
        } else if (e.target.rfind("solute:", 0) == 0) {
            if (ChemField* f = soluteByKey(e.target.substr(7)))
                setMeanConcentration(*f, std::max(0.0f, v));
        } else if (e.target.rfind("parameter:", 0) == 0) {
            if (const ParamInfo* p = findParameter(e.target.substr(10)))
                setParameterValue(*this, *p, v);
        }
    }
    if (medium)
        refreshMedium(); // Q10, viscosidad y gases de equilibrio
}
